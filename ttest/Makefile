CC = gcc
CFLAGS = -O0 -Wall
TARGET = ttest

INCLUDES = -I .
LIBS = -lpthread

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)
	@rm -f $^

%.o: %.c
	@$(CC) $(INCLUDES) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	@rm -rf $(OBJS) $(TARGET)
