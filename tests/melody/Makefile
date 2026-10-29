CXX ?= g++
CXXFLAGS ?= -std=c++11 -O1 -Wall -Wextra -I../../core
CORE_SRC := ../../core/engine.cpp ../../core/sched.cpp ../../core/track.cpp
TEST_SRC := $(wildcard test_*.cpp) main.cpp
BIN := ../../build/tests-melody.exe

all: run

$(BIN): $(TEST_SRC) $(CORE_SRC) $(wildcard ../../core/*.hpp) t.hpp
	@mkdir -p ../../build
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(CORE_SRC) -o $(BIN)

# From the repo root, not tests/: test_golden opens tests/melody/golden_seed42.txt.
run: $(BIN)
	cd ../.. && ./build/tests-melody.exe

.PHONY: all run
golden: $(BIN)
	cd ../.. && ./build/tests-melody.exe --write-golden
.PHONY: golden
