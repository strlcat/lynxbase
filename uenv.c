#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>

static void usage(void)
{
	printf("usage: uenv [-e VAR=NAME] [-u VAR] cmdline ...\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	char *s, *home, *shell, *uid, *user, *term;

	if (argc < 2) usage();

	home = shell = uid = user = term = NULL;

	s = getenv("HOME");
	if (s) home = strdup(s);
	if (!home) home = "/";
	s = getenv("SHELL");
	if (s) shell = strdup(s);
	if (!shell) shell = "/bin/sh";
	s = getenv("UID");
	if (s) uid = strdup(s);
	if (!uid) uid = "0";
	s = getenv("USER");
	if (s) user = strdup(s);
	if (!user) user = "root";
	s = getenv("TERM");
	if (s) term = getenv("TERM");
	if (!term) term = "vt100";

	clearenv();

	setenv("HOME", home, 1);
	setenv("SHELL", shell, 1);
	setenv("UID", uid, 1);
	setenv("USER", user, 1);
	if (geteuid() == 0) setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
	else setenv("PATH", "/bin:/usr/bin:/usr/local/bin", 1);

	opterr = 0;
	optind = 1;

	while ((c = getopt(argc, argv, "e:u:")) != -1) {
		switch (c) {
			case 'e': putenv(optarg); break;
			case 'u': unsetenv(optarg); break;
			default: usage(); break;
		}
	}

	if (!argv[optind]) usage();

	execvp(argv[optind], &argv[optind]);
	return 127;
}
