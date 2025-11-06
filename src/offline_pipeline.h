#pragma once
#include <string>
#include <vector>
#include "page_parser.h"

// 离线流程：
// (1) 建立网页库和网页偏移库（解析 XML）
// (2) 网页去重（SimHash）
// (3) 建立倒排索引（TF-IDF），并生成去重后的网页库与偏移库
class OfflinePipeline {
public:
    // 输入：多个 XML 文件路径；输出：写入到输出目录
    // 生成：
    //  - pages.bin     去重后的网页库（简易行式：docid\t link\ttitle\tdescription）
    //  - offsets.bin   偏移库（docid\toffset）
    //  - index.txt     倒排索引（term -> (docId, weight) 列表）
    bool run(const std::vector<std::string> &xml_files, const std::string &output_dir,
             int simhash_threshold = 3);
};


