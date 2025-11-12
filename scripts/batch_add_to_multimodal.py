#!/usr/bin/env python3
"""
批量将uploads目录下的文件添加到多模态向量库
"""

import os
import sys
import requests
from pathlib import Path

def get_file_type(ext):
    """根据扩展名判断文件类型"""
    image_exts = {'.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp'}
    text_exts = {'.txt', '.md', '.json', '.xml'}
    
    ext = ext.lower()
    if ext in image_exts:
        return 'image'
    elif ext in text_exts:
        return 'text'
    else:
        return 'other'

def add_file_to_multimodal(file_path, api_url="http://localhost:8084/add"):
    """添加单个文件到多模态索引"""
    path = Path(file_path)
    
    # 提取文件信息
    file_hash = path.stem  # 文件名（不含扩展名）
    filename = path.name
    file_type = get_file_type(path.suffix)
    
    # 计算相对于uploads的文件夹路径
    try:
        rel_path = path.relative_to('uploads')
        folder = str(rel_path.parent) if str(rel_path.parent) != '.' else ''
    except:
        folder = ''
    
    # 调用API
    try:
        response = requests.post(
            api_url,
            json={
                'file_hash': file_hash,
                'file_path': str(file_path),
                'filename': filename,
                'file_type': file_type,
                'folder': folder
            },
            timeout=10
        )
        
        result = response.json()
        if result.get('success'):
            return True, filename
        else:
            return False, result.get('error', 'Unknown error')
    
    except Exception as e:
        return False, str(e)

def batch_add_files(directory='uploads', api_url="http://localhost:8084/add"):
    """批量添加文件"""
    
    # 支持的扩展名
    supported_exts = {'.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp', 
                     '.txt', '.md', '.json', '.xml'}
    
    # 遍历目录
    files_to_add = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            ext = Path(file).suffix.lower()
            if ext in supported_exts:
                file_path = os.path.join(root, file)
                files_to_add.append(file_path)
    
    print(f"📁 找到 {len(files_to_add)} 个文件")
    
    # 批量添加
    success_count = 0
    fail_count = 0
    
    for i, file_path in enumerate(files_to_add, 1):
        success, info = add_file_to_multimodal(file_path, api_url)
        
        if success:
            print(f"[{i}/{len(files_to_add)}] ✅ {info}")
            success_count += 1
        else:
            print(f"[{i}/{len(files_to_add)}] ❌ {Path(file_path).name}: {info}")
            fail_count += 1
    
    print(f"\n{'='*50}")
    print(f"✅ 成功添加: {success_count} 个文件")
    print(f"❌ 失败: {fail_count} 个文件")
    print(f"{'='*50}")
    
    return success_count, fail_count

if __name__ == '__main__':
    directory = sys.argv[1] if len(sys.argv) > 1 else 'uploads'
    api_url = sys.argv[2] if len(sys.argv) > 2 else 'http://localhost:8084/add'
    
    print(f"🚀 开始批量添加文件到多模态向量库")
    print(f"目录: {directory}")
    print(f"API: {api_url}\n")
    
    success, fail = batch_add_files(directory, api_url)
    sys.exit(0 if fail == 0 else 1)

