/*
 * px - search for processes and print top(1)-like status
 *
 * To the extent possible under law, Leah Neukirchen <leah@vuxu.org>
 * has waived all copyright and related or neighboring rights to this work.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include <sys/auxv.h>
#include <sys/sysinfo.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <proc/readproc.h>

int fflag;

static void
print_human(intmax_t i)
{
        double d = i;
        const char *u = "\0\0K\0M\0G\0T\0P\0E\0Z\0Y\0";
        while (d >= 1024) {
                u += 2;
                d /= 1024.0;
        }

        if (!*u)
                printf("%5.0f", d);
        else if (d < 10.0)
                printf("%4.1f%s", d, u);
        else
                printf("%4.0f%s", d, u);
}

static void
print_time(time_t t) {
	unsigned int dd, hh, mm, ss;

	ss = t % 60; t /= 60;
	mm = t % 60; t /= 60;
	hh = t % 24; t /= 24;
	dd = t;

	if (dd)
		printf(" %3ud%02uh", dd, hh);
	else if (hh)
		printf("  %2uh%02um", hh, mm);
	else
		printf("   %2u:%02u", mm, ss);
}

int
main(int argc, char *argv[])
{
	int clktck;
	if ((clktck = getauxval(AT_CLKTCK)) <= 0)
		if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
			clktck = 100; 	/* educated guess */

	struct sysinfo si;
	sysinfo(&si);

	pid_t me = getpid();

	time_t now = time(0);

	proc_t **result;
	result = readproctab(PROC_FILLCOM | PROC_FILLUSR |
	    PROC_FILLSTAT | PROC_FILLSTATUS);

	int matched = 0;

	int c;
        while ((c = getopt(argc, argv, "f")) != -1)
                switch (c) {
		case 'f': fflag = 1; break;
                default:
                        fprintf(stderr,
			    "Usage: %s [-f] [PATTERN...]\n", argv[0]);
                        exit(1);
                }

	for (; *result; result++) {
		proc_t *p = *result;

		if (argc <= optind)
			goto match;

		for (int i = optind; i < argc; i++) {
			for (size_t j = 0; j < strlen(argv[i]); j++)
				if (!isdigit(argv[i][j]))
					goto word;
			if (p->tid == atoi(argv[i]))
				goto match;
			else
				continue;
word:
			if (fflag && p->cmdline) {
				if (p->tid == me)
					continue; /* we'd always match ourself */
				for (int j = 0; p->cmdline[j]; j++)
					if (strstr(p->cmdline[j], argv[i]))
						goto match;
			}
			else if (strstr(p->cmd, argv[i]))
				goto match;
		}
		continue;

match:
		matched++;

		if (matched == 1)
			printf("  PID USER     %%CPU  VSZ  RSS START ELAPSED CPUTIME COMMAND\n");

		printf("%5d", p->tid);

		printf(" %-8.8s", p->euser);

		double pcpu = 0;
		time_t seconds, cputime;
		cputime = (p->utime + p->stime) / clktck;
		seconds = si.uptime - p->start_time / clktck;
		if (seconds)
			pcpu = (cputime * 100.0) / seconds;
		printf(" %4.1f", pcpu);

		print_human(p->vm_size*1024);

		print_human(p->vm_rss*1024);

		char buf[7];
		time_t start;
		time_t seconds_ago;
		start = (now - si.uptime) + p->start_time / clktck;
		seconds_ago = now - start;
		if (seconds_ago < 0)
			seconds_ago = 0;
		if (seconds_ago > 3600*24)
			strftime(buf, sizeof buf, "%b%d", localtime(&start));
		else
			strftime(buf, sizeof buf, "%H:%M", localtime(&start));
		printf(" %s", buf);

		print_time(seconds_ago);

		print_time(cputime);

		if (p->cmdline)
			for (int i = 0; p->cmdline[i]; i++)
				printf(" %s", p->cmdline[i]);
		else		// kernel threads
			printf(" [%s]", p->cmd);
		if (p->state == 'Z')
			printf(" <defunct>");

		printf("\n");
	}

	exit(!matched);
}
