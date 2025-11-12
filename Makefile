# Project Makefile

CXX ?= g++
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -pthread

# Allow overriding include/library paths from environment
CPPJIEBA_INC ?="/home/oym/cppjieba/cppjieba-5.0.3"
TINYXML2_INC ?=
TINYXML2_LIB ?=
NLOHMANN_JSON_INC ?="/home/oym/newcpp/search-engine/include"

INC_FLAGS := $(if $(CPPJIEBA_INC),-I$(CPPJIEBA_INC),) $(if $(TINYXML2_INC),-I$(TINYXML2_INC),) $(if $(NLOHMANN_JSON_INC),-I$(NLOHMANN_JSON_INC),)
LDFLAGS ?=
LDLIBS ?= $(if $(TINYXML2_LIB),-L$(TINYXML2_LIB),) 
#-ltinyxml2

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
TARGET := ./a.out
WEB_TARGET := ./web_server

SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/thread_pool.cpp \
	$(SRC_DIR)/inverted_index.cpp \
	$(SRC_DIR)/tokenizer.cpp \
	$(SRC_DIR)/page_parser.cpp \
	$(SRC_DIR)/simhash.cpp \
	$(SRC_DIR)/weighted_inverted_index.cpp \
	$(SRC_DIR)/offline_pipeline.cpp \
	$(SRC_DIR)/search_engine.cpp \
	$(SRC_DIR)/tinyxml2.cpp \
	$(SRC_DIR)/keyword_config.cpp \
	$(SRC_DIR)/keyword_dict.cpp \
	$(SRC_DIR)/keyword_recommender.cpp \
	$(SRC_DIR)/app_config.cpp \
	$(SRC_DIR)/command_handler.cpp

OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# 旧的Web服务器已删除，使用微服务架构

# 微服务源文件
# 搜索服务
SEARCH_SERVICE_SRCS := \
	$(SRC_DIR)/search_service.cpp \
	$(SRC_DIR)/search_engine.cpp \
	$(SRC_DIR)/search_cache.cpp \
	$(SRC_DIR)/weighted_inverted_index.cpp \
	$(SRC_DIR)/inverted_index.cpp \
	$(SRC_DIR)/dynamic_index.cpp \
	$(SRC_DIR)/tokenizer.cpp \
	$(SRC_DIR)/thread_pool.cpp \
	$(SRC_DIR)/app_config.cpp

# 推荐服务
RECOMMEND_SERVICE_SRCS := \
	$(SRC_DIR)/recommend_service.cpp \
	$(SRC_DIR)/keyword_recommender.cpp \
	$(SRC_DIR)/keyword_dict.cpp \
	$(SRC_DIR)/tokenizer.cpp \
	$(SRC_DIR)/app_config.cpp

# 文件上传服务
FILE_SERVICE_SRCS := \
	$(SRC_DIR)/file_service.cpp \
	$(SRC_DIR)/app_config.cpp

SEARCH_SERVICE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SEARCH_SERVICE_SRCS))
RECOMMEND_SERVICE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(RECOMMEND_SERVICE_SRCS))
FILE_SERVICE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(FILE_SERVICE_SRCS))

# wfrest 和 workflow 的库路径（需要根据实际安装路径修改）
WFREST_INC ?= /usr/local/include
WFREST_LIB ?= /usr/local/lib
WORKFLOW_LIB ?= /usr/local/lib

WEB_LDFLAGS := -L$(WFREST_LIB) -L$(WORKFLOW_LIB) -lwfrest -lworkflow -lssl -lcrypto -lpthread -lhiredis
WEB_INC_FLAGS := -I$(WFREST_INC) $(INC_FLAGS)

# 微服务目标
SEARCH_SERVICE := ./search_service
RECOMMEND_SERVICE := ./recommend_service
FILE_SERVICE := ./file_service

.PHONY: all clean dirs run microservices

all: dirs $(TARGET)

# 编译所有微服务（推荐使用）
microservices: dirs $(SEARCH_SERVICE) $(RECOMMEND_SERVICE) $(FILE_SERVICE)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# 微服务编译规则
$(SEARCH_SERVICE): $(SEARCH_SERVICE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(WEB_LDFLAGS)

$(RECOMMEND_SERVICE): $(RECOMMEND_SERVICE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(WEB_LDFLAGS)

$(FILE_SERVICE): $(FILE_SERVICE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(WEB_LDFLAGS) -lcrypto

# 普通编译规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@

# search_cache 和微服务需要 wfrest 头文件
$(BUILD_DIR)/search_cache.o: $(SRC_DIR)/search_cache.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(WEB_INC_FLAGS) -c $< -o $@

$(BUILD_DIR)/search_service.o: $(SRC_DIR)/search_service.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(WEB_INC_FLAGS) -c $< -o $@

$(BUILD_DIR)/recommend_service.o: $(SRC_DIR)/recommend_service.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(WEB_INC_FLAGS) -c $< -o $@

$(BUILD_DIR)/file_service.o: $(SRC_DIR)/file_service.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(WEB_INC_FLAGS) -c $< -o $@

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(TARGET) $(SEARCH_SERVICE) $(RECOMMEND_SERVICE) $(FILE_SERVICE)

