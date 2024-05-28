CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

all: socks_server hw4.cgi 

debug: CXXFLAGS += -DDEBUG 
debug: socks_server hw4.cgi 

socks_server: socks_server.cpp
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) -o socks_server socks_server.cpp

hw4.cgi: console.cpp
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) -o hw4.cgi console.cpp

http_server: http_server.cpp
	$(CXX) http_server.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

clean:
	rm -f socks_server hw4.cgi 


