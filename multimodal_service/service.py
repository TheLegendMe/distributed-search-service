#!/usr/bin/env python3
"""
轻量级多模态搜索API服务
端口: 8084
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
from qdrant_engine import LightweightMultimodalEngine
import os

app = Flask(__name__)
CORS(app)

# 初始化引擎
print("🚀 初始化多模态搜索引擎...")
engine = LightweightMultimodalEngine()
print("✅ 引擎初始化完成")

@app.route('/health', methods=['GET'])
def health():
    """健康检查"""
    return jsonify({
        'status': 'ok',
        'service': 'multimodal_search',
        'ready': True
    })

@app.route('/stats', methods=['GET'])
def stats():
    """获取统计信息"""
    return jsonify(engine.get_stats())

@app.route('/add', methods=['POST'])
def add_file():
    """添加文件到向量库"""
    data = request.json
    
    file_hash = data.get('file_hash')
    file_path = data.get('file_path')
    filename = data.get('filename')
    file_type = data.get('file_type', 'text')
    folder = data.get('folder', '')
    
    if not all([file_hash, file_path, filename]):
        return jsonify({'success': False, 'error': 'Missing required fields'}), 400
    
    success = engine.add_file(file_hash, file_path, filename, file_type, folder)
    
    if success:
        return jsonify({
            'success': True,
            'message': 'File added to multimodal index',
            'file_hash': file_hash
        })
    else:
        return jsonify({'success': False, 'error': 'Failed to add file'}), 500

@app.route('/search', methods=['POST'])
def search():
    """多模态搜索"""
    data = request.json
    search_type = data.get('type', 'text')
    top_k = data.get('top_k', 10)
    
    results = []
    
    if search_type == 'text':
        query = data.get('query', '')
        if not query:
            return jsonify({'error': 'Missing query'}), 400
        results = engine.search_by_text(query, top_k)
    
    elif search_type == 'image':
        image_path = data.get('image_path', '')
        if not image_path:
            return jsonify({'error': 'Missing image_path'}), 400
        results = engine.search_by_image(image_path, top_k)
    
    else:
        return jsonify({'error': 'Invalid search type'}), 400
    
    return jsonify({
        'success': True,
        'count': len(results),
        'results': results
    })

if __name__ == '__main__':
    print("\n" + "="*50)
    print("  多模态搜索服务")
    print("="*50)
    print(f"统计: {engine.get_stats()}")
    print("\nAPI端点:")
    print("  GET  /health  - 健康检查")
    print("  GET  /stats   - 统计信息")  
    print("  POST /add     - 添加文件")
    print("  POST /search  - 搜索")
    print("\n监听: http://0.0.0.0:8084")
    print("="*50 + "\n")
    
    app.run(host='0.0.0.0', port=8084, debug=False)

