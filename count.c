/* Something in between 'cat' and 'wc -l'
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#include "tino/alarm.h"
#include "tino/buf_line.h"
#include "tino/getopt.h"
#include "tino/scale.h"

#include "count_version.h"

static unsigned long long	count, total, max;
static int			bs, current;
static unsigned long long	lastrun;
static int			flag_final, flag_no, had_progress;
static int			flag_zero;

static void
show_progress(FILE *fd)
{
  if (had_progress)
    putc('\r', fd);
  fprintf(fd,	"%s ",		tino_scale_interval(0, (long)lastrun, 1, -7)	);
  if (max)
    fprintf(fd,	"%s%% ",	tino_scale_percent(2, count, max, -5)		);
  fprintf(fd,	"%llu",		count						);
  if (max)
    fprintf(fd,	"/%llu",	max						);
  fprintf(fd,	" %s",		(bs<0 ? "lines" : ( bs>0 ? "blocks" : "byte") )	);
  if (current)
    fprintf(fd,	" at %s/s",	tino_scale_slew_avg(3,4, count, lastrun, 0, -6)	);
  fprintf(fd,	" %siB/s ",	tino_scale_speed(1, total, lastrun, 0, -6)	);
  if (current)
    fprintf(fd,	" at %siB/s",	tino_scale_slew_avg(5,6, total, lastrun, 0, -6)	);
  fflush(fd);
  had_progress	= 1;
}

static int
progress(void *user, long delta, time_t now, long run)
{
  lastrun	= run;
  if (!flag_no)
    show_progress(stderr);
  return 0;
}

static int
fin(int ret)
{
  if (flag_final)
    {
      show_progress(stderr);
      putc('\n', stderr);
    }
  exit(ret);
}

/* ideas: free flags: g jk xy */
/* Afile append output to given file (instead -d) */
/* Dn output to the given FD (leaves STDERR to exec) */
/* E exec command, count output of it, allows cmd args.. and return status of cmd */
/* L only count real (nonempty) lines */
/* Ps prefix string to output of final status */
/* Q do not output progress to given file (allows -a and -d) */
/* Rn read the given file descriptor (for exec) */
/* S count spaces, possibly needs -l to ignore spans of spaces */
/* T change terminator character (cannot be used with -s) */
/* U unbuffered output (write partial output blocks on BS) */
/* V verbose */
/* Wn write to the given file descriptor */

int
main(int argc, char **argv)
{
  TINO_BUF	buf;
  int		argn;
  int		ibs, obs;
  int		get, min;

  argn	= tino_getopt(argc, argv, 0, 0,
		      TINO_GETOPT_VERSION(COUNT_VERSION)
		      " \n"
		      "	Show progress by using it in a pipe like\n"
		      "	.. | count | ..\n"
		      "	.. | count -b1M | ..",

		      TINO_GETOPT_USAGE
		      "h	this help"
		      ,

		      TINO_GETOPT_INT
		      TINO_GETOPT_SUFFIX
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN
		      TINO_GETOPT_MAX
		      "b byte	count Blocks of given size, not lines.\n"
		      "		This also is the default blocksize for I/O.\n"
		      "		Give it as 0 to count bytes with a default blocksize"
		      , &bs,
		      -1,
		      0,
		      0x3fffffff,

		      TINO_GETOPT_FLAG
		      "c	show Current average speed (1/100)"
		      , &current,

		      TINO_GETOPT_FLAG
		      "f	show Final status"
		      , &flag_final,

		      TINO_GETOPT_INT
		      TINO_GETOPT_SUFFIX
		      TINO_GETOPT_MIN
		      TINO_GETOPT_MAX
		      "i byte	Input blocksize, defaults to -b"
		      , &ibs,
		      0,
		      0x3fffffff,

		      TINO_GETOPT_ULLONG
		      TINO_GETOPT_SUFFIX
		      "m max	Maximum transfer size, shows percentage, too.\n"
		      "		Note that this is according to -b, not bytes."
		      , &max,
		      
		      TINO_GETOPT_FLAG
		      "n	do Not show progress output\n"
		      "		With -f this only outputs the final status"
		      , &flag_no,

		      TINO_GETOPT_INT
		      TINO_GETOPT_SUFFIX
		      TINO_GETOPT_MIN
		      TINO_GETOPT_MAX
		      "o byte	Output blocksize, defaults to -b"
		      , &obs,
		      0,
		      0x3fffffff,

		      TINO_GETOPT_FLAG
		      "z	count NUL-terminated strings (for: find -print0)\n"
		      "		Please note that NUL always acts as line terminator anyway"
		      , &flag_zero,

		      NULL
		      );
  if (argn<=0)
    return 1;

  if (!obs && bs>0)
    obs	= bs;
  if (!ibs)
    ibs	= bs;
  if (ibs<=0)
    ibs	= BUFSIZ;

  tino_buf_initO(&buf);
  tino_alarm_set(1, progress, NULL);
  for (min=0; (get=tino_buf_readE(&buf, 0, ibs))!=0; min=tino_buf_get_lenO(&buf))
    {
      int	len;

      len	= tino_buf_get_lenO(&buf);
      total	+= len-min;
      if (bs>=0)
	count	= total/(bs ? bs : 1);
      else
	{
	  int	pos;

	  /* count the lines in the buffer	*/
	  for (pos=min; (pos=tino_buf_line_scan(&buf, flag_zero ? 0 : '\n', pos))<0; )
	    {
              pos	= -pos;
	      count++;
	    }
	}
      while (len)
	{
	  int	max;

	  max	= len;
	  if (obs)
	    {
	      if (max>obs)
		max	= obs;
	      else if (bs && max<obs)
		break;
	    }
	  if (tino_buf_write_away_allE(&buf, 1, max))
	    {
	      perror("write");
	      fin(2);
	    }
	  len	-= max;
	}
    }
  if (min)
    {
      tino_buf_write_away_allE(&buf, 1, 0);
      /* tell that last line is incomplete?
       */
    }
  tino_buf_freeO(&buf);
  fin(0);
  return 0;
}
