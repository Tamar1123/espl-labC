#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <string.h>

void handler(int sig)
{
	printf("Recieved Signal : %s\n", strsignal(sig));

	signal(sig, SIG_DFL);

	if (sig == SIGTSTP)
	{
		signal(sig, SIG_DFL);
		signal(SIGCONT, handler);
	}
	else if (sig == SIGCONT)
	{
		signal(sig, SIG_DFL);
		signal(SIGTSTP, handler);
	}
	raise(sig);
}

int main(int argc, char **argv)
{

	printf("Starting the program\n");
	signal(SIGINT, handler);
	signal(SIGTSTP, handler);
	signal(SIGCONT, handler);

	while (1)
	{
		sleep(1);
	}

	return 0;
}