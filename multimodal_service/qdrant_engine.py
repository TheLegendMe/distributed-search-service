#!/usr/bin/env python3
"""
基于Qdrant的轻量级多模态搜索引擎
不需要CLIP模型，使用简单的图片特征提取
"""

import numpy as np
from PIL import Image
from qdrant_client import QdrantClient
from qdrant_client.models import Distance, VectorParams, PointStruct
import hashlib
from pathlib import Path
from typing import List, Dict
import io

class LightweightMultimodalEngine:
    """轻量级多模态搜索引擎"""
    
    def __init__(self, qdrant_host="localhost", qdrant_port=6333):
        self.client = QdrantClient(host=qdrant_host, port=qdrant_port)
        self.collection_name = "multimodal_files"
        self.vector_size = 256  # 特征向量维度
        
        # 创建collection（如果不存在）
        try:
            self.client.get_collection(self.collection_name)
            print(f"✅ Collection '{self.collection_name}' 已存在")
        except:
            self.client.create_collection(
                collection_name=self.collection_name,
                vectors_config=VectorParams(size=self.vector_size, distance=Distance.COSINE)
            )
            print(f"✅ 已创建collection '{self.collection_name}'")
    
    def extract_image_features(self, image_path: str) -> np.ndarray:
        """提取图片特征（颜色直方图 + 简单统计）"""
        try:
            img = Image.open(image_path).convert('RGB')
            
            # 缩放到固定大小
            img = img.resize((64, 64))
            img_array = np.array(img)
            
            # 1. 颜色直方图 (RGB各32 bins = 96维)
            hist_r = np.histogram(img_array[:,:,0], bins=32, range=(0,256))[0]
            hist_g = np.histogram(img_array[:,:,1], bins=32, range=(0,256))[0]
            hist_b = np.histogram(img_array[:,:,2], bins=32, range=(0,256))[0]
            
            # 2. 统计特征 (12维)
            stats = [
                np.mean(img_array[:,:,0]), np.std(img_array[:,:,0]),
                np.mean(img_array[:,:,1]), np.std(img_array[:,:,1]),
                np.mean(img_array[:,:,2]), np.std(img_array[:,:,2]),
                np.mean(img_array), np.std(img_array),
                np.min(img_array), np.max(img_array),
                img_array.shape[0], img_array.shape[1]
            ]
            
            # 3. 纹理特征 (简化，148维)
            # 将图片分成16个小块，计算每块的平均值
            h, w = img_array.shape[:2]
            blocks = []
            for i in range(4):
                for j in range(4):
                    block = img_array[i*h//4:(i+1)*h//4, j*w//4:(j+1)*w//4]
                    blocks.extend([
                        np.mean(block[:,:,0]),
                        np.mean(block[:,:,1]),
                        np.mean(block[:,:,2])
                    ])
            
            # 组合特征
            features = np.concatenate([hist_r, hist_g, hist_b, stats, blocks[:100]])
            
            # 归一化
            features = features / (np.linalg.norm(features) + 1e-8)
            
            # 填充到256维
            if len(features) < self.vector_size:
                features = np.pad(features, (0, self.vector_size - len(features)))
            else:
                features = features[:self.vector_size]
            
            return features.astype(np.float32)
            
        except Exception as e:
            print(f"❌ 图片特征提取失败: {e}")
            return None
    
    def extract_text_features(self, text: str) -> np.ndarray:
        """提取文本特征（简单hash-based）"""
        # 使用简单的字符特征
        features = np.zeros(self.vector_size, dtype=np.float32)
        
        # 字符频率特征
        for i, char in enumerate(text[:self.vector_size]):
            features[i] = ord(char) / 65536.0
        
        # 添加文本统计特征
        if len(text) > 0:
            features[-10] = len(text) / 1000.0  # 长度
            features[-9] = len(set(text)) / 100.0  # 唯一字符数
            features[-8] = text.count(' ') / 100.0  # 空格数
            
        # 归一化
        norm = np.linalg.norm(features)
        if norm > 0:
            features = features / norm
        
        return features
    
    def add_file(self, file_hash: str, file_path: str, filename: str, file_type: str, folder: str = "") -> bool:
        """添加文件到向量库"""
        try:
            # 根据类型提取特征
            if file_type == 'image' and Path(file_path).exists():
                vector = self.extract_image_features(file_path)
            else:
                # 文本或其他类型，使用文件名
                vector = self.extract_text_features(filename)
            
            if vector is None:
                return False
            
            # 添加到Qdrant
            point_id = abs(hash(file_hash)) % (2**31)  # 生成唯一ID
            
            self.client.upsert(
                collection_name=self.collection_name,
                points=[
                    PointStruct(
                        id=point_id,
                        vector=vector.tolist(),
                        payload={
                            "file_hash": file_hash,
                            "filename": filename,
                            "file_type": file_type,
                            "folder": folder
                        }
                    )
                ]
            )
            
            print(f"✅ 已添加到向量库: {filename} (ID: {point_id})")
            return True
            
        except Exception as e:
            print(f"❌ 添加失败: {e}")
            return False
    
    def search_by_text(self, query: str, top_k: int = 10) -> List[Dict]:
        """文本搜索"""
        try:
            query_vector = self.extract_text_features(query)
            return self._search(query_vector, top_k)
        except Exception as e:
            print(f"❌ 搜索失败: {e}")
            return []
    
    def search_by_image(self, image_path: str, top_k: int = 10) -> List[Dict]:
        """图片搜索"""
        try:
            query_vector = self.extract_image_features(image_path)
            if query_vector is None:
                return []
            return self._search(query_vector, top_k)
        except Exception as e:
            print(f"❌ 搜索失败: {e}")
            return []
    
    def _search(self, query_vector: np.ndarray, top_k: int) -> List[Dict]:
        """执行向量搜索"""
        try:
            search_result = self.client.search(
                collection_name=self.collection_name,
                query_vector=query_vector.tolist(),
                limit=top_k
            )
            
            results = []
            for hit in search_result:
                results.append({
                    'file_hash': hit.payload.get('file_hash'),
                    'filename': hit.payload.get('filename'),
                    'file_type': hit.payload.get('file_type'),
                    'folder': hit.payload.get('folder', ''),
                    'score': hit.score,
                    'similarity': hit.score * 100
                })
            
            return results
        except Exception as e:
            print(f"❌ 向量搜索失败: {e}")
            return []
    
    def get_stats(self) -> Dict:
        """获取统计信息"""
        try:
            collection_info = self.client.get_collection(self.collection_name)
            return {
                'total_vectors': collection_info.points_count,
                'vector_dimension': self.vector_size,
                'status': 'ready'
            }
        except Exception as e:
            return {'status': 'error', 'error': str(e)}

if __name__ == '__main__':
    # 测试
    engine = LightweightMultimodalEngine()
    print("引擎已初始化")
    print("统计:", engine.get_stats())

