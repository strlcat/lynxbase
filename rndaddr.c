/*
 *	Random ipv4/ipv6/MAC address generator, version 1
 *
 *	Public domain: whole code or it's snippets can be taken from there without any permission.
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "xstrlcpy.c"

#define MAC_ADDRSTRLEN	18

#define ADDR_INVAL	0
#define ADDR_IPV6	1
#define ADDR_IPV4	2
#define ADDR_MAC 	3

static char *progname = NULL;

static void usage(void)
{
	fprintf(stdout, "usage: %s [-46m] [-u] [address]\n", progname);
	fprintf(stdout, "Random ipv4/ipv6/MAC address generator\n\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -4: generate ipv4 address\n");
	fprintf(stdout, "  -6: generate ipv6 address\n");
	fprintf(stdout, "  -m: generate MAC address\n");
	fprintf(stdout, " IPv6 options:\n");
	fprintf(stdout, "  -u: embed eui64 (MAC in ipv6) into resulting ipv6 address\n\n");
	fprintf(stdout, "Format of input address:\n");
	fprintf(stdout, "  ipv4: x.x.x.x/y, where x = [0-255] and y = [0-32]\n");
	fprintf(stdout, "  ipv6: x:x:x:x:x:x:x:x/y, where x = [0-ffff] and y = [0-128]\n");
	fprintf(stdout, "  ipv6: or in short form: x:x:x::/y, '::' "
			"here replaces remaining zeroes\n");
	fprintf(stdout, "   MAC: x:x:x:x:x:x.y, where x = [0-ff] and y = [0-48]\n\n");
	fprintf(stdout, "If no address given, then random address will be generated\n\n");
	fprintf(stdout, "Examples:\n");
	fprintf(stdout, "  %s 2000::/3: generate address within range from 2000::\n", progname);
	fprintf(stdout, "  \tto 3fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff\n");
	fprintf(stdout, "  %s 2001:db8:6fe3:8001::/64: generate random address for"
			" assigning to interface\n", progname);
	fprintf(stdout, "  %s ::/0: generate random ipv6 address\n", progname);
	fprintf(stdout, "  %s fd00::/8: generate address from private range\n", progname);
	fprintf(stdout, "  echo \"$(%s 2001:db8::/32 | cut -d':' -f-4)\"'::/64':\n", progname);
	fprintf(stdout, "  \tshell snippet to generate new /64 subnet\n");
	fprintf(stdout, "  %s 10.2.0.0/16: generate random address for 10.2.0.0/16 "
			"subnet\n", progname);
	fprintf(stdout, "  %s 0.0.0.0/0: generate random ipv4 address\n", progname);
	fprintf(stdout, "  %s 04:00:00:00:00:00.8: generate random even MAC address\n\n", progname);
	exit(1);
}

static unsigned int randrange(unsigned int s, unsigned int d)
{
	unsigned int c = 0;
	int f = -1;
	struct timeval t; memset(&t, 0, sizeof t);

	if (d < s) return s;

	f = open("/dev/urandom", O_RDONLY);
	if (f == -1) f = open("/dev/random", O_RDONLY);
	if (f == -1) goto _rand;
	read(f, &c, sizeof(unsigned int));
	close(f);
	goto _norand;

_rand:
	gettimeofday(&t, NULL);
	c ^= (((t.tv_sec * t.tv_usec) * t.tv_sec) << 15) ^ t.tv_usec;
	srandom(c);
	c = random();

_norand:
	c = c % (d - s + 1 == 0 ? d - s : d - s + 1) + s;

	return c;
}

static int getrandc(void)
{
	return randrange(0, 0xff);
}

static char *genrndipv6(const char *addr)
{
	unsigned char addr6[16] = {0}; int prefix = 0; unsigned char c = 0;
	char tmpaddr[INET6_ADDRSTRLEN] = {0};
	int i;
	char *s = NULL; const char *d = NULL;
	static char ret[INET6_ADDRSTRLEN] = {0};

	s = strchr(addr, '/');
	if (s && s[1]) s++;
	else return "\0IPv6 address contains no prefix";

	prefix = atoi(s);
	if (prefix < 0 || prefix > 128) return "\0Invalid IPv6 prefix";

	d = addr;
	xstrlcpy(tmpaddr, d, s - d);
	if (inet_pton(AF_INET6, tmpaddr, addr6) != 1) return "\0Invalid IPv6 address";

	if ((128 - prefix) % 8) {
		for (i = (prefix/8) + 1; i < 16; i++) addr6[i] = getrandc();
		c = getrandc();
		for (i = 0; i < (128 - prefix) % 8; i++) {
			if (c & (1 << i))
				addr6[prefix/8] |= (1 << i);
			else
				addr6[prefix/8] &= ~(1 << i);
		}
	}
	else
		for (i = (prefix/8); i < 16; i++) addr6[i] = getrandc();

	if (inet_ntop(AF_INET6, addr6, ret, INET6_ADDRSTRLEN) == NULL)
		return "\0IPv6 conversion failed";

	return ret;
}

static char *genrndipv4(const char *addr)
{
	unsigned char addr4[4] = {0}; int prefix = 0; unsigned char c = 0;
	char tmpaddr[INET_ADDRSTRLEN] = {0};
	int i;
	char *s = NULL; const char *d = NULL;
	static char ret[INET_ADDRSTRLEN] = {0};

	s = strchr(addr, '/');
	if (s && s[1]) s++;
	else return "\0IPv4 address contains no prefix";

	prefix = atoi(s);
	if (prefix < 0 || prefix > 32) return "\0Invalid IPv4 prefix";

	d = addr;
	xstrlcpy(tmpaddr, d, s - d);
	if (inet_pton(AF_INET, tmpaddr, addr4) != 1) return "\0Invalid IPv4 address";

	if ((32 - prefix) % 8) {
		for (i = (prefix/8) + 1; i < 4; i++) addr4[i] = getrandc();
		c = getrandc();
		for (i = 0; i < (32 - prefix) % 8; i++) {
			if (c & (1 << i))
				addr4[prefix/8] |= (1 << i);
			else
				addr4[prefix/8] &= ~(1 << i);
		}
	}
	else
		for (i = (prefix/8); i < 4; i++) addr4[i] = getrandc();

	if (inet_ntop(AF_INET, addr4, ret, INET_ADDRSTRLEN) == NULL)
		return "\0IPv4 conversion failed";

	return ret;
}

static char *genrndmac(const char *addr)
{
	unsigned char mac[6] = {0}; int prefix = 0; unsigned char c = 0;
	char tmpaddr[MAC_ADDRSTRLEN] = {0};
	char *s = NULL; const char *d = NULL;
	unsigned i;
	static char ret[MAC_ADDRSTRLEN] = {0};

	s = strchr(addr, '.');
	if (s && s[1]) s++;
	else return "\0MAC address contains no prefix";

	prefix = atoi(s);
	if (prefix < 0 || prefix > 48) return "\0Invalid MAC address prefix";

	d = addr;
	xstrlcpy(tmpaddr, d, s - d);

	if (sscanf(tmpaddr, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx%u",
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5], &i) != 6)
		return "\0Invalid MAC address";

	if ((48 - prefix) % 8) {
		for (i = (prefix/8) + 1; i < 6; i++) mac[i] = getrandc();
		c = getrandc();
		for (i = 0; i < (48 - prefix) % 8; i++) {
			if (c & (1 << i))
				mac[prefix/8] |= (1 << i);
			else
				mac[prefix/8] &= ~(1 << i);
		}
	}
	else
		for (i = (prefix/8); i < 6; i++) mac[i] = getrandc();

	if (prefix < 8) {
		if (mac[0] & (1 << 0))
			mac[0] ^= 1 << 0;
		if (mac[0] & (1 << 1))
			mac[0] ^= 1 << 1;
	}

	snprintf(ret, MAC_ADDRSTRLEN, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	return ret;
}

static char *eui64addr(const char *addr)
{
	unsigned char addr6[16] = {0};
	static char ret[INET6_ADDRSTRLEN] = {0};

	if (inet_pton(AF_INET6, addr, addr6) != 1) return "\0Invalid IPv6 address";

	addr6[11] = 0xff;
	addr6[12] = 0xfe;
	if (addr6[8] & (1 << 0))
		addr6[8] ^= 1 << 0;

	if (inet_ntop(AF_INET6, addr6, ret, INET6_ADDRSTRLEN) == NULL)
		return "\0IPv6 conversion failed";

	return ret;
}

static int getaddrprefix(const char *addr, int type)
{
	int prefix = 0;
	char *s = NULL;

	s = strchr(addr, '/');
	if (s && s[1]) s++;
	else goto _fail;

	prefix = atoi(s);
	if (prefix < 0 || prefix > (type == ADDR_IPV6 ? 128 : 32)) goto _fail;

	return prefix;

_fail:
	return -1;
}

static int whataddr(const char *addr)
{
	if (strchr(addr, ':') && strchr(addr, '/')) // 2001:db8::/32
		return ADDR_IPV6;
	else if (strchr(addr, '.') && strchr(addr, '/')) // 192.168.0.1/30
		return ADDR_IPV4;
	else if (strchr(addr, ':') && strchr(addr, '.')) // 01:23:45:67:89:ab.24
		return ADDR_MAC;
	else
		return ADDR_INVAL;
}

static int validaddr(const char *addr)
{
	return !!(addr && *addr);
}


int main(int argc, char **argv)
{
	progname = basename(argv[0]);
	const char *addr = NULL;
	char *t = NULL; int type = 0; int eui64 = 0;

	const char *nulladdrs[4] = {0};
	nulladdrs[ADDR_INVAL] = NULL;
	nulladdrs[ADDR_IPV6] = "::/0";
	nulladdrs[ADDR_IPV4] = "0.0.0.0/0";
	nulladdrs[ADDR_MAC] = "0:0:0:0:0:0.0";

	int c = 0;
	int idx = 0;
	opterr = 0;

	while ((c = getopt(argc, argv, "46muM:")) != -1) {
		switch (c) {
			case '6': type = ADDR_IPV6; break;
			case '4': type = ADDR_IPV4; break;
			case 'm': type = ADDR_MAC; break;
			case 'u': eui64 = 1; break;
			case 'M': type = ADDR_MAC; addr = optarg; break;
			default: usage(); break;
		}
	}

	idx = optind;

	if (argv[idx]) {
		addr = argv[idx];
		idx++;
	}
	else if (type && !addr) addr = nulladdrs[type];
	else if (!type && !addr) usage();

	if (!type) type = whataddr(addr);

	switch (type) {
		case ADDR_IPV6:
			t = genrndipv6(addr);
			if (validaddr(t) && eui64 && getaddrprefix(addr, ADDR_IPV6) <= 88)
				t = eui64addr(t);
			break;
		case ADDR_IPV4: t = genrndipv4(addr); break;
		case ADDR_MAC:
			t = genrndmac(addr);
			break;
		case ADDR_INVAL: default: usage(); break;
	}

	if (validaddr(t)) fprintf(stdout, "%s\n", t);
	else {
		t++; fprintf(stderr, "%s: %s\n", progname, t);
		return 1;
	}

	return 0;
}
