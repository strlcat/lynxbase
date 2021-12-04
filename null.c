#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

enum { DO_CLOSE_STDIN = 1, DO_CLOSE_STDOUT = 2, DO_CLOSE_STDERR = 4, DID_RESET = 8 };

int main(int argc, char **argv)
{
	char *targv[] = {argv[0], argv[1], NULL, NULL};
	int fd, c;
	int clflags = DO_CLOSE_STDOUT | DO_CLOSE_STDERR;

	if (argc < 2) return 0;

	opterr = 0;
	optind = 1;
	if (argv[2]) targv[2] = argv[2]; /* accept possible "--" */
	while ((c = getopt(targv[2] ? 3 : 2, targv, "012")) != -1) {
		switch (c) {
			case '0': clflags |= DO_CLOSE_STDIN; break;
			case '1':
				if (!(clflags & DID_RESET)) {
					clflags = (clflags & DO_CLOSE_STDIN) ? DO_CLOSE_STDIN : 0;
					clflags |= DID_RESET;
				}
				clflags |= DO_CLOSE_STDOUT;
			break;
			case '2':
				if (!(clflags & DID_RESET)) {
					clflags = (clflags & DO_CLOSE_STDIN) ? DO_CLOSE_STDIN : 0;
					clflags |= DID_RESET;
				}
				clflags |= DO_CLOSE_STDERR;
			break;
			default: exit(1); break;
		}
	}

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) return 1;
	if (clflags & DO_CLOSE_STDIN) {
		if (close(0) == -1) return 2;
		if (dup2(fd, 0) == -1) return 2;
	}
	if (clflags & DO_CLOSE_STDOUT) {
		if (close(1) == -1) return 2;
		if (dup2(fd, 1) == -1) return 2;
	}
	if (clflags & DO_CLOSE_STDERR) {
		if (close(2) == -1) return 2;
		if (dup2(fd, 2) == -1) return 2;
	}
	if (close(fd) == -1) return 3;

	if (argv[optind]) execvp(argv[optind], argv+optind);
	return 127;
}
