.PHONY: all clean

all: myfind

myfind: main.o search.o
	g++ -Wall -g -std=c++17 main.o search.o -o myfind

main.o: main.cpp search.h
	g++ -Wall -g -std=c++17 -c main.cpp

search.o: search.cpp search.h
	g++ -Wall -g -std=c++17 -c search.cpp

clean:
	rm -f main.o search.o myfind