CC=g++
FLAGS=-o
OBJ=obj
SOURCES=$(wildcard *.cpp)
EXECUTABLES=$(patsubst %.cpp, $(OBJ)/run_%, $(SOURCES))

all: $(EXECUTABLES) run clean

build: $(EXECUTABLES)

$(OBJ)/run_%: %.cpp
	$(CC) $< $(FLAGS) $@

run: 
	@for exe in $(EXECUTABLES); do \
		./$$exe; \
	done

clean:
	rm $(OBJ)/run_*