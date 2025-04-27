# 指定编译器
CXX = g++
CXXFLAGS = -std=c++17

# 指定链接库
LIBS = -lndn-cxx -lboost_system -lspdlog -lfmt -lstdc++fs -lboost_program_options

# 指定源文件和目标文件
PRODUCER_SRC = producer.cpp main.cpp InputGenerator.cpp
PRODUCER_OBJ = producer

# 默认目标
all: $(PRODUCER_OBJ)

# 编译 producer
$(PRODUCER_OBJ): $(PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $(PRODUCER_SRC) -o $(PRODUCER_OBJ) $(LIBS)

# 清理目标
clean:
	rm -f $(PRODUCER_OBJ)