/*
 * subnet: prints exact subnet for address/prefix pair.
 * example: 127.255.255.255/21 gives 127.255.248.0
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
	printf("usage: subnet ADDR/PFX\n");
	printf("subnet: prints exact subnet for mixed ADDR/PFX\n");
	printf("for example: 127.255.255.255/21 will print 127.255.248.0\n");
	printf("return codes are:\n");
	printf("  0: success\n");
	printf("  2: the conversion failed due to invalid ADDR/PFX\n");
	exit(1);
}

static int addr_type(const char *addr)
{
	if (strchr(addr, '.') && !strchr(addr, ':')) return AF_INET;
	else if (strchr(addr, ':') && !strchr(addr, '.')) return AF_INET6;
	return 0;
}

static int parse_addr(const char *addr, struct netaddr *na)
{
	int type;
	char *s;

	memset(na, 0, sizeof(struct netaddr));

	type = addr_type(addr);
	if (!type) return 0;
	na->type = type;

	if (na->type == AF_INET) na->pmax = 32;
	else if (na->type == AF_INET6) na->pmax = 128;

	xstrlcpy(na->saddr, addr, INET6_ADDRSTRLEN);

	s = strchr(na->saddr, '/');
	if (s && *(s+1)) {
		*s = 0; s++;
		na->pfx = atoi(s);
		if (type == AF_INET && na->pfx > 32) return 0;
		else if (type == AF_INET6 && na->pfx > 128) return 0;
	}
	else {
		if (type == AF_INET) na->pfx = 32;
		else na->pfx = 128;
	}

	if (inet_pton(type, na->saddr, na->addr) < 1) return 0;

	return 1;
}

static char *subnet_to_addr(struct netaddr *nsub)
{
	int x, y;

	if ((nsub->pmax - nsub->pfx) % 8) {
		for (x = (nsub->pfx/8) + 1; x < (nsub->pmax/8); x++)
			nsub->addr[x] = 0;
		y = (nsub->pfx/8);
		for (x = (8-((nsub->pmax - nsub->pfx) % 8)); x < 8; x++)
			nsub->addr[y] &= ~(1 << (7-x));
	}
	else {
		for (x = (nsub->pfx/8); x < (nsub->pmax/8); x++)
			nsub->addr[x] = 0;
	}

	return (char *)inet_ntop(nsub->type, nsub->addr, nsub->saddr, INET6_ADDRSTRLEN);
}

int main(int argc, char **argv)
{
	struct netaddr addr;

	if (!*(argv+optind)) usage();

	if (!parse_addr(*(argv+optind), &addr)) return 2;

	printf("%s\n", subnet_to_addr(&addr));
	return 0;
}
