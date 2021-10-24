#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	int fd;
	int close_stdin = 0;

	if (argc < 2) return 0;

	if (!strcmp(argv[1], "-0")) {
		argc--;
		argv++;
		close_stdin = 1;
	}

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) return 1;
	if (close_stdin) {
		if (close(0) == -1) return 2;
		if (dup2(fd, 0) == -1) return 2;
	}
	if (close(1) == -1) return 2;
	if (dup2(fd, 1) == -1) return 2;
	if (close(2) == -1) return 2;
	if (dup2(fd, 2) == -1) return 2;
	if (close(fd) == -1) return 3;

	execvp(argv[1], argv+1);
	return 127;
}
