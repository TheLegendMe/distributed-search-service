// API 基础 URL
const API_BASE = '';

// 元素引用
const searchInput = document.getElementById('search-input');
const searchBtn = document.getElementById('search-btn');
const searchResults = document.getElementById('search-results');
const searchLoading = document.getElementById('search-loading');
const searchSuggestions = document.getElementById('search-suggestions');

const recommendInput = document.getElementById('recommend-input');
const recommendBtn = document.getElementById('recommend-btn');
const recommendResults = document.getElementById('recommend-results');
const recommendLoading = document.getElementById('recommend-loading');

// 文件上传元素
const uploadArea = document.getElementById('upload-area');
const fileInput = document.getElementById('file-input');
const uploadList = document.getElementById('upload-list');

// 导航切换
document.querySelectorAll('.nav a').forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        const target = e.target.getAttribute('href').substring(1);
        
        // 更新导航激活状态
        document.querySelectorAll('.nav a').forEach(l => l.classList.remove('active'));
        e.target.classList.add('active');
        
        // 切换内容区域
        document.querySelectorAll('.section').forEach(s => s.classList.remove('active'));
        if (target === 'search') {
            document.getElementById('search-section').classList.add('active');
        } else if (target === 'recommend') {
            document.getElementById('recommend-section').classList.add('active');
        } else if (target === 'multimodal') {
            document.getElementById('multimodal-section').classList.add('active');
            // 检查多模态服务状态
            checkMultimodalService();
        } else if (target === 'upload') {
            document.getElementById('upload-section').classList.add('active');
            // 加载文件列表和统计
            refreshFileList();
            loadStorageStats();
        }
    });
});

// ========== 搜索功能 ==========

// 执行搜索
async function performSearch(query, topK = 20) {
    if (!query.trim()) return;
    
    searchLoading.classList.remove('hidden');
    searchResults.innerHTML = '';
    
    try {
        const response = await fetch(`${API_BASE}/api/search?q=${encodeURIComponent(query)}&topk=${topK}`);
        const data = await response.json();
        
        searchLoading.classList.add('hidden');
        
        if (data.error) {
            showError(searchResults, data.error);
            return;
        }
        
        if (data.results && data.results.length > 0) {
            displaySearchResults(data.results, data.query);
        } else {
            showNoResults(searchResults, query);
        }
    } catch (error) {
        searchLoading.classList.add('hidden');
        showError(searchResults, '搜索失败：' + error.message);
    }
}

// 显示搜索结果
function displaySearchResults(results, query) {
    searchResults.innerHTML = `
        <div style="background: white; padding: 15px 20px; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 1px 6px rgba(32, 33, 36, 0.28);">
            <p style="color: #5f6368; font-size: 14px;">
                找到约 <strong>${results.length}</strong> 条关于 "<strong>${escapeHtml(query)}</strong>" 的结果
            </p>
        </div>
    `;
    
    results.forEach((result, index) => {
        const resultDiv = document.createElement('div');
        resultDiv.className = 'result-item';
        resultDiv.style.animationDelay = `${index * 0.05}s`;
        
        resultDiv.innerHTML = `
            <div class="result-header">
                <a href="${escapeHtml(result.link)}" class="result-title" target="_blank">
                    ${escapeHtml(result.title) || '无标题'}
                </a>
                <span class="result-score">相关度: ${(result.score * 100).toFixed(1)}%</span>
            </div>
            <a href="${escapeHtml(result.link)}" class="result-link" target="_blank">
                ${escapeHtml(result.link)}
            </a>
            <p class="result-summary">${escapeHtml(result.summary)}</p>
            <div class="result-meta">文档ID: ${result.docid}</div>
        `;
        
        searchResults.appendChild(resultDiv);
    });
}

// 显示无结果
function showNoResults(container, query) {
    container.innerHTML = `
        <div class="no-results">
            <svg width="100" height="100" viewBox="0 0 100 100">
                <circle cx="40" cy="40" r="30" fill="none" stroke="#dfe1e5" stroke-width="4"/>
                <line x1="62" y1="62" x2="85" y2="85" stroke="#dfe1e5" stroke-width="4" stroke-linecap="round"/>
            </svg>
            <h3>未找到相关结果</h3>
            <p>没有找到关于 "${escapeHtml(query)}" 的内容</p>
            <p style="margin-top: 10px; font-size: 13px;">建议：</p>
            <ul style="text-align: left; display: inline-block; margin-top: 10px;">
                <li>检查输入的关键词是否正确</li>
                <li>尝试使用更通用的关键词</li>
                <li>尝试使用关键词推荐功能</li>
            </ul>
        </div>
    `;
}

// 快速搜索按钮
document.querySelectorAll('.quick-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const query = btn.getAttribute('data-query');
        searchInput.value = query;
        performSearch(query);
    });
});

// 搜索按钮点击
searchBtn.addEventListener('click', () => {
    performSearch(searchInput.value);
});

// 搜索输入回车
searchInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        performSearch(searchInput.value);
        searchSuggestions.classList.remove('show');
    }
});

// 实时推荐（防抖）
let suggestTimeout;
searchInput.addEventListener('input', () => {
    clearTimeout(suggestTimeout);
    const query = searchInput.value.trim();
    
    // 中文字符1个就够了，英文至少2个
    const hasChinese = /[\u4e00-\u9fff]/.test(query);
    if (!query || (!hasChinese && query.length < 2)) {
        searchSuggestions.classList.remove('show');
        return;
    }
    
    suggestTimeout = setTimeout(() => {
        fetchSuggestions(query);
    }, 300);
});

// 获取搜索建议
async function fetchSuggestions(query) {
    try {
        console.log('正在获取建议:', query);
        const response = await fetch(`${API_BASE}/api/recommend?q=${encodeURIComponent(query)}&topk=8`);
        const data = await response.json();
        console.log('收到建议:', data);
        
        if (data.suggestions && data.suggestions.length > 0) {
            displaySuggestions(data.suggestions);
        } else {
            searchSuggestions.classList.remove('show');
        }
    } catch (error) {
        console.error('获取建议失败:', error);
        searchSuggestions.classList.remove('show');
    }
}

// 显示搜索建议
function displaySuggestions(suggestions) {
    console.log('显示建议框，建议数量:', suggestions.length);
    searchSuggestions.innerHTML = suggestions.map((sug, idx) => `
        <div class="suggestion-item" data-word="${escapeHtml(sug.word)}">
            <svg class="suggestion-icon" width="16" height="16" viewBox="0 0 16 16">
                <circle cx="7" cy="7" r="5" fill="none" stroke="#5f6368" stroke-width="1.5"/>
                <line x1="11" y1="11" x2="15" y2="15" stroke="#5f6368" stroke-width="1.5" stroke-linecap="round"/>
            </svg>
            <div class="suggestion-content">
                <div class="suggestion-word">${escapeHtml(sug.word)}</div>
                <div class="suggestion-meta">
                    ${sug.frequency} 次出现
                </div>
            </div>
        </div>
    `).join('');
    
    searchSuggestions.classList.add('show');
    console.log('建议框已添加 show 类');
    
    // 绑定点击事件
    searchSuggestions.querySelectorAll('.suggestion-item').forEach(item => {
        item.addEventListener('click', () => {
            const word = item.getAttribute('data-word');
            searchInput.value = word;
            searchSuggestions.classList.remove('show');
            performSearch(word);
        });
    });
}

// 点击外部关闭建议
document.addEventListener('click', (e) => {
    if (!searchInput.contains(e.target) && !searchSuggestions.contains(e.target)) {
        searchSuggestions.classList.remove('show');
    }
});

// ========== 关键词推荐功能 ==========

// 执行推荐
async function performRecommend(query, topK = 10) {
    if (!query.trim()) return;
    
    recommendLoading.classList.remove('hidden');
    recommendResults.innerHTML = '';
    
    try {
        const response = await fetch(`${API_BASE}/api/recommend?q=${encodeURIComponent(query)}&topk=${topK}`);
        const data = await response.json();
        
        recommendLoading.classList.add('hidden');
        
        if (data.error) {
            showError(recommendResults, data.error);
            return;
        }
        
        if (data.suggestions && data.suggestions.length > 0) {
            displayRecommendations(data.suggestions);
        } else {
            showNoResults(recommendResults, query);
        }
    } catch (error) {
        recommendLoading.classList.add('hidden');
        showError(recommendResults, '推荐失败：' + error.message);
    }
}

// 显示推荐结果
function displayRecommendations(suggestions) {
    recommendResults.innerHTML = '';
    
    suggestions.forEach((sug, index) => {
        const suggDiv = document.createElement('div');
        suggDiv.className = 'recommend-item';
        suggDiv.style.animationDelay = `${index * 0.05}s`;
        
        // 根据编辑距离设置渐变色
        const colors = [
            'linear-gradient(135deg, #667eea 0%, #764ba2 100%)', // 距离0
            'linear-gradient(135deg, #f093fb 0%, #f5576c 100%)', // 距离1-2
            'linear-gradient(135deg, #4facfe 0%, #00f2fe 100%)', // 距离3+
        ];
        const colorIndex = sug.distance === 0 ? 0 : sug.distance <= 2 ? 1 : 2;
        suggDiv.style.background = colors[colorIndex];
        
        suggDiv.innerHTML = `
            <div class="recommend-word">${escapeHtml(sug.word)}</div>
            <div class="recommend-stats">
                <span class="stat-badge">编辑距离: ${sug.distance}</span>
                <span class="stat-badge">出现: ${sug.frequency} 次</span>
            </div>
        `;
        
        // 点击推荐词进行搜索
        suggDiv.addEventListener('click', () => {
            // 切换到搜索标签
            document.querySelector('.nav a[href="#search"]').click();
            searchInput.value = sug.word;
            performSearch(sug.word);
        });
        
        recommendResults.appendChild(suggDiv);
    });
}

// 示例按钮
document.querySelectorAll('.example-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const word = btn.getAttribute('data-word');
        recommendInput.value = word;
        performRecommend(word);
    });
});

// 推荐按钮点击
recommendBtn.addEventListener('click', () => {
    performRecommend(recommendInput.value);
});

// 推荐输入回车
recommendInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        performRecommend(recommendInput.value);
    }
});

// ========== 工具函数 ==========

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function showError(container, message) {
    container.innerHTML = `
        <div class="no-results">
            <svg width="80" height="80" viewBox="0 0 80 80">
                <circle cx="40" cy="40" r="35" fill="none" stroke="#ea4335" stroke-width="3"/>
                <line x1="30" y1="30" x2="50" y2="50" stroke="#ea4335" stroke-width="3" stroke-linecap="round"/>
                <line x1="50" y1="30" x2="30" y2="50" stroke="#ea4335" stroke-width="3" stroke-linecap="round"/>
            </svg>
            <h3 style="color: #ea4335;">出错了</h3>
            <p>${escapeHtml(message)}</p>
        </div>
    `;
}

// 页面加载完成
window.addEventListener('load', () => {
    console.log('SearchHub 已就绪');
    
    // 检查服务器状态
    fetch(`${API_BASE}/api/health`)
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                console.log('✓ 服务器连接正常');
            }
        })
        .catch(err => {
            console.error('✗ 服务器连接失败:', err);
        });
});

// ========== 文件上传功能 ==========

// 配置
const CHUNK_SIZE = 5 * 1024 * 1024; // 5MB每个分片
const MAX_CONCURRENT_CHUNKS = 3;     // 最大并发上传数

// 计算文件Hash（简单版本 - 基于文件信息）
async function calculateFileMD5(file) {
    // 使用文件名、大小、修改时间生成唯一标识（适用于HTTP环境）
    const fileInfo = file.name + '_' + file.size + '_' + file.lastModified;
    
    // 简单hash函数
    let hash = 0;
    for (let i = 0; i < fileInfo.length; i++) {
        const char = fileInfo.charCodeAt(i);
        hash = ((hash << 5) - hash) + char;
        hash = hash & hash; // 转换为32位整数
    }
    
    // 转换为16进制字符串
    const hashStr = Math.abs(hash).toString(16).padStart(8, '0');
    
    // 返回：hash_文件大小 格式
    return hashStr + '_' + file.size + '_' + file.lastModified;
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
    
    async init(folder = '') {
        // 初始化上传会话
        const requestData = {
            filename: this.file.name,
            hash: this.fileHash,
            total_size: this.file.size,
            total_chunks: this.totalChunks
        };
        
        // 添加文件夹参数（如果指定了）
        if (folder) {
            requestData.folder = folder;
        }
        
        const response = await fetch(`${API_BASE}/api/file/init`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(requestData)
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
    
    async upload(onProgress, folder = '', addToIndex = false) {
        this.uploading = true;
        
        // 检查秒传
        const exists = await this.checkExists();
        if (exists) {
            onProgress && onProgress(100, '秒传成功');
            return {success: true, message: '文件已存在，秒传成功'};
        }
        
        // 初始化上传
        if (!await this.init(folder)) {
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
            body: JSON.stringify({
                upload_id: this.uploadId,
                add_to_index: addToIndex  // 传递索引选项
            })
        });
        
        const data = await response.json();
        
        // 如果成功且需要添加到索引
        if (data.success && addToIndex) {
            // XML文件由后端直接处理
            if (data.xml_parsed) {
                console.log('📄 XML文件已由后端解析');
                if (data.parse_status === 0) {
                    console.log('✅ XML文件已成功添加到搜索索引');
                    data.indexed = true;
                    data.xml_items = data.parse_output;
                } else {
                    console.error('❌ XML解析失败');
                    data.indexed = false;
                }
            }
            // 其他文件由前端调用索引API
            else if (data.index_data) {
                console.log('📝 添加文件到搜索索引...');
                try {
                    const indexResponse = await fetch(`${API_BASE}/api/search/index/add`, {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify(data.index_data)
                    });
                    
                    const indexResult = await indexResponse.json();
                    if (indexResult.success) {
                        console.log('✅ 文件已添加到搜索索引');
                        data.indexed = true;
                    } else {
                        console.error('❌ 添加到索引失败:', indexResult.error);
                        data.indexed = false;
                    }
                } catch (err) {
                    console.error('❌ 添加到索引失败:', err);
                    data.indexed = false;
                }
            }
        }
        
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
    
    // 获取选择的文件夹
    const folderSelect = document.getElementById('upload-folder-select');
    const targetFolder = folderSelect ? folderSelect.value : '';
    
    // 获取是否添加到索引的选项
    const addToIndexCheckbox = document.getElementById('add-to-index-checkbox');
    const addToIndex = addToIndexCheckbox ? addToIndexCheckbox.checked : false;
    
    if (targetFolder) {
        console.log('📂 上传到文件夹:', targetFolder);
    }
    if (addToIndex) {
        console.log('📝 将添加到搜索索引');
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
        }, targetFolder, addToIndex);  // 传递索引选项
        
        console.log('✅ 上传结果:', result);
        
        if (result.success) {
            let message = result.message || '上传成功';
            if (result.indexed) {
                message += ' (已添加到搜索索引✨)';
            } else if (addToIndex && result.indexed === false) {
                message += ' (索引添加失败⚠️)';
            }
            updateUploadStatus(taskId, 'success', message);
            // 刷新当前文件夹的文件列表
            refreshFileList(currentFolder);
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

// ========== 文件管理器功能 ==========

let currentFolder = ''; // 当前文件夹路径

// 加载文件列表
async function refreshFileList(folder = '') {
    currentFolder = folder;
    
    try {
        const url = folder ? `${API_BASE}/api/file/list?folder=${encodeURIComponent(folder)}` : `${API_BASE}/api/file/list`;
        const response = await fetch(url);
        const data = await response.json();
        
        const fileGrid = document.getElementById('file-grid');
        if (!fileGrid) return;
        
        fileGrid.innerHTML = '';
        
        // 更新当前路径显示
        const pathSpan = document.getElementById('current-path');
        if (pathSpan) {
            pathSpan.textContent = folder ? ` / ${folder}` : '';
        }
        
        // 显示返回上级按钮
        if (folder) {
            const backBtn = document.createElement('div');
            backBtn.className = 'file-item folder-item';
            backBtn.innerHTML = `
                <div class="file-preview"><div class="file-icon-large">⬆️</div></div>
                <div class="file-details">
                    <div class="file-item-name">返回上级</div>
                </div>
            `;
            backBtn.onclick = () => refreshFileList('');
            fileGrid.appendChild(backBtn);
        }
        
        // 显示文件夹
        if (data.folders && data.folders.length > 0) {
            data.folders.forEach(folderItem => {
                const item = createFolderItem(folderItem);
                fileGrid.appendChild(item);
            });
        }
        
        // 显示文件
        if (data.files && data.files.length > 0) {
            data.files.forEach(file => {
                const item = createFileItem(file);
                fileGrid.appendChild(item);
            });
        }
        
        if (!data.folders?.length && !data.files?.length && !folder) {
            fileGrid.innerHTML = '<p style="text-align:center;color:#666;padding:40px;">暂无文件，点击上方上传或新建文件夹</p>';
        } else if (!data.folders?.length && !data.files?.length && folder) {
            fileGrid.innerHTML = '<p style="text-align:center;color:#666;padding:40px;">文件夹为空</p>';
        }
        
        // 更新文件夹选择器
        updateFolderSelect(data.folders || []);
        
        // 更新统计
        loadStorageStats();
    } catch (err) {
        console.error('加载文件列表失败:', err);
    }
}

// 更新文件夹选择器
function updateFolderSelect(folders) {
    const select = document.getElementById('upload-folder-select');
    if (!select) return;
    
    // 保存当前选择
    const currentValue = select.value;
    
    // 清空并重建选项
    select.innerHTML = '<option value="">根目录</option>';
    
    folders.forEach(folder => {
        const option = document.createElement('option');
        option.value = folder.name;
        option.textContent = `📁 ${folder.name}`;
        select.appendChild(option);
    });
    
    // 恢复选择或选择当前文件夹
    if (currentFolder) {
        select.value = currentFolder;
    } else {
        select.value = currentValue;
    }
}

// 创建文件夹项
function createFolderItem(folder) {
    const item = document.createElement('div');
    item.className = 'file-item folder-item';
    item.onclick = () => refreshFileList(folder.name);
    
    item.innerHTML = `
        <div class="file-preview"><div class="file-icon-large">📁</div></div>
        <div class="file-details">
            <div class="file-item-name">${folder.name}</div>
            <div class="file-item-info">${folder.file_count || 0} 个文件</div>
        </div>
    `;
    
    return item;
}

// 创建文件项
function createFileItem(file) {
    const item = document.createElement('div');
    item.className = 'file-item';
    
    const fileIcon = getFileIcon(file.type);
    const sizeStr = formatFileSize(file.size);
    
    // 使用完整文件名（包括扩展名）作为下载路径
    const downloadHash = file.name; // 现在name包含了扩展名
    
    let previewHtml = '';
    if (file.type === 'image') {
        // 图片使用hash（不含扩展名）来下载
        previewHtml = `<img src="/api/file/download/${file.hash}" alt="${file.name}" onerror="this.parentElement.innerHTML='<div class=\\'file-icon-large\\'>🖼️</div>'">`;
    } else {
        previewHtml = `<div class="file-icon-large">${fileIcon}</div>`;
    }
    
    const escapedName = file.name.replace(/'/g, "\\'");
    
    item.innerHTML = `
        <div class="file-preview">${previewHtml}</div>
        <div class="file-details">
            <div class="file-item-name" title="${file.name}">${file.name}</div>
            <div class="file-item-info">${sizeStr} · ${file.modified || ''}</div>
            <div class="file-item-actions">
                ${file.type === 'image' ? `<button class="btn-preview" onclick="previewFile('${file.hash}', '${escapedName}', '${file.type}')">👁 预览</button>` : ''}
                <button class="btn-download" onclick="downloadFile('${file.hash}', '${escapedName}')">⬇ 下载</button>
                <button class="btn-delete" onclick="deleteFile('${file.hash}', '${escapedName}')">🗑 删除</button>
            </div>
        </div>
    `;
    
    return item;
}

// 获取文件图标
function getFileIcon(type) {
    const icons = {
        'image': '🖼️',
        'pdf': '📄',
        'document': '📝',
        'text': '📃',
        'video': '🎬',
        'archive': '📦',
        'unknown': '📄'
    };
    return icons[type] || '📄';
}

// 预览文件
function previewFile(hash, name, type) {
    if (type === 'image') {
        const modal = document.createElement('div');
        modal.className = 'preview-modal active';
        modal.innerHTML = `
            <div class="preview-content">
                <button class="preview-close" onclick="this.parentElement.parentElement.remove()">×</button>
                <h3 style="margin-bottom: 15px;">${escapeHtml(name)}</h3>
                <img src="/api/file/download/${hash}" alt="${name}">
            </div>
        `;
        document.body.appendChild(modal);
        
        // 点击背景关闭
        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                modal.remove();
            }
        });
    }
}

// 下载文件
function downloadFile(hash, name) {
    const link = document.createElement('a');
    link.href = `/api/file/download/${hash}`;
    link.download = name;
    link.click();
}

// 删除文件
async function deleteFile(hash, name) {
    if (!confirm(`确定要删除 "${name}" 吗？`)) {
        return;
    }
    
    try {
        const response = await fetch(`/api/file/delete/${hash}`, {
            method: 'DELETE'
        });
        const data = await response.json();
        
        if (data.success) {
            alert('✓ 删除成功');
            refreshFileList(currentFolder);
        } else {
            alert('✗ 删除失败: ' + data.error);
        }
    } catch (err) {
        alert('✗ 删除失败: ' + err.message);
    }
}

// 切换视图模式
function changeViewMode(mode) {
    const fileGrid = document.getElementById('file-grid');
    if (mode === 'list') {
        fileGrid.classList.add('list-view');
    } else {
        fileGrid.classList.remove('list-view');
    }
}

// 创建新文件夹
async function createNewFolder() {
    const folderName = prompt('请输入文件夹名称:');
    if (!folderName) return;
    
    // 验证文件夹名
    if (!/^[a-zA-Z0-9_\u4e00-\u9fa5-]+$/.test(folderName)) {
        alert('文件夹名只能包含字母、数字、中文、下划线和横线');
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/file/mkdir`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                name: folderName,
                parent: currentFolder
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            alert(`✓ 文件夹"${folderName}"创建成功`);
            refreshFileList(currentFolder);
        } else {
            alert('创建失败: ' + data.message);
        }
    } catch (err) {
        console.error('创建文件夹失败:', err);
        alert('创建文件夹失败: ' + err.message);
    }
}


// ========== 多模态搜索功能 ==========

let selectedImageFile = null;

// 检查多模态服务状态
async function checkMultimodalService() {
    try {
        const response = await fetch(`${API_BASE}/api/multimodal/health`);
        const data = await response.json();
        
        if (data.status === 'ok' && data.ready) {
            console.log('✅ 多模态服务就绪');
            showMultimodalStatus('🎨 AI搜索服务已就绪', 'success');
        } else {
            showMultimodalStatus('⚠️ 服务未就绪', 'warning');
        }
    } catch (error) {
        showMultimodalStatus('❌ 多模态服务离线<br>请运行: ./start_multimodal.sh', 'error');
    }
}

function showMultimodalStatus(message, type) {
    const resultsDiv = document.getElementById('multimodal-results');
    if (!resultsDiv) return;
    
    const icons = {success: '✨', warning: '⚠️', error: '❌'};
    const classes = {success: 'success', warning: 'warning', error: 'error'};
    
    resultsDiv.innerHTML = `
        <div class="status-message ${classes[type]}">
            <span style="font-size: 24px;">${icons[type]}</span>
            <span>${message}</span>
        </div>
    `;
}

// 模式切换按钮事件
const modeBtns = document.querySelectorAll('.mode-btn');
if (modeBtns.length > 0) {
    modeBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const mode = btn.getAttribute('data-mode');
            modeBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            
            const textPanel = document.getElementById('text-search-panel');
            const imagePanel = document.getElementById('image-search-panel');
            
            if (mode === 'text') {
                textPanel?.classList.add('active');
                imagePanel?.classList.remove('active');
            } else {
                textPanel?.classList.remove('active');
                imagePanel?.classList.add('active');
            }
        });
    });
}

// 绑定文本搜索按钮
const multimodalTextBtn = document.getElementById('multimodal-text-btn');
const multimodalTextInput = document.getElementById('multimodal-text-input');

if (multimodalTextBtn) {
    multimodalTextBtn.addEventListener('click', () => {
        performMultimodalTextSearch();
    });
}

if (multimodalTextInput) {
    multimodalTextInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            performMultimodalTextSearch();
        }
    });
}

// 文本语义搜索
async function performMultimodalTextSearch() {
    const input = document.getElementById('multimodal-text-input');
    const query = input?.value.trim();
    if (!query) return;
    
    console.log('🔍 多模态搜索:', query);
    
    const loading = document.getElementById('multimodal-loading');
    const results = document.getElementById('multimodal-results');
    loading?.classList.remove('hidden');
    if (results) results.innerHTML = '';
    
    try {
        const response = await fetch(`${API_BASE}/api/multimodal/search`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({type: 'text', query: query, top_k: 20})
        });
        
        const data = await response.json();
        console.log('搜索结果:', data);
        loading?.classList.add('hidden');
        
        if (data.success) {
            displayMultimodalResults(data.results, 'text', query);
        } else {
            showMultimodalStatus(data.error || '搜索失败', 'error');
        }
    } catch (error) {
        console.error('多模态搜索错误:', error);
        loading?.classList.add('hidden');
        showMultimodalStatus('❌ 搜索失败: ' + error.message, 'error');
    }
}

// 显示搜索结果
function displayMultimodalResults(results, searchType, query) {
    const resultsDiv = document.getElementById('multimodal-results');
    if (!resultsDiv) return;
    
    if (results.length === 0) {
        resultsDiv.innerHTML = `
            <div class="no-results">
                <div style="font-size: 60px; margin-bottom: 20px;">🔍</div>
                <p style="font-size: 20px; font-weight: 600; color: #333;">未找到相关内容</p>
                <p class="hint">尝试使用不同的描述词或上传更多文件</p>
            </div>
        `;
        return;
    }
    
    let html = `
        <div class="results-header">
            <h3>🎯 找到 ${results.length} 个相关文件</h3>
            <p>搜索词: <strong>${escapeHtml(query || '图片')}</strong></p>
        </div>
        <div class="multimodal-grid">
    `;
    
    results.forEach((result, index) => {
        const isImage = result.file_type === 'image';
        const similarity = result.similarity || (result.score * 100);
        const rankColor = index === 0 ? '#ffd700' : index === 1 ? '#c0c0c0' : index === 2 ? '#cd7f32' : '#e0e0e0';
        
        html += `
            <div class="multimodal-item">
                ${isImage ? `
                    <div class="multimodal-image">
                        <img src="/api/file/download/${result.file_hash}" 
                             alt="${escapeHtml(result.filename)}"
                             onerror="this.parentElement.innerHTML='<div class=\\'multimodal-icon\\'>📷</div>'">
                        <div style="position: absolute; top: 10px; right: 10px; background: ${rankColor}; color: white; width: 30px; height: 30px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-weight: bold; box-shadow: 0 2px 8px rgba(0,0,0,0.2);">
                            ${index + 1}
                        </div>
                    </div>
                ` : `
                    <div class="multimodal-icon">
                        <span style="font-size: 80px;">📄</span>
                        <div style="position: absolute; top: 10px; right: 10px; background: ${rankColor}; color: white; width: 30px; height: 30px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-weight: bold;">
                            ${index + 1}
                        </div>
                    </div>
                `}
                <div class="multimodal-info">
                    <div class="multimodal-name" title="${escapeHtml(result.filename)}">
                        ${escapeHtml(result.filename)}
                    </div>
                    <div class="multimodal-meta">
                        <span class="file-type-badge">${result.file_type}</span>
                        <span class="similarity-badge">🎯 ${similarity.toFixed(1)}%</span>
                    </div>
                    ${result.folder ? `<div class="folder-path">📁 ${escapeHtml(result.folder)}</div>` : ''}
                </div>
                <div class="multimodal-actions">
                    <button onclick="window.open('/api/file/download/${result.file_hash}', '_blank')">
                        ⬇️ 下载文件
                    </button>
                </div>
            </div>
        `;
    });
    
    html += '</div>';
    resultsDiv.innerHTML = html;
}
