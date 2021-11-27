#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "xstrlcpy.c"

#define TRIES_INFINITE ((size_t)-1)

typedef unsigned long long rsp_nsecs;

#define DTONSECS(x) ((rsp_nsecs)((x) * 1000000000.0))
#define UTONSECS(x) ((rsp_nsecs)((x) * 1000000000UL))

static char *tty_path;
static rsp_nsecs respawn_time = DTONSECS(1.0);
static struct timespec respawn_time_tsp;
static size_t respawn_tries = TRIES_INFINITE;

static void usage(void)
{
	printf("usage: respawn [-T TTY] [-t timespec] [-n tries] [-e [!]exitcode] [-fv[v]] cmdline ...\n");
	printf("Run cmdline and watch it. When process exits, restart it.\n\n");
	printf("  -T TTY: open this TTY as process' TTY.\n");
	printf("  -t timespec: set sleeping time between respawn attempts (default: 1s).\n");
	printf("  -n tries: set amount of respawn attempts (default is infinite).\n");
	printf("  -e [!]exitcode: stop respawning once this exitcode was received from target.\n");
	printf("    '!' specified in front of exitcode will negate the match.\n");
	printf("  -f: do not daemonise (always stay in foreground).\n");
	printf("  -v: if going to background, tell the pid of waiter.\n");
	printf("  -vv: also tell pid of each process going to be started.\n\n");
	exit(1);
}

static void to_timespec(rsp_nsecs nsecs, struct timespec *tsp)
{
	memset(tsp, 0, sizeof(struct timespec));
	tsp->tv_sec = nsecs / 1000000000UL;
	tsp->tv_nsec = nsecs % 1000000000UL;
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

static rsp_nsecs nanotime_prefixed(const char *s, char **stoi)
{
	char pfx[2] = {0};
	char N[32];
	size_t l;
	int frac = 0;
	rsp_nsecs ret;

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
	int fd;

	pid = fork();
	if (pid < 0) exit(2);
	if (pid > 0) {
		if (sayit) printf("%ld\n", (long)pid);
		exit(0);
	}

	sid = setsid();
	if (sid < 0) exit(2);

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) exit(2);
	close(0);
	if (dup2(fd, 0) == -1) exit(2);
	close(1);
	if (dup2(fd, 1) == -1) exit(2);
	close(2);
	if (dup2(fd, 2) == -1) exit(2);
	if (fd > 2) close(fd);
}

typedef void (*sighandler_t)(int);

static void signal_handler(int sig)
{
	switch (sig) {
		case SIGTERM:
		case SIGINT: exit(0); break;
		case SIGCHLD: while (waitpid(0, NULL, WNOHANG) >= 0); break;
	}
}

int main(int argc, char **argv)
{
	sigset_t set;
	pid_t x, y;
	int fd, c, tellpid = 0, no_daemon = 0, do_exitcode_check = 0, exitcode_good = 0, exitcode_not = 0;
	char *stoi;

	opterr = 0;
	while ((c = getopt(argc, argv, "T:t:n:fve:")) != -1) {
		switch (c) {
			case 'T': tty_path = optarg; break;
			case 't':
				respawn_time = nanotime_prefixed(optarg, &stoi);
				if (!str_empty(stoi)) usage();
				break;
			case 'n': respawn_tries = (size_t)atol(optarg); break;
			case 'f': no_daemon = 1; tty_path = ttyname(0); break;
			case 'v': tellpid++; break;
			case 'e':
				do_exitcode_check = 1;
				if (optarg[0] == '!') {
					exitcode_not = 1;
					optarg++;
				}
				exitcode_good = atoi(optarg);
				break;
			default: usage(); break;
		}
	}

	if (!argv[optind]) usage();

	to_timespec(respawn_time, &respawn_time_tsp);

	if (!no_daemon) do_daemonise(tellpid);

	sigfillset(&set);
	sigdelset(&set, SIGCHLD);
	sigdelset(&set, SIGTERM);
	sigdelset(&set, SIGINT);
	sigprocmask(SIG_BLOCK, &set, NULL);
	signal(SIGCHLD, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	if (tty_path) {
		if ((fd = open(tty_path ? tty_path : "/dev/null", O_RDWR | O_NOCTTY)) != -1) {
			close(0);
			if (dup2(fd, 0) == -1) return 2;
			close(1);
			if (dup2(fd, 1) == -1) return 2;
			close(2);
			if (dup2(fd, 2) == -1) return 2;
			if (fd > 2) close(fd);
		}
		else return 2;
	}

	while (1) {
		int ret = 0;

		if ((x = fork())) {
			if (tellpid > 1) no_daemon ? printf("%ld\n", (long)x) : fprintf(stderr, "%ld\n", (long)x);
			while (1) {
				y = waitpid(x, do_exitcode_check ? &ret : NULL, 0);
				if (y == x) break;
				if (y == -1 && errno != EINTR) break;
			}
			if (do_exitcode_check) {
				if (WIFEXITED(ret)
				&& exitcode_not ? (WEXITSTATUS(ret) != exitcode_good) : (WEXITSTATUS(ret) == exitcode_good)) break;
			}
		}
		else {
			if (setsid() == -1) return 2;

			sigfillset(&set);
			sigprocmask(SIG_UNBLOCK, &set, NULL);

			argc -= optind;
			argv += optind;
			execvp(*argv, argv);
			return 127;
		}

		if (respawn_tries && respawn_tries != TRIES_INFINITE) respawn_tries--;
		if (respawn_tries == 0) break;
		nanosleep(&respawn_time_tsp, NULL);
	}

	return 0;
}
