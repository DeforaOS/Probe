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



#include <unistd.h>
#include <stdio.h>
#include "damon.h"
#include "../config.h"

/* constants */
#ifndef PROGNAME
# define PROGNAME	"DaMon"
#endif


/* functions */
/* private */
/* prototypes */
static int _damon(char const * config);

static int _damon_usage(void);


/* public */
/* functions */
/* damon */
static int _damon(char const * config)
{
	DaMon * damon;
	Event * event;

	if((damon = damon_new(config)) == NULL)
		return 1;
	event = damon_get_event(damon);
	if(event_loop(event) != 0)
		error_print(PROGNAME);
	damon_delete(damon);
	return 1;
}


/* damon_usage */
static int _damon_usage(void)
{
	fputs("Usage: " PROGNAME " [-f filename]\n"
"  -f\tConfiguration file to load\n", stderr);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	int o;
	char const * config = NULL;

	while((o = getopt(argc, argv, "f:")) != -1)
		switch(o)
		{
			case 'f':
				config = optarg;
				break;
			default:
				return _damon_usage();
		}
	if(optind != argc)
		return _damon_usage();
	return (_damon(config) == 0) ? 0 : 2;
}
