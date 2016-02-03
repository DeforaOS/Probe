#/bin/sh
#$Id$
#Copyright (c) 2010-2016 Pierre Pronchery <khorben@defora.org>
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
PROGNAME="create.sh"
#executables
DATE="date"
MKDIR="mkdir -p"
RRDTOOL="rrdtool"


#functions
#create
create()
{
	ret=0
	i="$1"

	$MKDIR -- "$i"						|| ret=3

	$RRDTOOL create "$i/uptime.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:uptime:GAUGE:120:0:U \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	$RRDTOOL create "$i/load.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:load1:GAUGE:30:0:U \
		DS:load5:GAUGE:30:0:U \
		DS:load15:GAUGE:30:0:U \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	$RRDTOOL create "$i/ram.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:ramtotal:GAUGE:30:0:U \
		DS:ramfree:GAUGE:30:0:U \
		DS:ramshared:GAUGE:30:0:U \
		DS:rambuffer:GAUGE:30:0:U \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	$RRDTOOL create "$i/swap.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:swaptotal:GAUGE:30:0:U \
		DS:swapfree:GAUGE:30:0:U \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	$RRDTOOL create "$i/users.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:users:GAUGE:30:0:65536 \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	$RRDTOOL create "$i/procs.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:procs:GAUGE:30:0:65536 \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	$RRDTOOL create "$i/eth0.rrd" --start "$($DATE +%s)" \
		--step 10 \
		DS:ifrxbytes:COUNTER:30:0:U \
		DS:iftxbytes:COUNTER:30:0:U \
		RRA:AVERAGE:0.5:1:360 \
		RRA:AVERAGE:0.5:24:360 \
		RRA:AVERAGE:0.5:168:360				|| ret=3

	return $ret
}


#usage
usage()
{
	echo "Usage: $PROGNAME host..." 1>&2
	return 1;
}


#main
if [ $# -lt 1 ]; then
	usage
	exit $?
fi
ret=0
for i in "$@"; do
	if ! create "$i"; then
		echo "$PROGNAME: $i: Could not create one or more database" 1>&2
		ret=2
	fi
done
exit $ret
