# 指定编译器
CXX = g++
CXXFLAGS = -std=c++17

# 指定链接库
LIBS = -lndn-cxx -lboost_system -lspdlog -lfmt -lstdc++fs

# 指定源文件和目标文件
PRODUCER_SRC = producer_test.cpp
CONSUMER_SRC = consumer_test.cpp
PRODUCER_OBJ = producer_test
CONSUMER_OBJ = consumer_test
M_PRODUCER_SRC = mproducer.cpp
S_CONSUMER_SRC = sconsumer.cpp
M_PRODUCER_OBJ= MProducer
S_CONSUMER_OBJ= SConsumer

NDN_PRODUCER_SRC = ndn-producer.cpp ndn-app.cpp ModelData.cpp
NDN_CONSUMER_INA_SRC = ndn-consumer-INA.cpp ndn-consumer.cpp ndn-app.cpp
NDN_AGGREGATOR_SRC = ndn-aggregator.cpp ndn-app.cpp
NDN_PRODUCER_OBJ = ndn-producer
NDN_CONSUMER_INA_OBJ = ndn-consumer-INA
NDN_AGGREGATOR_OBJ = ndn-aggregator


ALGORITHM_SRC = $(wildcard algorithm/src/*.cpp algorithm/utility/*.cpp)
# 默认目标
all: $(PRODUCER_OBJ) $(CONSUMER_OBJ)

# 编译 producer
$(PRODUCER_OBJ): $(PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $(PRODUCER_SRC) -o $(PRODUCER_OBJ) $(LIBS)

# 编译 consumer
$(CONSUMER_OBJ): $(CONSUMER_SRC)
	$(CXX) $(CXXFLAGS) $(CONSUMER_SRC) -o $(CONSUMER_OBJ) $(LIBS)

# 编译 MProducer
$(M_PRODUCER_OBJ): $(M_PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $(M_PRODUCER_SRC) -o $(M_PRODUCER_OBJ) $(LIBS)

# 编译 SConsumer
$(S_CONSUMER_OBJ): $(S_CONSUMER_SRC)
	$(CXX) $(CXXFLAGS) $(S_CONSUMER_SRC) -o $(S_CONSUMER_OBJ) $(LIBS)

# 编译 NDN Producer
$(NDN_PRODUCER_OBJ): $(NDN_PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) $(NDN_PRODUCER_SRC) -o $(NDN_PRODUCER_OBJ) $(LIBS)

# 编译 NDN Consumer-INA
$(NDN_CONSUMER_INA_OBJ): $(NDN_CONSUMER_INA_SRC) $(ALGORITHM_SRC)
	$(CXX) $(CXXFLAGS) $(NDN_CONSUMER_INA_SRC) $(ALGORITHM_SRC) -o $(NDN_CONSUMER_INA_OBJ) $(LIBS)
# 编译 NDN Aggregator
$(NDN_AGGREGATOR_OBJ): $(NDN_AGGREGATOR_SRC) $(ALGORITHM_SRC)
	$(CXX) $(CXXFLAGS) $(NDN_AGGREGATOR_SRC) $(ALGORITHM_SRC) -o $(NDN_AGGREGATOR_OBJ) $(LIBS)


scmp: $(M_PRODUCER_OBJ) $(S_CONSUMER_OBJ)

ndn: $(NDN_PRODUCER_OBJ) $(NDN_CONSUMER_INA_OBJ) $(NDN_AGGREGATOR_OBJ)

# 测试目标
test: all

# 清理目标
clean:
	rm -f $(PRODUCER_OBJ) $(CONSUMER_OBJ) $(M_PRODUCER_OBJ) $(S_CONSUMER_OBJ) $(NDN_PRODUCER_OBJ) $(NDN_CONSUMER_INA_OBJ) $(NDN_AGGREGATOR_OBJ)