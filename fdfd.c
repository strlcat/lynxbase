/*
 * fdfd: relocate file descriptors
 * usage: fdfd oldfd,fd,fd... newfd,fd,fd... cmdline ...
 * clones comma separated sets of oldfds to newfds, renumbers them, closes oldfds
 * example: fdfd 3,4,5 10,11,12 true will renumber 3 4 5 opened fds to 10 11 12, and will
 * close old 3 4 5 fds, then it will execute "true".
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void usage(void)
{
	printf("usage: fdfd oldfd,fd,fd... newfd,fd,fd... cmdline ...\n");
	printf("fdfd renumbers sets of old fds to new fds, closing oldfds\n");
	exit(1);
}

static void xerror(const char *s)
{
	if (errno) perror(s);
	exit(2);
}

int main(int argc, char **argv)
{
	char *s, *ss, *d, *dd, *t, *tt;
	int a, b;

	if (argc < 4) usage();

	s = ss = *(argv+1);
	d = dd = *(argv+2);
	while ((s = strtok_r(ss, ",", &t)) && (d = strtok_r(dd, ",", &tt))) {
		if (ss) ss = NULL; if (dd) dd = NULL;
		a = atoi(s);
		b = atoi(d);
		if (dup2(a, b) == -1) xerror("dup2");
		close(a);
	}

	argv += 3;
	execvp(*argv, argv);

	return 127;
}
