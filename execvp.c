#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc < 2) return 0;
	return execvp(*(argv+1), *(argv+2) ? argv+2 : argv+1);
}
