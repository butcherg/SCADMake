CC = g++
CFLAGS = -std=c++17 -ggdb
LFLAGS = -static
PROG = scadmake
EXE = 

$(PROG)$(EXE): $(PROG).o
	$(CC) $(LFLAGS) -o $(PROG)$(EXE) $(PROG).o

$(PROG).o: $(PROG).cpp
	$(CC) $(CFLAGS) -o $(PROG).o -c $(PROG).cpp

clean:
	rm -rf *.o $(PROG)$(EXE)
