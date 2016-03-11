CC = g++

OBJS = ./harness.o ./test_program.o
SRCS = ./$(OBJS: .o=.cpp)

CPPFLAGS = -g -O3 -std=gnu++11 -Wall -Werror

all: harness

harness: $(OBJS)
	$(CC) -o tester $(OBJS) $(CPPFLAGS)

clean:
	rm -rf $(OBJS)

allclean:
	rm -rf $(OBJS) tester
