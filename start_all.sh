#!/bin/bash
# 启动所有微服务（搜索 + 推荐 + Nginx）

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================="
echo "  🚀 Starting All Microservices"
echo "========================================="

# 检查服务是否已编译
if [ ! -f "./search_service" ] || [ ! -f "./recommend_service" ] || [ ! -f "./file_service" ]; then
    echo "❌ 微服务未编译，正在编译..."
    make microservices
    if [ $? -ne 0 ]; then
        echo "❌ 编译失败"
        exit 1
    fi
fi

# 创建日志目录
mkdir -p logs

# 先停止已有服务
echo "清理旧服务..."
./stop_all.sh 2>/dev/null || true
sleep 1

# 启动搜索服务
echo ""
echo "🔍 Starting Search Service (port 8081)..."
nohup ./search_service > logs/search_service.log 2>&1 &
SEARCH_PID=$!
echo $SEARCH_PID > logs/search_service.pid
echo "   ✓ PID: $SEARCH_PID"
sleep 1

# 启动推荐服务
echo ""
echo "💡 Starting Recommend Service (port 8082)..."
nohup ./recommend_service > logs/recommend_service.log 2>&1 &
RECOMMEND_PID=$!
echo $RECOMMEND_PID > logs/recommend_service.pid
echo "   ✓ PID: $RECOMMEND_PID"
sleep 1

# 启动文件服务
echo ""
echo "📁 Starting File Service (port 8083)..."
nohup ./file_service > logs/file_service.log 2>&1 &
FILE_PID=$!
echo $FILE_PID > logs/file_service.pid
echo "   ✓ PID: $FILE_PID"
sleep 1

# 启动 Nginx 网关
echo ""
echo "🌐 Starting Nginx Gateway (port 9999)..."
nginx -c "$SCRIPT_DIR/nginx_microservices.conf"
sleep 2

# 验证服务
echo ""
echo "⏳ Verifying services..."
echo ""

SUCCESS=0

echo -n "   Search:    "
if curl -s http://127.0.0.1:8081/health > /dev/null 2>&1; then
    echo "✓ OK"
    ((SUCCESS++))
else
    echo "✗ FAILED"
fi

echo -n "   Recommend: "
if curl -s http://127.0.0.1:8082/health > /dev/null 2>&1; then
    echo "✓ OK"
    ((SUCCESS++))
else
    echo "✗ FAILED"
fi

echo -n "   File:      "
if curl -s http://127.0.0.1:8083/health > /dev/null 2>&1; then
    echo "✓ OK"
    ((SUCCESS++))
else
    echo "✗ FAILED"
fi

echo -n "   Nginx:     "
if curl -s http://127.0.0.1:9999/api/health > /dev/null 2>&1; then
    echo "✓ OK"
    ((SUCCESS++))
else
    echo "✗ FAILED"
fi

echo ""
echo "========================================="
if [ $SUCCESS -eq 4 ]; then
    echo "✅ All services started successfully!"
else
    echo "⚠️  Some services failed to start ($SUCCESS/4)"
fi
echo "========================================="
echo ""
echo "🌐 访问地址:"
echo "   http://localhost:9999/"
echo "   http://192.168.139.130:9999/"
echo ""
echo "📝 查看日志:"
echo "   tail -f logs/search_service.log"
echo "   tail -f logs/recommend_service.log"
echo "   tail -f /tmp/nginx_access.log"
echo ""
echo "🛑 停止服务: ./stop_all.sh"
echo ""

