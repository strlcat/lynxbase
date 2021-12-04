PREFIX = /usr
SRCS = $(filter-out xstrlcpy.c, $(wildcard *.c))
PROGS = $(SRCS:.c=)

ifneq (,$(DEBUG))
override CFLAGS+=-Wall -O0 -g
else
override CFLAGS+=-O2
endif

ifneq (,$(STATIC))
override LDFLAGS+=-static
endif

ifneq (,$(STRIP))
override LDFLAGS+=-s
endif

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(PROGS)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m0755 $^ $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f $(PROGS)
