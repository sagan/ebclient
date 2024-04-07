TARGET = ebclient
SRC = $(wildcard *.c *.cpp)
OBJS = $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SRC)))
CC = gcc
CPP = g++
INCLUDE =  -I. 
LINKPARAM = -static -pthread -L. /usr/local/lib/libebu.a /usr/local/lib/libmxml.a /usr/lib/x86_64-linux-gnu/libz.a
#LINKPARAM =  -pthread -L. -lebu -lmxml
CFLAGS = -fPIC

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.cpp
	$(CPP) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(TARGET):$(OBJS)
	libtool --mode=link $(CPP) -o $(TARGET) $(LINKPARAM) $(OBJS)

clean:
	rm $(OBJS) $(TARGET)
