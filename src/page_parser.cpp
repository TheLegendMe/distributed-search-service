#include "page_parser.h"
#include "/home/oym/tinyxml2-master/tinyxml2.h"
#include <cstdlib>

using namespace tinyxml2;

static std::string getChildText(XMLElement *parent, const char *name) {
    if (!parent) return {};
    auto *elem = parent->FirstChildElement(name);
    if (!elem || !elem->GetText()) return {};
    return elem->GetText();
}

bool PageParser::parseFromXmlFile(const std::string &xml_file, std::vector<Page> &out_pages) {
    XMLDocument doc;
    if (doc.LoadFile(xml_file.c_str()) != XML_SUCCESS) return false;
    auto *root = doc.RootElement();
    if (!root) return false;

    out_pages.clear();
    for (auto *p = root->FirstChildElement("doc"); p; p = p->NextSiblingElement("doc")) {
        Page page{};
        page.id = std::atoi(getChildText(p, "docid").c_str());
        page.url = getChildText(p, "url");
        page.title = getChildText(p, "title");
        page.content = getChildText(p, "content");
        if (!page.content.empty()) out_pages.push_back(std::move(page));
    }
    return true;
}


