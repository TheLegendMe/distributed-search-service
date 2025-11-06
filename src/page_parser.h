#pragma once
#include <string>
#include <vector>

// 单个网页的结构
struct Page {
    int docid;
    std::string link;    
    std::string title;
    std::string description; // 正文纯文本
};

// 使用 tinyxml2 解析 XML/HTML（示例按如下结构获取字段）：
// <doc>
//   <docid>1</docid>
//   <link>https://example.com</link>
//   <title>标题</title>
//   <description>正文...</description>
// </doc>
// 若你的真实数据结构不同，请在实现中调整标签名。
namespace PageParser {
    // 从单个 XML 文件解析出所有 Page
    bool parseFromXmlFile(const std::string &xml_file, std::vector<Page> &out_pages);
}


