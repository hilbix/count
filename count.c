/* $Header$
 *
 * Something in between 'cat' and 'wc -l'
 *
 * Copyright (C)2009 Valentin Hilbig <webmaster@scylla-charybdis.com>
 *
 * This is release early code.  Use at own risk.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 *
 * $Log$
 * Revision 1.4  2009-05-23 17:01:47  tino
 * Usage corrected
 *
 * Revision 1.3  2009-03-24 17:38:11  tino
 * Option -c should work now
 *
 * Revision 1.2  2009-03-24 03:15:33  tino
 * Usage corrected, slew corrected (option -c)
 *
 * Revision 1.1  2009-03-24 02:24:40  tino
 * First version
 */

#include "tino/alarm.h"
#include "tino/buf_line.h"
#include "tino/getopt.h"
#include "tino/scale.h"

#include "count_version.h"

static unsigned long long	count, total, max;
static int			bs, current;

static int
progress(void *user, long delta, time_t now, long run)
{
  fprintf(stderr,	"\r%s ",	tino_scale_interval(0, run, 1, -7)				);
  if (max)
    fprintf(stderr,	"%s%% ",	tino_scale_percent(2, count, max, -5)				);
  fprintf(stderr,	"%llu",		count								);
  if (max)
    fprintf(stderr,	"/%llu",	max								);
  fprintf(stderr,	" %s",		(bs<0 ? "Lines" : ( bs>0 ? "Blocks" : "Byte") )			);
  if (current)
    fprintf(stderr,	" at %s/s",	tino_scale_slew_avg(3,4, count, (unsigned long long)run, 0, -6)	);
  fprintf(stderr,	" %siB/s ",	tino_scale_speed(1, total, (unsigned long long)run, 0, -6)	);
  if (current)
    fprintf(stderr,	" at %siB/s",	tino_scale_slew_avg(5,6, total, (unsigned long long)run, 0, -6)	);
  fflush(stderr);
  return 0;
}

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
		      
		      TINO_GETOPT_INT
		      TINO_GETOPT_SUFFIX
		      TINO_GETOPT_MIN
		      TINO_GETOPT_MAX
		      "o byte	Output blocksize, defaults to -b"
		      , &obs,
		      0,
		      0x3fffffff,

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
	  for (pos=min; (pos=tino_buf_line_scan(&buf, '\n', pos))<0; )
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
	      return 2;
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
  return 0;
}
