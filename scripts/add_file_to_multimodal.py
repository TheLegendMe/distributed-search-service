#!/usr/bin/env python3
"""
将上传的文件添加到多模态索引
由file_service调用
"""

import sys
import json
import requests
from pathlib import Path

def add_to_multimodal_index(file_path, file_hash, filename, file_type, folder=""):
    """添加文件到多模态索引"""
    
    # 判断文件类型
    ext = Path(file_path).suffix.lower()
    
    if ext in ['.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp']:
        actual_type = 'image'
    elif ext in ['.txt', '.md', '.json', '.xml', '.pdf']:
        actual_type = 'text'
    else:
        actual_type = 'other'
    
    # 调用multimodal服务API
    try:
        response = requests.post(
            'http://localhost:8084/add',
            json={
                'file_hash': file_hash,
                'file_path': file_path,
                'filename': filename,
                'file_type': actual_type,
                'folder': folder
            },
            timeout=30
        )
        
        result = response.json()
        return result
    
    except Exception as e:
        return {
            'success': False,
            'error': f'Failed to connect to multimodal service: {e}'
        }

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("用法: add_file_to_multimodal.py <file_path> <file_hash> <filename> [folder]")
        sys.exit(1)
    
    file_path = sys.argv[1]
    file_hash = sys.argv[2]
    filename = sys.argv[3]
    folder = sys.argv[4] if len(sys.argv) > 4 else ""
    
    result = add_to_multimodal_index(file_path, file_hash, filename, "", folder)
    print(json.dumps(result, ensure_ascii=False))

