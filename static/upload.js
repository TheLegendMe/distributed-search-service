// 文件上传功能
// API基础URL (如果app.js中未定义)
if (typeof API_BASE === 'undefined') {
    var API_BASE = '';
}

// 配置
const CHUNK_SIZE = 5 * 1024 * 1024; // 5MB每个分片
const MAX_CONCURRENT_CHUNKS = 3;     // 最大并发上传数

// 计算文件Hash（用于秒传）- 简化版本，直接用文件名+大小+时间生成唯一标识
async function calculateFileMD5(file) {
    // 对于大文件，计算完整MD5太慢，这里用简化方案：
    // 读取文件开头1MB + 文件大小 + 文件名 来生成hash
    return new Promise((resolve) => {
        const chunkSize = Math.min(1024 * 1024, file.size); // 最多读1MB
        const blob = file.slice(0, chunkSize);
        
        const reader = new FileReader();
        reader.onload = async (e) => {
            try {
                const buffer = e.target.result;
                const hashBuffer = await crypto.subtle.digest('SHA-256', buffer);
                const hashArray = Array.from(new Uint8Array(hashBuffer));
                const hashHex = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
                // 加上文件大小作为后缀，确保唯一性
                resolve(hashHex.substring(0, 32) + '_' + file.size);
            } catch (error) {
                console.error('Hash计算失败:', error);
                // 降级方案：使用文件名和大小
                const fallback = file.name + '_' + file.size + '_' + Date.now();
                resolve(btoa(fallback).replace(/[^a-zA-Z0-9]/g, '').substring(0, 32));
            }
        };
        reader.onerror = () => {
            // 错误降级
            const fallback = file.name + '_' + file.size + '_' + Date.now();
            resolve(btoa(fallback).replace(/[^a-zA-Z0-9]/g, '').substring(0, 32));
        };
        reader.readAsArrayBuffer(blob);
    });
}

// 文件上传类
class FileUploader {
    constructor(file) {
        this.file = file;
        this.uploadId = null;
        this.fileHash = null;
        this.totalChunks = Math.ceil(file.size / CHUNK_SIZE);
        this.uploadedChunks = new Set();
        this.uploading = false;
        this.paused = false;
    }
    
    async checkExists() {
        // 检查文件是否已存在（秒传）
        console.log('🔍 计算文件Hash...');
        this.fileHash = await calculateFileMD5(this.file);
        console.log('✓ 文件Hash:', this.fileHash);
        
        console.log('🔍 检查文件是否存在...');
        const response = await fetch(`${API_BASE}/api/file/check`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({hash: this.fileHash})
        });
        
        const data = await response.json();
        console.log('✓ 检查结果:', data);
        return data.exists;
    }
    
    async init() {
        // 初始化上传会话
        const response = await fetch(`${API_BASE}/api/file/init`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                filename: this.file.name,
                hash: this.fileHash,
                total_size: this.file.size,
                total_chunks: this.totalChunks
            })
        });
        
        const data = await response.json();
        if (data.success) {
            this.uploadId = data.upload_id;
            return true;
        }
        return false;
    }
    
    async uploadChunk(chunkIndex) {
        const start = chunkIndex * CHUNK_SIZE;
        const end = Math.min(start + CHUNK_SIZE, this.file.size);
        const chunk = this.file.slice(start, end);
        
        const response = await fetch(
            `${API_BASE}/api/file/chunk?upload_id=${this.uploadId}&chunk_index=${chunkIndex}`,
            {
                method: 'POST',
                body: chunk
            }
        );
        
        const data = await response.json();
        if (data.success) {
            this.uploadedChunks.add(chunkIndex);
            return true;
        }
        return false;
    }
    
    async upload(onProgress) {
        this.uploading = true;
        
        // 检查秒传
        const exists = await this.checkExists();
        if (exists) {
            onProgress && onProgress(100, '秒传成功');
            return {success: true, message: '文件已存在，秒传成功'};
        }
        
        // 初始化上传
        if (!await this.init()) {
            return {success: false, message: '初始化上传失败'};
        }
        
        // 分片上传
        const chunks = [];
        for (let i = 0; i < this.totalChunks; i++) {
            chunks.push(i);
        }
        
        // 并发上传分片
        while (chunks.length > 0 && !this.paused) {
            const batch = chunks.splice(0, MAX_CONCURRENT_CHUNKS);
            const promises = batch.map(i => this.uploadChunk(i));
            
            try {
                await Promise.all(promises);
                const progress = (this.uploadedChunks.size / this.totalChunks) * 100;
                onProgress && onProgress(progress, '上传中');
            } catch (err) {
                console.error('Upload chunk failed:', err);
                return {success: false, message: '分片上传失败'};
            }
        }
        
        if (this.paused) {
            return {success: false, message: '上传已暂停'};
        }
        
        // 完成上传
        const response = await fetch(`${API_BASE}/api/file/complete`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({upload_id: this.uploadId})
        });
        
        const data = await response.json();
        return data;
    }
    
    pause() {
        this.paused = true;
    }
    
    async resume(onProgress) {
        this.paused = false;
        return await this.upload(onProgress);
    }
}

// 当前上传任务
const uploadTasks = new Map();

// 等待DOM加载完成
document.addEventListener('DOMContentLoaded', function() {
    // 获取UI元素
    const uploadArea = document.getElementById('upload-area');
    const fileInput = document.getElementById('file-input');
    const uploadList = document.getElementById('upload-list');
    
    if (!uploadArea || !fileInput || !uploadList) {
        console.error('Upload elements not found');
        return;
    }

    // 点击上传区域
    uploadArea.addEventListener('click', () => {
        fileInput.click();
    });

    // 文件选择
    fileInput.addEventListener('change', (e) => {
        const files = Array.from(e.target.files);
        files.forEach(file => handleFileUpload(file));
        e.target.value = ''; // 清空input
    });

    // 拖拽上传
    uploadArea.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadArea.classList.add('dragover');
    });

    uploadArea.addEventListener('dragleave', () => {
        uploadArea.classList.remove('dragover');
    });

    uploadArea.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadArea.classList.remove('dragover');
        
        const files = Array.from(e.dataTransfer.files);
        files.forEach(file => handleFileUpload(file));
    });
    
    console.log('✓ File upload listeners initialized');
});

// 处理文件上传
async function handleFileUpload(file) {
    console.log('📁 开始上传文件:', file.name, file.size);
    
    const uploadList = document.getElementById('upload-list');
    if (!uploadList) {
        console.error('❌ Upload list not found');
        return;
    }
    
    const taskId = Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    const uploader = new FileUploader(file);
    uploadTasks.set(taskId, uploader);
    
    // 创建UI
    const item = createUploadItem(taskId, file);
    uploadList.appendChild(item);
    
    try {
        // 开始上传
        console.log('🚀 开始上传流程...');
        const result = await uploader.upload((progress, status) => {
            console.log(`📊 进度: ${progress.toFixed(1)}% - ${status}`);
            updateUploadProgress(taskId, progress, status);
        });
        
        console.log('✅ 上传结果:', result);
        
        if (result.success) {
            updateUploadStatus(taskId, 'success', result.message || '上传成功');
            loadStorageStats();
        } else {
            updateUploadStatus(taskId, 'error', result.message || '上传失败');
        }
    } catch (error) {
        console.error('❌ 上传出错:', error);
        updateUploadStatus(taskId, 'error', '上传异常: ' + error.message);
    }
}

// 创建上传项UI
function createUploadItem(taskId, file) {
    const item = document.createElement('div');
    item.className = 'upload-item';
    item.id = `upload-${taskId}`;
    
    const sizeStr = formatFileSize(file.size);
    
    item.innerHTML = `
        <div class="upload-info">
            <div class="file-icon">📄</div>
            <div class="file-details">
                <div class="file-name">${escapeHtml(file.name)}</div>
                <div class="file-size">${sizeStr}</div>
            </div>
        </div>
        <div class="upload-progress-container">
            <div class="upload-progress-bar" style="width: 0%"></div>
        </div>
        <div class="upload-status">准备上传...</div>
        <div class="upload-actions">
            <button class="btn-pause" onclick="pauseUpload('${taskId}')">暂停</button>
            <button class="btn-cancel" onclick="cancelUpload('${taskId}')">取消</button>
        </div>
    `;
    
    return item;
}

// 更新上传进度
function updateUploadProgress(taskId, progress, status) {
    const item = document.getElementById(`upload-${taskId}`);
    if (!item) return;
    
    const progressBar = item.querySelector('.upload-progress-bar');
    const statusText = item.querySelector('.upload-status');
    
    progressBar.style.width = `${progress}%`;
    statusText.textContent = `${status} - ${progress.toFixed(1)}%`;
}

// 更新上传状态
function updateUploadStatus(taskId, status, message) {
    const item = document.getElementById(`upload-${taskId}`);
    if (!item) return;
    
    const statusText = item.querySelector('.upload-status');
    const actions = item.querySelector('.upload-actions');
    
    if (status === 'success') {
        statusText.textContent = '✓ ' + message;
        statusText.style.color = '#34a853';
        actions.innerHTML = '<button class="btn-done" onclick="removeUploadItem(\'' + taskId + '\')">完成</button>';
    } else if (status === 'error') {
        statusText.textContent = '✗ ' + message;
        statusText.style.color = '#ea4335';
        actions.innerHTML = '<button class="btn-retry" onclick="retryUpload(\'' + taskId + '\')">重试</button>';
    }
}

// 暂停上传
function pauseUpload(taskId) {
    const uploader = uploadTasks.get(taskId);
    if (uploader) {
        uploader.pause();
        updateUploadStatus(taskId, 'paused', '已暂停');
    }
}

// 取消上传
function cancelUpload(taskId) {
    uploadTasks.delete(taskId);
    removeUploadItem(taskId);
}

// 移除上传项
function removeUploadItem(taskId) {
    const item = document.getElementById(`upload-${taskId}`);
    if (item) {
        item.remove();
    }
    uploadTasks.delete(taskId);
}

// 加载存储统计
async function loadStorageStats() {
    try {
        const response = await fetch(`${API_BASE}/api/file/stats`);
        const data = await response.json();
        
        document.getElementById('total-files').textContent = data.total_files || 0;
        document.getElementById('total-size').textContent = (data.total_size_mb || 0).toFixed(2);
    } catch (err) {
        console.error('Failed to load stats:', err);
    }
}

// 格式化文件大小
function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toFixed(2) + ' ' + sizes[i];
}

// HTML转义
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

