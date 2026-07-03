CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall
LDLIBS   := -lraylib -lm

squarebound: src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)

run: squarebound
	./squarebound

clean:
	rm -f squarebound

.PHONY: run clean
