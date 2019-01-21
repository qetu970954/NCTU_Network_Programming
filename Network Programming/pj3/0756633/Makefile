CXX=clang++
CXX_LIB_PARAM=-L /usr/local/lib
CXX_INCLUDE_PARAM=-I /usr/local/include
CXXFLAGS=-Wall -pedantic -std=c++11 -pthread -lboost_system $(CXX_LIB_PARAM) $(CXX_INCLUDE_PARAM)
.PHONY: all clean

all: http_server console.cgi

http_server: http_server.cpp string_manipulator.h
	$(CXX) $< -o $@ $(CXXFLAGS)

console.cgi: console.cpp string_manipulator.h
	$(CXX) $< -o $@ $(CXXFLAGS)

clean:
	rm -f http_server
	rm -f console.cgi