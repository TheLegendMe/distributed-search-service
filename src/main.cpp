#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "app_config.h"
#include "command_handler.h"

namespace {
    void printUsage(const char *prog) {
        std::cout << "Usage:\n"
                  << "  " << prog << " --build-index [config]\n"
                  << "      Build search index from XML files\n\n"
                  << "  " << prog << " --build-keywords [config]\n"
                  << "      Build keyword dictionary from corpus\n\n"
                  << "  " << prog << " --query [config] <term1> <term2> ... [topK]\n"
                  << "      Search documents by keywords\n\n"
                  << "  " << prog << " --recommend [config] <query> [topK]\n"
                  << "      Get keyword recommendations\n\n"
                  << "Config file (optional): defaults to ./conf/app.conf\n";
    }

    size_t parseTopK(std::vector<std::string> &args, size_t defaultValue) {
        if (args.empty()) return defaultValue;
        try {
            size_t pos = 0;
            int maybe = std::stoi(args.back(), &pos);
            if (pos == args.back().size() && maybe > 0) {
                args.pop_back();
                return static_cast<size_t>(maybe);
            }
        } catch (...) {}
        return defaultValue;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    
    // 默认配置文件路径
    std::string config_path = "./conf/app.conf";
    
    // 加载配置
    AppConfig config;
    if (!loadAppConfig(config_path, config)) {
        std::cerr << "Warning: Could not load config from " << config_path 
                  << ", using defaults\n";
    }

    try {
        std::unique_ptr<CommandHandler> handler;

        if (command == "--build-index") {
            // 可选：允许指定配置文件
            if (argc >= 3) {
                config_path = argv[2];
                if (!loadAppConfig(config_path, config)) {
                    std::cerr << "Failed to load config from " << config_path << "\n";
                    return 1;
                }
            }
            handler = std::make_unique<BuildIndexCommand>(config);
        }
        else if (command == "--build-keywords") {
            if (argc >= 3) {
                config_path = argv[2];
                if (!loadAppConfig(config_path, config)) {
                    std::cerr << "Failed to load config from " << config_path << "\n";
                    return 1;
                }
            }
            handler = std::make_unique<BuildKeywordDictCommand>(config);
        }
        else if (command == "--query") {
            int start_idx = 2;
            // 检查是否指定了配置文件
            if (argc >= 3 && argv[2][0] != '-') {
                std::string maybe_config = argv[2];
                if (maybe_config.find(".conf") != std::string::npos || 
                    maybe_config.find('/') != std::string::npos) {
                    config_path = maybe_config;
                    if (!loadAppConfig(config_path, config)) {
                        std::cerr << "Failed to load config from " << config_path << "\n";
                        return 1;
                    }
                    start_idx = 3;
                }
            }
            
            std::vector<std::string> terms;
            for (int i = start_idx; i < argc; ++i) {
                terms.push_back(argv[i]);
            }
            
            size_t topK = parseTopK(terms, config.default_topk);
            handler = std::make_unique<QueryCommand>(config, terms, topK);
        }
        else if (command == "--recommend") {
            int start_idx = 2;
            if (argc >= 3 && argv[2][0] != '-') {
                std::string maybe_config = argv[2];
                if (maybe_config.find(".conf") != std::string::npos || 
                    maybe_config.find('/') != std::string::npos) {
                    config_path = maybe_config;
                    if (!loadAppConfig(config_path, config)) {
                        std::cerr << "Failed to load config from " << config_path << "\n";
                        return 1;
                    }
                    start_idx = 3;
                }
            }
            
            std::vector<std::string> parts;
            for (int i = start_idx; i < argc; ++i) {
                parts.push_back(argv[i]);
            }
            
            size_t topK = parseTopK(parts, config.recommend_topk);
            
            std::string input;
            for (size_t i = 0; i < parts.size(); ++i) {
                input += parts[i];
                if (i + 1 < parts.size()) input += ' ';
            }
            
            handler = std::make_unique<RecommendCommand>(config, input, topK);
        }
        else {
            std::cerr << "Unknown command: " << command << "\n\n";
            printUsage(argv[0]);
            return 1;
        }

        return handler->execute();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
