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



#include <stdlib.h>
#include <stdio.h>
#include "rrd.h"
#include "damon.h"
#include "../config.h"

/* constants */
#ifndef APPSERVER_PROBE_NAME
# define APPSERVER_PROBE_NAME	PACKAGE
#endif
#ifndef PROGNAME_DAMON
# define PROGNAME_DAMON		"DaMon"
#endif

#define DAMON_SEP		'/'


/* functions */
/* damon_refresh */
static AppClient * _refresh_connect(DaMonHost * host, Event * event);
static int _refresh_uptime(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_load(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_ram(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_swap(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_procs(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_users(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_ifaces(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_ifaces_if(AppClient * ac, DaMonHost * host, char * rrd,
		char const * iface);
static int _refresh_vols(AppClient * ac, DaMonHost * host, char * rrd);
static int _refresh_vols_vol(AppClient * ac, DaMonHost * host, char * rrd,
		char * vol);

int damon_refresh(DaMon * damon)
{
	size_t i;
	AppClient * ac = NULL;
	char * rrd = NULL;
	char * p;
	DaMonHost * host;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s()\n", __func__);
#endif
	for(i = 0; (host = damon_get_host_by_id(damon, i)) != NULL; i++)
	{
		if((ac = host->appclient) == NULL)
			if((ac = _refresh_connect(host, damon_get_event(damon)))
					== NULL)
				continue;
		if((p = realloc(rrd, string_get_length(host->hostname) + 12))
				== NULL) /* XXX avoid this constant */
			break;
		rrd = p;
		if(_refresh_uptime(ac, host, rrd) != 0
				|| _refresh_load(ac, host, rrd) != 0
				|| _refresh_ram(ac, host, rrd) != 0
				|| _refresh_swap(ac, host, rrd) != 0
				|| _refresh_procs(ac, host, rrd) != 0
				|| _refresh_users(ac, host, rrd) != 0
				|| _refresh_ifaces(ac, host, rrd) != 0
				|| _refresh_vols(ac, host, rrd) != 0)
		{
			appclient_delete(ac);
			host->appclient = NULL;
			continue;
		}
		ac = NULL;
	}
	free(rrd);
	if(ac != NULL)
		error_set_print(PROGNAME_DAMON, 1, "%s",
				"refresh: An error occured");
	return 0;
}

static AppClient * _refresh_connect(DaMonHost * host, Event * event)
{
	if(setenv("APPSERVER_Probe", host->hostname, 1) != 0)
		return NULL;
	if((host->appclient = appclient_new_event(NULL, APPSERVER_PROBE_NAME,
					NULL, event)) == NULL)
		error_print(PROGNAME_DAMON);
	return host->appclient;
}

static int _refresh_uptime(AppClient * ac, DaMonHost * host, char * rrd)
{
	int32_t ret;

	if(appclient_call(ac, (void **)&ret, "uptime") != 0)
		return error_print(PROGNAME_DAMON);
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "uptime.rrd");
	damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 1, ret);
	return 0;
}

static int _refresh_load(AppClient * ac, DaMonHost * host, char * rrd)
{
	int32_t res;
	uint32_t load[3];

	if(appclient_call(ac, (void **)&res, "load", &load[0], &load[1],
				&load[2]) != 0)
		return error_print(PROGNAME_DAMON);
	if(res != 0)
		return 0;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "load.rrd");
	damon_update(host->damon, RRDTYPE_LOAD, rrd, 3,
			load[0], load[1], load[2]);
	return 0;
}

static int _refresh_procs(AppClient * ac, DaMonHost * host, char * rrd)
{
	int32_t res;

	if(appclient_call(ac, (void **)&res, "procs") != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "procs.rrd");
	damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 1, res);
	return 0;
}

static int _refresh_ram(AppClient * ac, DaMonHost * host, char * rrd)
{
	int32_t res;
	uint32_t ram[4];

	if(appclient_call(ac, (void **)&res, "ram", &ram[0], &ram[1], &ram[2],
				&ram[3]) != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "ram.rrd");
	damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 4,
			ram[0], ram[1], ram[2], ram[3]);
	return 0;
}

static int _refresh_swap(AppClient * ac, DaMonHost * host, char * rrd)
{
	int32_t res;
	uint32_t swap[2];

	if(appclient_call(ac, (void **)&res, "swap", &swap[0], &swap[1]) != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "swap.rrd");
	damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 2, swap[0], swap[1]);
	return 0;
}

static int _refresh_users(AppClient * ac, DaMonHost * host, char * rrd)
{
	int32_t res;

	if(appclient_call(ac, (void **)&res, "users") != 0)
		return 1;
	sprintf(rrd, "%s%c%s", host->hostname, DAMON_SEP, "users.rrd");
	damon_update(host->damon, RRDTYPE_USERS, rrd, 1, res);
	return 0;
}

static int _refresh_ifaces(AppClient * ac, DaMonHost * host, char * rrd)
{
	char ** p = host->ifaces;
	int ret = 0;

	if(p == NULL)
		return 0;
	for(; *p != NULL; p++)
		ret |= _refresh_ifaces_if(ac, host, rrd, *p);
	return ret;
}

static int _refresh_ifaces_if(AppClient * ac, DaMonHost * host, char * rrd,
		char const * iface)
{
	int32_t res[2];

	if(appclient_call(ac, (void **)&res[0], "ifrxbytes", iface) != 0
			|| appclient_call(ac, (void **)&res[1], "iftxbytes",
				iface) != 0)
		return 1;
	sprintf(rrd, "%s%c%s%s", host->hostname, DAMON_SEP, iface, ".rrd");
	damon_update(host->damon, RRDTYPE_UNKNOWN, rrd, 2, res[0], res[1]);
	return 0;
}

static int _refresh_vols(AppClient * ac, DaMonHost * host, char * rrd)
{
	char ** p = host->vols;
	int ret = 0;

	if(p == NULL)
		return 0;
	for(; *p != NULL; p++)
		ret |= _refresh_vols_vol(ac, host, rrd, *p);
	return ret;
}

static int _refresh_vols_vol(AppClient * ac, DaMonHost * host, char * rrd,
		char * vol)
{
	int32_t res[2];

	if(appclient_call(ac, (void **)&res[0], "voltotal", vol) != 0
			|| appclient_call(ac, (void **)&res[1], "volfree", vol)
			!= 0)
		return 1;
	sprintf(rrd, "%s%s%s", host->hostname, vol, ".rrd"); /* FIXME */
	damon_update(host->damon, RRDTYPE_VOLUME, rrd, 2, res[0], res[1]);
	return 0;
}
