#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <libgen.h>

static char oomadjpath[PATH_MAX];

int main(int argc, char **argv)
{
	char *progname;
	pid_t mypid;
	uid_t myuid;
	gid_t mygid;
	int fd;

	progname = basename(argv[0]);

	if (argc < 2) {
		fprintf(stdout, "usage: %s cmdline\n", progname);
		fprintf(stdout, "this program requires setuid assistance to work properly.\n");
		return 1;
	}

	myuid = getuid();
	mygid = getgid();

	if (geteuid() != 0) {
		fprintf(stderr, "%s: this program requires setuid helper to be used effectively.\n", progname);
		return 1;
	}

	fd = open("/proc/self/oom_score_adj", O_RDWR);
	if (fd != -1) goto _writeexec;
	fd = open("/proc/self/oom_adj", O_RDWR);
	if (fd != -1) goto _writeexec;

	mypid = getpid();

	if (snprintf(oomadjpath, PATH_MAX, "/proc/%lu/oom_score_adj", (unsigned long)mypid) >= PATH_MAX) return 255;
	fd = open(oomadjpath, O_RDWR);
	if (fd != -1) goto _writeexec;
	if (snprintf(oomadjpath, PATH_MAX, "/proc/%lu/oom_adj", (unsigned long)mypid) >= PATH_MAX) return 255;
	fd = open(oomadjpath, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "%s: cannot open /proc/self/oom_adj nor /proc/self/oom_score_adj.\n", progname);
		fprintf(stderr, "%s: is /proc mounted here? Check that first.\n", progname);
		return 2;
	}

_writeexec:
	if (write(fd, "-1000\n", 6) == -1) { /* echo -1000 > /proc/self/oom_score_adj */
_wrerror:	fprintf(stderr, "%s: unable to write -1000 to oomadj file: %s\n", progname, strerror(errno));
		fprintf(stderr, "%s: do you have enough permissions to perform this?\n", progname);
		return 3;
	}

	if (close(fd) == -1) goto _wrerror;

	if (setregid(mygid, mygid) == -1) {
		fprintf(stderr, "%s: cannot drop privileges: %s\n", progname, strerror(errno));
		return 4;
	}

	if (setreuid(myuid, myuid) == -1) {
		fprintf(stderr, "%s: cannot drop privileges: %s\n", progname, strerror(errno));
		return 4;
	}

	return execvp(argv[1], argv+1);
}
