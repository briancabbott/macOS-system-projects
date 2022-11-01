/*
 * "$Id: escputil.c,v 1.1.1.3 2004/07/23 06:26:31 jlovell Exp $"
 *
 *   Printer maintenance utility for EPSON Stylus (R) printers
 *
 *   Copyright 2000-2003 Robert Krawitz (rlk@alum.mit.edu)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#if defined(HAVE_VARARGS_H) && !defined(HAVE_STDARG_H)
#include <varargs.h>
#else
#include <stdarg.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif
#include <gimp-print/gimp-print-intl-internal.h>

void do_align(void);
char *do_get_input (const char *prompt);
void do_head_clean(void);
void do_help(int code);
void do_identify(void);
void do_ink_level(void);
void do_nozzle_check(void);
void do_status(void);
int do_print_cmd(void);


const char *banner = N_("\
Escputil version " VERSION ", Copyright (C) 2000-2003 Robert Krawitz\n\
Escputil comes with ABSOLUTELY NO WARRANTY; for details type 'escputil -l'\n\
This is free software, and you are welcome to redistribute it\n\
under certain conditions; type 'escputil -l' for details.\n");

const char *license = N_("\
Copyright 2000 Robert Krawitz (rlk@alum.mit.edu)\n\
\n\
This program is free software; you can redistribute it and/or modify it\n\
under the terms of the GNU General Public License as published by the Free\n\
Software Foundation; either version 2 of the License, or (at your option)\n\
any later version.\n\
\n\
This program is distributed in the hope that it will be useful, but\n\
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY\n\
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License\n\
for more details.\n\
\n\
You should have received a copy of the GNU General Public License\n\
along with this program; if not, write to the Free Software\n\
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n");


#ifdef HAVE_GETOPT_H

struct option optlist[] =
{
  { "printer-name",	1,	NULL,	(int) 'P' },
  { "raw-device",	1,	NULL,	(int) 'r' },
  { "ink-level",	0,	NULL,	(int) 'i' },
  { "clean-head",	0,	NULL,	(int) 'c' },
  { "nozzle-check",	0,	NULL,	(int) 'n' },
  { "align-head",	0,	NULL,	(int) 'a' },
  { "status",           0,      NULL,   (int) 's' },
  { "new",		0,	NULL,	(int) 'u' },
  { "help",		0,	NULL,	(int) 'h' },
  { "identify",		0,	NULL,	(int) 'd' },
  { "model",		1,	NULL,	(int) 'm' },
  { "quiet",		0,	NULL,	(int) 'q' },
  { "license",		0,	NULL,	(int) 'l' },
  { "list-models",	0,	NULL,	(int) 'M' },
  { "short-name",	0,	NULL,	(int) 'S' },
  { NULL,		0,	NULL,	0 	  }
};

const char *help_msg = N_("\
Usage: escputil [-c | -n | -a | -i | -s | -d | -l | -M]\n\
                [-P printer | -r device] [-u] [-q] [-m model] [ -S ]\n\
Perform maintenance on EPSON Stylus (R) printers.\n\
Examples: escputil --clean-head --printer stpex-on-third-floor\n\
          escputil --ink-level --new --raw-device /dev/lp0\n\
\n\
  Commands:\n\
    -c|--clean-head    Clean the print head.\n\
    -n|--nozzle-check  Print a nozzle test pattern.\n\
                       Dirty or clogged nozzles will show as gaps in the\n\
                       pattern.  If you see any gaps, you should clean\n\
                       the print head.\n\
    -a|--align-head    Align the print head.  CAUTION: Misuse of this\n\
                       utility may result in poor print quality and/or\n\
                       damage to the printer.\n\
    -s|--status        Retrieve printer status.\n\
    -i|--ink-level     Obtain the ink level from the printer.  This requires\n\
                       read/write access to the raw printer device.\n\
    -d|--identify      Query the printer for make and model information.\n\
                       This requires read/write access to the raw printer\n\
                       device.\n\
    -l|--license       Display the license/warranty terms of this program.\n\
    -M|--list-models   List the available printer models.\n\
    -h|--help          Print this help message.\n\
  Options:\n\
    -P|--printer-name  Specify the name of the printer queue to operate on.\n\
                       Default is the default system printer.\n\
    -r|--raw-device    Specify the name of the device to write to directly\n\
                       rather than going through a printer queue.\n\
    -u|--new           The printer is a new printer (Stylus Color 740 or\n\
                       newer).\n\
    -q|--quiet         Suppress the banner.\n\
    -S|--short-name    Print the short name of the printer with --identify.\n\
    -m|--model         Specify the precise printer model for head alignment.\n");
#else
const char *help_msg = N_("\
Usage: escputil [OPTIONS] [COMMAND]\n\
Usage: escputil [-c | -n | -a | -i | -s | -d | -l | -M]\n\
                [-P printer | -r device] [-u] [-q] [-m model] [ -S ]\n\
Perform maintenance on EPSON Stylus (R) printers.\n\
Examples: escputil -c -P stpex-on-third-floor\n\
          escputil -i -u -r /dev/lp0\n\
\n\
  Commands:\n\
    -c Clean the print head.\n\
    -n Print a nozzle test pattern.\n\
          Dirty or clogged nozzles will show as gaps in the\n\
          pattern.  If you see any gaps, you should clean\n\
          the print head.\n\
    -a Align the print head.  CAUTION: Misuse of this\n\
          utility may result in poor print quality and/or\n\
          damage to the printer.\n\
    -s Retrieve printer status.\n\
    -i Obtain the ink level from the printer.  This requires\n\
          read/write access to the raw printer device.\n\
    -d Query the printer for make and model information.  This\n\
          requires read/write access to the raw printer device.\n\
    -l Display the license/warranty terms of this program.\n\
    -M List the available printer models.\n\
    -h Print this help message.\n\
  Options:\n\
    -P Specify the name of the printer queue to operate on.\n\
          Default is the default system printer.\n\
    -r Specify the name of the device to write to directly\n\
          rather than going through a printer queue.\n\
    -u The printer is a new printer (Stylus Color 740 or newer).\n\
    -q Suppress the banner.\n\
    -S Print the short name of the printer with -d.\n\
    -m Specify the precise printer model for head alignment.\n");
#endif

char *the_printer = NULL;
char *raw_device = NULL;
char *printer_model = NULL;
char printer_cmd[1025];
int bufpos = 0;
int isnew = 0;
int print_short_name = 0;

static void
print_models(void)
{
  int printer_count = stp_printer_model_count();
  int i;
  for (i = 0; i < printer_count; i++)
    {
      const stp_printer_t *printer = stp_get_printer_by_index(i);
      if (strcmp(stp_printer_get_family(printer), "escp2") == 0)
	{
	  printf("%-15s %s\n", stp_printer_get_driver(printer),
		 _(stp_printer_get_long_name(printer)));
	}
    }
}

void
do_help(int code)
{
  printf("%s", _(help_msg));
  exit(code);
}

static void
exit_packet_mode(void)
{
  static char hdr[] = "\000\000\000\033\001@EJL 1284.4\n@EJL     \n\033@";
  memcpy(printer_cmd + bufpos, hdr, sizeof(hdr) - 1); /* DON'T include null! */
  bufpos += sizeof(hdr) - 1;
}

static void
initialize_print_cmd(void)
{
  bufpos = 0;
  if (isnew)
    exit_packet_mode();
}

int
main(int argc, char **argv)
{
  int quiet = 0;
  int operation = 0;
  int c;

  /* Set up gettext */
#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
#endif

  stp_init();
  while (1)
    {
#if defined(HAVE_GETOPT_H) && defined(HAVE_GETOPT_LONG)
      int option_index = 0;
      c = getopt_long(argc, argv, "P:r:icnasduqm:hlMS", optlist, &option_index);
#else
      c = getopt(argc, argv, "P:r:icnasduqm:hlMS");
#endif
      if (c == -1)
	break;
      switch (c)
	{
	case 'q':
	  quiet = 1;
	  break;
	case 'c':
	case 'i':
	case 'n':
	case 'a':
	case 'd':
	case 's':
	case 'o':
	  if (operation)
	    do_help(1);
	  operation = c;
	  break;
	case 'P':
	  if (the_printer || raw_device)
	    {
	      printf(_("You may only specify one printer or raw device."));
	      do_help(1);
	    }
	  the_printer = stp_malloc(strlen(optarg) + 1);
	  strcpy(the_printer, optarg);
	  break;
	case 'r':
	  if (the_printer || raw_device)
	    {
	      printf(_("You may only specify one printer or raw device."));
	      do_help(1);
	    }
	  raw_device = stp_malloc(strlen(optarg) + 1);
	  strcpy(raw_device, optarg);
	  break;
	case 'm':
	  if (printer_model)
	    {
	      printf(_("You may only specify one printer model."));
	      do_help(1);
	    }
	  printer_model = stp_malloc(strlen(optarg) + 1);
	  strcpy(printer_model, optarg);
	  break;
	case 'u':
	  isnew = 1;
	  break;
	case 'h':
	  do_help(0);
	  break;
	case 'l':
	  printf("%s\n", _(license));
	  exit(0);
	case 'M':
	  print_models();
	  exit(0);
	case 'S':
	  print_short_name = 1;
	  break;
	default:
	  printf("%s\n", _(banner));
	  fprintf(stderr, _("Unknown option %c\n"), c);
	  do_help(1);
	}
    }
  if (!quiet)
    printf("%s\n", banner);
  if (operation == 0)
    {
      fprintf(stderr, "Usage: %s [OPTIONS] command\n", argv[0]);
#ifdef __GNU_LIBRARY__
      fprintf(stderr, "Type `%s --help' for more information.\n", argv[0]);
#else
      fprintf(stderr, "Type `%s -h' for more information.\n", argv[0]);
#endif
      exit(1);
    }
  initialize_print_cmd();
  switch(operation)
    {
    case 'c':
      do_head_clean();
      break;
    case 'n':
      do_nozzle_check();
      break;
    case 'i':
      do_ink_level();
      break;
    case 'a':
      do_align();
      break;
    case 'd':
      do_identify();
      break;
    case 's':
      do_status();
      break;
    default:
      do_help(1);
    }
  exit(0);
}

int
do_print_cmd(void)
{
  FILE *pfile;
  int bytes = 0;
  int retries = 0;
  char command[1024];
  memcpy(printer_cmd + bufpos, "\f\033\000\033\000", 5);
  bufpos += 5;
  if (raw_device)
    {
      pfile = fopen(raw_device, "wb");
      if (!pfile)
	{
	  fprintf(stderr, _("Cannot open device %s: %s\n"), raw_device,
		  strerror(errno));
	  return 1;
	}
    }
  else
    {
      if (!access("/bin/lpr", X_OK) ||
          !access("/usr/bin/lpr", X_OK) ||
          !access("/usr/bsd/lpr", X_OK))
        {
        if (the_printer == NULL)
          strcpy(command, "lpr -l");
	else
          snprintf(command, 1023, "lpr -P%s -l", the_printer);
        }
      else if (the_printer == NULL)
	strcpy(command, "lp -s -oraw");
      else
	snprintf(command, 1023, "lp -s -oraw -d%s", the_printer);

      if ((pfile = popen(command, "w")) == NULL)
	{
	  fprintf(stderr, _("Cannot print to printer %s with %s\n"),
		  the_printer, command);
	  return 1;
	}
    }
  while (bytes < bufpos)
    {
      int status = fwrite(printer_cmd + bytes, 1, bufpos - bytes, pfile);
      if (status == 0)
	{
	  retries++;
	  if (retries > 2)
	    {
	      fprintf(stderr, _("Unable to send command to printer\n"));
	      if (raw_device)
		fclose(pfile);
	      else
		pclose(pfile);
	      return 1;
	    }
	}
      else if (status == -1)
	{
	  fprintf(stderr, _("Unable to send command to printer\n"));
	  if (raw_device)
	    fclose(pfile);
	  else
	    pclose(pfile);
	  return 1;
	}
      else
	{
	  bytes += status;
	  retries = 0;
	}
    }
  if (raw_device)
    fclose(pfile);
  else
    pclose(pfile);
  return 0;
}

static int
read_from_printer(int fd, char *buf, int bufsize)
{
#ifdef HAVE_POLL
  struct pollfd ufds;
#endif
  int status;
  int retry = 5;

#ifdef HAVE_FCNTL_H
  fcntl(fd, F_SETFL,
	O_NONBLOCK | fcntl(fd, F_GETFL));
#endif
  memset(buf, 0, bufsize);

  do
    {
#ifdef HAVE_POLL
      ufds.fd = fd;
      ufds.events = POLLIN;
      ufds.revents = 0;
      if ((status = poll(&ufds, 1, 1000)) < 0)
	break;
#endif
      status = read(fd, buf, bufsize - 1);
      if (status == 0 || (status < 0 && errno == EAGAIN))
	{
	  sleep(1);
	  status = 0; /* not an error (read would have blocked) */
	}
    }
  while ((status == 0) && (--retry != 0));

  if (status == 0 && retry == 0)
    fprintf(stderr, _("Read from printer timed out\n"));
  else if (status < 0)
    fprintf(stderr, _("Cannot read from %s: %s\n"), raw_device, strerror(errno));

  return status;
}

static void
do_remote_cmd(const char *cmd, int nargs, ...)
{
  static char remote_hdr[] = "\033@\033(R\010\000\000REMOTE1";
  static char remote_trailer[] = "\033\000\000\000\033\000";
  int i;
  va_list args;
  va_start(args, nargs);

  memcpy(printer_cmd + bufpos, remote_hdr, sizeof(remote_hdr) - 1);
  bufpos += sizeof(remote_hdr) - 1;
  memcpy(printer_cmd + bufpos, cmd, 2);
  bufpos += 2;
  printer_cmd[bufpos] = nargs % 256;
  printer_cmd[bufpos + 1] = (nargs >> 8) % 256;
  if (nargs > 0)
    for (i = 0; i < nargs; i++)
      printer_cmd[bufpos + 2 + i] = va_arg(args, int);
  bufpos += 2 + nargs;
  memcpy(printer_cmd + bufpos, remote_trailer, sizeof(remote_trailer) - 1);
  bufpos += sizeof(remote_trailer) - 1;
}

static void
add_newlines(int count)
{
  int i;
  for (i = 0; i < count; i++)
    {
      printer_cmd[bufpos++] = '\r';
      printer_cmd[bufpos++] = '\n';
    }
}

static void
add_resets(int count)
{
  int i;
  for (i = 0; i < count; i++)
    {
      printer_cmd[bufpos++] = '\033';
      printer_cmd[bufpos++] = '\000';
    }
}

const char *colors[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
  N_("Light Cyan/Red"),
  N_("Light Magenta/Blue"),
  N_("Black/Dark Yellow"),
  N_("Gloss Optimizer"),
  0
};

static const stp_printer_t *
get_printer(int quiet)
{
  int printer_count = stp_printer_model_count();
  int i;
  if (!printer_model)
    {
      char buf[1024];
      int fd;
      int status;
      char *pos = NULL;
      char *spos = NULL;
      if (!raw_device)
	{
	  fprintf(stderr,
		  _("Printer alignment must be done with a raw device or else\n"
		   "the -m option must be used to specify a printer.\n"));
	  do_help(1);
	}
      if (!quiet)
	{
	  printf(_("Attempting to detect printer model..."));
	  fflush(stdout);
	}
      fd = open(raw_device, O_RDWR, 0666);
      if (fd == -1)
	{
	  printf(_("\nCannot open %s read/write: %s\n"), raw_device,
		  strerror(errno));
	  exit(1);
	}
      bufpos = 0;
      sprintf(printer_cmd, "\033\001@EJL ID\r\n");
      if (write(fd, printer_cmd, strlen(printer_cmd)) < strlen(printer_cmd))
	{
	  printf(_("\nCannot write to %s: %s\n"), raw_device, strerror(errno));
	  exit(1);
	}
      status = read_from_printer(fd, buf, 1024);
      if (status < 0)
	exit(1);
      (void) close(fd);
      pos = strchr(buf, (int) ';');
      if (pos)
	pos = strchr(pos + 1, (int) ';');
      if (pos)
	pos = strchr(pos, (int) ':');
      if (pos)
	spos = strchr(pos, (int) ';');
      if (!pos)
	{
	  printf(_("\nCannot detect printer type.\n"
		   "Please use -m to specify your printer model.\n"));
	  do_help(1);
	}
      if (spos)
	*spos = '\000';
      printer_model = pos + 1;
      if (!quiet)
	printf("%s\n\n", printer_model);
    }
  for (i = 0; i < printer_count; i++)
    {
      const stp_printer_t *printer = stp_get_printer_by_index(i);

      if (strcmp(stp_printer_get_family(printer), "escp2") == 0)
	{
	  const char *short_name = stp_printer_get_driver(printer);
	  const char *long_name = stp_printer_get_long_name(printer);
	  if (!strcasecmp(printer_model, short_name) ||
	      !strcasecmp(printer_model, long_name) ||
	      (!strncmp(short_name, "escp2-", strlen("escp2-")) &&
	       !strcasecmp(printer_model, short_name + strlen("escp2-"))) ||
	      (!strncmp(long_name, "EPSON ", strlen("EPSON ")) &&
	       !strcasecmp(printer_model, long_name + strlen("EPSON "))))
	    return printer;
	}
    }
  printf(_("Printer model %s is not known.\n"), printer_model);
  if (!quiet)
    do_help(1);
  return NULL;
}

void
do_ink_level(void)
{
  int fd;
  int status;
  int retry = 6;
  char buf[1024];
  char *ind;
  int i;
  if (!raw_device)
    {
      fprintf(stderr,_("Obtaining ink levels requires using a raw device.\n"));
      exit(1);
    }
  do
    {
      fd = open(raw_device, O_RDWR, 0666);
      if (fd == -1)
	{
	  fprintf(stderr, _("Cannot open %s read/write: %s\n"), raw_device,
		  strerror(errno));
	  exit(1);
	}
      add_resets(2);
      initialize_print_cmd();
      /*
       * Some new printers like IQ and others like ST for retrieving status.
       * Some printers will take either, but not reliably in either
       * case.  In some cases, alternating between the two works best.
       * -- rlk 20040508
       */
      if (isnew && !(retry & 1))
	do_remote_cmd("IQ", 1, 1);
      else
	do_remote_cmd("ST", 2, 0, 1);
      add_resets(2);
      if (write(fd, printer_cmd, bufpos) < bufpos)
	{
	  fprintf(stderr, _("Cannot write to %s: %s\n"), raw_device,
		  strerror(errno));
	  exit(1);
	}
      status = read_from_printer(fd, buf, 1024);
      if (status < 0)
	exit(1);
      (void) close(fd);
      ind = buf;
      do
	ind = strchr(ind, 'I');
      while (ind && ind[1] != 'Q' && (ind[1] != '\0' && ind[2] != ':'));
      if (!ind || ind[1] != 'Q' || ind[2] != ':' || ind[3] == ';')
	{
	  ind = NULL;
	}
    } while (--retry != 0 && !ind);
  if (!ind)
    {
      fprintf(stderr, _("Cannot parse output from printer\n"));
      exit(1);
    }
  ind += 3;
  printf("%20s    %s\n", _("Ink color"), _("Percent remaining"));
  for (i = 0; i < 8; i++)
    {
      int val, j;
      if (!ind[0] || ind[0] == ';')
	exit(0);
      for (j = 0; j < 2; j++)
	{
	  if (ind[j] >= '0' && ind[j] <= '9')
	    ind[j] -= '0';
	  else if (ind[j] >= 'A' && ind[j] <= 'F')
	    ind[j] = ind[j] - 'A' + 10;
	  else if (ind[j] >= 'a' && ind[j] <= 'f')
	    ind[j] = ind[j] - 'a' + 10;
	  else
	    exit(1);
	}
      val = (ind[0] << 4) + ind[1];
      printf("%20s    %3d\n", _(colors[i]), val);
      ind += 2;
    }
  exit(0);
}

void
do_identify(void)
{
  const stp_printer_t *printer;
  if (!raw_device)
    {
      fprintf(stderr,
	      _("Printer identification requires using a raw device.\n"));
      exit(1);
    }
  if (printer_model)
    printer_model = NULL;
  printer = get_printer(1);
  if (printer)
    {
      if (print_short_name)
	printf("%s\n", stp_printer_get_driver(printer));
      else
	printf("%s\n", _(stp_printer_get_long_name(printer)));
    }
  exit(0);
}

void
do_status(void)
{
  int fd;
  int status;
  char buf[1024];
  char *where;
  memset(buf, 0, 1024);
  if (!raw_device)
    {
      fprintf(stderr, _("Printer status requires using a raw device.\n"));
      exit(1);
    }
  fd = open(raw_device, O_RDWR, 0666);
  if (fd == -1)
    {
      fprintf(stderr, _("Cannot open %s read/write: %s\n"), raw_device,
	      strerror(errno));
      exit(1);
    }
  bufpos = 0;
  initialize_print_cmd();
  do_remote_cmd("ST", 2, 0, 1);
  if (write(fd, printer_cmd, bufpos) < bufpos)
    {
      fprintf(stderr, _("Cannot write to %s: %s\n"),
	      raw_device, strerror(errno));
      exit(1);
    }
  status = read_from_printer(fd, buf, 1024);
  if (status < 0)
    exit(1);
  initialize_print_cmd();
  do_remote_cmd("ST", 2, 0, 0);
  add_resets(2);
  (void) write(fd, printer_cmd, bufpos);
  (void) read_from_printer(fd, buf, 1024);
  while ((where = strchr(buf, ';')) != NULL)
    *where = '\n';
  printf("%s\n", buf);
  (void) close(fd);
  exit(0);
}


void
do_head_clean(void)
{
  do_remote_cmd("CH", 2, 0, 0);
  printf(_("Cleaning heads...\n"));
  exit(do_print_cmd());
}

void
do_nozzle_check(void)
{
  do_remote_cmd("VI", 2, 0, 0);
  do_remote_cmd("NC", 2, 0, 0);
  printf(_("Running nozzle check, please ensure paper is in the printer.\n"));
  exit(do_print_cmd());
}

const char *new_align_help = N_("\
Please read these instructions very carefully before proceeding.\n\
\n\
This utility lets you align the print head of your Epson Stylus inkjet\n\
printer.  Misuse of this utility may cause your print quality to degrade\n\
and possibly damage your printer.  This utility has not been reviewed by\n\
Seiko Epson for correctness, and is offered with no warranty at all.  The\n\
entire risk of using this utility lies with you.\n\
\n\
This utility prints %d test patterns.  Each pattern looks very similar.\n\
The patterns consist of a series of pairs of vertical lines that overlap.\n\
Below each pair of lines is a number between %d and %d.\n\
\n\
When you inspect the pairs of lines, you should find the pair of lines that\n\
is best in alignment, that is, that best forms a single vertical line.\n\
Inspect the pairs very carefully to find the best match.  Using a loupe\n\
or magnifying glass is recommended for the most critical inspection.\n\
It is also suggested that you use a good quality paper for the test,\n\
so that the lines are well-formed and do not spread through the paper.\n\
After picking the number matching the best pair, place the paper back in\n\
the paper input tray before typing it in.\n\
\n\
Each pattern is similar, but later patterns use finer dots for more\n\
critical alignment.  You must run all of the passes to correctly align your\n\
printer.  After running all the alignment passes, the alignment\n\
patterns will be printed once more.  You should find that the middle-most\n\
pair (#%d out of the %d) is the best for all patterns.\n\
\n\
After the passes are printed once more, you will be offered the\n\
choices of (s)aving the result in the printer, (r)epeating the process,\n\
or (q)uitting without saving.  Quitting will not restore the previous\n\
settings, but powering the printer off and back on will.  If you quit,\n\
you must repeat the entire process if you wish to later save the results.\n\
It is essential that you not turn your printer off during this procedure.\n\n");

const char *old_align_help = N_("\
Please read these instructions very carefully before proceeding.\n\
\n\
This utility lets you align the print head of your Epson Stylus inkjet\n\
printer.  Misuse of this utility may cause your print quality to degrade\n\
and possibly damage your printer.  This utility has not been reviewed by\n\
Seiko Epson for correctness, and is offered with no warranty at all.  The\n\
entire risk of using this utility lies with you.\n\
\n\
This utility prints a test pattern that consist of a series of pairs of\n\
vertical lines that overlap.  Below each pair of lines is a number between\n\
%d and %d.\n\
\n\
When you inspect the pairs of lines, you should find the pair of lines that\n\
is best in alignment, that is, that best forms a single vertical align.\n\
Inspect the pairs very carefully to find the best match.  Using a loupe\n\
or magnifying glass is recommended for the most critical inspection.\n\
It is also suggested that you use a good quality paper for the test,\n\
so that the lines are well-formed and do not spread through the paper.\n\
After picking the number matching the best pair, place the paper back in\n\
the paper input tray before typing it in.\n\
\n\
After running the alignment pattern, it will be printed once more.  You\n\
should find that the middle-most pair (#%d out of the %d) is the best.\n\
You will then be offered the choices of (s)aving the result in the printer,\n\
(r)epeating the process, or (q)uitting without saving.  Quitting will not\n\
restore the previous settings, but powering the printer off and back on will.\n\
If you quit, you must repeat the entire process if you wish to later save\n\
the results.  It is essential that you not turn off your printer during\n\
this procedure.\n\n");

static void
do_align_help(int passes, int choices)
{
  if (passes > 1)
    printf(_(new_align_help), passes, 1, choices, (choices + 1) / 2, choices);
  else
    printf(_(old_align_help), 1, choices, (choices + 1) / 2, choices);
  fflush(stdout);
}

static void
printer_error(void)
{
  printf(_("Unable to send command to the printer, exiting.\n"));
  exit(1);
}

static int
do_final_alignment(void)
{
  while (1)
    {
      char *inbuf;
      printf(_("Please inspect the final output very carefully to ensure that your\n"
	       "printer is in proper alignment. You may now:\n"
	       "  (s)ave the results in the printer,\n"
	       "  (q)uit without saving the results, or\n"
	       "  (r)epeat the entire process from the beginning.\n"
	       "You will then be asked to confirm your choice.\n"
	       "What do you want to do (s, q, r)?\n"));
      fflush(stdout);
      inbuf = do_get_input(_("> "));
      switch (inbuf[0])
	{
	case 'q':
	case 'Q':
	  printf(_("Please confirm by typing 'q' again that you wish to quit without saving:\n"));
	  fflush(stdout);
	  inbuf = do_get_input (_("> "));
	  if (inbuf[0] == 'q' || inbuf[0] == 'Q')
	    {
	      printf(_("OK, your printer is aligned, but the alignment has not been saved.\n"
		       "If you wish to save the alignment, you must repeat this process.\n"));
	      return 1;
	    }
	  break;
	case 'r':
	case 'R':
	  printf(_("Please confirm by typing 'r' again that you wish to repeat the\n"
		   "alignment process:\n"));
	  fflush(stdout);
	  inbuf = do_get_input(_("> "));
	  if (inbuf[0] == 'r' || inbuf[0] == 'R')
	    {
	      printf(_("Repeating the alignment process.\n"));
	      return 0;
	    }
	  break;
	case 's':
	case 'S':
	  printf(_("This will permanently alter the configuration of your printer.\n"
		   "WARNING: this procedure has not been approved by Seiko Epson, and\n"
		   "it may damage your printer. Proceed?\n"
		   "Please confirm by typing 's' again that you wish to save the settings\n"
		   "to your printer:\n"));

	  fflush(stdout);
	  inbuf = do_get_input(_("> "));
	  if (inbuf[0] == 's' || inbuf[0] == 'S')
	    {
	      printf(_("About to save settings..."));
	      fflush(stdout);
	      initialize_print_cmd();
	      do_remote_cmd("SV", 0);
	      if (do_print_cmd())
		{
		  printf(_("failed!\n"));
		  printf(_("Your settings were not saved successfully.  You must repeat the\n"
			   "alignment procedure.\n"));
		  exit(1);
		}
	      printf(_("succeeded!\n"));
	      printf(_("Your alignment settings have been saved to the printer.\n"));
	      return 1;
	    }
	  break;
	default:
	  printf(_("Unrecognized command.\n"));
	  continue;
	}
      printf(_("Final command was not confirmed.\n"));
    }
}

const char *printer_msg =
N_("This procedure assumes that your printer is an Epson %s.\n"
   "If this is not your printer model, please type control-C now and\n"
   "choose your actual printer model.\n\n"
   "Please place a sheet of paper in your printer to begin the head\n"
   "alignment procedure.\n");

/*
 * This is the thorny one.
 */
void
do_align(void)
{
  char *inbuf;
  long answer;
  char *endptr;
  int curpass;
  const stp_printer_t *printer = get_printer(0);
  stp_parameter_t desc;
  int passes = 0;
  int choices = 0;
  const char *printer_name;
  stp_vars_t *v = stp_vars_create();

  if (!printer)
    return;

  printer_name = stp_printer_get_long_name(printer);
  stp_set_driver(v, stp_printer_get_driver(printer));

  stp_describe_parameter(v, "AlignmentPasses", &desc);
  if (desc.p_type != STP_PARAMETER_TYPE_INT)
    {
      fprintf(stderr,
	      "Unable to retrieve number of alignment passes for printer %s\n",
	      printer_name);
      return;
    }
  passes = desc.deflt.integer;
  stp_parameter_description_destroy(&desc);

  stp_describe_parameter(v, "AlignmentChoices", &desc);
  if (desc.p_type != STP_PARAMETER_TYPE_INT)
    {
      fprintf(stderr,
	      "Unable to retrieve number of alignment choices for printer %s\n",
	      printer_name);
      return;
    }
  choices = desc.deflt.integer;
  stp_parameter_description_destroy(&desc);
  if (passes <= 0 || choices <= 0)
    {
      printf("No alignment required for printer %s\n", printer_name);
      return;
    }

  do
    {
      do_align_help(passes, choices);
      printf(_(printer_msg), _(printer_name));
      inbuf = do_get_input(_("Press enter to continue > "));
    top:
      initialize_print_cmd();
      for (curpass = 0; curpass < passes; curpass++)
	do_remote_cmd("DT", 3, 0, curpass, 0);
      if (do_print_cmd())
	printer_error();
      printf(_("Please inspect the print, and choose the best pair of lines in each pattern.\n"
	       "Type a pair number, '?' for help, or 'r' to repeat the procedure.\n"));
      initialize_print_cmd();
      for (curpass = 1; curpass <= passes; curpass ++)
	{
	reread:
	  printf(_("Pass #%d"), curpass);
	  inbuf = do_get_input(_("> "));
	  switch (inbuf[0])
	    {
	    case 'r':
	    case 'R':
	      printf(_("Please insert a fresh sheet of paper.\n"));
	      fflush(stdout);
	      initialize_print_cmd();
	      (void) do_get_input(_("Press enter to continue > "));
	      /* Ick. Surely there's a cleaner way? */
	      goto top;
	    case 'h':
	    case '?':
	      do_align_help(passes, choices);
	      fflush(stdout);
	    case '\n':
	    case '\000':
	      goto reread;
	    default:
	      break;
	    }
	  answer = strtol(inbuf, &endptr, 10);
	  if (errno == ERANGE)
	    {
	      printf(_("Number out of range!\n"));
	      goto reread;
	    }
	  if (endptr == inbuf)
	    {
	      printf(_("I cannot understand what you typed!\n"));
	      fflush(stdout);
	      goto reread;
	    }
	  if (answer < 1 || answer > choices)
	    {
	      printf(_("The best pair of lines should be numbered between 1 and %d.\n"),
		     choices);
	      fflush(stdout);
	      goto reread;
	    }
	  do_remote_cmd("DA", 4, 0, curpass - 1, 0, answer);
	}
      printf(_("Attempting to set alignment..."));
      if (do_print_cmd())
	printer_error();
      printf(_("succeeded.\n"));
      printf(_("Please verify that the alignment is correct.  After the alignment pattern\n"
	       "is printed again, please ensure that the best pattern for each line is\n"
	       "pattern %d.  If it is not, you should repeat the process to get the best\n"
	       "quality printing.\n"), (choices + 1) / 2);
      printf(_("Please insert a fresh sheet of paper.\n"));
      (void) do_get_input(_("Press enter to continue > "));
      initialize_print_cmd();
      for (curpass = 0; curpass < passes; curpass++)
	do_remote_cmd("DT", 3, 0, curpass, 0);
      if (do_print_cmd())
	printer_error();
    } while (!do_final_alignment());
  exit(0);
}

char *
do_get_input (const char *prompt)
{
	static char *input = NULL;
#if (HAVE_LIBREADLINE == 0 || !defined HAVE_READLINE_READLINE_H)
	char *fgets_status;
#endif
	/* free only if previously allocated */
	if (input)
	{
		stp_free (input);
		input = NULL;
	}
#if (HAVE_LIBREADLINE > 0 && defined HAVE_READLINE_READLINE_H)
	/* get input with libreadline, if present */
	input = readline (prompt);
	/* if input, add to history list */
#ifdef HAVE_READLINE_HISTORY_H
	if (input && *input)
	{
		add_history (input);
	}
#endif
#else
	/* no libreadline; use fgets instead */
	input = stp_malloc (sizeof (char) * BUFSIZ);
	memset(input, 0, BUFSIZ);
	printf ("%s", prompt);
	fgets_status = fgets (input, BUFSIZ, stdin);
	if (fgets_status == NULL)
	{
		fprintf (stderr, _("Error in input\n"));
		return (NULL);
	}
	else if (strlen (input) == 1 && input[0] == '\n')
	{
		/* user just hit enter: empty input buffer */
		/* remove line feed */
		input[0] = '\0';
	}
	else
	{
		/* remove line feed */
		input[strlen (input) - 1] = '\0';
	}
#endif
	return (input);
}
