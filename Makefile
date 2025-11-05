# Project Makefile

CXX ?= g++
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -pthread

# Allow overriding include/library paths from environment
CPPJIEBA_INC ?="/home/oym/cppjieba/cppjieba-5.0.3"
TINYXML2_INC ?=
TINYXML2_LIB ?=

INC_FLAGS := $(if $(CPPJIEBA_INC),-I$(CPPJIEBA_INC),) $(if $(TINYXML2_INC),-I$(TINYXML2_INC),)
LDFLAGS ?=
LDLIBS ?= $(if $(TINYXML2_LIB),-L$(TINYXML2_LIB),) -ltinyxml2

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/offline_build

SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/thread_pool.cpp \
	$(SRC_DIR)/inverted_index.cpp \
	$(SRC_DIR)/tokenizer.cpp \
	$(SRC_DIR)/page_parser.cpp \
	$(SRC_DIR)/simhash.cpp \
	$(SRC_DIR)/weighted_inverted_index.cpp \
	$(SRC_DIR)/offline_pipeline.cpp

OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean dirs run

all: dirs $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -c $< -o $@

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

