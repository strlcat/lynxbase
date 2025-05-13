#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void usage(void)
{
	printf("usage: uenv [-e VAR=NAME] [-u VAR] [-p VAR] cmdline ...\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	char *s, *home, *shell, *uid, *user, *term;
	char **senv, **sarg;
	size_t x, senvsz;

	if (argc < 2) usage();

	home = shell = uid = user = term = NULL;
	senv = sarg = NULL;
	senvsz = 0;

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

	opterr = 0;
	optind = 1;

	while ((c = getopt(argc, argv, "e:u:p:")) != -1) {
		switch (c) {
			case 'e':
				senv = realloc(senv, sizeof(char *) * (senvsz + 1));
				if (!senv) return errno;
				sarg = realloc(sarg, sizeof(char *) * (senvsz + 1));
				senv[senvsz] = strdup(optarg);
				if (!senv[senvsz]) return errno;
				sarg[senvsz] = NULL;
				senvsz++;
				break;
			case 'u': if (unsetenv(optarg) != 0) return errno; break;
			case 'p':
				s = getenv(optarg);
				if (s) {
					senv = realloc(senv, sizeof(char *) * (senvsz + 1));
					if (!senv) return errno;
					sarg = realloc(sarg, sizeof(char *) * (senvsz + 1));
					senv[senvsz] = strdup(optarg);
					if (!senv[senvsz]) return errno;
					sarg[senvsz] = strdup(s);
					if (!sarg[senvsz]) return errno;
					senvsz++;
				}
				break;
			default: usage(); break;
		}
	}

	if (!argv[optind]) usage();

	if (clearenv() != 0) return errno;

	if (setenv("HOME", home, 1) != 0) return errno;
	if (setenv("SHELL", shell, 1) != 0) return errno;
	if (setenv("UID", uid, 1) != 0) return errno;
	if (setenv("USER", user, 1) != 0) return errno;
	if (geteuid() == 0) {
		if (setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1) != 0) return errno;
	}
	else {
		if (setenv("PATH", "/bin:/usr/bin:/usr/local/bin", 1) != 0) return errno;
	}

	for (x = 0; x < senvsz; x++) {
		if (sarg[x]) {
			if (setenv(senv[x], sarg[x], 1) != 0) return errno;
		}
		else {
			if (putenv(senv[x]) != 0) return errno;
		}
	}

	execvp(argv[optind], &argv[optind]);
	return errno;
}
