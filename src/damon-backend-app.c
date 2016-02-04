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
		error_set_print(PROGNAME, 1, "%s", "refresh: An error occured");
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
