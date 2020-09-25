CFLAGS=-Wall -Wno-strict-aliasing -std=gnu11 -g -I. -O0
OBJS=lex.o map.o buffer.o map.o dict.o String.o File.o Node.o parse.o

mcc: mcc.h main.o $(OBJS)
	cc -o $@ main.o $(OBJS) $(LDFLAGS)

$(OBJS) main.o: mcc.h

