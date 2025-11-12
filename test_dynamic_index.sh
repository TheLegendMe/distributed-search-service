#!/bin/bash
# 动态索引功能测试脚本

BASE_URL="http://localhost:8081"

echo "=========================================="
echo "   动态索引功能测试"
echo "=========================================="
echo ""

# 1. 查看初始统计
echo "📊 1. 查看索引统计"
curl -s "$BASE_URL/index/stats" | python3 -m json.tool
echo ""

# 2. 添加单个文档
echo "➕ 2. 添加新文档 (docid=99999)"
curl -s -X POST "$BASE_URL/index/add" \
  -H "Content-Type: application/json" \
  -d '{"docid": 99999, "text": "深度学习是人工智能的重要分支，包括神经网络、卷积网络等技术"}' | python3 -m json.tool
echo ""

# 3. 搜索新文档
echo "🔍 3. 搜索'人工智能'（应该包含新文档）"
curl -s "$BASE_URL/search?q=%E4%BA%BA%E5%B7%A5%E6%99%BA%E8%83%BD&topk=5" | python3 -c "
import sys, json
d = json.load(sys.stdin)
print(f'找到 {d[\"count\"]} 个结果')
for r in d['results'][:5]:
    print(f'  DocID {r[\"docid\"]}: {r[\"title\"][:60]}... (score: {r[\"score\"]:.4f})')
"
echo ""

# 4. 批量添加文档
echo "📦 4. 批量添加文档"
curl -s -X POST "$BASE_URL/index/batch/add" \
  -H "Content-Type: application/json" \
  -d '{
    "documents": [
      {"docid": 99998, "text": "云计算和大数据是现代信息技术的基础设施"},
      {"docid": 99997, "text": "区块链技术在金融领域有广泛应用"},
      {"docid": 99996, "text": "物联网连接了数十亿设备"}
    ]
  }' | python3 -m json.tool
echo ""

# 5. 更新统计
echo "📊 5. 查看更新后的统计"
curl -s "$BASE_URL/index/stats" | python3 -m json.tool
echo ""

# 6. 更新文档
echo "✏️  6. 更新文档 99999"
curl -s -X PUT "$BASE_URL/index/99999" \
  -H "Content-Type: application/json" \
  -d '{"text": "人工智能已经在医疗、金融、教育等多个领域得到应用"}' | python3 -m json.tool
echo ""

# 7. 删除文档
echo "🗑️  7. 删除文档 99998"
curl -s -X DELETE "$BASE_URL/index/99998" | python3 -m json.tool
echo ""

# 8. 最终统计
echo "📊 8. 最终统计"
curl -s "$BASE_URL/index/stats" | python3 -c "
import sys, json
d = json.load(sys.stdin)
print(f'总文档数: {d[\"total_docs\"]}')
print(f'活跃文档: {d[\"active_docs\"]}')
print(f'已删除: {d[\"deleted_docs\"]}')
print(f'词汇表大小: {d[\"total_terms\"]}')
print(f'需要压缩: {\"是\" if d[\"needs_compaction\"] else \"否\"}')
"
echo ""

# 9. 持久化索引
echo "💾 9. 保存索引到文件"
curl -s -X POST "$BASE_URL/index/save" | python3 -m json.tool
echo ""

echo "=========================================="
echo "   测试完成！"
echo "=========================================="

