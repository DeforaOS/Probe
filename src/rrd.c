/* $Id$ */
/* Copyright (c) 2016 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Network Probe */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */



#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <System.h>
#include "rrd.h"

/* constants */
#ifndef RRDTOOL
# define RRDTOOL		"rrdtool"
#endif


/* RRD */
/* private */
/* prototypes */
static int _rrd_exec(char * argv[]);
static int _rrd_perror(char const * message, int ret);


/* public */
/* functions */
/* rrd_update */
int rrd_update(RRDType type, char const * filename, int args_cnt, ...)
{
	int ret;
	va_list args;

	va_start(args, args_cnt);
	ret = rrd_updatev(type, filename, args_cnt, args);
	va_end(args);
	return ret;
}


/* rrd_updatev */
int rrd_updatev(RRDType type, char const * filename, int args_cnt, va_list args)
{
	char * argv[] = { RRDTOOL, "update", NULL, NULL, NULL };
	struct timeval tv;
	size_t s;
	int pos;
	int i;
	int ret;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%u, \"%s\")\n", __func__, type, filename);
#endif
	/* prepare the parameters */
	if(gettimeofday(&tv, NULL) != 0)
		return _rrd_perror("gettimeofday", -errno);
	if((argv[2] = string_new(filename)) == NULL)
		return 1;
	s = (args_cnt + 1) * 12;
	if((argv[3] = malloc(s)) == NULL)
	{
		string_delete(argv[2]);
		return _rrd_perror(NULL, -errno);
	}
	pos = snprintf(argv[3], s, "%ld", tv.tv_sec);
	for(i = 0; i < args_cnt; i++)
		pos += snprintf(&argv[3][pos], s - pos, ":%u",
				va_arg(args, unsigned int));
	/* update the database */
	ret = _rrd_exec(argv);
	free(argv[3]);
	string_delete(argv[2]);
	return ret;
}


/* private */
/* functions */
/* rrd_exec */
static int _rrd_exec(char * argv[])
{
	pid_t pid;
	int status;
	int ret;

	if((pid = fork()) == -1)
		return _rrd_perror("fork", -errno);
	if(pid == 0)
	{
		execvp(argv[0], argv);
		_rrd_perror(argv[0], 1);
		exit(2);
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() ", __func__);
	while(*argv != NULL)
		fprintf(stderr, "%s ", *argv++);
	fprintf(stderr, "\n");
#endif
	while((ret = waitpid(pid, &status, 0)) != -1)
		if(WIFEXITED(status))
			break;
	if(ret == -1)
		return _rrd_perror("waitpid", -errno);
	return WEXITSTATUS(status);
}


/* rrd_perror */
static int _rrd_perror(char const * message, int ret)
{
	return error_set_code(ret, "%s%s%s", (message != NULL) ? message : "",
			(message != NULL) ? ": " : "", strerror(errno));
}
