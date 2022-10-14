CC = /bin/g++
EXE = npshell
OBJ = npshell.o

%.o: %.c
	$(CC)  $< -o $@

all: $(OBJ)
	$(CC)  $(OBJ) -o $(EXE)

clean:
	rm $(OBJ) $(EXE)
