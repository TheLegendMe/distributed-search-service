# 🚀 分布式搜索引擎系统

一个完整的微服务架构搜索系统，支持倒排索引、动态索引、多模态搜索、文件管理等功能。

## ✨ 核心功能

### 1. 精确关键词搜索
- 基于TF-IDF倒排索引
- 支持AND查询和余弦相似度排序
- 响应时间: 5-20ms
- 数据量: 500+文档

### 2. 动态索引
- 支持实时增删改文档
- 无需重建索引
- 线程安全
- 可持久化

### 3. 多模态智能搜索 🎨
- 文本语义搜索
- 图片特征搜索
- 基于Qdrant向量数据库
- 支持跨模态检索

### 4. 文件管理系统
- Windows资源管理器风格
- 支持文件夹导航
- 图片预览
- 文件上传自动索引

### 5. 关键词推荐
- 基于编辑距离算法
- 智能纠错和补全

## 🏗️ 系统架构

```
浏览器 (192.168.x.x:9999)
    ↓
Nginx 网关 (9999)
    ├─→ search_service (8081) - 倒排索引 + 动态索引
    ├─→ recommend_service (8082) - 关键词推荐
    ├─→ file_service (8083) - 文件管理
    └─→ multimodal_service (8084) - 多模态搜索
            ↓
        Qdrant (6333) - 向量数据库
```

## 🚀 快速启动

### 1. 编译服务

```bash
make microservices
```

### 2. 启动所有服务

```bash
./start_all.sh          # 启动基础微服务
./start_multimodal.sh   # 启动多模态搜索（可选）
```

### 3. 访问系统

```
http://localhost:9999/
```

### 4. 停止服务

```bash
./stop_all.sh
./stop_multimodal.sh
```

## 📊 技术栈

### 后端
- **C++17**: 核心搜索引擎
- **wfrest**: HTTP服务框架
- **Redis**: 缓存和会话存储
- **cppjieba**: 中文分词
- **Python3**: 多模态服务和工具脚本

### 前端
- **原生JavaScript**: 无框架依赖
- **CSS3**: 现代化UI设计
- **HTML5**: 语义化标签

### 基础设施
- **Nginx**: API网关和反向代理
- **Docker**: Qdrant容器化部署
- **Qdrant**: 向量数据库

## 📁 目录结构

```
├── src/                    # C++源代码
│   ├── search_service.cpp
│   ├── recommend_service.cpp
│   ├── file_service.cpp
│   ├── dynamic_index.cpp
│   └── ...
├── static/                 # 前端资源
├── multimodal_service/     # 多模态搜索
├── scripts/                # 工具脚本
├── conf/                   # 配置文件
├── Makefile                # 编译配置
└── nginx_microservices.conf # Nginx配置
```

## 🔧 配置

### 应用配置

编辑 `conf/app.conf`:

```ini
index_dir=./output
pages_file=./output/pages.bin
offsets_file=./output/offsets.bin
```

### 服务端口

| 服务 | 端口 | 说明 |
|------|------|------|
| search_service | 8081 | 搜索服务 |
| recommend_service | 8082 | 推荐服务 |
| file_service | 8083 | 文件服务 |
| multimodal_service | 8084 | 多模态搜索 |
| Nginx | 9999 | HTTP网关 |
| Nginx | 9443 | HTTPS网关 |
| Qdrant | 6333 | 向量数据库 |

## 📝 API文档

### 搜索API

```bash
# 关键词搜索
GET /api/search?q=关键词&topk=20

# 多模态搜索
POST /api/multimodal/search
{
  "type": "text",
  "query": "可爱的小动物",
  "top_k": 10
}
```

### 动态索引API

```bash
# 添加文档
POST /api/search/index/add
{
  "docid": 10001,
  "title": "标题",
  "link": "https://example.com",
  "summary": "摘要",
  "text": "完整内容"
}

# 批量添加
POST /api/search/index/batch/add
{
  "documents": [...]
}

# 查看统计
GET /api/search/index/stats
```

### 文件API

```bash
# 上传文件
POST /api/file/init
POST /api/file/chunk
POST /api/file/complete

# 文件列表
GET /api/file/list?folder=文件夹名

# 下载文件
GET /api/file/download/{hash}
```

## 🎯 特色功能

### XML自动解析
上传XML文件时自动解析`<item>`标签，每个item作为独立文档索引。

### 文件上传自动索引
勾选"自动添加到搜索索引"，文件上传后：
- XML文件 → 解析后添加到倒排索引
- 图片文件 → 添加到多模态向量库
- 文本文件 → 同时添加到两个索引

### 混合搜索
系统同时查询倒排索引和动态索引，合并结果。

## 📈 性能指标

- **搜索响应**: 5-20ms (倒排索引)
- **动态添加**: <1ms (标记删除)
- **并发支持**: 1000+ QPS
- **缓存命中率**: 90%+

## 🛠️ 开发

### 编译单个服务

```bash
make search_service
make recommend_service
make file_service
```

### 调试

```bash
# 查看服务日志
tail -f logs/search_service.log

# 查看Nginx日志
tail -f /tmp/nginx_access.log
```

## 📜 许可证

MIT License

## 👥 作者

Distributed Search Engine Team

