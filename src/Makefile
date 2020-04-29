PROGRAM = logoframe
OBJS = logoframe.o logoframe_det.o logoframe_mul.o

CC = gcc
CFLAGS = -O3 -I../avisynth -ffast-math -Wall -Wshadow -Wempty-body -I. -std=gnu99 -fomit-frame-pointer -s -fno-tree-vectorize -fno-zero-initialized-in-bss
LDLAGS = -ldl

.SUFFIXES: .c .o

$(PROGRAM): $(OBJS)
	$(CC) $(LDLAGS) -o $(PROGRAM) $^

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	$(RM) $(PROGRAM) $(OBJS)
