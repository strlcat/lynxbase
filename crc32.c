#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>

#define NOSIZE ((size_t)-1)
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))
#define ATOX(s) ((uint32_t)strtoul(s, NULL, 16))

static uint32_t crc32_reverse(uint32_t x)
{
	x = ((x & 0x55555555) << 1) | ((x >> 1) & 0x55555555);
	x = ((x & 0x33333333) << 2) | ((x >> 2) & 0x33333333);
	x = ((x & 0x0F0F0F0F) << 4) | ((x >> 4) & 0x0F0F0F0F);
	x = (x << 24) | ((x & 0xFF00) << 8) | ((x >> 8) & 0xFF00) | (x >> 24);
	return x;
}

static uint32_t crc32(uint32_t iv, uint32_t sv, const void *data, size_t n)
{
	const unsigned char *ptr;
	unsigned x;
	uint32_t byte, crc;

	crc = iv;
	ptr = data;
	while (n--) {
		byte = *ptr++;
		byte = crc32_reverse(byte);
		for (x = 0; x < 8; x++, byte <<= 1) crc = ((crc ^ byte) & 0x80000000U) ? (crc << 1) ^ sv : (crc << 1);
	}

	return crc;
}

static uint32_t crc32_final(uint32_t iv)
{
	return crc32_reverse(iv ^ ~0U);
}

static unsigned char xblk[65536], *pblk;

static void usage(void)
{
	printf("usage: crc32 [-I iv] [-S poly] [-/FILE] [...]\n");
	printf("Compute CRC32 of file(s).\n");
	printf("\n");
	printf("  -: read standard input instead of file.\n");
	printf("  -I iv: set IV to hexstr.\n");
	printf("  -S poly: set polynomial.\n");
	printf("\n");
	exit(1);
}

#define CRC32_POLY_DEFAULT 0x04c11db7U
static uint32_t SV = CRC32_POLY_DEFAULT;
static uint32_t IV = ~0U;

int main(int argc, char **argv)
{
	int fd, x, do_stop, c, v;
	size_t ldone, lrem, lio;
	char *fname;
	uint32_t crc;

	opterr = 0;
	while ((c = getopt(argc, argv, "I:S:")) != -1) {
		switch (c) {
			case 'I': IV = ATOX(optarg); break;
			case 'S': SV = ATOX(optarg); break;
			default: usage(); break;
		}
	}

	for (x = optind; argv[x] && x < argc; x++);
	v = (x-optind > 1) ? 1 : 0;

	if (!argv[optind]) {
		fd = 0;
		goto _stdin;
	}

	for (x = optind; argv[x] && x < argc; x++) {
		if (!strcmp(argv[x], "-")) fd = 0;
		else fd = open(argv[x], O_RDONLY);
		if (fd == -1) return 1;

_stdin:		crc = IV;
		fname = (fd != 0) ? argv[x] : "stdin";

		do_stop = 0;
		while (1) {
			if (do_stop) break;
			pblk = xblk;
			lrem = sizeof(xblk);
			ldone = 0;
_ragain:		lio = read(fd, pblk, lrem);
			if (lio == 0) do_stop = 1;
			if (lio != NOSIZE) ldone += lio;
			else return 2;
			if (lio && lio < lrem) {
				pblk += lio;
				lrem -= lio;
				goto _ragain;
			}
			crc = crc32(crc, SV, xblk, ldone);
		}

		if (fd > 2) close(fd);
		if (v) printf("%s:\t%08x\n", fname, crc32_final(crc));
		else printf("%08x\n", crc32_final(crc));
	}

	return 0;
}
