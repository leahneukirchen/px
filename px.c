// gcc -g -O2 -Wall -o px px.c -lprocps

#include <sys/sysinfo.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <proc/readproc.h>

time_t seconds_since_boot;
int Hertz;

static int want_this_proc_pcpu(proc_t *buf){
  unsigned long long used_jiffies;
  unsigned long pcpu = 0;
  unsigned long long seconds;

//  if(!want_this_proc(buf)) return 0;

  used_jiffies = buf->utime + buf->stime;
//  if(include_dead_children) used_jiffies += (buf->cutime + buf->cstime);

  seconds = seconds_since_boot - buf->start_time / Hertz;
  if(seconds) pcpu = (used_jiffies * 1000ULL / Hertz) / seconds;

  buf->pcpu = pcpu;  // fits in an int, summing children on 128 CPUs

  return 1;
}

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

int
main(int argc, char *argv[])
{
	proc_t **result;
	int hz;

	if((hz = sysconf(_SC_CLK_TCK)) > 0){
		Hertz = hz;
	} else {
		Hertz = 100; 	/* shrug */
	}

	struct sysinfo si;
	sysinfo(&si);

	seconds_since_boot = si.uptime;
	time_t now = time(0);

	result = readproctab(PROC_FILLMEM | PROC_FILLCOM | PROC_FILLUSR |
	    PROC_FILLSTAT | PROC_FILLSTATUS);

	int matched = 0;

	for (; *result; result++) {
		proc_t *p = *result;

		if (argc > 0) {
			for (int i = 1; i < argc; i++)
				if (strstr(p->cmd, argv[i]))
					goto match;
			continue;
match:
			matched++;
		} else {
			matched++;
		}

		if (matched == 1)
			printf("  PID USER     %%CPU  VSZ  RSS START ELAPSED   TIME COMMAND\n");

		want_this_proc_pcpu(p);

		printf("%5d", p->tid);
		printf(" %-8.8s", p->euser);
		printf(" %4.1f", p->pcpu/10.0);
		print_human(p->vm_size*1024);
		print_human(p->vm_rss*1024);

		time_t start;
		time_t seconds_ago;
		start = (now - si.uptime) + p->start_time / Hertz;
		seconds_ago = now - start;
		if(seconds_ago < 0) seconds_ago=0;

		char buf[7];
		if (seconds_ago > 3600*24)
			strftime(buf, sizeof buf, "%b%d", localtime(&start));
		else
			strftime(buf, sizeof buf, "%H:%M", localtime(&start));
		printf(" %s", buf);

		time_t t = seconds_ago;
		unsigned dd,hh,mm,ss;
		ss = t%60;
		t /= 60;
		mm = t%60;
		t /= 60;
		hh = t%24;
		t /= 24;
		dd = t;

		if (dd)
			printf(" %3ud%02uh", dd, hh);
		else if (hh)
			printf("  %2uh%02um", hh, mm);
		else
			printf("   %2u:%02u", mm, ss);


		t = (p->utime + p->stime) / Hertz;
		ss = t%60;
		t /= 60;
		mm = t%60;
		t /= 60;
		hh = t%24;

		if (hh)
			printf(" %3uh%02u", hh, mm);
		else
			printf("  %2u:%02u", mm, ss);

		if (p->cmdline) {
			for (int i = 0; p->cmdline[i]; i++) {
				printf(" %s", p->cmdline[i]);
			}
		} else {
			// kernel threads
			printf(" [%s]", p->cmd);
		}

		if (p->state == 'Z')
			printf(" <defunct>");
		printf("\n");

#if 0
		printf("%d %s %f %ld %ld %ld %lld %lld %lld %s\n",
		    p->tid, p->euser, p->pcpu/10.0, pmem,
		    p->vm_size, p->vm_rss, p->start_time, 0ULL, p->utime,
		    p->cmd
		);
#endif
	}

//	freeproctab(result);

	exit(matched > 0);
}
