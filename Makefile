CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic

TARGET := arm
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp include/%.h
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

src/main.o: src/main.cpp
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
