#pragma once
#include <string>
#include <vector>
#include <mutex>

// 基于 cppjieba 的分词单例封装
// 说明：需要在系统中安装 cppjieba 词典；这里通过构造函数传入词典路径，
// 默认使用常见文件名，用户可在运行时设置环境变量指定路径。
class JiebaTokenizer {
public:
    static JiebaTokenizer &instance();

    // 初始化词典路径（可选）。若未调用，则使用默认路径/环境变量。
    void initialize(const std::string &dict_dir);

    // 中文分词（已做小写标准化）。
    void tokenize(const std::string &text, std::vector<std::string> &out_tokens);

private:
    JiebaTokenizer();
    ~JiebaTokenizer();
    JiebaTokenizer(const JiebaTokenizer&) = delete;
    JiebaTokenizer& operator=(const JiebaTokenizer&) = delete;

    void ensureInitialized();

    // 延迟加载，避免进程启动时耗时初始化
    bool initialized;
    std::mutex init_mutex;

    // 这几个路径来源于 cppjieba
    std::string dict_path;
    std::string hmm_path;
    std::string user_dict_path;
    std::string idf_path;
    std::string stop_words_path;

    // 前向声明，避免在头文件引入第三方头（降低编译耦合）
    struct Impl;
    Impl *impl;
};


