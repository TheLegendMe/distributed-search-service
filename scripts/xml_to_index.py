#!/usr/bin/env python3
"""
XML文件解析并添加到搜索索引
"""

import xml.etree.ElementTree as ET
import json
import sys
import hashlib
import re
import requests

def clean_html(text):
    """移除HTML标签"""
    if not text:
        return ""
    # 移除CDATA标记
    text = re.sub(r'<!\[CDATA\[(.*?)\]\]>', r'\1', text, flags=re.DOTALL)
    # 移除HTML标签
    text = re.sub(r'<[^>]+>', '', text)
    # 移除多余空白
    text = re.sub(r'\s+', ' ', text)
    return text.strip()

def parse_xml_file(xml_path):
    """解析XML文件，提取所有item"""
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        
        documents = []
        
        # 查找所有item标签
        for item in root.findall('.//item'):
            title_elem = item.find('title')
            desc_elem = item.find('description')
            link_elem = item.find('link')
            
            if link_elem is None or not link_elem.text:
                continue
            
            # 提取文本内容
            title_text = title_elem.text if title_elem is not None else ""
            desc_text = desc_elem.text if desc_elem is not None else ""
            link_text = link_elem.text.strip()
            
            # 清理HTML
            title = clean_html(title_text) if title_text else ""
            desc = clean_html(desc_text) if desc_text else ""
            
            # 至少要有标题或描述
            if not title and not desc:
                continue
            
            # 生成唯一docid（基于URL的hash）
            try:
                docid = int(hashlib.md5(link_text.encode('utf-8')).hexdigest()[:8], 16)
            except:
                continue
            
            documents.append({
                "docid": docid,
                "title": title if title else "无标题",
                "link": link_text,
                "summary": desc[:200] if len(desc) > 200 else desc,
                "text": f"{title} {desc}"
            })
        
        return documents
    
    except Exception as e:
        print(f"❌ 解析XML失败: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return []

def add_to_index(documents, search_api="http://localhost:8081/index/batch/add"):
    """批量添加文档到搜索索引"""
    if not documents:
        return {"success": False, "error": "No documents to add"}
    
    try:
        response = requests.post(
            search_api,
            json={"documents": documents},
            timeout=30
        )
        return response.json()
    except Exception as e:
        return {"success": False, "error": str(e)}

def main():
    if len(sys.argv) < 2:
        print("用法: xml_to_index.py <xml_file> [search_api_url]")
        sys.exit(1)
    
    xml_file = sys.argv[1]
    search_api = sys.argv[2] if len(sys.argv) > 2 else "http://localhost:8081/index/batch/add"
    
    print(f"📄 解析XML文件: {xml_file}")
    documents = parse_xml_file(xml_file)
    
    if not documents:
        print("⚠️  没有找到有效的文档")
        sys.exit(1)
    
    print(f"✓ 解析完成: {len(documents)} 个文档")
    
    # 批量添加到索引（每次最多100个）
    batch_size = 100
    total_added = 0
    
    for i in range(0, len(documents), batch_size):
        batch = documents[i:i+batch_size]
        print(f"📤 添加批次 {i//batch_size + 1}: {len(batch)} 个文档...")
        
        result = add_to_index(batch, search_api)
        
        if result.get("success"):
            total_added += len(batch)
            print(f"✅ 批次添加成功")
        else:
            print(f"❌ 批次添加失败: {result.get('error', 'Unknown error')}")
    
    print(f"\n🎉 完成！共添加 {total_added}/{len(documents)} 个文档到搜索索引")
    
    # 返回JSON供程序调用
    return {
        "success": True,
        "total_documents": len(documents),
        "added": total_added
    }

if __name__ == "__main__":
    result = main()
    print(json.dumps(result, ensure_ascii=False))

