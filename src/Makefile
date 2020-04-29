PROGRAM = logoframe
OBJS = logoframe.o logoframe_det.o logoframe_mul.o

CC = g++
CFLAGS = -O3 -I/usr/local/include/avisynth -ffast-math -Wall -Wshadow -Wempty-body -I. -fpermissive -std=gnu99 -fomit-frame-pointer -s -fno-tree-vectorize -fno-zero-initialized-in-bss
LDLAGS = -ldl -pthread

.SUFFIXES: .c .o

$(PROGRAM): $(OBJS)
	$(CC) -o $(PROGRAM) $^ $(LDLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	$(RM) $(PROGRAM) $(OBJS)
