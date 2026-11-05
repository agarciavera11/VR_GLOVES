# Compila el firmware en el PC con stubs de Arduino y ejecuta las pruebas.
CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -DHOST_TEST -Istubs -I../guante_haptico -g
SRC = test_main.cpp $(wildcard ../guante_haptico/*.cpp)

test: test_host
	./test_host

test_host: $(SRC) $(wildcard ../guante_haptico/*.h) $(wildcard stubs/*.h)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f test_host golden_vectors.txt
.PHONY: test clean
