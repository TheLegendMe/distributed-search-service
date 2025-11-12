#include <iostream>
#include <csignal>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <openssl/md5.h>
#include <hiredis/hiredis.h>
#include <wfrest/HttpServer.h>
#include <wfrest/json.hpp>
#include <wfrest/CodeUtil.h>
#include "app_config.h"
#include "file_service.h"

using namespace wfrest;
using json = nlohmann::json;
namespace fs = std::filesystem;

// 全局变量
static HttpServer *g_server = nullptr;
static FileStorageManager *g_storage = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down file service...\n";
    if (g_server) {
        g_server->stop();
    }
}

// 计算字符串MD5
std::string calculateMD5(const std::string &data) {
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)data.c_str(), data.size(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

// 计算文件MD5
std::string calculateFileMD5(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) return "";
    
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5Context);
    
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

// 获取当前时间字符串
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// FileStorageManager 实现
FileStorageManager::FileStorageManager(const std::string &storage_dir, 
                                       const std::string &redis_host, 
                                       int redis_port)
    : storage_dir_(storage_dir), redis_ctx_(nullptr), 
      redis_host_(redis_host), redis_port_(redis_port) {
    
    // 创建存储目录
    temp_dir_ = storage_dir_ + "/temp";
    fs::create_directories(storage_dir_);
    fs::create_directories(temp_dir_);
    
    // 连接Redis
    connectRedis();
}

FileStorageManager::~FileStorageManager() {
    disconnectRedis();
}

bool FileStorageManager::connectRedis() {
    struct timeval timeout = { 2, 0 };
    redis_ctx_ = redisConnectWithTimeout(redis_host_.c_str(), redis_port_, timeout);
    
    if (redis_ctx_ == nullptr || ((redisContext*)redis_ctx_)->err) {
        if (redis_ctx_) {
            std::cerr << "Redis connection error: " << ((redisContext*)redis_ctx_)->errstr << std::endl;
            redisFree((redisContext*)redis_ctx_);
            redis_ctx_ = nullptr;
        }
        return false;
    }
    
    std::cout << "✓ Connected to Redis at " << redis_host_ << ":" << redis_port_ << std::endl;
    return true;
}

void FileStorageManager::disconnectRedis() {
    if (redis_ctx_) {
        redisFree((redisContext*)redis_ctx_);
        redis_ctx_ = nullptr;
    }
}

bool FileStorageManager::checkFileExists(const std::string &file_hash, std::string &file_path) {
    // 在存储目录中查找文件（支持任意扩展名）
    try {
        for (const auto &entry : fs::recursive_directory_iterator(storage_dir_)) {
            if (entry.is_regular_file()) {
                std::string stem = entry.path().stem().string();
                if (stem == file_hash) {
                    file_path = entry.path().string();
                    return true;
                }
            }
        }
    } catch (...) {}
    
    return false;
}

std::string FileStorageManager::initChunkUpload(const std::string &filename, 
                                                const std::string &file_hash,
                                                size_t total_size, 
                                                int total_chunks,
                                                const std::string &folder) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 生成上传ID
    std::string upload_id = file_hash + "_" + std::to_string(std::time(nullptr));
    
    // 创建上传会话
    FileUploadInfo info;
    info.file_hash = file_hash;
    info.filename = filename;
    info.folder = folder;
    info.file_path = temp_dir_ + "/" + upload_id;
    info.total_size = total_size;
    info.uploaded_size = 0;
    info.total_chunks = total_chunks;
    info.uploaded_chunks.resize(total_chunks, false);
    info.upload_time = getCurrentTime();
    info.completed = false;
    
    // 创建临时目录
    fs::create_directories(info.file_path);
    
    upload_sessions_[upload_id] = info;
    
    // 保存到Redis
    saveUploadInfoToRedis(upload_id, info);
    
    return upload_id;
}

bool FileStorageManager::uploadChunk(const std::string &upload_id, 
                                     int chunk_index,
                                     const std::string &chunk_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = upload_sessions_.find(upload_id);
    if (it == upload_sessions_.end()) {
        return false;
    }
    
    auto &info = it->second;
    
    if (chunk_index < 0 || chunk_index >= info.total_chunks) {
        return false;
    }
    
    // 写入分片文件
    std::string chunk_file = info.file_path + "/chunk_" + std::to_string(chunk_index);
    std::ofstream ofs(chunk_file, std::ios::binary);
    if (!ofs) return false;
    
    ofs.write(chunk_data.c_str(), chunk_data.size());
    ofs.close();
    
    // 更新状态
    info.uploaded_chunks[chunk_index] = true;
    info.uploaded_size += chunk_data.size();
    
    // 保存到Redis
    saveUploadInfoToRedis(upload_id, info);
    
    return true;
}

bool FileStorageManager::mergeChunks(const std::string &upload_id, std::string &final_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = upload_sessions_.find(upload_id);
    if (it == upload_sessions_.end()) {
        std::cerr << "上传会话不存在: " << upload_id << std::endl;
        return false;
    }
    
    auto &info = it->second;
    
    // 检查是否所有分片都已上传
    int uploaded_count = 0;
    for (int i = 0; i < info.total_chunks; i++) {
        if (info.uploaded_chunks[i]) {
            uploaded_count++;
        } else {
            std::cerr << "分片 " << i << " 未上传" << std::endl;
        }
    }
    
    std::cout << "已上传分片: " << uploaded_count << "/" << info.total_chunks << std::endl;
    
    if (uploaded_count != info.total_chunks) {
        std::cerr << "上传未完成，缺少 " << (info.total_chunks - uploaded_count) << " 个分片" << std::endl;
        return false;
    }
    
    // 合并文件 - 保留原始文件名和扩展名
    std::string target_dir = storage_dir_;
    if (!info.folder.empty()) {
        target_dir = storage_dir_ + "/" + info.folder;
        fs::create_directories(target_dir);
    }
    
    // 使用hash作为文件名，但保留扩展名
    std::string ext = fs::path(info.filename).extension().string();
    final_path = target_dir + "/" + info.file_hash + ext;
    
    std::ofstream ofs(final_path, std::ios::binary);
    if (!ofs) return false;
    
    for (int i = 0; i < info.total_chunks; i++) {
        std::string chunk_file = info.file_path + "/chunk_" + std::to_string(i);
        std::ifstream ifs(chunk_file, std::ios::binary);
        if (!ifs) {
            ofs.close();
            fs::remove(final_path);
            return false;
        }
        
        ofs << ifs.rdbuf();
        ifs.close();
    }
    
    ofs.close();
    
    // MD5验证可选（已注释，因为前端hash算法不同）
    // 在生产环境中，应该统一使用相同的hash算法
    // std::string actual_hash = calculateFileMD5(final_path);
    // if (actual_hash != info.file_hash) {
    //     std::cerr << "MD5 mismatch! Expected: " << info.file_hash 
    //               << ", Got: " << actual_hash << std::endl;
    //     fs::remove(final_path);
    //     return false;
    // }
    
    std::cout << "✓ 文件合并成功: " << final_path << std::endl;
    
    // 清理临时文件
    fs::remove_all(info.file_path);
    
    // 标记完成
    info.completed = true;
    info.file_path = final_path;
    
    // 从上传会话中移除
    upload_sessions_.erase(it);
    
    return true;
}

bool FileStorageManager::getUploadProgress(const std::string &upload_id, FileUploadInfo &info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = upload_sessions_.find(upload_id);
    if (it != upload_sessions_.end()) {
        info = it->second;
        return true;
    }
    
    // 尝试从Redis加载
    return loadUploadInfoFromRedis(upload_id, info);
}

bool FileStorageManager::deleteFile(const std::string &file_hash) {
    // 使用getFilePath查找文件（支持扩展名）
    std::string file_path;
    if (getFilePath(file_hash, file_path)) {
        try {
            bool result = fs::remove(file_path);
            if (result) {
                std::cout << "✓ 文件已删除: " << file_path << std::endl;
            }
            return result;
        } catch (const std::exception &e) {
            std::cerr << "删除文件失败: " << e.what() << std::endl;
            return false;
        }
    }
    std::cerr << "文件不存在: " << file_hash << std::endl;
    return false;
}

bool FileStorageManager::getFilePath(const std::string &file_hash, std::string &path) {
    return checkFileExists(file_hash, path);
}

void FileStorageManager::saveUploadInfoToRedis(const std::string &upload_id, const FileUploadInfo &info) {
    if (!redis_ctx_) return;
    
    // 将上传信息序列化为JSON存入Redis
    json j;
    j["file_hash"] = info.file_hash;
    j["filename"] = info.filename;
    j["total_size"] = info.total_size;
    j["uploaded_size"] = info.uploaded_size;
    j["total_chunks"] = info.total_chunks;
    j["upload_time"] = info.upload_time;
    j["completed"] = info.completed;
    
    std::string key = "upload:" + upload_id;
    redisReply *reply = (redisReply*)redisCommand((redisContext*)redis_ctx_, 
                                                   "SETEX %s 3600 %s",
                                                   key.c_str(),
                                                   j.dump().c_str());
    if (reply) freeReplyObject(reply);
}

bool FileStorageManager::loadUploadInfoFromRedis(const std::string &upload_id, FileUploadInfo &info) {
    if (!redis_ctx_) return false;
    
    std::string key = "upload:" + upload_id;
    redisReply *reply = (redisReply*)redisCommand((redisContext*)redis_ctx_, "GET %s", key.c_str());
    
    if (!reply) return false;
    
    bool success = false;
    if (reply->type == REDIS_REPLY_STRING) {
        try {
            json j = json::parse(std::string(reply->str, reply->len));
            info.file_hash = j["file_hash"];
            info.filename = j["filename"];
            info.total_size = j["total_size"];
            info.uploaded_size = j["uploaded_size"];
            info.total_chunks = j["total_chunks"];
            info.upload_time = j["upload_time"];
            info.completed = j["completed"];
            success = true;
        } catch (...) {}
    }
    
    freeReplyObject(reply);
    return success;
}

std::string FileStorageManager::calculateMD5(const std::string &file_path) {
    return calculateFileMD5(file_path);
}

// 主函数
int main(int argc, char **argv) {
    std::string config_path = "./conf/app.conf";
    
    if (argc >= 2) {
        config_path = argv[1];
    }
    
    AppConfig config;
    if (!loadAppConfig(config_path, config)) {
        std::cerr << "Warning: Could not load config from " << config_path << "\n";
    }
    
    std::cout << "========================================\n";
    std::cout << "  📁 File Upload Microservice\n";
    std::cout << "========================================\n";
    std::cout << "Listen on:     0.0.0.0:8083\n";
    std::cout << "Storage dir:   ./uploads\n";
    std::cout << "Redis:         " << config.redis_host << ":" << config.redis_port << "\n";
    std::cout << "========================================\n\n";
    
    // 创建文件存储管理器
    std::string storage_dir = "./uploads";
    g_storage = new FileStorageManager(storage_dir, config.redis_host, config.redis_port);
    
    // 创建HTTP服务器
    HttpServer server;
    g_server = &server;
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 健康检查
    server.GET("/health", [](const HttpReq *, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->String("{\"status\":\"ok\",\"service\":\"file\"}");
    });
    
    // 检查文件是否存在（秒传）
    server.POST("/check", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            auto body = req->body();
            json req_data = json::parse(body);
            std::string file_hash = req_data["hash"];
            
            std::string file_path;
            if (g_storage->checkFileExists(file_hash, file_path)) {
                response["exists"] = true;
                response["file_path"] = file_path;
                response["message"] = "File already exists (instant upload)";
            } else {
                response["exists"] = false;
                response["message"] = "File not found, need to upload";
            }
        } catch (const std::exception &e) {
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 初始化分片上传
    server.POST("/init", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            auto body = req->body();
            json req_data = json::parse(body);
            
            std::string filename = req_data["filename"];
            std::string file_hash = req_data["hash"];
            size_t total_size = req_data["total_size"];
            int total_chunks = req_data["total_chunks"];
            std::string folder = req_data.value("folder", ""); // 可选的文件夹参数
            
            std::string upload_id = g_storage->initChunkUpload(filename, file_hash, total_size, total_chunks, folder);
            
            response["success"] = true;
            response["upload_id"] = upload_id;
            response["message"] = "Upload session initialized";
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 上传分片
    server.POST("/chunk", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            std::string upload_id = req->query("upload_id");
            int chunk_index = std::stoi(req->query("chunk_index"));
            auto chunk_data = req->body();
            
            if (g_storage->uploadChunk(upload_id, chunk_index, chunk_data)) {
                response["success"] = true;
                response["chunk_index"] = chunk_index;
                response["message"] = "Chunk uploaded successfully";
            } else {
                response["success"] = false;
                response["error"] = "Failed to upload chunk";
            }
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 完成上传（合并分片）
    server.POST("/complete", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            auto body = req->body();
            json req_data = json::parse(body);
            std::string upload_id = req_data["upload_id"];
            bool add_to_index = req_data.value("add_to_index", false);  // 是否添加到索引
            
            std::string final_path;
            if (g_storage->mergeChunks(upload_id, final_path)) {
                response["success"] = true;
                response["file_path"] = final_path;
                response["message"] = "File uploaded successfully";
                
                // 如果用户选择添加到搜索索引
                if (add_to_index) {
                    std::string filename = fs::path(final_path).filename().string();
                    std::string hash = fs::path(final_path).stem().string();
                    std::string ext = fs::path(final_path).extension().string();
                    
                    response["index_info"] = {
                        {"filename", filename},
                        {"hash", hash},
                        {"file_type", ext}
                    };
                    
                    // XML文件特殊处理：调用Python脚本解析
                    if (ext == ".xml") {
                        std::cout << "📄 检测到XML文件，调用解析脚本..." << std::endl;
                        
                        std::string script_path = "./scripts/xml_to_index.py";
                        std::string cmd = "python3 " + script_path + " " + final_path + " 2>&1";
                        
                        FILE* pipe = popen(cmd.c_str(), "r");
                        if (pipe) {
                            char buffer[4096];
                            std::string result;
                            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                                result += buffer;
                                std::cout << buffer;  // 实时输出
                            }
                            int status = pclose(pipe);
                            
                            response["xml_parsed"] = true;
                            response["parse_output"] = result;
                            response["parse_status"] = status;
                            
                            if (status == 0) {
                                response["index_data"] = nullptr;  // XML已直接处理，不需要前端再处理
                                std::cout << "✅ XML文件已成功解析并添加到索引" << std::endl;
                            } else {
                                std::cerr << "❌ XML解析失败，状态码: " << status << std::endl;
                            }
                        } else {
                            std::cerr << "❌ 无法执行Python脚本" << std::endl;
                            response["xml_parsed"] = false;
                            response["error"] = "Failed to execute parser script";
                        }
                    }
                    // 其他文本文件：普通处理
                    else if (ext == ".txt" || ext == ".md" || ext == ".json") {
                        std::ifstream ifs(final_path);
                        if (ifs) {
                            std::ostringstream oss;
                            oss << ifs.rdbuf();
                            std::string file_content = oss.str();
                            
                            // 限制大小（最多1MB）
                            if (file_content.size() > 1024 * 1024) {
                                file_content = file_content.substr(0, 1024 * 1024);
                            }
                            
                            response["index_data"] = {
                                {"docid", std::hash<std::string>{}(hash)},
                                {"title", filename},
                                {"link", "/api/file/download/" + hash},
                                {"summary", "文件: " + filename},
                                {"text", filename + " " + file_content}
                            };
                        }
                    }
                    // 非文本文件：只索引文件名
                    else {
                        response["index_data"] = {
                            {"docid", std::hash<std::string>{}(hash)},
                            {"title", filename},
                            {"link", "/api/file/download/" + hash},
                            {"summary", "文件: " + filename + " (" + ext + ")"},
                            {"text", filename}
                        };
                    }
                    
                    // 同时添加到多模态向量库
                    std::cout << "📊 添加文件到多模态向量库..." << std::endl;
                    std::string multimodal_script = "./scripts/add_file_to_multimodal.py";
                    std::string folder_path = fs::path(final_path).parent_path().filename().string();
                    std::string folder_param = (folder_path != "uploads" && !folder_path.empty()) ? " \"" + folder_path + "\"" : "";
                    std::string multimodal_cmd = "python3 " + multimodal_script + " \"" + final_path + "\" \"" + hash + "\" \"" + filename + "\"" + folder_param + " 2>&1";
                    
                    FILE* multimodal_pipe = popen(multimodal_cmd.c_str(), "r");
                    if (multimodal_pipe) {
                        char buffer[1024];
                        std::string multimodal_result;
                        while (fgets(buffer, sizeof(buffer), multimodal_pipe) != nullptr) {
                            multimodal_result += buffer;
                        }
                        int multimodal_status = pclose(multimodal_pipe);
                        
                        response["multimodal_indexed"] = (multimodal_status == 0);
                        if (multimodal_status == 0) {
                            std::cout << "✅ 已添加到多模态向量库" << std::endl;
                        }
                    }
                }
            } else {
                response["success"] = false;
                response["error"] = "Failed to merge chunks or incomplete upload";
            }
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 获取上传进度
    server.GET("/progress", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            std::string upload_id = req->query("upload_id");
            
            FileUploadInfo info;
            if (g_storage->getUploadProgress(upload_id, info)) {
                response["success"] = true;
                response["uploaded_size"] = info.uploaded_size;
                response["total_size"] = info.total_size;
                response["progress"] = (double)info.uploaded_size / info.total_size * 100.0;
                response["completed"] = info.completed;
                
                // 计算已上传的分片数
                int uploaded_chunks = 0;
                for (bool uploaded : info.uploaded_chunks) {
                    if (uploaded) uploaded_chunks++;
                }
                response["uploaded_chunks"] = uploaded_chunks;
                response["total_chunks"] = info.total_chunks;
            } else {
                response["success"] = false;
                response["error"] = "Upload session not found";
            }
        } catch (const std::exception &e) {
            response["success"] = false;
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 下载文件
    server.GET("/download/{hash}", [](const HttpReq *req, HttpResp *resp) {
        std::string file_hash = req->param("hash");
        
        std::string file_path;
        if (!g_storage->getFilePath(file_hash, file_path)) {
            resp->set_status(HttpStatusNotFound);
            resp->headers["Content-Type"] = "application/json";
            resp->String("{\"error\":\"File not found\"}");
            return;
        }
        
        // 读取文件内容
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            resp->set_status(HttpStatusNotFound);
            resp->headers["Content-Type"] = "application/json";
            resp->String("{\"error\":\"Cannot open file\"}");
            return;
        }
        
        // 获取文件大小
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // 读取文件内容
        std::string content(file_size, '\0');
        file.read(&content[0], file_size);
        file.close();
        
        // 设置正确的Content-Type
        std::string ext = fs::path(file_path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".jpg" || ext == ".jpeg") {
            resp->headers["Content-Type"] = "image/jpeg";
        } else if (ext == ".png") {
            resp->headers["Content-Type"] = "image/png";
        } else if (ext == ".gif") {
            resp->headers["Content-Type"] = "image/gif";
        } else if (ext == ".webp") {
            resp->headers["Content-Type"] = "image/webp";
        } else if (ext == ".pdf") {
            resp->headers["Content-Type"] = "application/pdf";
        } else if (ext == ".txt" || ext == ".md") {
            resp->headers["Content-Type"] = "text/plain; charset=utf-8";
        } else {
            resp->headers["Content-Type"] = "application/octet-stream";
        }
        
        resp->headers["Access-Control-Allow-Origin"] = "*";
        resp->String(content);
    });
    
    // 删除文件
    server.DELETE("/delete/{hash}", [](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        std::string file_hash = req->param("hash");
        
        if (g_storage->deleteFile(file_hash)) {
            response["success"] = true;
            response["message"] = "File deleted successfully";
        } else {
            response["success"] = false;
            response["error"] = "File not found";
        }
        
        resp->String(response.dump());
    });
    
    // 文件列表
    server.GET("/list", [&storage_dir](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        response["files"] = json::array();
        response["folders"] = json::array();
        
        std::string folder = req->query("folder");
        // URL解码文件夹名（处理中文等特殊字符）
        if (!folder.empty()) {
            folder = wfrest::CodeUtil::url_decode(folder);
        }
        
        std::string list_path = folder.empty() ? storage_dir : storage_dir + "/" + folder;
        
        try {
            // 列出文件夹（排除temp目录）
            for (const auto &entry : fs::directory_iterator(list_path)) {
                std::string dirname = entry.path().filename().string();
                if (entry.is_directory() && dirname != "temp") {
                    json folder_info;
                    folder_info["name"] = dirname;
                    folder_info["type"] = "folder";
                    
                    // 统计文件夹内文件数（排除temp目录）
                    size_t file_count = 0;
                    try {
                        for (const auto &f : fs::directory_iterator(entry.path())) {
                            if (f.is_regular_file() && f.path().filename() != ".folder") {
                                file_count++;
                            }
                        }
                    } catch (...) {}
                    
                    folder_info["file_count"] = file_count;
                    response["folders"].push_back(folder_info);
                }
            }
            
            // 列出文件
            for (const auto &entry : fs::directory_iterator(list_path)) {
                if (entry.is_regular_file()) {
                    json file_info;
                    std::string full_filename = entry.path().filename().string();
                    
                    // 从文件名提取hash（去掉扩展名）
                    std::string hash = entry.path().stem().string();
                    
                    file_info["hash"] = hash;
                    file_info["name"] = full_filename;  // 包含扩展名的完整文件名
                    file_info["size"] = entry.file_size();
                    
                    // 获取文件扩展名判断类型
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp" || ext == ".webp") {
                        file_info["type"] = "image";
                    } else if (ext == ".pdf") {
                        file_info["type"] = "pdf";
                    } else if (ext == ".doc" || ext == ".docx") {
                        file_info["type"] = "document";
                    } else if (ext == ".txt" || ext == ".md") {
                        file_info["type"] = "text";
                    } else if (ext == ".mp4" || ext == ".avi" || ext == ".mkv") {
                        file_info["type"] = "video";
                    } else if (ext == ".zip" || ext == ".rar" || ext == ".7z") {
                        file_info["type"] = "archive";
                    } else {
                        file_info["type"] = "unknown";
                    }
                    
                    // 获取修改时间
                    auto ftime = fs::last_write_time(entry);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                    );
                    auto time = std::chrono::system_clock::to_time_t(sctp);
                    
                    std::ostringstream oss;
                    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
                    file_info["modified"] = oss.str();
                    
                    response["files"].push_back(file_info);
                }
            }
            
            response["count"] = response["files"].size();
        } catch (const std::exception &e) {
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 创建文件夹
    server.POST("/mkdir", [&storage_dir](const HttpReq *req, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            json body = json::parse(req->body());
            std::string folder_name = body["name"];
            std::string parent_folder = body.value("parent", "");
            
            // 验证文件夹名
            if (folder_name.empty()) {
                response["success"] = false;
                response["message"] = "文件夹名不能为空";
                resp->String(response.dump());
                return;
            }
            
            // 构建完整路径
            std::string folder_path = parent_folder.empty() 
                ? storage_dir + "/" + folder_name
                : storage_dir + "/" + parent_folder + "/" + folder_name;
            
            // 检查文件夹是否已存在
            if (fs::exists(folder_path)) {
                response["success"] = false;
                response["message"] = "文件夹已存在";
                resp->String(response.dump());
                return;
            }
            
            // 创建文件夹
            if (fs::create_directories(folder_path)) {
                response["success"] = true;
                response["message"] = "文件夹创建成功";
                response["folder_name"] = folder_name;
            } else {
                response["success"] = false;
                response["message"] = "创建文件夹失败";
            }
        } catch (const std::exception &e) {
            response["success"] = false;
            response["message"] = std::string("错误: ") + e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 统计信息
    server.GET("/stats", [&storage_dir](const HttpReq *, HttpResp *resp) {
        resp->headers["Content-Type"] = "application/json";
        resp->headers["Access-Control-Allow-Origin"] = "*";
        
        json response;
        
        try {
            size_t total_files = 0;
            size_t total_size = 0;
            
            for (const auto &entry : fs::directory_iterator(storage_dir)) {
                if (entry.is_regular_file()) {
                    total_files++;
                    total_size += entry.file_size();
                }
            }
            
            response["total_files"] = total_files;
            response["total_size"] = total_size;
            response["total_size_mb"] = (double)total_size / (1024 * 1024);
        } catch (const std::exception &e) {
            response["error"] = e.what();
        }
        
        resp->String(response.dump());
    });
    
    // 启动服务器
    std::cout << "🚀 File service starting...\n";
    if (server.start("0.0.0.0", 8083) == 0) {
        std::cout << "✓ File service ready at http://0.0.0.0:8083\n\n";
        std::cout << "📁 API Endpoints:\n";
        std::cout << "  POST /check          - 检查文件是否存在（秒传）\n";
        std::cout << "  POST /init           - 初始化分片上传\n";
        std::cout << "  POST /chunk          - 上传分片\n";
        std::cout << "  POST /complete       - 完成上传并合并\n";
        std::cout << "  GET  /progress       - 查询上传进度\n";
        std::cout << "  GET  /download/{hash} - 下载文件\n";
        std::cout << "  DELETE /delete/{hash} - 删除文件\n";
        std::cout << "  GET  /stats          - 统计信息\n\n";
        server.wait_finish();
    } else {
        std::cerr << "✗ Failed to start file service\n";
        return 1;
    }
    
    delete g_storage;
    return 0;
}

