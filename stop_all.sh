#!/bin/bash
# 停止所有微服务（搜索 + 推荐 + Nginx）

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================="
echo "  🛑 Stopping All Microservices"
echo "========================================="

STOPPED=0

# 停止 Nginx
echo ""
echo "Stopping Nginx..."
if [ -f /tmp/nginx.pid ]; then
    PID=$(cat /tmp/nginx.pid)
    if ps -p $PID > /dev/null 2>&1; then
        kill $PID 2>/dev/null
        sleep 1
        if ps -p $PID > /dev/null 2>&1; then
            kill -9 $PID 2>/dev/null
        fi
        rm -f /tmp/nginx.pid
        echo "   ✓ Nginx stopped"
        ((STOPPED++))
    fi
else
    # 尝试查找并停止
    PIDS=$(ps aux | grep "nginx.*nginx_microservices" | grep -v grep | awk '{print $2}')
    if [ -n "$PIDS" ]; then
        kill $PIDS 2>/dev/null
        echo "   ✓ Nginx stopped"
        ((STOPPED++))
    else
        echo "   ℹ  Nginx not running"
    fi
fi

# 停止推荐服务
echo ""
echo "Stopping Recommend Service..."
if [ -f logs/recommend_service.pid ]; then
    PID=$(cat logs/recommend_service.pid)
    if ps -p $PID > /dev/null 2>&1; then
        kill $PID 2>/dev/null
        sleep 1
        if ps -p $PID > /dev/null 2>&1; then
            kill -9 $PID 2>/dev/null
        fi
        echo "   ✓ Stopped (PID: $PID)"
        ((STOPPED++))
    fi
    rm -f logs/recommend_service.pid
else
    echo "   ℹ  Not running"
fi

# 停止文件服务
echo ""
echo "Stopping File Service..."
if [ -f logs/file_service.pid ]; then
    PID=$(cat logs/file_service.pid)
    if ps -p $PID > /dev/null 2>&1; then
        kill $PID 2>/dev/null
        sleep 1
        if ps -p $PID > /dev/null 2>&1; then
            kill -9 $PID 2>/dev/null
        fi
        echo "   ✓ Stopped (PID: $PID)"
        ((STOPPED++))
    fi
    rm -f logs/file_service.pid
else
    echo "   ℹ  Not running"
fi

# 停止搜索服务
echo ""
echo "Stopping Search Service..."
if [ -f logs/search_service.pid ]; then
    PID=$(cat logs/search_service.pid)
    if ps -p $PID > /dev/null 2>&1; then
        kill $PID 2>/dev/null
        sleep 1
        if ps -p $PID > /dev/null 2>&1; then
            kill -9 $PID 2>/dev/null
        fi
        echo "   ✓ Stopped (PID: $PID)"
        ((STOPPED++))
    fi
    rm -f logs/search_service.pid
else
    echo "   ℹ  Not running"
fi

# 清理可能残留的进程
echo ""
echo "Checking for remaining processes..."
REMAINING=$(ps aux | grep -E "(search_service|recommend_service|file_service)" | grep -v grep | wc -l)
if [ $REMAINING -gt 0 ]; then
    echo "⚠️  Found $REMAINING remaining process(es), force killing..."
    pkill -9 search_service 2>/dev/null
    pkill -9 recommend_service 2>/dev/null
    pkill -9 file_service 2>/dev/null
    sleep 1
fi

echo ""
echo "========================================="
if [ $STOPPED -gt 0 ]; then
    echo "✅ Stopped $STOPPED service(s)"
else
    echo "ℹ️  No services were running"
fi
echo "========================================="

