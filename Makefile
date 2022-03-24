all: main

main: ai_client.cpp ai_client.h main.cpp space.cpp
	g++ ai_client.cpp main.cpp space.cpp jsoncpp/jsoncpp.cpp -o main -std=c++14