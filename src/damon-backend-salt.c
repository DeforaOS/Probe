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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <System.h>
#include "damon.h"

/* constants */
#ifndef SALT
# define SALT		"salt"
#endif


/* damon_refresh */
typedef enum _SaltFunction
{
	SF_STATUS_ALL_STATUS = 0
} SaltFunction;
#define SF_LAST SF_STATUS_ALL_STATUS
#define SF_COUNT (SF_LAST + 1)

static int _refresh_salt(DaMon * damon,
		int (*callback)(DaMon * damon, json_t * json),
		SaltFunction function, ...);
static int _refresh_status(DaMon * damon);
static int _refresh_status_parse(DaMon * damon, json_t * json);
static int _refresh_status_parse_diskusage(DaMon * damon, char const * hostname,
		json_t * json);
static int _refresh_status_parse_diskusage_volume(DaMon * damon,
		char const * hostname, char const * volume, json_t * json);
static int _refresh_status_parse_loadavg(DaMon * damon, char const * hostname,
		json_t * json);
static int _refresh_status_parse_procs(DaMon * damon, char const * hostname,
		json_t * json);
static int _refresh_status_parse_w(DaMon * damon, char const * hostname,
		json_t * json);

int damon_refresh(DaMon * damon)
{
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	_refresh_status(damon);
	return 0;
}

static int _refresh_salt(DaMon * damon,
		int (*callback)(DaMon * damon, json_t * json),
		SaltFunction function, ...)
{
	char * functions[SF_COUNT] = {
		SALT " --out=json '*' status.all_status" };
	FILE * fp;
	const size_t flags = JSON_DISABLE_EOF_CHECK;
	json_t * json;
	json_error_t error;
	int res;

	if((fp = popen(functions[function], "rb")) == NULL)
		return damon_perror("popen", -errno);
	while(!feof(fp) && (json = json_loadf(fp, flags, &error)) != NULL)
	{
#if 0
		json_dump_file(json, "/dev/stdout", 0);
		fprintf(stderr, "DEBUG: %d\n", json_typeof(json));
#else
		callback(damon, json);
		json_decref(json);
#endif
	}
#ifdef DEBUG
	if(json == NULL)
		fprintf(stderr, "DEBUG: %s\n", error.text);
#endif
	if((res = pclose(fp)) == 127)
		return damon_error(SALT ": Could not execute", res);
	else if(res != 0)
		return damon_error(SALT ": An error occured", res);
	return 0;
}

static int _refresh_status(DaMon * damon)
{
	_refresh_salt(damon, _refresh_status_parse, SF_STATUS_ALL_STATUS);
	return 0;
}

static int _refresh_status_parse(DaMon * damon, json_t * json)
{
	char const * hostname;
	json_t * what;
	char const * status;
	json_t * value;

	if(!json_is_object(json))
		return -1;
	json_object_foreach(json, hostname, what)
	{
#ifdef DEBUG
		fprintf(stderr, "DEBUG: hostname: %s\n", hostname);
#endif
		/* XXX report errors */
		if(string_find(hostname, "/") != NULL)
			return -1;
		if(!json_is_object(what))
			return -1;
		json_object_foreach(what, status, value)
			if(strcmp("diskusage", status) == 0)
				_refresh_status_parse_diskusage(damon, hostname,
						value);
			else if(strcmp("loadavg", status) == 0)
				_refresh_status_parse_loadavg(damon, hostname,
						value);
			else if(strcmp("procs", status) == 0)
				_refresh_status_parse_procs(damon, hostname,
						value);
			else if(strcmp("w", status) == 0)
				_refresh_status_parse_w(damon, hostname, value);
	}
	return 0;
}

static int _refresh_status_parse_diskusage(DaMon * damon, char const * hostname,
		json_t * json)
{
	int ret = 0;
	char const * volume;
	json_t * value;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %d)\n", __func__, hostname,
			json_typeof(json));
#endif
	if(!json_is_object(json))
		return -1;
	json_object_foreach(json, volume, value)
	{
		if(string_find(volume, "/..") != NULL)
			/* XXX report */
			continue;
		ret |= _refresh_status_parse_diskusage_volume(damon, hostname,
				volume, value);
	}
	return ret;
}

static int _refresh_status_parse_diskusage_volume(DaMon * damon,
		char const * hostname, char const * volume, json_t * json)
{
	int ret;
	uint64_t usage[2] = { 0, 0 };
	char const * key;
	json_t * value;
	char * rrd;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", %d)\n", __func__, hostname,
			volume, json_typeof(json));
#endif
	if(!json_is_object(json))
		return -1;
	json_object_foreach(json, key, value)
		if(strcmp(key, "available") == 0)
			usage[0] = json_integer_value(value);
		else if(strcmp(key, "total") == 0)
			usage[1] = json_integer_value(value);
	if(usage[0] == 0 && usage[1] == 0)
		/* ignore empty volumes */
		return 0;
	if(usage[0] > usage[1])
		/* invalid or partial input */
		return -1;
	/* graph the volume used instead */
	usage[0] = usage[1] - usage[0];
	if((rrd = string_new_append(hostname, "/volume",
					(strcmp(volume, "/") == 0)
					? "" : volume,
					".rrd", NULL)) == NULL)
		return -1;
	ret = damon_update(damon, RRDTYPE_VOLUME, rrd, 2, usage[0], usage[1]);
	string_delete(rrd);
	return ret;
}

static int _refresh_status_parse_loadavg(DaMon * damon, char const * hostname,
		json_t * json)
{
	int ret;
	uint64_t load[3] = { 0, 0, 0 };
	char const * key;
	json_t * value;
	char * rrd;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %d)\n", __func__, hostname,
			json_typeof(json));
#endif
	if(!json_is_object(json))
		return -1;
	json_object_foreach(json, key, value)
		if(strcmp(key, "1-min") == 0)
			load[0] = json_real_value(value) * 1000;
		else if(strcmp(key, "5-min") == 0)
			load[1] = json_real_value(value) * 1000;
		else if(strcmp(key, "15-min") == 0)
			load[2] = json_real_value(value) * 1000;
	if((rrd = string_new_append(hostname, "/load.rrd", NULL)) == NULL)
		return -1;
	ret = damon_update(damon, RRDTYPE_LOAD, rrd,
			3, load[0], load[1], load[2]);
	string_delete(rrd);
	return ret;
}

static int _refresh_status_parse_procs(DaMon * damon, char const * hostname,
		json_t * json)
{
	int ret;
	uint64_t count = 0;
	size_t index;
	json_t * value;
	char * rrd;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %d)\n", __func__, hostname,
			json_typeof(json));
#endif
	if(!json_is_array(json))
		return -1;
	json_array_foreach(json, index, value)
		count++;
	if((rrd = string_new_append(hostname, "/procs.rrd", NULL)) == NULL)
		return -1;
	ret = damon_update(damon, RRDTYPE_PROCS, rrd, 1, count);
	string_delete(rrd);
	return ret;
}

static int _refresh_status_parse_w(DaMon * damon, char const * hostname,
		json_t * json)
{
	int ret;
	uint64_t count = 0;
	size_t index;
	json_t * value;
	char * rrd;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", %d)\n", __func__, hostname,
			json_typeof(json));
#endif
	if(!json_is_array(json))
		return -1;
	json_array_foreach(json, index, value)
		count++;
	if((rrd = string_new_append(hostname, "/users.rrd", NULL)) == NULL)
		return -1;
	ret = damon_update(damon, RRDTYPE_USERS, rrd, 1, count);
	string_delete(rrd);
	return ret;
}
