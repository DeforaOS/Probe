/* $Id$ */
/* Copyright (c) 2005-2022 Pierre Pronchery <khorben@defora.org> */
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
/* FIXME: catch SIGPIPE, determine if we can avoid catching it if an AppServer
 * exits (eg avoid writing to a closed socket) */



#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <System.h>
#include <System/App.h>
#include "damon.h"
#include "../config.h"

/* constants */
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef SYSCONFDIR
# define SYSCONFDIR	PREFIX "/etc"
#endif
#ifndef PROGNAME_DAMON
# define PROGNAME_DAMON	"DaMon"
#endif


/* DaMon */
/* private */
/* types */
struct _DaMon
{
	String * prefix;
	String * rrdcached;
	unsigned int refresh;
	DaMonHost * hosts;
	unsigned int hosts_cnt;
	Event * event;
	bool event_delete;
};


/* constants */
#define DAMON_DEFAULT_REFRESH	60


/* prototypes */
static int _damon_init(DaMon * damon, char const * config, Event * event);
static void _damon_destroy(DaMon * damon);


/* functions */
/* public */
/* essential */
/* damon_new */
DaMon * damon_new(char const * config)
{
	DaMon * damon;
	Event * event;

	if((event = event_new()) == NULL)
		return NULL;
	if((damon = damon_new_event(config, event)) == NULL)
	{
		event_delete(event);
		return NULL;
	}
	damon->event_delete = true;
	return damon;
}


/* damon_new_event */
DaMon * damon_new_event(char const * config, Event * event)
{
	DaMon * damon;

	if((damon = object_new(sizeof(*damon))) == NULL)
		return NULL;
	if(_damon_init(damon, config, event) != 0)
	{
		object_delete(damon);
		return NULL;
	}
	return damon;
}


/* damon_delete */
void damon_delete(DaMon * damon)
{
	_damon_destroy(damon);
	object_delete(damon);
}


/* accessors */
/* damon_get_event */
Event * damon_get_event(DaMon * damon)
{
	return damon->event;
}

/* damon_get_host_by_id */
DaMonHost * damon_get_host_by_id(DaMon * damon, size_t id)
{
	if(damon->hosts_cnt >= id)
		return NULL;
	return &damon->hosts[id];
}


/* useful */
/* damon_error */
int damon_error(char const * message, int ret)
{
	return error_set_print(PROGNAME_DAMON, ret, "%s", message);
}


/* damon_perror */
int damon_perror(char const * message, int ret)
{
	return error_set_print(PROGNAME_DAMON, ret, "%s%s%s",
			(message != NULL) ? message : "",
			(message != NULL) ? ": " : "", strerror(errno));
}


/* damon_serror */
int damon_serror(void)
{
	return error_print(PROGNAME_DAMON);
}


/* damon_update */
int damon_update(DaMon * damon, RRDType type, char const * filename,
		int args_cnt, ...)
{
	int ret;
	char * path;
	va_list args;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%u, \"%s\", %d, ...) \"%s\"\n", __func__,
			type, filename, args_cnt, damon->prefix);
#endif
	if((path = string_new_append(damon->prefix, "/", filename, NULL))
			== NULL)
		return -1;
	va_start(args, args_cnt);
	ret = rrd_updatev(type, damon->rrdcached, path, args_cnt, args);
	va_end(args);
	string_delete(path);
	if(ret != 0)
		damon_serror();
	return ret;
}


/* private */
/* functions */
/* damon_init */
static int _init_config(DaMon * damon, char const * filename);
static int _init_config_hosts(DaMon * damon, Config * config,
		String const * hosts);
static int _init_config_hosts_host(DaMon * damon, Config * config, DaMonHost * host,
		String const * h, unsigned int pos);
static char ** _init_config_hosts_host_comma(char const * line);

static int _damon_init(DaMon * damon, char const * config, Event * event)
{
	struct timeval tv;

	if(_init_config(damon, config) != 0)
		return 1;
	damon->event = event;
	damon->event_delete = false;
	damon_refresh(damon);
	tv.tv_sec = damon->refresh;
	tv.tv_usec = 0;
	event_register_timeout(damon->event, &tv,
			(EventTimeoutFunc)damon_refresh, damon);
	return 0;
}

static int _init_config(DaMon * damon, char const * filename)
{
	Config * config;
	String const * p;
	char * q;
	int tmp;

	if((config = config_new()) == NULL)
		return 1;
	damon->prefix = NULL;
	damon->rrdcached = NULL;
	damon->refresh = DAMON_DEFAULT_REFRESH;
	damon->hosts = NULL;
	damon->hosts_cnt = 0;
	if(filename == NULL)
		filename = SYSCONFDIR "/" PROGNAME_DAMON ".conf";
	if(config_load(config, filename) != 0)
	{
		error_print(PROGNAME_DAMON);
		config_delete(config);
		return -1;
	}
	if((p = config_get(config, NULL, "prefix")) == NULL)
		p = ".";
	if((damon->prefix = string_new(p)) == NULL)
	{
		config_delete(config);
		return -1;
	}
	if((p = config_get(config, NULL, "rrdcached")) != NULL
			&& (damon->rrdcached = string_new(p)) == NULL)
	{
		string_delete(damon->prefix);
		config_delete(config);
		return -1;
	}
	if((p = config_get(config, NULL, "refresh")) != NULL)
	{
		tmp = strtol(p, &q, 10);
		damon->refresh = (*p == '\0' || *q != '\0' || tmp <= 0)
			? DAMON_DEFAULT_REFRESH : tmp;
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() refresh=%d\n", __func__,
				damon->refresh);
#endif
	}
	if((p = config_get(config, NULL, "hosts")) != NULL)
		_init_config_hosts(damon, config, p);
	config_delete(config);
	return 0;
}

static int _init_config_hosts(DaMon * damon, Config * config,
		String const * hosts)
{
	String const * h = hosts;
	unsigned int pos = 0;
	DaMonHost * p;

	while(h[0] != '\0')
	{
		if(h[pos] != '\0' && h[pos] != ',')
		{
			pos++;
			continue;
		}
		if(pos == 0)
		{
			h++;
			continue;
		}
		if((p = realloc(damon->hosts, sizeof(*p) * (damon->hosts_cnt
							+ 1))) == NULL)
			return damon_perror(NULL, -errno);
		damon->hosts = p;
		p = &damon->hosts[damon->hosts_cnt];
		if(_init_config_hosts_host(damon, config, p, h, pos) != 0)
			return -1;
		damon->hosts_cnt++;
		h += pos;
		pos = 0;
	}
	return 0;
}

static int _init_config_hosts_host(DaMon * damon, Config * config, DaMonHost * host,
		String const * h, unsigned int pos)
{
	String const * p;

	host->damon = damon;
	host->appclient = NULL;
	host->ifaces = NULL;
	host->vols = NULL;
	if((host->hostname = string_new_length(h, pos)) == NULL)
		return damon_perror(NULL, -errno);
#ifdef DEBUG
	fprintf(stderr, "config: Host %s\n", host->hostname);
#endif
	if((p = config_get(config, host->hostname, "interfaces")) != NULL)
		host->ifaces = _init_config_hosts_host_comma(p);
	if((p = config_get(config, host->hostname, "volumes")) != NULL)
		host->vols = _init_config_hosts_host_comma(p);
	return 0;
}

static char ** _init_config_hosts_host_comma(char const * line)
{
	char const * l = line;
	unsigned int pos = 0;
	char ** values = NULL;
	char ** p;
	unsigned int cnt = 0;

	while(l[0] != '\0')
	{
		if(l[pos] != '\0' && l[pos] != ',')
		{
			pos++;
			continue;
		}
		if(pos == 0)
		{
			l++;
			continue;
		}
		if((p = realloc(values, sizeof(char *) * (cnt + 2))) == NULL)
			break;
		values = p;
		if((values[cnt] = malloc(pos + 1)) != NULL)
		{
			strncpy(values[cnt], l, pos);
			values[cnt][pos] = '\0';
#ifdef DEBUG
			fprintf(stderr, "config: %s\n", values[cnt]);
#endif
		}
		values[++cnt] = NULL;
		if(values[cnt - 1] == NULL)
			break;
		l += pos;
		pos = 0;
	}
	if(l[0] == '\0')
		return values;
	if(values == NULL)
		return NULL;
	for(p = values; *p != NULL; p++)
		free(*p);
	free(values);
	return NULL;
}


/* damon_destroy */
static void _destroy_host(DaMonHost * host);

static void _damon_destroy(DaMon * damon)
{
	unsigned int i;

	for(i = 0; i < damon->hosts_cnt; i++)
		_destroy_host(&damon->hosts[i]);
	if(damon->event_delete)
		event_delete(damon->event);
	free(damon->hosts);
	string_delete(damon->rrdcached);
	string_delete(damon->prefix);
}

static void _destroy_host(DaMonHost * host)
{
	string_delete(host->hostname);
	if(host->appclient != NULL)
		appclient_delete(host->appclient);
}
