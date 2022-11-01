/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2008 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Venkat Raghavan S <rvenkat@novell.com>                      |
   +----------------------------------------------------------------------+
*/

/* $Id: tsrm_nw.c,v 1.1.6.3.2.3 2007/12/31 07:22:45 sebastian Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "TSRM.h"

#ifdef NETWARE

#ifdef USE_MKFIFO
#include <sys/stat.h>
#elif !defined(USE_PIPE_OPEN)   /* NXFifoOpen */
#include <nks/fsio.h>
#endif

#include <nks/vm.h>
#include <nks/memory.h>

#include <string.h>

#include "mktemp.h"

/* strtok() call in LibC is abending when used in a different address space -- hence using
   PHP's version itself for now : Venkat (30/4/02) */
#include "tsrm_strtok_r.h"
#define tsrm_strtok_r(a,b,c) strtok((a),(b))

#define WHITESPACE  " \t"
#define MAX_ARGS    10


TSRM_API FILE* popen(const char *commandline, const char *type)
{
    char *command = NULL, *argv[MAX_ARGS] = {'\0'}, **env = NULL;
	char *tempName = "sys:/php/temp/phpXXXXXX.tmp";
    char *filePath = NULL;
    char *ptr = NULL;
    int ptrLen = 0, argc = 0, i = 0, envCount = 0, err = 0;
	FILE *stream = NULL;
#if defined(USE_PIPE_OPEN) || defined(USE_MKFIFO)
    int pipe_handle;
    int mode = O_RDONLY;
#else
    NXHandle_t pipe_handle;
    NXMode_t mode = NX_O_RDONLY;
#endif
    NXExecEnvSpec_t envSpec;
    NXNameSpec_t nameSpec;
    NXVmId_t newVM = 0;

    /* Check for validity of input parameters */
    if (!commandline || !type)
        return NULL;

    /* Get temporary file name */
    filePath = mktemp(tempName);
/*consoleprintf ("PHP | popen: file path = %s, mode = %s\n", filePath, type);*/
	if (!filePath)
		return NULL;

    /* Set pipe mode according to type -- for now allow only "r" or "w" */
    if (strcmp(type, "r") == 0)
#if defined(USE_PIPE_OPEN) || defined(USE_MKFIFO)
        mode = O_RDONLY;
#else
        mode = NX_O_RDONLY;
#endif
    else if (strcmp(type, "w") == 0)
#if defined(USE_PIPE_OPEN) || defined(USE_MKFIFO)
        mode = O_WRONLY;
#else
        mode = NX_O_WRONLY;
#endif
    else
        return NULL;

#ifdef USE_PIPE_OPEN
    pipe_handle = pipe_open(filePath, mode);
/*consoleprintf ("PHP | popen: pipe_open() returned %d\n", pipe_handle);*/
    if (pipe_handle == -1)
        return NULL;
#elif defined(USE_MKFIFO)
    pipe_handle = mkfifo(filePath, mode);
consoleprintf ("PHP | popen: mkfifo() returned %d\n", pipe_handle);
    if (pipe_handle == -1)
        return NULL;
#else
    /*
        - NetWare doesn't require first parameter
        - Allowing LibC to choose the buffer size for now
    */
    err = NXFifoOpen(0, filePath, mode, 0, &pipe_handle);
/*consoleprintf ("PHP | popen: NXFifoOpen() returned %d\n", err);*/
    if (err)
        return NULL;
#endif

    /* Copy the environment variables in preparation for the spawn call */

    envCount = NXGetEnvCount() + 1;  /* add one for NULL */
    env = (char**)NXMemAlloc(sizeof(char*) * envCount, 0);
    if (!env)
        return NULL;

    err = NXCopyEnv(env, envCount);
consoleprintf ("PHP | popen: NXCopyEnv() returned %d\n", err);
    if (err)
    {
        NXMemFree (env);
        return NULL;
    }

    /* Separate commandline string into words */
consoleprintf ("PHP | popen: commandline = %s\n", commandline);
    ptr = tsrm_strtok_r((char*)commandline, WHITESPACE, NULL);
    ptrLen = strlen(ptr);

    command = (char*)malloc(ptrLen + 1);
    if (!command)
    {
        NXMemFree (env);
        return NULL;
    }

    strcpy (command, ptr);

    ptr = tsrm_strtok_r(NULL, WHITESPACE, NULL);
    while (ptr && (argc < MAX_ARGS))
    {
        ptrLen = strlen(ptr);

        argv[argc] = (char*)malloc(ptrLen + 1);
        if (!argv[argc])
        {
            NXMemFree (env);

            if (command)
                free (command);

            for (i = 0; i < argc; i++)
            {
                if (argv[i])
                free (argv[i]);
            }

            return NULL;
        }

        strcpy (argv[argc], ptr);

        argc++;

        ptr = tsrm_strtok_r(NULL, WHITESPACE, NULL);
    }
consoleprintf ("PHP | popen: commandline string parsed into tokens\n");
    /* Setup the execution environment and spawn new process */

    envSpec.esFlags = 0;    /* Not used */
    envSpec.esArgc = argc;
    envSpec.esArgv = (void**)argv;
    envSpec.esEnv = (void**)env;

    envSpec.esStdin.ssType =
    envSpec.esStdout.ssType = NX_OBJ_FIFO;
    envSpec.esStderr.ssType = NX_OBJ_FILE;
/*
    envSpec.esStdin.ssHandle =
    envSpec.esStdout.ssHandle =
    envSpec.esStderr.ssHandle = -1;
*/
    envSpec.esStdin.ssPathCtx =
    envSpec.esStdout.ssPathCtx =
    envSpec.esStderr.ssPathCtx = NULL;

#if defined(USE_PIPE_OPEN) || defined(USE_MKFIFO)
    if (mode == O_RDONLY)
#else
    if (mode == NX_O_RDONLY)
#endif
    {
        envSpec.esStdin.ssPath = filePath;
        envSpec.esStdout.ssPath = stdout;
    }
    else /* Write Only */
    {
        envSpec.esStdin.ssPath = stdin;
        envSpec.esStdout.ssPath = filePath;
    }

    envSpec.esStderr.ssPath = stdout;

    nameSpec.ssType = NX_OBJ_FIFO;
/*    nameSpec.ssHandle = 0; */ /* Not used */
    nameSpec.ssPathCtx = NULL;  /* Not used */
    nameSpec.ssPath = argv[0];
consoleprintf ("PHP | popen: environment setup\n");
    err = NXVmSpawn(&nameSpec, &envSpec, 0, &newVM);
consoleprintf ("PHP | popen: NXVmSpawn() returned %d\n", err);
    if (!err)
        /* Get file pointer corresponding to the pipe (file) opened */
        stream = fdopen(pipe_handle, type);

    /* Clean-up */

    if (env)
        NXMemFree (env);

    if (pipe_handle)
#if defined(USE_PIPE_OPEN) || defined(USE_MKFIFO)
        close(pipe_handle);
#else
        NXClose(pipe_handle);
#endif

    if (command)
        free (command);

    for (i = 0; i < argc; i++)
    {
        if (argv[i])
            free (argv[i]);
    }
consoleprintf ("PHP | popen: all clean-up done, returning...\n");
    return stream;
}

TSRM_API int pclose(FILE* stream)
{
    int err = 0;
    NXHandle_t fd = 0;

    /* Get the process associated with this pipe (file) handle and terminate it */
    fd = fileno(stream);
    NXClose (fd);

    err = fclose(stream);

    return err;
}

#endif
