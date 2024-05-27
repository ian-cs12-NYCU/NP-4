CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

all: socks_server

debug: CXXFLAGS += -DDEBUG 
debug: socks_server socks_server2

socks_server: socks_server.cpp
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) -o socks_server socks_server.cpp

socks_server2 : socks_server_2.cpp
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) -o socks_server2 socks_server_2.cpp

test: test.cpp
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) -o test test.cpp
