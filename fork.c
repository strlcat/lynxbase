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
 * lhexecvp: return immediately after call.
 * The same as execvp().
 */
static int lhexecvp(const char *file, char *const argv[])
{
	int pfd[2];

	if (!*file) return -1;

	if (pipe(pfd) != 0) return -1;
	fcntl(pfd[0], F_SETFD, fcntl(pfd[0], F_GETFD) | FD_CLOEXEC);
	fcntl(pfd[1], F_SETFD, fcntl(pfd[1], F_GETFD) | FD_CLOEXEC);

	switch (fork()) {
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

		execvp(file, argv);

		write(pfd[1], &errno, sizeof(errno));
		close(pfd[1]);
		exit(127);

		break;
	default: {
		int x = 0;

		close(pfd[1]);
		while (read(pfd[0], &x, sizeof(errno)) != -1)
			if (errno != EAGAIN && errno != EINTR) break;
		close(pfd[0]);

		if (x) {
			errno = x;
			return -1;
		}
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
	if (argc < 2) exit(0);

	setenv("_", argv[1], 1);
	if (lhexecvp(argv[1], argv+1) != 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "%s: not found\n", argv[1]);
			exit(127);
		}
		else {
			fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
			exit(126);
		}
	}

	return 0;
}
