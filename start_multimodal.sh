#!/bin/bash
# 启动多模态搜索服务

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# 检查依赖
echo "🔍 检查Python依赖..."
python3 -c "import sentence_transformers, faiss, PIL" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "❌ 依赖未安装"
    echo "正在后台安装，查看进度: tail -f logs/pip_install.log"
    echo "或手动安装: pip3 install -r multimodal_service/requirements.txt"
    exit 1
fi

echo "✅ 依赖已安装"

# 启动服务
echo "🚀 启动多模态搜索服务..."
cd multimodal_service
nohup python3 api_server.py > ../logs/multimodal_service.log 2>&1 &
MULTIMODAL_PID=$!

echo "$MULTIMODAL_PID" > ../logs/multimodal_service.pid
cd ..

# 等待服务启动
sleep 3

# 健康检查
if curl -s http://localhost:8084/health | grep -q '"status":"ok"'; then
    echo "✅ 多模态搜索服务已启动 (PID: $MULTIMODAL_PID)"
    echo "   端口: 8084"
    echo "   日志: logs/multimodal_service.log"
    
    # 显示统计
    curl -s http://localhost:8084/stats | python3 -m json.tool
else
    echo "❌ 服务启动失败，查看日志:"
    tail -20 logs/multimodal_service.log
    exit 1
fi

