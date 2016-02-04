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



#ifndef DAMON_DAMON_H
# define DAMON_DAMON_H

# include <System.h>
# include "rrd.h"


/* DaMon */
/* types */
typedef struct _DaMon DaMon;


/* functions */
DaMon * damon_new(char const * config);
DaMon * damon_new_event(char const * config, Event * event);
void damon_delete(DaMon * damon);

/* accessors */
Event * damon_get_event(DaMon * damon);

/* useful */
int damon_error(char const * message, int error);
int damon_perror(char const * message, int error);

int damon_refresh(DaMon * damon);
int damon_update(DaMon * damon, RRDType type, char const * filename,
		int args_cnt, ...);

#endif /* !DAMON_DAMON_H */
