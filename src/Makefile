TARGET=smalloc_test
CC=gcc
CCFLAGS=-c -std=c89 -Werror -Wall -DSMALLOC_DEBUG
LD=gcc
RM=rm -rf
OBJS=	main.o \
	smalloc.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(OBJS) -o $(TARGET) $(LDFLAGS)

main.o: main.c
	$(CC) $(CCFLAGS) main.c -o main.o

smalloc.o: smalloc.c smalloc.h
	$(CC) $(CCFLAGS) smalloc.c -o smalloc.o

clean:
	$(RM) $(OBJS)

distclean:
	$(RM) $(OBJS) $(TARGET)
