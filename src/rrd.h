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



#ifndef DAMON_RRD_H
# define DAMON_RRD_H

# include <stdarg.h>


/* RRD */
/* types */
typedef enum _RRDType
{
	RRDTYPE_UNKNOWN = 0,
	RRDTYPE_LOAD,
	RRDTYPE_VOLUME
} RRDType;


/* functions */
int rrd_create(RRDType type, char const * filename);

int rrd_update(RRDType type, char const * filename, int args_cnt, ...);
int rrd_updatev(RRDType type, char const * filename,
		int args_cnt, va_list args);

#endif /* !DAMON_RRD_H */
