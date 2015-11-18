#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>

static char *progname = NULL;
static int direrr = 0;

static void usage(void)
{
	fprintf(stdout, "usage: %s directory program [program args]\n", progname);
	fprintf(stdout, "Executes a program in directory with optional [program args]\n");
	exit(1);
}

static void xerror(const char *s)
{
	if (errno == ENOENT && !direrr) {
		fprintf(stderr, "%s: not found\n", s);
		exit(127);
	}
	fprintf(stderr, "%s: %s\n", s, strerror(errno));
	exit(1);
}


int main(int argc, char **argv)
{
	progname = basename(argv[0]);
	char *wdarg = argv[1], **remargs = argv+2;

	if (argc < 3) usage();

	if (chdir(wdarg) != 0) { direrr = 1; xerror(wdarg); }
	if (execvp(remargs[0], remargs)) xerror(remargs[0]);

	return 0;
}
