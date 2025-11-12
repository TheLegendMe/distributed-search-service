# Nginx 反向代理部署指南

本文档说明如何为分布式搜索服务配置和部署 Nginx 反向代理。

## 📋 架构说明

```
客户端请求
    ↓
  Nginx:80 (反向代理)
    ↓
    ├── /static/*  → 直接返回静态文件（高性能）
    ├── /          → 返回 index.html
    └── /api/*     → 反向代理到 C++ Backend:8080
```

### 优势
- ✅ **性能提升**: Nginx 高效处理静态文件，释放 C++ 服务资源
- ✅ **缓存优化**: 静态资源缓存 7 天，减少重复读取
- ✅ **安全性**: Nginx 作为前端屏障，隐藏后端实现
- ✅ **扩展性**: 便于后续添加负载均衡、SSL 等功能

## 🚀 快速开始

### 前置要求

1. **安装 Nginx**
```bash
#sudo apt-get update
#sudo apt-get install nginx
```

2. **验证安装**
```bash
nginx -v
# 输出示例: nginx version: nginx/1.18.0
```

### 一键启动所有服务

```bash
# 启动 C++ 后端 + Nginx（推荐）
./start_all.sh

# 停止所有服务
./stop_all.sh
```

### 分步启动

#### 1. 启动 C++ 后端服务
```bash
./start_web.sh
```

后端服务将监听 `127.0.0.1:8080`（仅本地访问）

#### 2. 启动 Nginx
```bash
sudo ./nginx_start.sh
```

Nginx 将监听 `80` 端口对外提供服务

### 验证部署

```bash
# 检查服务状态
sudo ./nginx_status.sh

# 测试访问
curl http://localhost/                      # 首页
curl http://localhost/api/health            # 健康检查
curl http://localhost/api/search?q=测试     # 搜索 API
```

## 📁 文件说明

| 文件 | 说明 |
|------|------|
| `nginx.conf` | Nginx 主配置文件 |
| `nginx_start.sh` | Nginx 启动脚本 |
| `nginx_stop.sh` | Nginx 停止脚本 |
| `nginx_status.sh` | 状态检查脚本 |
| `start_all.sh` | 一键启动所有服务 |
| `stop_all.sh` | 一键停止所有服务 |

## 🔧 配置详解

### Nginx 配置亮点

1. **静态资源处理**
```nginx
location /static/ {
    alias /home/oym/new/distributed-search-service/static/;
    expires 7d;  # 缓存 7 天
    add_header Cache-Control "public, immutable";
}
```

2. **API 反向代理**
```nginx
location /api/ {
    proxy_pass http://search_backend;
    proxy_http_version 1.1;
    proxy_set_header Host $host;
    # ... 更多配置
}
```

3. **Gzip 压缩**
```nginx
gzip on;
gzip_comp_level 6;
gzip_types text/plain text/css application/json application/javascript;
```

4. **CORS 支持**
```nginx
add_header Access-Control-Allow-Origin "*" always;
```

### 端口配置

- **Nginx**: 监听 `0.0.0.0:80` (对外)
- **C++ 后端**: 监听 `127.0.0.1:8080` (仅内网)

> 💡 **安全提示**: 后端服务只监听本地回环地址，外部无法直接访问

## 🛠️ 管理命令

### 启动服务
```bash
# 方法 1: 一键启动（推荐）
./start_all.sh

# 方法 2: 分步启动
./start_web.sh           # 先启动后端
sudo ./nginx_start.sh    # 再启动 Nginx
```

### 停止服务
```bash
# 一键停止
./stop_all.sh

# 单独停止
sudo ./nginx_stop.sh     # 停止 Nginx
./restart_web.sh stop    # 停止后端
```

### 重载配置（无需停机）
```bash
# 修改 nginx.conf 后重载
sudo nginx -s reload -c /home/oym/new/distributed-search-service/nginx.conf
```

### 查看状态
```bash
sudo ./nginx_status.sh
```

### 查看日志
```bash
# Nginx 访问日志
tail -f /var/log/nginx/search_access.log

# Nginx 错误日志
tail -f /var/log/nginx/search_error.log

# C++ 后端日志
tail -f ./web_server.log
```

## 🧪 测试 API

### 搜索 API
```bash
# 基本搜索
curl "http://localhost/api/search?q=测试"

# 限制结果数量
curl "http://localhost/api/search?q=测试&topk=10"
```

### 关键词推荐 API
```bash
# 获取推荐
curl "http://localhost/api/recommend?q=测试"

# 限制推荐数量
curl "http://localhost/api/recommend?q=测试&topk=5"
```

### 健康检查
```bash
curl http://localhost/api/health
# 输出: {"status":"ok"}
```

## 🐛 故障排查

### 问题 1: Nginx 启动失败

**症状**: `nginx: [emerg] bind() to 0.0.0.0:80 failed`

**原因**: 端口 80 被占用

**解决**:
```bash
# 查看占用端口 80 的进程
sudo lsof -i :80

# 停止 Apache（如果安装了）
sudo systemctl stop apache2

# 或修改 nginx.conf 中的监听端口
# listen 8000;  # 改为其他端口
```

### 问题 2: 502 Bad Gateway

**症状**: 访问页面返回 502 错误

**原因**: C++ 后端服务未启动或连接失败

**解决**:
```bash
# 检查后端服务
netstat -tuln | grep 8080

# 如果没有输出，启动后端
./start_web.sh

# 查看 Nginx 错误日志
sudo tail -50 /var/log/nginx/search_error.log
```

### 问题 3: 静态文件 403 Forbidden

**症状**: 访问 `/static/` 返回 403

**原因**: Nginx 没有读取文件的权限

**解决**:
```bash
# 给 static 目录添加读取权限
chmod -R 755 /home/oym/new/distributed-search-service/static

# 确保父目录也有执行权限
chmod 755 /home/oym/new/distributed-search-service
chmod 755 /home/oym/new
chmod 755 /home/oym
```

### 问题 4: 配置文件语法错误

**症状**: `nginx: configuration file test failed`

**解决**:
```bash
# 测试配置文件
nginx -t -c /home/oym/new/distributed-search-service/nginx.conf

# 查看具体错误信息并修复
```

## 🔐 安全加固（可选）

### 1. 添加 HTTPS 支持

```bash
# 生成自签名证书（测试用）
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout /etc/nginx/ssl/search.key \
  -out /etc/nginx/ssl/search.crt
```

在 `nginx.conf` 中添加:
```nginx
server {
    listen 443 ssl;
    ssl_certificate /etc/nginx/ssl/search.crt;
    ssl_certificate_key /etc/nginx/ssl/search.key;
    # ... 其他配置
}
```

### 2. 添加基本认证

```bash
# 安装工具
sudo apt-get install apache2-utils

# 创建密码文件
sudo htpasswd -c /etc/nginx/.htpasswd admin
```

在 `nginx.conf` 的 `location /api/` 中添加:
```nginx
auth_basic "Restricted Access";
auth_basic_user_file /etc/nginx/.htpasswd;
```

### 3. 限流配置

在 `http` 块中添加:
```nginx
limit_req_zone $binary_remote_addr zone=api_limit:10m rate=10r/s;

location /api/ {
    limit_req zone=api_limit burst=20 nodelay;
    # ... 其他配置
}
```

## 📊 性能监控

### 查看 Nginx 统计

```bash
# 活动连接数
ps aux | grep nginx | wc -l

# 访问统计
cat /var/log/nginx/search_access.log | wc -l
```

### 性能测试

```bash
# 安装 ab (Apache Bench)
sudo apt-get install apache2-utils

# 测试静态资源
ab -n 1000 -c 100 http://localhost/static/style.css

# 测试 API
ab -n 1000 -c 100 "http://localhost/api/search?q=test"
```

## 🎯 生产环境建议

1. **修改服务器名称**
   - 将 `nginx.conf` 中的 `server_name localhost;` 改为实际域名

2. **启用 HTTPS**
   - 使用 Let's Encrypt 免费证书

3. **配置日志轮转**
```bash
sudo nano /etc/logrotate.d/nginx-search
```
```
/var/log/nginx/search_*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    create 0640 www-data adm
    sharedscripts
    postrotate
        [ -f /var/run/nginx.pid ] && kill -USR1 `cat /var/run/nginx.pid`
    endscript
}
```

4. **设置开机自启**
```bash
# 创建 systemd 服务
sudo nano /etc/systemd/system/search-nginx.service
```
```ini
[Unit]
Description=Search Engine Nginx Service
After=network.target

[Service]
Type=forking
ExecStart=/home/oym/new/distributed-search-service/nginx_start.sh
ExecStop=/home/oym/new/distributed-search-service/nginx_stop.sh
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
```bash
sudo systemctl enable search-nginx
sudo systemctl start search-nginx
```

## 📞 技术支持

- 配置文件位置: `./nginx.conf`
- 日志位置: `/var/log/nginx/search_*.log`
- 官方文档: https://nginx.org/en/docs/

## 📝 更新日志

### 2025-11-08
- ✨ 初始版本
- ✅ 完成基础反向代理配置
- ✅ 添加静态资源缓存
- ✅ 支持 CORS
- ✅ 添加健康检查端点
- ✅ 创建管理脚本集

