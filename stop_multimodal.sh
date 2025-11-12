#!/bin/bash
# 停止多模态搜索服务

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

PID_FILE="logs/multimodal_service.pid"

if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        kill "$PID"
        echo "✅ 多模态服务已停止 (PID: $PID)"
        rm "$PID_FILE"
    else
        echo "⚠️ 进程不存在"
        rm "$PID_FILE"
    fi
else
    echo "⚠️ PID文件不存在"
fi

# 备用：通过进程名杀死
pkill -f "api_server.py" && echo "✅ 已通过进程名停止服务"

