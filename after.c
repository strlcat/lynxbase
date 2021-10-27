#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "xstrlcpy.c"

typedef unsigned long long af_nsecs;

#define DTONSECS(x) ((af_nsecs)((x) * 1000000000.0))
#define UTONSECS(x) ((af_nsecs)((x) * 1000000000))

static af_nsecs after_that_time;
static struct timespec after_that_time_tsp;

static void usage(void)
{
	printf("usage: after [-fv] timespec cmdline ...\n");
	printf("Wait for a timespec time then run cmdline.\n\n");
	printf("  -f: do not go to background.\n");
	printf("  -v: if going to background, tell the pid of waiter.\n\n");
	exit(1);
}

static void to_timespec(af_nsecs nsecs, struct timespec *tsp)
{
	memset(tsp, 0, sizeof(struct timespec));
	tsp->tv_sec = nsecs / 1000000000;
	tsp->tv_nsec = nsecs % 1000000000;
}

static int str_empty(const char *str)
{
	if (!*str) return 1;
	return 0;
}


static int isnum(const char *s)
{
	char *p;
	if (!s || str_empty(s)) return 0;
	strtoul(s, &p, 10);
	return str_empty(p) ? 1 : 0;
}

static af_nsecs nanotime_prefixed(const char *s, char **stoi)
{
	char pfx[2] = {0};
	char N[32];
	size_t l;
	int frac = 0;
	af_nsecs ret;

	if (!s) return 0;

	xstrlcpy(N, s, sizeof(N));
	l = strnlen(N, sizeof(N));
_again:	*pfx = *(N+l-1);
	if (!isnum(pfx)) *(N+l-1) = 0;
	if (*pfx == 's') {
		*pfx = 0;
		l--;
		frac = 1;
		goto _again;
	}

	*stoi = NULL;
	if (*pfx == 'm' && frac) ret = UTONSECS(strtoull(N, stoi, 10)) / 1000;
	else if (*pfx == 'u' && frac) ret = UTONSECS(strtoull(N, stoi, 10)) / 1000000;
	else if (*pfx == 'n' && frac) ret = UTONSECS(strtoull(N, stoi, 10)) / 1000000000;
	else if (*pfx == 'm' && !frac) ret = DTONSECS(strtod(N, stoi)) * 60;
	else if (*pfx == 'h' && !frac) ret = DTONSECS(strtod(N, stoi)) * 3600;
	else if (*pfx == 'd' && !frac) ret = DTONSECS(strtod(N, stoi)) * 86400;
	else ret = DTONSECS(strtod(N, stoi));

	return ret;
}

static void do_daemonise(int sayit)
{
	pid_t pid, sid;
	int i;

	pid = fork();
	if (pid < 0)
		exit(-1);
	if (pid > 0) {
		if (sayit) printf("%ld\n", (long)pid);
		exit(0);
	}

	sid = setsid();
	if (sid < 0)
		exit(-1);

	close(0);
	close(1);
	close(2);
	for (i = 0; i < 3; i++)
		open("/dev/null", O_RDWR);
}

int main(int argc, char **argv)
{
	int tellpid = 0, no_daemon = 0;
	char *stoi;
	int c;

	while ((c = getopt(argc, argv, "fv")) != -1) {
		switch (c) {
			case 'f': no_daemon = 1; break;
			case 'v': tellpid = 1; break;
			default: usage(); break;
		}
	}

	if (!argv[optind]) usage();

	after_that_time = nanotime_prefixed(argv[optind], &stoi);
	if (!str_empty(stoi)) usage();
	to_timespec(after_that_time, &after_that_time_tsp);

	argc -= optind+1;
	argv += optind+1;
	if (argc < 1) usage();

	if (!no_daemon) do_daemonise(tellpid);
	nanosleep(&after_that_time_tsp, NULL);

	execvp(*argv, argv);
	fprintf(stderr, "%s: not found\n", *argv);

	return 127;
}
