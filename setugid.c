/*
 *	setugid: program to execute processes under different privileges
 *
 *	*****  BEWARE: This program does NOT do any AUTHENTICATION!  *****
 *	***** DO NOT INSTALL SETUID BIT ON IT IF NOT WELL PROTECTED! *****
 *	*****			USE SUDO INSTEAD!		     *****
 *
 *	Recommended to link statically.
 *
 *	History:
 *	- version 0: (not this) just a quick sketch wrapper to run prog without sudo, hence without writing in auth.log
 *	- version 1: only -uUgGsS flags to set ugid/groups and execve() given prog.
 *	- version 2: clear and set environment variables
 *	- version 3: set prog's argv0 (like bash's exec -a) and place dash in argv0 (bash's exec -l)
 *	- version 4: big "setup login shell" stuff (setup argv0, re-set envvars, act like "sudo -iu user")
 *	- version 5: case keepenv bug fix, init user-related envvars and move away them from login stuff
 *	- version 6: added 'D' option for cd'ing into user's pw_dir directory
 *	- version 7: cleanups, adding -I option and releasing as public domain
 *
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>

#include "xstrlcpy.c"

extern char **environ;

#define NGIDS 1024
/* "NO" is for nobody */
#define NOUID ((uid_t)-1)
#define NOGID ((gid_t)-1)

#define _s(x) (sizeof(x)/sizeof(*x))

static char *progname;
static char *errstr;
static int warn = 0;
static char defsh[] = "/bin/sh";
static char root[] = "/";
static char zerouid[] = "0";

static void usage(void)
{
	printf("usage: %s [-uUgGsSaAeEiIPDv] PROG [ARGS]\n\n", progname);
	printf("  -u uid/user: setuid\n");
	printf("  -U euid/user: seteuid\n");
	printf("  -g gid/group: setgid\n");
	printf("  -G egid/group: setegid\n");
	printf("  -s gid,group,gid,... : setgroups explicit\n");
	printf("  -S gid,-group,-gid,... : setgroups adding/removing (-)\n\n");
	printf("  -a altargv0: set another program display name (argv0)\n");
	printf("  -A: place a dash before program display name\n");
	printf("  -e VAR=VAL: set environment variable\n");
	printf("  -E: start with empty environment\n");
	printf("  -i: start a login shell (implies -AE, also sets basic environment, PROG not needed)\n");
	printf("  -I: like -i, but always use %s as login shell\n", defsh);
	printf("  -P: preserve most of current environment (usable only with -i)\n");
	printf("  -D: chdir to user's pw_dir directory specified in passwd file\n");
	printf("  -v: report any failures (fatal ones still reported and program terminates)\n\n");
	printf(" By default, if user specified, group id and group list are set to target user is in.\n");
	printf(" ALWAYS check target permissions with id(1) program!\n\n");
	exit(1);
}

static void lperror(const char *s)
{
	if (!warn) return;
	perror(s);
}

static void serror(const char *s)
{
	if (!warn) return;
	if (errstr) fprintf(stderr, "%s: %s\n", s, errstr);
}

static void xerror(const char *s)
{
	if (errstr) warn = 1;
	serror(s);
	if (errno) perror(s);
	exit(2);
}

static void reseterr(void)
{
	errstr = NULL;
	errno = 0;
}

/* Since we are exec{l,v}e(), we don't need to free() */
static void *xmalloc(size_t n)
{
	void *r = malloc(n);
	if (!r) {
		errstr = NULL;
		xerror("malloc");
		return NULL; /* gcc bark-bark-bark! */
	}
	else return r;
}

static int isnum(const char *s)
{
	char *p;
	if (!s || *s == 0) return 0;
	strtol(s, &p, 10);
	return (*p == 0);
}

/* Work with passwd */
static uid_t uidbyname(const char *name)
{
	struct passwd *p;

	reseterr();
	if (isnum(name))
		return (uid_t)atoi(name);
	p = getpwnam(name);
	if (p) return p->pw_uid;
	else {
		errstr = "No such user";
		errno = 0;
		return NOUID;
	}
}

static gid_t gidbyuid(uid_t uid)
{
	struct passwd *p;

	reseterr();
	p = getpwuid(uid);
	if (p) return p->pw_gid;
	else {
		errstr = "No such uid";
		errno = 0;
		return NOGID;
	}
}

static gid_t gidbyname(const char *name)
{
	struct group *g;

	reseterr();
	if (isnum(name))
		return (gid_t)atoi(name);
	g = getgrnam(name);
	if (g) return g->gr_gid;
	else {
		errstr = "No such group";
		errno = 0;
		return NOGID;
	}
}

static int getugroups(const char *name, gid_t gr, gid_t *grps, int *ngrps)
{
	reseterr();
	if (isnum(name)) {
		struct passwd *p;
		p = getpwuid(atoi(name));
		if (p) name = p->pw_name;
		else { errstr = "No such uid"; errno = 0; }
	}
	return getgrouplist(name, gr, grps, ngrps);
}

/* Duplicates, I know */
static char *shellbyname(const char *name)
{
	struct passwd *p;
	char *r;

	reseterr();
	if (isnum(name)) {
		p = getpwuid(atoi(name));
		if (!p) goto _binsh;
	}
	else {
		p = getpwnam(name);
		if (!p) goto _binsh;
	}
	r = strdup(p->pw_shell);
	if (!r) goto _binsh;
	return r;

_binsh:
	return defsh;
}

static char *udirbyname(const char *name)
{
	struct passwd *p;
	char *r;

	reseterr();
	if (isnum(name)) {
		p = getpwuid(atoi(name));
		if (!p) goto _root;
	}
	else {
		p = getpwnam(name);
		if (!p) goto _root;
	}
	r = strdup(p->pw_dir);
	if (!r) goto _root;
	return r;

_root:
	return root;
}

static char *namebyuid(uid_t uid)
{
	struct passwd *p;

	reseterr();
	p = getpwuid(uid);
	if (p) return strdup(p->pw_name);
	else {
		errstr = "No such user";
		errno = 0;
		return NULL;
	}
}

static void mkenv(uid_t u, const char *usr, const char *usrdir)
{
	char *s;

	if (!usr) usr = namebyuid(u);
	s = xmalloc(11);
	snprintf(s, 11, "%u", u);
	if (!usr) usr = s;
	if (!usrdir) usrdir = udirbyname(usr);
	setenv("HOME", usrdir, 1);
	setenv("USER", usr, 1);
	if (getenv("LOGNAME")) setenv("LOGNAME", usr, 1);
	if (getenv("USERNAME")) setenv("USERNAME", usr, 1);
	setenv("UID", s, 1);
}


int main(int argc, char **argv)
{
	progname = basename(*argv);

	gid_t ogid[NGIDS], ngid[NGIDS];
	int osz, nsz;
	int i, c, grused, eugid, dashused, keepenv, login, do_chdir;
	char *s, *d, *usr, *altargv0, *logsh, *usrdir, *term;
	size_t n;
	uid_t u, U;
	gid_t g, G, sg, orig;
	u = U = NOUID;
	g = G = sg = orig = NOGID;
	osz = nsz = grused = eugid = dashused = keepenv = login = do_chdir = 0;
	usr = altargv0 = logsh = usrdir = term = NULL;

	if (argc < 2) usage();

	opterr = 0;
	optind = 1;
	errstr = NULL;

	u = getuid();
	g = getgid();

	while ((c = getopt(argc, argv, "u:U:g:G:s:S:e:Ea:AiIPDv")) != -1) {
		switch (c) {
			case 'u':
				usr = optarg;
				u = uidbyname(optarg);
				if (u == NOUID) xerror("uidbyname");
				g = gidbyuid(u);
				if (g == NOGID) { serror("gidbyuid"); g = (gid_t)u; }
				break;
			case 'U':
				usr = optarg;
				eugid = 1;
				U = uidbyname(optarg);
				if (U == NOUID) xerror("uidbyname");
				break;
			case 'g':
				g = gidbyname(optarg);
				if (g == NOGID) xerror("gidbyname");
				orig = gidbyuid(u);
				if (orig == NOGID) serror("gidbyuid");
				if (g == NOGID) g = orig;
				break;
			case 'G':
				eugid = 1;
				G = gidbyname(optarg);
				if (G == NOGID) xerror("gidbyname");
				break;
			case 's':
			case 'S':
				grused = 1;
				s = optarg; s--;
				if (c == 'S') {
					osz = _s(ogid);
					if (!usr) {
						if ((osz = getgroups(osz, ogid)) == -1) lperror("getgroups");
					}
					else {
						if (getugroups(usr, orig == NOGID ? g : orig, ogid, &osz) == -1) serror("getugroups");
					}
					for (i = 0; i < osz && i < NGIDS; i++, nsz++) ngid[i] = ogid[i];
				}
				do {
					char x[256];
					memset(x, 0, sizeof(x));
					s++;
					d = strchr(s, ',');
					xstrlcpy(x, s, d ? d-s : sizeof(x));

					if (c == 'S' && x[0] == '-') {
						sg = gidbyname(x+1);
						if (sg == NOGID) serror("gidbyname");
						for (i = 0; i < nsz && i < NGIDS; i++)
							if (sg == ngid[i]) ngid[i] = NOGID;
					}
					else {
						sg = gidbyname(x);
						if (sg == NOGID) xerror("gidbyname");
						ngid[nsz] = sg; nsz++;
					}
				} while ((s = strchr(s, ',')) && nsz < NGIDS);
				for (i = 0, osz = 0; i < nsz && i < NGIDS; i++) {
					if (ngid[i] != NOGID) {
						ogid[osz] = ngid[i];
						osz++;
					}
				}
				break;
			case 'a':
				altargv0 = optarg;
				break;
			case 'A':
				dashused = 1;
				break;
			case 'P':
				keepenv = 1;
				break;
			case 'i':
			case 'I':
/* Part 0 of login stuff */
				if (grused) usage();
				login++;
				if (c == 'I') login++;
				if (!keepenv) {
					if (!term) term = getenv("TERM");
					*environ = NULL;
				}
				break;
			case 'e':
				if (strchr(optarg, '=')) putenv(optarg);
				break;
			case 'E':
				*environ = NULL;
				break;
			case 'D':
				do_chdir = 1;
				break;
			case 'v':
				warn = 1;
				break;
			default: usage(); break;
		}
	}

/* Part 1 of login stuff */
	if (login) {
		s = NULL;
		if (!usr) {
			usr = namebyuid(0);
			if (!usr) usr = zerouid;
			u = g = 0;
		}
		if (login == 2) logsh = defsh;
		else logsh = shellbyname(usr);
		s = basename(logsh);
		n = strlen(s)+2;
		altargv0 = xmalloc(n);
		snprintf(altargv0, n, "-%s", s);
		usrdir = udirbyname(usr);
		if (chdir(usrdir) == -1) lperror("chdir");
		if (term) setenv("TERM", term, 1); /* To have no troubles */
	}
	
	if (U == NOUID) U = u;
	if (G == NOGID) G = g;
	if (!grused) {
		if (!usr) goto _keepgrp;
		osz = _s(ogid);
		if (getugroups(usr, orig == NOGID ? g : orig, ogid, &osz) == -1) serror("getugroups");
	}
	if (setgroups(osz, ogid) == -1) lperror("setgroups");
_keepgrp:
	if (setregid(g, G) == -1) xerror("setregid");
	if (setreuid(u, U) == -1) xerror("setreuid");

	if (!keepenv) mkenv(u, usr, usrdir);
	if (do_chdir) {
		if (!usr) usr = namebyuid(u);
		if (!usrdir) usrdir = udirbyname(usr);
		if (chdir(usrdir) == -1) xerror("chdir");
	}

/* Part 2 of login stuff */
	if (login) {
		execlp(logsh, altargv0, NULL);
		perror(logsh);
		return 127;
	}

	if (argv[optind]) {
		char *orig;
		if (dashused) {
			n = strlen(argv[optind])+2;
			altargv0 = xmalloc(n);
			snprintf(altargv0, n, "-%s", argv[optind]);
		}
		if (altargv0) {
			orig = argv[optind];
			argv[optind] = altargv0;
		}
		execvp(altargv0 ? orig : argv[optind], argv+optind);
		perror(altargv0 ? orig : argv[optind]);
		return 127;
	}
	else usage();

	return 0;
}
