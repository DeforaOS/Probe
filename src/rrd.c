/* $Id$ */
/* Copyright (c) 2016-2022 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <System.h>
#include "rrd.h"

/* constants */
#ifndef PROGNAME_DAMON
# define PROGNAME_DAMON		"DaMon"
#endif
#ifndef RRDTOOL
# define RRDTOOL		"rrdtool"
#endif
#ifndef RRD_XFF
# define RRD_XFF		"0.5"
#endif
#ifndef RRD_AVERAGE_DAY
# define RRD_AVERAGE_DAY	"RRA:AVERAGE:" RRD_XFF ":1:1440"
#endif
#ifndef RRD_AVERAGE_WEEK
# define RRD_AVERAGE_WEEK	"RRA:AVERAGE:" RRD_XFF ":6:2160"
#endif
#ifndef RRD_AVERAGE_4WEEK
# define RRD_AVERAGE_4WEEK	"RRA:AVERAGE:" RRD_XFF ":24:4320"
#endif
#ifndef RRD_AVERAGE_YEAR
# define RRD_AVERAGE_YEAR	"RRA:AVERAGE:" RRD_XFF ":288:8640"
#endif
#ifndef RRD_MAX_DAY
# define RRD_MAX_DAY		"RRA:MAX:" RRD_XFF ":1:1440"
#endif
#ifndef RRD_MAX_WEEK
# define RRD_MAX_WEEK		"RRA:MAX:" RRD_XFF ":2:2160"
#endif
#ifndef RRD_MAX_4WEEK
# define RRD_MAX_4WEEK		"RRA:MAX:" RRD_XFF ":8:4320"
#endif
#ifndef RRD_MAX_YEAR
# define RRD_MAX_YEAR		"RRA:MAX:" RRD_XFF ":104:8640"
#endif


/* RRD */
/* private */
/* prototypes */
static int _rrd_exec(char * argv[]);
static int _rrd_perror(char const * message, int ret);
static char * _rrd_timestamp(off_t offset);


/* public */
/* functions */
/* rrd_create */
static int _create_directories(char const * filename);

int rrd_create(RRDType type, char const * rrdcached, char const * filename)
{
	int ret;
	char * argv[22] = { RRDTOOL, "create", NULL, "--start", NULL };
	size_t i = 5;

	/* create parent directories */
	if(_create_directories(filename) != 0)
		return -1;
	if(rrdcached != NULL)
	{
		argv[i++] = "--daemon";
		if((argv[i++] = string_new(rrdcached)) == NULL)
			return -1;
	}
	switch(type)
	{
		case RRDTYPE_LOAD:
			argv[i++] = "--step";
			argv[i++] = "300";
			argv[i++] = "DS:load1:GAUGE:600:0:U";
			argv[i++] = "DS:load5:GAUGE:600:0:U";
			argv[i++] = "DS:load15:GAUGE:600:0:U";
			argv[i++] = RRD_AVERAGE_DAY;
			argv[i++] = RRD_AVERAGE_WEEK;
			argv[i++] = RRD_AVERAGE_4WEEK;
			argv[i++] = RRD_AVERAGE_YEAR;
			argv[i++] = RRD_MAX_DAY;
			argv[i++] = RRD_MAX_WEEK;
			argv[i++] = RRD_MAX_4WEEK;
			argv[i++] = RRD_MAX_YEAR;
			break;
		case RRDTYPE_PROCS:
			argv[i++] = "--step";
			argv[i++] = "300";
			argv[i++] = "DS:procs:GAUGE:600:0:U";
			argv[i++] = RRD_AVERAGE_DAY;
			argv[i++] = RRD_AVERAGE_WEEK;
			argv[i++] = RRD_AVERAGE_4WEEK;
			argv[i++] = RRD_AVERAGE_YEAR;
			argv[i++] = RRD_MAX_DAY;
			argv[i++] = RRD_MAX_WEEK;
			argv[i++] = RRD_MAX_4WEEK;
			argv[i++] = RRD_MAX_YEAR;
			break;
		case RRDTYPE_UPGRADES:
			argv[i++] = "--step";
			argv[i++] = "300";
			argv[i++] = "DS:upgrades:GAUGE:600:0:U";
			argv[i++] = RRD_AVERAGE_DAY;
			argv[i++] = RRD_AVERAGE_WEEK;
			argv[i++] = RRD_AVERAGE_4WEEK;
			argv[i++] = RRD_AVERAGE_YEAR;
			argv[i++] = RRD_MAX_DAY;
			argv[i++] = RRD_MAX_WEEK;
			argv[i++] = RRD_MAX_4WEEK;
			argv[i++] = RRD_MAX_YEAR;
			break;
		case RRDTYPE_USERS:
			argv[i++] = "--step";
			argv[i++] = "300";
			argv[i++] = "DS:users:GAUGE:600:0:U";
			argv[i++] = RRD_AVERAGE_DAY;
			argv[i++] = RRD_AVERAGE_WEEK;
			argv[i++] = RRD_AVERAGE_4WEEK;
			argv[i++] = RRD_AVERAGE_YEAR;
			argv[i++] = RRD_MAX_DAY;
			argv[i++] = RRD_MAX_WEEK;
			argv[i++] = RRD_MAX_4WEEK;
			argv[i++] = RRD_MAX_YEAR;
			break;
		case RRDTYPE_VOLUME:
			argv[i++] = "--step";
			argv[i++] = "300";
			argv[i++] = "DS:used:GAUGE:600:0:U";
			argv[i++] = "DS:total:GAUGE:600:0:U";
			argv[i++] = RRD_AVERAGE_DAY;
			argv[i++] = RRD_AVERAGE_WEEK;
			argv[i++] = RRD_AVERAGE_4WEEK;
			argv[i++] = RRD_AVERAGE_YEAR;
			argv[i++] = RRD_MAX_DAY;
			argv[i++] = RRD_MAX_WEEK;
			argv[i++] = RRD_MAX_4WEEK;
			argv[i++] = RRD_MAX_YEAR;
			break;
		default:
			/* unsupported graph */
			if(rrdcached != NULL)
				string_delete(argv[6]);
			return -1;
	}
	argv[2] = string_new(filename);
	argv[4] = _rrd_timestamp(-1);
	argv[i++] = NULL;
	/* create the database */
	if(argv[2] != NULL && argv[4] != NULL)
		ret = _rrd_exec(argv);
	else
		ret = -1;
	if(rrdcached != NULL)
		string_delete(argv[6]);
	string_delete(argv[4]);
	string_delete(argv[2]);
	return ret;
}

static int _create_directories(char const * filename)
{
	int ret = 0;
	char * p;
	size_t i;

	if((p = strdup(filename)) == NULL)
		return -1;
	for(i = 0; p[i] != '\0'; i++)
	{
		if(i == 0 || p[i] != '/')
			continue;
		p[i] = '\0';
		if(mkdir(p, 0777) != 0 && errno != EEXIST)
		{
			error_set_print(PROGNAME_DAMON, -errno, "%s: %s", p,
					strerror(errno));
			ret = -1;
			break;
		}
		p[i] = '/';
	}
	free(p);
	return ret;
}


/* rrd_update */
int rrd_update(RRDType type, char const * rrdcached, char const * filename, int
		args_cnt, ...)
{
	int ret;
	va_list args;

	va_start(args, args_cnt);
	ret = rrd_updatev(type, rrdcached, filename, args_cnt, args);
	va_end(args);
	return ret;
}


/* rrd_updatev */
int rrd_updatev(RRDType type, char const * rrdcached, char const * filename,
		int args_cnt, va_list args)
{
	struct stat st;
	char * argv[] = { RRDTOOL, "update", NULL, NULL, NULL, NULL, NULL };
	size_t i = 3;
	struct timeval tv;
	size_t s;
	int pos;
	int arg;
	int ret;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%u, \"%s\")\n", __func__, type, filename);
#endif
	/* create the database if not available */
	if(stat(filename, &st) != 0)
	{
		if(errno != ENOENT)
			return _rrd_perror(filename, -errno);
		if(rrd_create(type, rrdcached, filename) != 0)
			return -1;
	}
	/* prepare the parameters */
	if(gettimeofday(&tv, NULL) != 0)
		return _rrd_perror("gettimeofday", -errno);
	if((argv[2] = string_new(filename)) == NULL)
		return 1;
	if(rrdcached != NULL)
	{
		argv[i++] = "--daemon";
		if((argv[i++] = string_new(rrdcached)) == NULL)
		{
			string_delete(argv[2]);
			return 1;
		}
	}
	s = (args_cnt + 1) * 12;
	if((argv[i] = malloc(s)) == NULL)
	{
		if(rrdcached != NULL)
			string_delete(argv[4]);
		string_delete(argv[2]);
		return _rrd_perror(NULL, -errno);
	}
	pos = snprintf(argv[i], s, "%ld", tv.tv_sec);
	for(arg = 0; arg < args_cnt; arg++)
		pos += snprintf(&argv[i][pos], s - pos, ":%"PRIu64,
				va_arg(args, uint64_t));
	/* update the database */
	ret = _rrd_exec(argv);
	free(argv[i]);
	if(rrdcached != NULL)
		string_delete(argv[4]);
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
		fprintf(stderr, "%s ", *(argv++));
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


/* rrd_timestamp */
static char * _rrd_timestamp(off_t offset)
{
	struct timeval tv;

	if(gettimeofday(&tv, NULL) != 0)
	{
		_rrd_perror("gettimeofday", -errno);
		return NULL;
	}
	return string_new_format("%ld", tv.tv_sec + offset);
}
