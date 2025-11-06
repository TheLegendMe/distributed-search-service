#include "page_parser.h"
#include "/home/oym/tinyxml2-master/tinyxml2.h"
#include <cstdlib>
#include <iostream>

using namespace tinyxml2;

static std::string getChildText(XMLElement *parent, const char *name) {
    if (!parent) return {};
    auto *elem = parent->FirstChildElement(name);
    if (!elem || !elem->GetText()) return {};
    return elem->GetText();
}

bool PageParser::parseFromXmlFile(const std::string &xml_file, std::vector<Page> &out_pages) {
    XMLDocument doc;
    if (doc.LoadFile(xml_file.c_str()) != XML_SUCCESS) 
    {
        std::cout << "Failed to load XML file: " << xml_file << std::endl;
        return false;
    }
    auto *root = doc.RootElement(); 
    if (!root) 
    {
        std::cout << "Failed to get root element from XML file: " << xml_file << std::endl;
        return false;
    }

    out_pages.clear();
    // 1) 自定义 <doc> 结构
    for (auto *p = root->FirstChildElement("doc"); p; p = p->NextSiblingElement("doc")) {
        Page page{};
        page.docid = std::atoi(getChildText(p, "docid").c_str());
        page.link = getChildText(p, "link");
        page.title = getChildText(p, "title");
        page.description = getChildText(p, "description");
        if (!page.description.empty() || !page.title.empty()) out_pages.push_back(std::move(page));
    }
    if (!out_pages.empty()) return true;

    // 2) RSS: <rss><channel><item>
    XMLElement *channel = root->FirstChildElement("channel");
    if (!channel) {
        // 非预期格式
        return false;
    }
    int nextId = 1;
    for (auto *item = channel->FirstChildElement("item"); item; item = item->NextSiblingElement("item")) {
        Page page{};
        page.docid = nextId++;
        page.link = getChildText(item, "link");
        page.title = getChildText(item, "title");
        std::string content = getChildText(item, "content:encoded");
        if (content.empty()) content = getChildText(item, "description");
        page.description = std::move(content);
        if (!page.title.empty() || !page.description.empty()) {
            out_pages.push_back(std::move(page));
        }
    }
    return !out_pages.empty();
}


