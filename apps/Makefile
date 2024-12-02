# 指定编译器
CXX = g++
CXXFLAGS = -std=c++17

# 指定链接库
LIBS = -lndn-cxx -lboost_system -lspdlog -lfmt

# 指定源文件和目标文件
PRODUCER_SRC = producer_test.cpp
CONSUMER_SRC = consumer_test.cpp
PRODUCER_OBJ = producer_test
CONSUMER_OBJ = consumer_test

# 默认目标
all: $(PRODUCER_OBJ) $(CONSUMER_OBJ)

# 编译 producer
$(PRODUCER_OBJ): $(PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $(PRODUCER_SRC) -o $(PRODUCER_OBJ) $(LIBS)

# 编译 consumer
$(CONSUMER_OBJ): $(CONSUMER_SRC)
	$(CXX) $(CXXFLAGS) $(CONSUMER_SRC) -o $(CONSUMER_OBJ) $(LIBS)

# 测试目标
test: all

# 清理目标
clean:
	rm -f $(PRODUCER_OBJ) $(CONSUMER_OBJ)