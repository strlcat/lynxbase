PREFIX = /usr
SRCS = $(filter-out xstrlcpy.c, $(wildcard *.c))
PROGS = $(SRCS:.c=)
override CFLAGS += -Wall -Os

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(PROGS)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m0755 $^ $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f $(PROGS)
