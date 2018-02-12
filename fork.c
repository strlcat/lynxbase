/*
 *	fork(1) or lh(1) - program daemonizer
 *
 *	Usage: fork program [options]
 *
 *	This program always returns zero and returns to shell
 *	immediately if program executed successfully.
 *	Returns 127 or 126 if ENOENT (127), or other error (126) occured.
 */

#define _POSIX_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/*
 * dexecvp: return immediately after call.
 * The same as execvp().
 */
static int dexecvp(const char *file, char *const argv[], pid_t *pid)
{
	pid_t idp;
	int pfd[2];
	int x;

	if (!file || !*file) return -1;

	if (pipe(pfd) != 0) return -1;
	fcntl(pfd[0], F_SETFD, fcntl(pfd[0], F_GETFD) | FD_CLOEXEC);
	fcntl(pfd[1], F_SETFD, fcntl(pfd[1], F_GETFD) | FD_CLOEXEC);

	idp = fork();
	switch (idp) {
	case -1:
		goto _fail;
		break;
	case 0:
		if (setsid() < 0) goto _fail;
		close(0);
		close(1);
		close(2);
		open("/dev/null", O_RDWR);
		open("/dev/null", O_RDWR);
		open("/dev/null", O_RDWR);
		close(pfd[0]);
		if (execvp(file, argv) == -1)
			write(pfd[1], &errno, sizeof(errno));
		close(pfd[1]);
		exit(127);

		break;
	default:
		x = 0;
		if (pid) *pid = idp;
		close(pfd[1]);
		while (read(pfd[0], &x, sizeof(errno)) != -1)
			if (errno != EAGAIN && errno != EINTR) break;
		close(pfd[0]);
		if (x) {
			errno = x;
			return -1;
		}
		break;
	}

	return 0;

_fail:
	close(pfd[0]);
	close(pfd[1]);
	return -1;
}


int main(int argc, char **argv)
{
	pid_t runner;
	int tellpid = 0;

	if (argc < 2) exit(0);

	if (!strcmp(*(argv+1), "-v")) {
		argc--;
		argv++;
		tellpid = 1;
	}

	setenv("_", argv[1], 1);
	if (dexecvp(argv[1], argv+1, tellpid ? &runner : NULL) != 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "%s: not found\n", argv[1]);
			exit(127);
		}
		else {
			fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
			exit(126);
		}
	}

	if (tellpid) printf("%ld\n", (long)runner);

	return 0;
}
