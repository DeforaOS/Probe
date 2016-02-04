/* $Id$ */
/* Copyright (c) 2005-2016 Pierre Pronchery <khorben@defora.org> */
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
#include "rrd.h"
#include "damon.h"
#include "../config.h"

/* constants */
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef SYSCONFDIR
# define SYSCONFDIR	PREFIX "/etc"
#endif
#ifndef PROGNAME
# define PROGNAME	"DaMon"
#endif


/* DaMon */
/* private */
/* types */
typedef struct _Host
{
	DaMon * damon;
	AppClient * appclient;
	char * hostname;
	char ** ifaces;
	char ** vols;
} Host;

struct _DaMon
{
	char const * prefix;
	unsigned int refresh;
	Host * hosts;
	unsigned int hosts_cnt;
	Event * event;
};


/* constants */
#ifdef DEBUG
# define DAMON_DEFAULT_REFRESH	10
#else
# define DAMON_DEFAULT_REFRESH	300
#endif
#define DAMON_SEP		'/'


/* prototypes */
static int _damon_init(DaMon * damon, char const * config, Event * event);
static void _damon_destroy(DaMon * damon);

static int _damon_perror(char const * message, int error);

static int _damon_update(DaMon * damon, RRDType type, char const * filename,
		int args_cnt, ...);


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
	/* FIXME let event be deleted eventually */
	if((damon = damon_new_event(config, event)) == NULL)
	{
		event_delete(event);
		return NULL;
	}
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


/* useful */
/* damon_refresh */
static AppClient * _refresh_connect(Host * host, Event * event);
static int _refresh_uptime(AppClient * ac, Host * host, char * rrd);
static int _refresh_load(AppClient * ac, Host * host, char * rrd);
static int _refresh_ram(AppClient * ac, Host * host, char * rrd);
static int _refresh_swap(AppClient * ac, Host * host, char * rrd);
static int _refresh_procs(AppClient * ac, Host * host, char * rrd);
static int _refresh_users(AppClient * ac, Host * host, char * rrd);
static int _refresh_ifaces(AppClient * ac, Host * host, char * rrd);
static int _refresh_ifaces_if(AppClient * ac, Host * host, char * rrd,
		char const * iface);
static int _refresh_vols(AppClient * ac, Host * host, char * rrd);
static int _refresh_vols_vol(AppClient * ac, Host * host, char * rrd,
		char * vol);

int damon_refresh(DaMon * damon)
{
	unsigned int i;
	AppClient * ac = NULL;
	char * rrd = NULL;
	char * p;
	Host * hosts = damon->hosts;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	for(i = 0; i < damon->hosts_cnt; i++)
	{
		if((ac = hosts[i].appclient) == NULL)
			if((ac = _refresh_connect(&hosts[i], damon->event))
					== NULL)
				continue;
		if((p = realloc(rrd, string_length(hosts[i].hostname) + 12))
				== NULL) /* XXX avoid this constant */
			break;
		rrd = p;
		if(_refresh_uptime(ac, &hosts[i], rrd) != 0
				|| _refresh_load(ac, &hosts[i], rrd) != 0
				|| _refresh_ram(ac, &hosts[i], rrd) != 0
				|| _refresh_swap(ac, &hosts[i], rrd) != 0
				|| _refresh_procs(ac, &hosts[i], rrd) != 0
				|| _refresh_users(ac, &hosts[i], rrd) != 0
				|| _refresh_ifaces(ac, &hosts[i], rrd) != 0
				|| _refresh_vols(ac, &hosts[i], rrd) != 0)
		{
			appclient_delete(ac);
			hosts[i].appclient = NULL;
			continue;
		}
		ac = NULL;
	}
	free(rrd);
	if(ac != NULL)
		fprintf(stderr, "%s: %s\n", PROGNAME,
				"refresh: An error occured");
	return 0;
}

static AppClient * _refresh_connect(Host * host, Event * event)
{
	if(setenv("APPSERVER_Probe", host->hostname, 1) != 0)
		return NULL;
	if((host->appclient = appclient_new_event(NULL, "Probe", NULL, event))
			== NULL)
		error_print(PROGNAME);
	return host->appclient;
}

static int _refresh_uptime(AppClient * ac, Host * host, char * rrd)
{
	int32_t ret;

	if(appclient_call(ac, (void **)&ret, "uptime") != 0)
		return error_print(PROGNAME);
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "uptime.rrd");
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 1, ret);
	return 0;
}

static int _refresh_load(AppClient * ac, Host * host, char * rrd)
{
	int32_t res;
	uint32_t load[3];

	if(appclient_call(ac, (void **)&res, "load", &load[0], &load[1],
				&load[2]) != 0)
		return error_print(PROGNAME);
	if(res != 0)
		return 0;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "load.rrd");
	_damon_update(host->damon, RRDTYPE_LOAD, rrd, 3,
			load[0], load[1], load[2]);
	return 0;
}

static int _refresh_procs(AppClient * ac, Host * host, char * rrd)
{
	int32_t res;

	if(appclient_call(ac, (void **)&res, "procs") != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "procs.rrd");
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 1, res);
	return 0;
}

static int _refresh_ram(AppClient * ac, Host * host, char * rrd)
{
	int32_t res;
	uint32_t ram[4];

	if(appclient_call(ac, (void **)&res, "ram", &ram[0], &ram[1], &ram[2],
				&ram[3]) != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "ram.rrd");
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 4,
			ram[0], ram[1], ram[2], ram[3]);
	return 0;
}

static int _refresh_swap(AppClient * ac, Host * host, char * rrd)
{
	int32_t res;
	uint32_t swap[2];

	if(appclient_call(ac, (void **)&res, "swap", &swap[0], &swap[1]) != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "swap.rrd");
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 2, swap[0], swap[1]);
	return 0;
}

static int _refresh_users(AppClient * ac, Host * host, char * rrd)
{
	int32_t res;

	if(appclient_call(ac, (void **)&res, "users") != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "users.rrd");
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 1, res);
	return 0;
}

static int _refresh_ifaces(AppClient * ac, Host * host, char * rrd)
{
	char ** p = host->ifaces;
	int ret = 0;

	if(p == NULL)
		return 0;
	for(; *p != NULL; p++)
		ret |= _refresh_ifaces_if(ac, host, rrd, *p);
	return ret;
}

static int _refresh_ifaces_if(AppClient * ac, Host * host, char * rrd,
		char const * iface)
{
	int32_t res[2];

	if(appclient_call(ac, (void **)&res[0], "ifrxbytes", iface) != 0
			|| appclient_call(ac, (void **)&res[1], "iftxbytes",
				iface) != 0)
		return 1;
	sprintf(rrd, "%s%c%s%s", host->hostname, DAMON_SEP, iface, ".rrd");
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 2, res[0], res[1]);
	return 0;
}

static int _refresh_vols(AppClient * ac, Host * host, char * rrd)
{
	char ** p = host->vols;
	int ret = 0;

	if(p == NULL)
		return 0;
	for(; *p != NULL; p++)
		ret |= _refresh_vols_vol(ac, host, rrd, *p);
	return ret;
}

static int _refresh_vols_vol(AppClient * ac, Host * host, char * rrd,
		char * vol)
{
	int32_t res[2];

	if(appclient_call(ac, (void **)&res[0], "voltotal", vol) != 0
			|| appclient_call(ac, (void **)&res[1], "volfree", vol)
			!= 0)
		return 1;
	sprintf(rrd, "%s%s%s", host->hostname, vol, ".rrd"); /* FIXME */
	_damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 2, res[0], res[1]);
	return 0;
}


/* private */
/* functions */
/* damon_init */
static int _init_config(DaMon * damon, char const * filename);
static int _init_config_hosts(DaMon * damon, Config * config,
		char const * hosts);
static int _init_config_hosts_host(DaMon * damon, Config * config, Host * host,
		char const * h, unsigned int pos);
static char ** _init_config_hosts_host_comma(char const * line);

static int _damon_init(DaMon * damon, char const * config, Event * event)
{
	struct timeval tv;

	if(_init_config(damon, config) != 0)
		return 1;
	damon->event = event;
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
	char const * p;
	char * q;
	int tmp;

	if((config = config_new()) == NULL)
		return 1;
	damon->prefix = NULL;
	damon->refresh = DAMON_DEFAULT_REFRESH;
	damon->hosts = NULL;
	damon->hosts_cnt = 0;
	if(filename == NULL)
		filename = SYSCONFDIR "/" PROGNAME ".conf";
	if(config_load(config, filename) != 0)
	{
		error_print(PROGNAME);
		config_delete(config);
		return -1;
	}
	if((damon->prefix = config_get(config, "", "prefix")) == NULL)
		damon->prefix = ".";
	if((damon->prefix = strdup(damon->prefix)) == NULL)
	{
		config_delete(config);
		return -1;
	}
	if((p = config_get(config, "", "refresh")) != NULL)
	{
		tmp = strtol(p, &q, 10);
		damon->refresh = (*p == '\0' || *q != '\0' || tmp <= 0)
			? DAMON_DEFAULT_REFRESH : tmp;
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s() refresh=%d\n", __func__,
				damon->refresh);
#endif
	}
	if((p = config_get(config, "", "hosts")) != NULL)
		_init_config_hosts(damon, config, p);
	config_delete(config);
	return 0;
}

static int _init_config_hosts(DaMon * damon, Config * config,
		char const * hosts)
{
	char const * h = hosts;
	unsigned int pos = 0;
	Host * p;

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
			return _damon_perror(NULL, -errno);
		damon->hosts = p;
		p = &damon->hosts[damon->hosts_cnt++];
		if(_init_config_hosts_host(damon, config, p, h, pos) != 0)
			return -1;
		h += pos;
		pos = 0;
	}
	return 0;
}

static int _init_config_hosts_host(DaMon * damon, Config * config, Host * host,
		char const * h, unsigned int pos)
{
	char const * p;

	host->damon = damon;
	host->appclient = NULL;
	host->ifaces = NULL;
	host->vols = NULL;
	if((host->hostname = malloc(pos + 1)) == NULL)
		return _damon_perror(NULL, -errno);
	strncpy(host->hostname, h, pos);
	host->hostname[pos] = '\0';
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
		if((p = realloc(values, sizeof(char*) * (cnt + 2))) == NULL)
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
static void _damon_destroy(DaMon * damon)
{
	unsigned int i;

	for(i = 0; i < damon->hosts_cnt; i++)
	{
		free(damon->hosts[i].hostname);
		if(damon->hosts[i].appclient != NULL)
			appclient_delete(damon->hosts[i].appclient);
	}
	event_delete(damon->event);
	free(damon->hosts);
}


/* useful */
/* damon_perror */
static int _damon_perror(char const * message, int ret)
{
	return error_set_print(PROGNAME, ret, "%s%s%s",
			(message != NULL) ? message : "",
			(message != NULL) ? ": " : "", strerror(errno));
}


/* damon_update */
static int _damon_update(DaMon * damon, RRDType type, char const * filename,
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
	ret = rrd_updatev(type, path, args_cnt, args);
	va_end(args);
	string_delete(path);
	return ret;
}
