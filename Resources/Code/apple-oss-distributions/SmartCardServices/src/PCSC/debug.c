/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 1999-2008
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: debug.c 123 2010-03-27 10:50:42Z ludovic.rousseau@gmail.com $
 */

/**
 * @file
 * @brief This handles debugging for libpcsclite.
 */

#include "config.h"
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "debug.h"
//#include "strlcpycat.h"

#define DEBUG_BUF_SIZE 2048

/** default level is quiet to avoid polluting fd 2 (possibly NOT stderr) */
static char LogLevel = PCSC_LOG_CRITICAL+1;

static signed char LogDoColor = 0;	/**< no color by default */

static void log_init(void)
{
	char *e;

#ifdef LIBPCSCLITE
	e = getenv("PCSCLITE_DEBUG");
#else
	e = getenv("MUSCLECARD_DEBUG");
#endif
	if (e)
		LogLevel = atoi(e);

	/* log to stderr and stderr is a tty? */
	if (isatty(fileno(stderr)))
	{
		const char *terms[] = { "linux", "xterm", "xterm-color", "Eterm", "rxvt", "rxvt-unicode" };
		char *term;

		term = getenv("TERM");
		if (term)
		{
			unsigned int i;

			/* for each known color terminal */
			for (i = 0; i < sizeof(terms) / sizeof(terms[0]); i++)
			{
				/* we found a supported term? */
				if (0 == strcmp(terms[i], term))
				{
					LogDoColor = 1;
					break;
				}
			}
		}
	}
} /* log_init */

void log_msg(const int priority, const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;
	static int is_initialized = 0;

	if (!is_initialized)
	{
		log_init();
		is_initialized = 1;
	}

	if (priority < LogLevel) /* log priority lower than threshold? */
		return;

	va_start(argptr, fmt);
	(void)vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
	va_end(argptr);

	{
		if (LogDoColor)
		{
			const char *color_pfx = "", *color_sfx = "\33[0m";

			switch (priority)
			{
				case PCSC_LOG_CRITICAL:
					color_pfx = "\33[01;31m"; /* bright + Red */
					break;

				case PCSC_LOG_ERROR:
					color_pfx = "\33[35m"; /* Magenta */
					break;

				case PCSC_LOG_INFO:
					color_pfx = "\33[34m"; /* Blue */
					break;

				case PCSC_LOG_DEBUG:
					color_pfx = ""; /* normal (black) */
					color_sfx = "";
					break;
			}
			fprintf(stderr, "%s%s%s\n", color_pfx, DebugBuffer, color_sfx);
		}
		else
			fprintf(stderr, "%s\n", DebugBuffer);
	}
} /* log_msg */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	int i;
	char *c;
	char *debug_buf_end;

	if (priority < LogLevel) /* log priority lower than threshold? */
		return;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	(void)strlcpy(DebugBuffer, msg, sizeof(DebugBuffer));
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += strlen(c);
	}

	fprintf(stderr, "%s\n", DebugBuffer);
} /* log_xxd */

// unused in PCSC framework
int DebugLogSetCategory(const int dbginfo)
{
	return 0;
}

void DebugLogCategory(const int category, const unsigned char *buffer,
	const int len)
{
}

void DebugLogSetLevel(const int level)
{
}

void DebugLogSetLogType(const int dbgtype)
{
}

