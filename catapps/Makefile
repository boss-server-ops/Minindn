# 指定编译器
CXX = g++
CXXFLAGS = -std=c++17

# 指定链接库
LIBS = -lndn-cxx -lboost_system -lspdlog -lfmt -lstdc++fs -lboost_program_options

# 指定源文件和目标文件
SRC_DIRS = chunk pipeline aggtree controller
CONSUMER_SRC = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp)) main.cpp
CONSUMER_OBJ = consumer

# 默认目标
all: $(CONSUMER_OBJ)

# 编译 consumer
$(CONSUMER_OBJ): $(CONSUMER_SRC)
	$(CXX) $(CXXFLAGS) $(CONSUMER_SRC) -o $(CONSUMER_OBJ) $(LIBS)

# 清理目标
clean:
	rm -f $(CONSUMER_OBJ)