#!/bin/bash
# 微服务性能基准测试

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "  📊 Microservices Performance Benchmark"
echo "========================================="
echo ""

# 检查服务是否运行
check_service() {
    local url=$1
    if ! curl -s "$url" > /dev/null 2>&1; then
        echo -e "${RED}❌ 服务未运行，请先启动: ./start_all.sh${NC}"
        exit 1
    fi
}

echo "🔍 Checking services..."
check_service "http://localhost:8081/health"
check_service "http://localhost:8082/health"
check_service "http://localhost:9999/api/health"
echo -e "${GREEN}✓ All services running${NC}"
echo ""

# 配置
REQUESTS=10000
CONCURRENCY=1000

echo "========================================="
echo "  ⚙️  Test Configuration"
echo "========================================="
echo "  Total Requests:  $REQUESTS"
echo "  Concurrency:     $CONCURRENCY"
echo "  Test Query:      '中国'"
echo ""

# 测试函数
run_benchmark() {
    local name=$1
    local url=$2
    
    echo "========================================="
    echo "  📈 Testing: $name"
    echo "========================================="
    
    if command -v ab > /dev/null 2>&1; then
        # 使用 Apache Bench
        echo "使用 Apache Bench 压力测试..."
        ab -n $REQUESTS -c $CONCURRENCY -q "$url" 2>&1 | \
        grep -E "Complete requests|Failed requests|Requests per second|Time per request|Transfer rate|50%|95%|99%" | \
        sed 's/^/  /'
    else
        # 使用简单的循环测试
        echo "使用简单测试方法..."
        local start=$(date +%s%N)
        local success=0
        local failed=0
        
        for i in $(seq 1 100); do
            if curl -s -o /dev/null -w "%{http_code}" "$url" 2>/dev/null | grep -q "200"; then
                ((success++))
            else
                ((failed++))
            fi
        done
        
        local end=$(date +%s%N)
        local duration=$(( ($end - $start) / 1000000 ))
        local avg=$(( $duration / 100 ))
        local qps=$(( 100000 / $avg ))
        
        echo "  完成请求: $success/100"
        echo "  失败请求: $failed"
        echo "  总耗时:   ${duration}ms"
        echo "  平均耗时: ${avg}ms"
        echo "  QPS:      ~${qps} req/s"
    fi
    echo ""
}

# 1. 测试搜索服务（直连）
run_benchmark "Search Service (Direct)" "http://localhost:8081/search?q=%E4%B8%AD%E5%9B%BD&topk=10"

# 2. 测试推荐服务（直连）
run_benchmark "Recommend Service (Direct)" "http://localhost:8082/recommend?q=%E4%B8%AD%E5%9B%BD&topk=5"

# 3. 测试通过Nginx访问搜索
run_benchmark "Search via Nginx Gateway" "http://localhost:9999/api/search?q=%E4%B8%AD%E5%9B%BD&topk=10"

# 4. 测试通过Nginx访问推荐
run_benchmark "Recommend via Nginx Gateway" "http://localhost:9999/api/recommend?q=%E4%B8%AD%E5%9B%BD&topk=5"

# 缓存命中率测试
echo "========================================="
echo "  💾 Cache Performance Test"
echo "========================================="
echo "热身缓存..."
for i in {1..5}; do
    curl -s "http://localhost:8081/search?q=%E4%B8%AD%E5%9B%BD" > /dev/null
done
sleep 1

echo ""
echo "测试缓存命中性能（同一查询）..."
local_start=$(date +%s%N)
for i in {1..100}; do
    curl -s "http://localhost:8081/search?q=%E4%B8%AD%E5%9B%BD" > /dev/null
done
local_end=$(date +%s%N)
cache_duration=$(( ($local_end - $local_start) / 1000000 ))
cache_avg=$(( $cache_duration / 100 ))

echo "  100次相同查询耗时: ${cache_duration}ms"
echo "  平均耗时: ${cache_avg}ms (含缓存)"
echo ""

# 获取缓存统计
echo "缓存统计:"
curl -s "http://localhost:8081/cache/stats" | python3 -c '
import sys, json
try:
    data = json.load(sys.stdin)
    local_hits = data.get("local_hits", 0)
    redis_hits = data.get("redis_hits", 0)
    misses = data.get("misses", 0)
    hit_rate = data.get("hit_rate", 0)
    print(f"  本地缓存命中: {local_hits}")
    print(f"  Redis命中:    {redis_hits}")
    print(f"  未命中:       {misses}")
    print(f"  命中率:       {hit_rate:.2f}%")
except: pass
' 2>/dev/null || echo "  (统计数据获取失败)"

echo ""
echo "========================================="
echo "  ✅ Benchmark Complete"
echo "========================================="
echo ""
echo "💡 Tip: 安装 ab 工具获得更详细的测试:"
echo "   sudo apt-get install apache2-utils"
echo ""

