CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3 -march=native

MAIN_TARGET = orderbook
TEST_TARGET = orderbook_tests
BENCH_TARGET = orderbook_benchmark

MAIN_SRCS = main.cpp orderbook.cpp
TEST_SRCS = test.cpp orderbook.cpp
BENCH_SRCS = benchmark.cpp orderbook.cpp

.PHONY: all clean run test benchmark debug

all: $(MAIN_TARGET) $(TEST_TARGET)

$(MAIN_TARGET): $(MAIN_SRCS) orderbook.hpp
	$(CXX) $(CXXFLAGS) $(MAIN_SRCS) -o $(MAIN_TARGET)

$(TEST_TARGET): $(TEST_SRCS) orderbook.hpp
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) -o $(TEST_TARGET)

$(BENCH_TARGET): $(BENCH_SRCS) orderbook.hpp
	$(CXX) $(CXXFLAGS) $(BENCH_SRCS) -o $(BENCH_TARGET)

run: $(MAIN_TARGET)
	./$(MAIN_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

benchmark: $(BENCH_TARGET)
	./$(BENCH_TARGET)

debug:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -g -O0 $(MAIN_SRCS) -o $(MAIN_TARGET)

clean:
	rm -f $(MAIN_TARGET) $(TEST_TARGET) $(BENCH_TARGET)
	