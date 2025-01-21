all:reactor_server client

reactor_server:main.cc
	g++ $^ -o $@ -std=c++11 -ljsoncpp -lpthread

client:client.cc
	g++ $^ -o $@ -std=c++11 -ljsoncpp

.PHONY:clean
clean:
	rm -f reactor_server client	