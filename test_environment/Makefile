CC = g++

CPPFLAGS = -g -O3 -std=gnu++11 -Wall -Werror

OBJS = ./src/harness.o ./src/test_program.o
SRCS = ./src/$(OBJS: .o=.cpp)
BIN = ./bin/
INCLUDE = ./src

CPPFLAGS += -I$(INCLUDE)

all: harness

harness: $(OBJS)
	$(CC) -o $(BIN)tester $(OBJS) $(CPPFLAGS)

clean:
	rm -rf $(OBJS)

allclean:
	rm -rf $(OBJS) $(BIN)*
