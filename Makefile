CC = g++

CPPFLAGS = -O2 -Wunused-result
CPPFLAGS_DEBUG = -Wall -g

OBJS = ./src/main.o ./src/clustering.o ./src/batch_add.o ./src/batch_delete.o
SRCS = ./src/$(OBJS:.o=.cpp)
BIN = ./bin/
INCLUDE = ./src
LIBNAME = -lpthread -lboost_system

CPPFLAGS += -I$(INCLUDE)

all: spp

spp: $(OBJS)
	$(CC) -o $(BIN)spp $(OBJS) $(CPPFLAGS) $(LIBNAME)

debug: $(OBJS)
	$(CC) -o $(BIN)spp $(OBJS) $(CPPFLAGS_DEBUG) $(LIBNAME)

clean:
	rm -rf $(OBJS)

allclean:
	rm -rf $(OBJS) $(BIN)*
