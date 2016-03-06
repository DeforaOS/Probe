#!/bin/sh
#$Id$
#Copyright (c) 2016 Pierre Pronchery <khorben@defora.org>
#This file is part of DeforaOS Network Probe
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, version 3 of the License.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.



#variables
NEWPATH="new"
OLDPATH1="old1"
OLDPATH2="old2"
OUTPATH="out"
#executables
CP="cp -f"
GREP="grep"
RRDTOOL="rrdtool"
RRDTOOL_ARGS="--daemon unix:/var/run/rrdcached/rrdcached.sock"


#functions
#backup
backup()
{
	$RRDTOOL fetch "$1" AVERAGE -s -604800 $RRDTOOL_ARGS
}


#restore1
restore1()
{
	while read timestamp val1 junk; do 
		$RRDTOOL update "$1" "$timestamp$val1" $RRDTOOL_ARGS
	done
}


#restore3
restore3()
{
	while read timestamp val1 val2 val3 junk; do 
		$RRDTOOL update "$1" "$timestamp$val1:$val2:$val3" $RRDTOOL_ARGS
	done
}


#main
rrd="load.rrd"
$CP -- "$NEWPATH/$rrd" "$OUTPATH/$rrd" || exit 2
backup "$OLDPATH1/$rrd" | $GREP -v ': nan nan nan$' | restore3 "$OUTPATH/$rrd"
backup "$OLDPATH2/$rrd" | $GREP -v ': nan nan nan$' | restore3 "$OUTPATH/$rrd"

rrd="procs.rrd"
$CP -- "$NEWPATH/$rrd" "$OUTPATH/$rrd" || exit 2
backup "$OLDPATH1/$rrd" | $GREP -v ': nan$' | restore1 "$OUTPATH/$rrd"
backup "$OLDPATH2/$rrd" | $GREP -v ': nan$' | restore1 "$OUTPATH/$rrd"

rrd="users.rrd"
$CP -- "$NEWPATH/$rrd" "$OUTPATH/$rrd" || exit 2
backup "$OLDPATH1/$rrd" | $GREP -v ': nan$' | restore1 "$OUTPATH/$rrd"
backup "$OLDPATH2/$rrd" | $GREP -v ': nan$' | restore1 "$OUTPATH/$rrd"
