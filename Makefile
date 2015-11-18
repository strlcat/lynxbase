PREFIX = /usr
SRCS = $(wildcard *.c)
PROGS = $(SRCS:.c=)
override CFLAGS += -Wall -Os

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

install: $(PROGS)
	cp -ai $(PROGS) $(PREFIX)/bin

clean:
	rm -f $(PROGS)
