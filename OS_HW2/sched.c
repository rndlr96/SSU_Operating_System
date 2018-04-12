/*
 * OS Assignment #2
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

#define MSG(x...) fprintf (stderr, x)
#define STRERROR  strerror (errno)

#define PROCESS_MAX (26 * 10)
#define ID_MAX      2

#define ARRIVE_TIME_MIN 0
#define ARRIVE_TIME_MAX 30

#define SERVICE_TIME_MIN 1
#define SERVICE_TIME_MAX 30

#define PRIORITY_MIN 1
#define PRIORITY_MAX 10

#define SLOT_MAX ((ARRIVE_TIME_MAX + SERVICE_TIME_MAX) * PROCESS_MAX)

enum
{
  SCHED_SJF = 0,
  SCHED_SRT,
  SCHED_RR,
  SCHED_PR,
  SCHED_MAX
};

typedef struct _Process Process;
struct _Process
{
  int    idx;
  int    queue_idx;

  char   id[ID_MAX + 1];
  int    arrive_time;
  int    service_time;
  int    priority;

  int    remain_time;
  int    complete_time;
  int    turnaround_time;
  int    wait_time;
};

static Process  processes[PROCESS_MAX];
static int      process_total;

static Process *queue[PROCESS_MAX];
static int      queue_len;

static char     schedule[PROCESS_MAX][SLOT_MAX];

static char *
strstrip (char *str)
{
  char  *start;
  size_t len;

  len = strlen (str);
  while (len--)
    {
      if (!isspace (str[len]))
	break;
      str[len] = '\0';
    }

  for (start = str; *start && isspace (*start); start++)
    ;
  memmove (str, start, strlen (start) + 1);

  return str;
}

static int
check_valid_id (const char *str)
{
  size_t len;
  int    i;

  len = strlen (str);
  if (len != ID_MAX)
    return -1;

  for (i = 0; i < len; i++)
    if (!(isupper (str[i]) || isdigit (str[i])))
      return -1;

  return 0;
}

static Process *
lookup_process (const char *id)
{
  int i;

  for (i = 0; i < process_total; i++)
    if (!strcmp (id, processes[i].id))
      return &processes[i];

  return NULL;
}

static void
append_process (Process *process)
{
  processes[process_total] = *process;
  processes[process_total].idx = process_total;
  process_total++;
}

static int
read_config (const char *filename)
{
  FILE *fp;
  char  line[256];
  int   line_nr;

  fp = fopen (filename, "r");
  if (!fp)
    return -1;

  process_total = 0;

  line_nr = 0;
  while (fgets (line, sizeof (line), fp))
    {
      Process process;
      char  *p;
      char  *s;
      size_t len;

      line_nr++;
      memset (&process, 0x00, sizeof (process));

      len = strlen (line);
      if (line[len - 1] == '\n')
	line[len - 1] = '\0';

      if (0)
	MSG ("config[%3d] %s\n", line_nr, line);

      strstrip (line);

      /* comment or empty line */
      if (line[0] == '#' || line[0] == '\0')
	continue;

      /* id */
      s = line;
      p = strchr (s, ' ');
      if (!p)
	goto invalid_line;
      *p = '\0';
      strstrip (s);
      if (check_valid_id (s))
	{
	  MSG ("invalid process id '%s' in line %d, ignored\n", s, line_nr);
	  continue;
	}
      if (lookup_process (s))
	{
	  MSG ("duplicate process id '%s' in line %d, ignored\n", s, line_nr);
	  continue;
	}
      strcpy (process.id, s);

      /* arrive time */
      s = p + 1;
      p = strchr (s, ' ');
      if (!p)
	goto invalid_line;
      *p = '\0';
      strstrip (s);

      process.arrive_time = strtol (s, NULL, 10);
      if (process.arrive_time < ARRIVE_TIME_MIN
	  || ARRIVE_TIME_MAX < process.arrive_time
	  || (process_total > 0 &&
	      processes[process_total - 1].arrive_time > process.arrive_time))
	{
	  MSG ("invalid arrive-time '%s' in line %d, ignored\n", s, line_nr);
	  continue;
	}

      /* service time */
      s = p + 1;
      p = strchr (s, ' ');
      if (!p)
	goto invalid_line;
      *p = '\0';
      strstrip (s);
      process.service_time = strtol (s, NULL, 10);
      if (process.service_time < SERVICE_TIME_MIN
	  || SERVICE_TIME_MAX < process.service_time)
	{
	  MSG ("invalid service-time '%s' in line %d, ignored\n", s, line_nr);
	  continue;
	}

      /* priority */
      s = p + 1;
      strstrip (s);
      process.priority = strtol (s, NULL, 10);
      if (process.priority < PRIORITY_MIN
	  || PRIORITY_MAX < process.priority)
	{
	  MSG ("invalid priority '%s' in line %d, ignored\n", s, line_nr);
	  continue;
	}

      append_process (&process);
      continue;

    invalid_line:
      MSG ("invalid format in line %d, ignored\n", line_nr);
    }

  fclose (fp);

  return 0;
}

static void
simulate (int sched)
{
  Process *process;
  int      p;
  int      p_done;
  int      cpu_time; //스케줄링 할 프로세스가 없을 때까지 걸리는 시간
  int      sum_turnaround_time; // 프로세스 완료시간 합계
  int      sum_waiting_time; // 프로세스 대기 시간 합계
  float    avg_turnaround_time; // 평균
  float    avg_waiting_time; // 평균

  for (p = 0; p < PROCESS_MAX; p++)
    {
      int slot;

      for (slot = 0; slot < SLOT_MAX; slot++)
         schedule[p][slot] = 0;
      queue[p] = NULL;
    }

  p = 0;
  p_done = 0;
  queue_len = 0;
  process = NULL;

  for (cpu_time = 0; p_done < process_total; cpu_time++)
    {
      /* Insert arrived process into the queue. */
      for (; p < process_total; p++)
	{
	  Process *pp;

	  pp = &processes[p];
	  if (pp->arrive_time != cpu_time)
	    break;
	  pp->remain_time = pp->service_time;
	  pp->queue_idx = queue_len;

	  queue[queue_len] = pp;
	  queue_len++;
	}

      /* Pick a process according to scheduling algorithm. */
      switch (sched)
	{
	case SCHED_SJF:
	  if (!process)
	    {
	      int i;
	      int shortest;

	      shortest = SERVICE_TIME_MAX + 1;
	      for (i = 0; i < queue_len; i++)
		if (queue[i]->service_time < shortest)
		  {
		    process = queue[i];
		    shortest = process->service_time;
		  }
	    }
	  break;
	case SCHED_SRT:
	  {
	    int i;
	    int shortest;

	    shortest = SERVICE_TIME_MAX + 1;
	    for (i = 0; i < queue_len; i++)
	      if (queue[i]->remain_time < shortest)
		    {
		         process = queue[i];
		         shortest = process->remain_time;
		    }
	  }
	  break;
	case SCHED_RR:
	  {
	    int i;

	    process = queue[0];
	    for (i = 0; i < (queue_len - 1); i++)
	      {
		queue[i] = queue[i + 1];
		queue[i]->queue_idx = i;
	      }
	    queue[i] = process;
	    queue[i]->queue_idx = i;
	  }
	  break;
	case SCHED_PR:
	  /* TO BE IMPLEMENTED */

/*================================Edit Code========================================*/
    {

        int i;
        int highest; //현재 가장 높은 우선 순위를 저장

        highest = 10; // 처음 들어오는 프로세스를 실행시키기 위해 우선순위를 가장 낮은 10으로 초기화

        for(i = 0 ; i < queue_len ; i++){ // 모든 프로세스가 들어올 때까지 반복
            if(queue[i]->priority < highest){ // 도착한 프로세스의 우선순위가 현재 진행중인 프로세스의 우선순위보다 높은지 확인
                process = queue[i]; // 높다면 해당 프로세스에 cpu할당
                highest = process->priority; // cpu가 할당된 프로세스의 우선순위가 현재 가장 높기 때문에 highest에 우선순위 저장
            }
        }
    }

/*================================Edit Code========================================*/

	  break;
	default:
	  MSG ("invalid scheduing algorithm '%d', ignored\n", sched);
	  return;
	}

      if (0)
	MSG ("[%02d] %s[%d:%d] %d/%d\n",
	     cpu_time,
	     process->id,
	     process->idx,
	     process->queue_idx,
	     process->remain_time,
	     process->service_time);

      /* no process to schedule. */
      if (!process)
	continue;

      schedule[process->idx][cpu_time] = 1;
      process->remain_time--;
      if (process->remain_time <= 0)
	{
	  int i;

	  for (i = process->queue_idx; i < (queue_len - 1); i++)
	    {
	      queue[i] = queue[i + 1];
	      queue[i]->queue_idx = i;
	    }
	  queue_len--;

	  process->complete_time = cpu_time + 1;
	  process->turnaround_time =
	    process->complete_time - process->arrive_time;
	  process->wait_time =
	    process->turnaround_time - process->service_time;

	  process = NULL;
	  p_done++;
	}
    }

  printf ("\n[%s]\n",
	  sched == SCHED_SJF ? "SJF" :
	  sched == SCHED_SRT ? "SRT" :
	  sched == SCHED_RR  ? "RR" :
	  sched == SCHED_PR  ? "PR" : "UNKNOWN");

  sum_turnaround_time = 0;
  sum_waiting_time = 0;
  for (p = 0; p < process_total; p++)
    {
      int slot;

      printf ("%s ", processes[p].id);
      for (slot = 0; slot <= cpu_time; slot++)
	putchar (schedule[p][slot] ? '*' : ' ');
      printf ("\n");

      sum_turnaround_time += processes[p].turnaround_time;
      sum_waiting_time += processes[p].wait_time;
    }

  avg_turnaround_time = (float) sum_turnaround_time / (float) process_total;
  avg_waiting_time = (float) sum_waiting_time / (float) process_total;

  printf ("CPU TIME: %d\n", cpu_time);
  printf ("AVERAGE TURNAROUND TIME: %.2f\n", avg_turnaround_time);
  printf ("AVERAGE WAITING TIME: %.2f\n", avg_waiting_time);
}

int
main (int    argc,
      char **argv)
{
  int sched;

  if (argc <= 1)
    {
      MSG ("usage: %s input-file\n", argv[0]);
      return -1;
    }

  if (read_config (argv[1]))
    {
      MSG ("failed to load config file '%s': %s\n", argv[1], STRERROR);
      return -1;
    }

  for (sched = 0; sched < SCHED_MAX; sched++)
    simulate (sched);

  return 0;
}
