# Makefile — 编译 threadpool 测试（无需 CMake）
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -Iservice/threadpool -Ibuild/_gtest/googletest/include -Ibuild/_gtest/googletest
BUILD    := build

# ---- 源文件 ----
LIB_SRCS  := service/threadpool/task_queue.cpp service/threadpool/thread_pool.cpp
DEMO_SRCS := service/threadpool/main.cpp
TEST_SRCS := tests/threadpool/unit/test_task_queue.cpp tests/threadpool/unit/test_thread_pool.cpp
GTEST_SRC := build/_gtest/googletest/src/gtest-all.cc
GTEST_DIR := build/_gtest/googletest

# ---- 目标 ----
LIB_OBJS  := $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_SRCS))
GTEST_OBJ  := $(BUILD)/gtest-all.o
TEST_OBJS  := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRCS))

.PHONY: all test clean demo

all: test threadpool_demo.exe

# ============= Google Test 静态库 =============
$(GTEST_OBJ): $(GTEST_SRC)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============= ThreadPool 库目标文件 =============
$(BUILD)/%.o: %.cpp %.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============= 测试目标文件 =============
$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============= 可执行文件 =============
threadpool_demo.exe: $(LIB_OBJS) $(BUILD)/service/threadpool/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/service/threadpool/main.o: service/threadpool/main.cpp
	@mkdir -p $(BUILD)/service/threadpool
	$(CXX) $(CXXFLAGS) -c $< -o $@

test_task_queue.exe: $(BUILD)/service/threadpool/task_queue.o $(BUILD)/tests/threadpool/unit/test_task_queue.o $(GTEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

test_thread_pool.exe: $(BUILD)/service/threadpool/task_queue.o $(BUILD)/service/threadpool/thread_pool.o $(BUILD)/tests/threadpool/unit/test_thread_pool.o $(GTEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ============= 构建测试 =============
test: test_task_queue.exe test_thread_pool.exe
	@echo ""
	@echo "============================================"
	@echo "  Running: test_task_queue"
	@echo "============================================"
	@./test_task_queue.exe
	@echo ""
	@echo "============================================"
	@echo "  Running: test_thread_pool"
	@echo "============================================"
	@./test_thread_pool.exe

demo: threadpool_demo.exe
	@./threadpool_demo.exe

clean:
	rm -rf $(BUILD)/*.o $(BUILD)/tests/*.o $(BUILD)/service/*.o $(BUILD)/gtest-all.o test_task_queue.exe test_thread_pool.exe threadpool_demo.exe
