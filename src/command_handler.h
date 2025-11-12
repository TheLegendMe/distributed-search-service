#pragma once
#include <string>
#include <vector>
#include "app_config.h"

// 命令处理器基类
class CommandHandler {
public:
    virtual ~CommandHandler() = default;
    virtual int execute() = 0;
};

// 构建索引命令
class BuildIndexCommand : public CommandHandler {
public:
    explicit BuildIndexCommand(const AppConfig &cfg);
    int execute() override;
private:
    AppConfig config_;
};

// 关键词字典构建命令
class BuildKeywordDictCommand : public CommandHandler {
public:
    explicit BuildKeywordDictCommand(const AppConfig &cfg);
    int execute() override;
private:
    AppConfig config_;
};

// 查询命令
class QueryCommand : public CommandHandler {
public:
    QueryCommand(const AppConfig &cfg, const std::vector<std::string> &terms, size_t topK);
    int execute() override;
private:
    AppConfig config_;
    std::vector<std::string> terms_;
    size_t topK_;
};

// 关键词推荐命令
class RecommendCommand : public CommandHandler {
public:
    RecommendCommand(const AppConfig &cfg, const std::string &input, size_t topK);
    int execute() override;
private:
    AppConfig config_;
    std::string input_;
    size_t topK_;
};

