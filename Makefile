SRCS = $(wildcard *.c)
PROGS = $(SRCS:.c=)
override CFLAGS += -Wall -Os

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)
