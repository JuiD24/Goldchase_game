game: game.cpp libmap.a goldchase.h libmap.a
	g++ -std=c++17 game.cpp -o game -L. -lmap -lpanel -lncurses -lrt -pthread

libmap.a: Screen.o Map.o
	ar -r libmap.a Screen.o Map.o

Screen.o: Screen.cpp
	g++ -std=c++17 -c Screen.cpp

Map.o: Map.cpp
	g++ -std=c++17 -c Map.cpp

clean:
	rm -f Screen.o Map.o libmap.a game
