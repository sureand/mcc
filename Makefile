CFLAGS=-Wall -Wno-strict-aliasing -std=gnu11 -g -I -O0
OBJS=error.o map.o lex.o buffer.o dict.o String.o File.o Node.o parse.o
LIBS=-lm

mcc: mcc.h main.o $(OBJS)
	cc -o $@ main.o $(OBJS) $(CFLAGS) $(LDFLAGS) $(LIBS)

$(OBJS) main.o: mcc.h

clean:
	rm *.o