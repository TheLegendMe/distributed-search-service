#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

// 文件上传信息
struct FileUploadInfo {
    std::string file_hash;       // 文件MD5哈希
    std::string filename;        // 原始文件名
    std::string file_path;       // 存储路径
    std::string folder;          // 文件夹路径（可选）
    size_t total_size;           // 总大小
    size_t uploaded_size;        // 已上传大小
    int total_chunks;            // 总分片数
    std::vector<bool> uploaded_chunks;  // 已上传的分片
    std::string upload_time;     // 上传时间
    bool completed;              // 是否完成
};

// 文件存储管理器
class FileStorageManager {
public:
    FileStorageManager(const std::string &storage_dir, const std::string &redis_host, int redis_port);
    ~FileStorageManager();
    
    // 检查文件是否已存在（秒传）
    bool checkFileExists(const std::string &file_hash, std::string &file_path);
    
    // 初始化分片上传
    std::string initChunkUpload(const std::string &filename, const std::string &file_hash, 
                               size_t total_size, int total_chunks, const std::string &folder = "");
    
    // 上传分片
    bool uploadChunk(const std::string &upload_id, int chunk_index, 
                    const std::string &chunk_data);
    
    // 合并分片
    bool mergeChunks(const std::string &upload_id, std::string &final_path);
    
    // 获取上传进度
    bool getUploadProgress(const std::string &upload_id, FileUploadInfo &info);
    
    // 删除文件
    bool deleteFile(const std::string &file_hash);
    
    // 获取文件路径
    bool getFilePath(const std::string &file_hash, std::string &path);
    
private:
    std::string storage_dir_;
    std::string temp_dir_;
    std::unordered_map<std::string, FileUploadInfo> upload_sessions_;
    std::mutex mutex_;
    
    // Redis连接（用于分布式环境下的状态同步）
    void* redis_ctx_;
    std::string redis_host_;
    int redis_port_;
    
    bool connectRedis();
    void disconnectRedis();
    
    // 计算文件MD5
    std::string calculateMD5(const std::string &file_path);
    
    // 保存上传信息到Redis
    void saveUploadInfoToRedis(const std::string &upload_id, const FileUploadInfo &info);
    
    // 从Redis加载上传信息
    bool loadUploadInfoFromRedis(const std::string &upload_id, FileUploadInfo &info);
};

