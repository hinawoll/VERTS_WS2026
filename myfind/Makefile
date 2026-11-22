# Diese Ziele sind keine Dateien und werden immer als Befehle ausgeführt.
.PHONY: all clean

all: myfind

# Das Programm besteht aus den beiden Objektdateien.
myfind: main.o search.o
	# Kompiliert und verbindet alle Objektdateien zu einem Programm.
	g++ -Wall -g -std=c++17 main.o search.o -o myfind

# Erstellt die Objektdatei für die Hauptdatei.
main.o: main.cpp search.h
	g++ -Wall -g -std=c++17 -c main.cpp

# Erstellt die Objektdatei für die Suchfunktionen.
search.o: search.cpp search.h
	g++ -Wall -g -std=c++17 -c search.cpp

# Entfernt alle erzeugten Objektdateien und das Programm.
clean:
	rm -f main.o search.o myfind