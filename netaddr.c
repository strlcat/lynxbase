/*
 * netaddr: checks that address is in subnet
 * both ipv4 and ipv6 addresses supported
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "xstrlcpy.c"

struct netaddr {
	int type;
	char addr[16];
	char saddr[INET6_ADDRSTRLEN];
	int pfx, pmax;
};

static void usage(void)
{
	printf("usage: netaddr [-v] SUBNET ADDR\n");
	printf("netaddr checks if ADDR is in SUBNET.\n");
	printf("  -v: also print result\n");
	printf("return codes are:\n");
	printf("  0: ADDR is in SUBNET\n");
	printf("  1: ADDR is NOT in SUBNET\n");
	printf("  2: no or bogus data given\n");
	printf("both IPv4 and IPv6 addresses are supported.\n");
	exit(1);
}

static int addrtype(const char *addr)
{
	if (strchr(addr, '.') && !strchr(addr, ':')) return AF_INET;
	else if (strchr(addr, ':') && !strchr(addr, '.')) return AF_INET6;
	return 0;
}

static int filladdr(const char *addr, struct netaddr *na)
{
	int type;
	char *s;

	type = addrtype(addr);
	if (!type) return 0;
	na->type = type;

	if (na->type == AF_INET) na->pmax = 32;
	else if (na->type == AF_INET6) na->pmax = 128;

	xstrlcpy(na->saddr, addr, INET6_ADDRSTRLEN);

	s = strchr(na->saddr, '/');
	if (s && *(s+1)) {
		*s = 0; s++;
		na->pfx = atoi(s);
		if (na->pfx < 0) return 0;
		else if (type == AF_INET && na->pfx > 32) return 0;
		else if (type == AF_INET6 && na->pfx > 128) return 0;
	}
	else {
		if (type == AF_INET) na->pfx = 32;
		else na->pfx = 128;
	}

	if (inet_pton(type, na->saddr, na->addr) < 1) return 0;

	return 1;
}

static int matchaddr(struct netaddr *n, struct netaddr *a)
{
	int x, y;

	if (n->type != a->type) return 2;

	if ((n->pmax - n->pfx) % 8) {
		for (x = 0; x < (n->pfx/8); x++)
			if (n->addr[x] != a->addr[x]) return 1;
		y = x;
		for (x = (n->pmax - n->pfx) % 8; x < 8; x++) {
			if ((n->addr[y] & (1 << x)) != (a->addr[y] & (1 << x))) return 1;
		}
	}
	else {
		for (x = 0; x < (n->pfx/8); x++)
			if (n->addr[x] != a->addr[x]) return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct netaddr net, addr;
	int c;
	int verbose = 0;

	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
			case 'v': verbose = 1; break;
			default: usage();
		}
	}
	if (!*(argv+optind) || !*(argv+optind+1)) usage();

	memset(&net, 0, sizeof(struct netaddr));
	memset(&addr, 0, sizeof(struct netaddr));

	if (!filladdr(*(argv+optind), &net)) goto _err;
	if (!filladdr(*(argv+optind+1), &addr)) goto _err;

	c = matchaddr(&net, &addr);
	if (verbose) {
		if (c == 2) goto _err;
		else {
			if (net.pfx == net.pmax) printf("%s %s %s\n", addr.saddr, (c == 1) ? "!=" : "==", net.saddr);
			else printf("%s %s %s/%u\n", addr.saddr, (c == 1) ? "!=" : "==", net.saddr, net.pfx);
		}
	}

	return c;

_err:
	if (verbose) fputs("Error\n", stderr);
	return 2;
}
