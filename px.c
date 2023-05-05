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
#include <unistd.h>

#include <libproc2/pids.h>
#include <libproc2/stat.h>

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
	pid_t me = getpid();

	time_t now = time(0);

	struct stat_info *stat_info = 0;
	if (procps_stat_new(&stat_info) < 0)
		abort();
	time_t boot_time = STAT_GET(stat_info, STAT_SYS_TIME_OF_BOOT, ul_int);
	procps_stat_unref(&stat_info);

	struct pids_info *Pids_info = 0;
	enum pids_item items[] = {
		PIDS_CMD,
		PIDS_CMDLINE_V,
		PIDS_ID_EUSER,
		PIDS_ID_TID,
		PIDS_STATE,
		PIDS_TIME_ALL,
		PIDS_TIME_ELAPSED,
		PIDS_TIME_START,
		PIDS_TTY_NAME,
		PIDS_UTILIZATION,
		PIDS_VM_RSS,
		PIDS_VM_SIZE,
	};
	enum rel_items {
		EU_CMD,
		EU_CMDLINE_V,
		EU_ID_EUSER,
		EU_ID_TID,
		EU_STATE,
		EU_TIME_ALL,
		EU_TIME_ELAPSED,
		EU_TIME_START,
		EU_TTY_NAME,
		EU_UTILIZATION,
		EU_VM_RSS,
		EU_VM_SIZE,
	};
	if (procps_pids_new(&Pids_info, items, 12) < 0) {
		abort();
	}

	struct pids_fetch *reap;
	if (!(reap = procps_pids_reap(Pids_info, PIDS_FETCH_TASKS_ONLY))) {
		abort();
	}

//	result = readproctab(PROC_FILLCOM | PROC_FILLUSR |
//	    PROC_FILLSTAT | PROC_FILLSTATUS);

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

	int total_procs = reap->counts->total;
	for (int i = 0; i < total_procs; i++) {
#define PIDS_GETCHR(e) PIDS_VAL(EU_ ## e, s_ch, reap->stacks[i], Pids_info)
#define PIDS_GETINT(e) PIDS_VAL(EU_ ## e, s_int, reap->stacks[i], Pids_info)
#define PIDS_GETUNT(e) PIDS_VAL(EU_ ## e, u_int, reap->stacks[i], Pids_info)
#define PIDS_GETULINT(e) PIDS_VAL(EU_ ## e, ul_int, reap->stacks[i], Pids_info)
#define PIDS_GETREAL(e) PIDS_VAL(EU_ ## e, real, reap->stacks[i], Pids_info)
#define PIDS_GETSTR(e) PIDS_VAL(EU_ ## e, str, reap->stacks[i], Pids_info)
#define PIDS_GETSTR_V(e) PIDS_VAL(EU_ ## e, strv, reap->stacks[i], Pids_info)
		if (argc <= optind)
			goto match;

		for (int j = optind; j < argc; j++) {
			for (size_t k = 0; k < strlen(argv[j]); k++)
				if (!isdigit(argv[j][k]))
					goto word;
			if (PIDS_GETINT(ID_TID) == atoi(argv[j]))
				goto match;
			else
				continue;
word:
			if (fflag && PIDS_GETSTR_V(CMDLINE_V)) {
				if (PIDS_GETINT(ID_TID) == me)
					continue; /* we'd always match ourself */
				for (int k = 0; PIDS_GETSTR_V(CMDLINE_V)[k]; k++)
					if (strstr(PIDS_GETSTR_V(CMDLINE_V)[k], argv[j]))
						goto match;
			}
			else if (strstr(PIDS_GETSTR(CMD), argv[j]))
				goto match;
		}
		continue;

match:
		matched++;

		if (matched == 1)
			printf("  PID USER     %%CPU  VSZ  RSS START ELAPSED CPUTIME COMMAND\n");

		printf("%5d", PIDS_GETINT(ID_TID));

		printf(" %-8.8s", PIDS_GETSTR(ID_EUSER));

		printf(" %4.1f", PIDS_GETREAL(UTILIZATION));

		print_human(PIDS_GETULINT(VM_SIZE)*1024);

		print_human(PIDS_GETULINT(VM_RSS)*1024);

		char buf[7];
		time_t start = boot_time + PIDS_GETREAL(TIME_START);
		time_t seconds_ago = now - start;
		if (seconds_ago < 0)
			seconds_ago = 0;
		if (seconds_ago > 3600*24)
			strftime(buf, sizeof buf, "%b%d", localtime(&start));
		else
			strftime(buf, sizeof buf, "%H:%M", localtime(&start));
		printf(" %s", buf);

		print_time(PIDS_GETREAL(TIME_ELAPSED));

		print_time(PIDS_GETREAL(TIME_ALL));

		if (PIDS_GETSTR_V(CMDLINE_V))
			for (int j = 0; PIDS_GETSTR_V(CMDLINE_V)[j]; j++)
				printf(" %s", PIDS_GETSTR_V(CMDLINE_V)[j]);
		else		// kernel threads
			printf(" [%s]", PIDS_GETSTR(CMD));
		if (PIDS_GETCHR(STATE) == 'Z')
			printf(" <defunct>");

		printf("\n");
	}

	exit(!matched);
}
