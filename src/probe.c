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
/* TODO:
 * - check ifinfo code (and thus volinfo)
 * - free memory allocated
 * - kvm_getprocs() may be more portable for *BSD */



#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <System.h>
#include <System/App.h>
#include "../data/Probe.h"
#include "../config.h"

#ifndef APPSERVER_PROBE_NAME
# define APPSERVER_PROBE_NAME	PACKAGE
#endif
#ifndef PROGNAME_PROBE
# define PROGNAME_PROBE		PACKAGE
#endif


#if defined(__linux__)
# define _sysinfo_linux			_sysinfo
# define _userinfo_utmpx		_userinfo
# define _ifinfo_linux			_ifinfo
# define _volinfo_mtab			_volinfo
#elif defined(__FreeBSD__)
# define _sysinfo_generic		_sysinfo
# define _userinfo_utmpx		_userinfo
# define _ifinfo_bsd			_ifinfo
# define _volinfo_statfs		_volinfo
#elif defined(__NetBSD__)
# define _sysinfo_generic		_sysinfo
# define _userinfo_utmpx		_userinfo
# define _ifinfo_bsd			_ifinfo
# define _volinfo_statvfs		_volinfo
#else
# define _sysinfo_generic		_sysinfo
# define _userinfo_utmpx		_userinfo
# define _ifinfo_generic		_ifinfo
# define _volinfo_generic		_volinfo
#endif

#define PROBE_REFRESH 10


/* functions */
static int _probe_error(int ret);
static int _probe_perror(char const * message, int ret);


/* sysinfo */
#if defined(_sysinfo_linux)
# include <sys/sysinfo.h>
# define _sysinfo sysinfo
#endif /* defined(_sysinfo_linux) */

#if defined(_sysinfo_generic)
# if defined(__FreeBSD__)
#  define _sysinfo_uptime_generic	_sysinfo_uptime
#  define _sysinfo_loads_generic	_sysinfo_loads
#  define _sysinfo_ram_generic		_sysinfo_ram
#  define _sysinfo_procs_generic	_sysinfo_procs
# elif defined(__NetBSD__)
#  define _sysinfo_uptime_sysctl	_sysinfo_uptime
#  define _sysinfo_loads_sysctl		_sysinfo_loads
#  define _sysinfo_ram_sysctl_uvm	_sysinfo_ram
#  define _sysinfo_procs_sysctl		_sysinfo_procs
# else
#  define _sysinfo_uptime_generic	_sysinfo_uptime
#  define _sysinfo_loads_generic	_sysinfo_loads
#  define _sysinfo_ram_generic		_sysinfo_ram
#  define _sysinfo_procs_generic	_sysinfo_procs
# endif
struct sysinfo
{
	long uptime;
	unsigned long loads[3];
	unsigned long totalram;
	unsigned long freeram;
	unsigned long sharedram;
	unsigned long bufferram;
	unsigned long totalswap;
	unsigned long freeswap;
	unsigned short procs;
};

static int _sysinfo_uptime(struct sysinfo * info);
static int _sysinfo_loads(struct sysinfo * info);
static int _sysinfo_ram(struct sysinfo * info);
static int _sysinfo_procs(struct sysinfo * info);
static int _sysinfo_generic(struct sysinfo * info)
{
	int ret = 0;

	ret |= _sysinfo_uptime(info);
	ret |= _sysinfo_loads(info);
	ret |= _sysinfo_ram(info);
	ret |= _sysinfo_procs(info);
	return ret;
}

/* sysinfo sysctl */
# if defined(_sysinfo_uptime_sysctl) \
	|| defined(_sysinfo_loads_sysctl) \
	|| defined(_sysinfo_ram_sysctl_uvm) \
	|| defined(_sysinfo_procs_sysctl)
#  include <sys/param.h>
#  include <sys/sysctl.h>
# endif
# if defined(_sysinfo_uptime_sysctl)
static int _sysinfo_uptime_sysctl(struct sysinfo * info)
{
	int mib[2];
	struct timeval now;
	struct timeval tv;
	size_t len = sizeof(tv);

	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	if(gettimeofday(&now, NULL) != 0
			|| sysctl(mib, 2, &tv, &len, NULL, 0) == -1)
	{
		info->uptime = 0;
		return 1;
	}
	info->uptime = now.tv_sec - tv.tv_sec;
	return 0;
}
# endif

# if defined(_sysinfo_loads_sysctl)
#  include <sys/resource.h>
static int _sysinfo_loads_sysctl(struct sysinfo * info)
{
	int mib[2];
	struct loadavg la;
	size_t len = sizeof(la);

	mib[0] = CTL_VM;
	mib[1] = VM_LOADAVG;
	if(sysctl(mib, 2, &la, &len, NULL, 0) == -1)
	{
		memset(info->loads, 0, sizeof(info->loads));
		return 1;
	}
	info->loads[0] = la.ldavg[0] * la.fscale / 64;
	info->loads[1] = la.ldavg[1] * la.fscale / 64;
	info->loads[2] = la.ldavg[2] * la.fscale / 64;
	return 0;
}
# endif

# if defined(_sysinfo_ram_sysctl_uvm)
#  include <uvm/uvm.h>
static int _sysinfo_ram_sysctl_uvm(struct sysinfo * info)
{
	int mib[2];
	struct uvmexp ue;
	size_t len = sizeof(ue);

	mib[0] = CTL_VM;
	mib[1] = VM_UVMEXP;
	if(sysctl(mib, 2, &ue, &len, NULL, 0) == -1)
	{
		memset(info, 0, sizeof(*info));
		return 1;
	}
	info->totalram = ue.pagesize * ue.npages;
	info->freeram = ue.pagesize * ue.free;
	info->sharedram = ue.pagesize * ue.execpages;
	info->bufferram = ue.pagesize * ue.filepages;
	info->totalswap = ue.pagesize * ue.swpages;
	info->freeswap = info->totalswap - (ue.pagesize * ue.swpgonly);
	return 0;
}
# endif

# if defined(_sysinfo_procs_sysctl)
static int _sysinfo_procs_sysctl(struct sysinfo * info)
{
	int mib[3];
	size_t len = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	if(sysctl(mib, 3, NULL, &len, NULL, 0) == -1)
	{
		info->procs = 0;
		return 1;
	}
	info->procs = len / sizeof(struct kinfo_proc);
	return 0;
}
# endif

/* sysinfo generic */
# if defined(_sysinfo_uptime_generic)
#  warning Generic uptime reporting is not supported
static int _sysinfo_uptime_generic(struct sysinfo * info)
{
	info->uptime = 0;
	return 0;
}
# endif

# if defined(_sysinfo_loads_generic)
static int _sysinfo_loads_generic(struct sysinfo * info)
{
	double la[3];

	if(getloadavg(la, 3) != 3)
	{
		memset(info->loads, 0, sizeof(info->loads));
		return 1;
	}
	info->loads[0] = la[0];
	info->loads[1] = la[1];
	info->loads[2] = la[2];
	return 0;
}
# endif

# if defined(_sysinfo_ram_generic)
#  warning Generic RAM reporting is not supported
static int _sysinfo_ram_generic(struct sysinfo * info)
{
	info->totalram = 0;
	info->freeram = 0;
	info->sharedram = 0;
	info->bufferram = 0;
	info->totalswap = 0;
	info->freeswap = 0;
	return 0;
}
# endif

# if defined(_sysinfo_procs_generic)
#  include <dirent.h>
static int _sysinfo_procs_generic(struct sysinfo * info)
{
	DIR * dir;
	struct dirent * de;
	long l;


	info->procs = 0;
	if((dir = opendir("/proc")) == NULL)
		return _probe_perror("/proc", 0);
	while((de = readdir(dir)) != NULL)
	{
#  ifdef DT_DIR
		if(de->d_type != DT_DIR)
			continue;
#  endif
		if((l = strtol(de->d_name, NULL, 10)) <= 0)
			continue;
		info->procs++;
	}
	closedir(dir);
	return 0;
}
# endif
#endif /* defined(_sysinfo_generic) */


/* userinfo */
#if defined(_userinfo_none)
static int _userinfo_none(unsigned int * userinfo)
{
	*userinfo = 0;
	return 0;
}
#elif defined(_userinfo_utmpx)
# include <utmpx.h>
static int _userinfo_utmpx(unsigned int * userinfo)
{
	struct utmpx * ut;

	for(*userinfo = 0; (ut = getutxent()) != NULL;)
		if(ut->ut_type == USER_PROCESS)
			(*userinfo)++;
	endutxent();
	return 0;
}
#endif


/* ifinfo */
struct ifinfo
{
	char name[6];
	unsigned int ibytes;
	unsigned int obytes;
};

/* ifinfo linux */
#if defined(_ifinfo_linux)
enum InterfaceInfo
{
	IF_RX_BYTES = 0, IF_RX_PACKETS, IF_RX_ERRS, IF_RX_DROP, IF_RX_FIFO,
	IF_RX_FRAME, IF_RX_COMPRESSED, IF_RX_MULTICAST,
	IF_TX_BYTES, IF_TX_PACKETS, IF_TX_ERRS, IF_TX_DROP, IF_TX_FIFO,
	IF_TX_FRAME, IF_TX_COMPRESSED
};
# define IF_LAST IF_TX_COMPRESSED

static int _ifinfo_linux_append(struct ifinfo ** dev, char * buf, int nb);
static int _ifinfo_linux(struct ifinfo ** dev)
{
	int ret = 0;
	FILE * fp;
	char buf[200];
	int i;
	int len;

	if((fp = fopen("/proc/net/dev", "r")) == NULL)
		return -1;
	for(i = 0; fgets(buf, sizeof(buf), fp) != NULL; i++)
	{
		len = string_get_length(buf);
		if(buf[len - 1] != '\n')
		{
			ret = -1;
			break;
		}
		if(i < 2)
			continue;
		if(_ifinfo_linux_append(dev, buf, i - 2) != 0)
		{
			ret = -1;
			break;
		}
		ret++;
	}
	fclose(fp);
	return ret;
}

static int _ifinfo_linux_append(struct ifinfo ** dev, char * buf, int nb)
{
	struct ifinfo * p;
	size_t i;
	char * q;
	int j = 0;

	if((p = realloc(*dev, sizeof(*p) * (nb + 1))) == NULL)
		return _probe_perror(NULL, 1);
	*dev = p;
	for(i = 0; i < sizeof(p->name) && buf[i] != '\0'; i++);
	if(i != sizeof(p->name))
		return 1;
	buf[sizeof(p->name)] = '\0';
	for(q = buf; q[0] == ' '; q++);
	strcpy(p[nb].name, q);
# if defined(DEBUG)
	fprintf(stderr, "_ifinfo_append: %s\n", p[nb].name);
# endif
	for(i++; buf[i] != '\0'; i++)
	{
		if(j > IF_LAST)
			break;
		if(buf[i] == ' ')
			continue;
		q = &buf[i];
		for(; buf[i] >= '0' && buf[i] <= '9'; i++);
		buf[i] = '\0';
		switch(j++)
		{
			case IF_RX_BYTES:
				(*dev)[nb].ibytes = strtoll(q, &q, 10);
				break;
			case IF_TX_BYTES:
				(*dev)[nb].obytes = strtoll(q, &q, 10);
				break;
			default:
				continue;
		}
		if(*q != '\0')
			return 1;
	}
	return 0;
}
#endif /* defined(_ifinfo_linux) */

/* ifinfo netbsd */
#if defined(_ifinfo_bsd)
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <net/if.h>
# include <ifaddrs.h>
static int _ifinfo_bsd_append(struct ifinfo ** dev, char * ifname, int fd,
		int nb);
static int _ifinfo_bsd(struct ifinfo ** dev)
{
	int ret = 0;
	static int fd = -1;
	struct ifaddrs * ifa;
	struct ifaddrs * p;

	if(fd < 0 && (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return _probe_perror("socket", -1);
	if(getifaddrs(&ifa) != 0)
		return _probe_perror("getifaddrs", -1);
	for(p = ifa; p != NULL; p = p->ifa_next)
	{
		if(p->ifa_addr->sa_family != AF_LINK)
			continue;
		if(_ifinfo_bsd_append(dev, p->ifa_name, fd, ret++) == 0)
			continue;
		ret = -1;
		break;
	}
	freeifaddrs(ifa);
	return ret;
}

static int _ifinfo_bsd_append(struct ifinfo ** dev, char * ifname, int fd,
		int nb)
{
	struct ifdatareq ifdr;
	struct ifinfo * p;

	strcpy(ifdr.ifdr_name, ifname);
	if(ioctl(fd, SIOCGIFDATA, &ifdr) == -1)
		return _probe_perror("SIOCGIFDATA", 1);
	if((p = realloc(*dev, sizeof(*p) * (nb + 1))) == NULL)
		return _probe_perror(NULL, 1);
	*dev = p;
	strcpy(p[nb].name, ifname);
# if defined(DEBUG)
	fprintf(stderr, "_ifinfo_append: %s\n", p[nb].name);
# endif
	p[nb].ibytes = ifdr.ifdr_data.ifi_ibytes;
	p[nb].obytes = ifdr.ifdr_data.ifi_obytes;
	return 0;
}
#endif /* defined(_ifinfo_bsd) */

/* ifinfo generic */
#if defined(_ifinfo_generic)
# warning Generic interface reporting is not supported
static int _ifinfo_generic(struct ifinfo ** dev)
{
	*dev = NULL;
	return 0;
}
#endif /* defined(_ifinfo_generic) */


/* volinfo */
struct volinfo
{
	char name[256];
	unsigned long block_size;
	unsigned long total;
	unsigned long free;
};

/* volinfo linux */
#if defined(_volinfo_mtab)
# include <sys/statvfs.h>
enum VolInfo
{
	VI_DEVICE = 0, VI_MOUNTPOINT, VI_FS, VI_OPTIONS, VI_DUMP, VI_PASS
};
#define VI_LAST VI_PASS

static int _volinfo_mtab_append(struct volinfo ** dev, char * buf, int nb);
static int _volinfo_mtab(struct volinfo ** dev)
{
	int ret = 0;
	FILE * fp;
	char buf[200];
	int i;
	int len;

	if((fp = fopen("/etc/mtab", "r")) == NULL)
		return _probe_perror("/etc/mtab", -1);
	for(i = 0; fgets(buf, sizeof(buf), fp) != NULL; i++)
	{
		len = string_get_length(buf);
		if(buf[len-1] != '\n')
		{
			ret = -1;
			break;
		}
		if(_volinfo_mtab_append(dev, buf, i) != 0)
		{
			ret = -1;
			break;
		}
		ret++;
	}
	fclose(fp);
	return ret;
}

static int _volinfo_mtab_append(struct volinfo ** dev, char * buf, int nb)
{
	unsigned int i;
	unsigned int j;
	struct volinfo * p;
	struct statvfs sv;

	for(i = 0; buf[i] != '\0' && buf[i] != ' '; i++);
	if(buf[i] == '\0')
		return 1;
	for(j = ++i; buf[j] != '\0' && buf[j] != ' '; j++);
	if(buf[j] == '\0')
		return 1;
	if((p = realloc(*dev, sizeof(struct volinfo) * (nb + 1))) == NULL)
		return 1;
	*dev = p;
	memset(&p[nb], 0, sizeof(struct volinfo));
	if(j-i >= sizeof(p[nb].name)-1)
		return 1;
	strncpy(p[nb].name, &buf[i], j-i);
	p[nb].name[j-i] = '\0';
# if defined(DEBUG)
	fprintf(stderr, "_volinfo_append: %s\n", p[nb].name);
# endif
	if(statvfs(p[nb].name, &sv) != 0)
		return 1;
	p[nb].block_size = sv.f_bsize;
	p[nb].total = sv.f_blocks;
	p[nb].free = sv.f_bavail;
	return 0;
}
#endif /* defined(_volinfo_mtab) */

/* volinfo_statfs */
#if defined(_volinfo_statfs)
# include <sys/param.h>
# include <sys/mount.h>
static int _volinfo_statfs_append(struct volinfo ** dev, struct statfs * buf,
		int nb);
static int _volinfo_statfs(struct volinfo ** dev)
{
	struct statfs buf;

	if(statfs("/", &buf) != 0)
		return _probe_perror("statfs", -1);
	return _volinfo_statfs_append(dev, &buf, 0);
}

static int _volinfo_statfs_append(struct volinfo ** dev, struct statfs * buf,
		int nb)
{
	struct volinfo * p;

	if((p = realloc(*dev, sizeof(*p) * (nb + 1))) == NULL)
		return _probe_perror(NULL, 1);
	*dev = p;
	strcpy(p[nb].name, buf->f_mntonname);
# if defined(DEBUG)
	fprintf(stderr, "_volinfo_append: %s\n", p[nb].name);
# endif
	p[nb].block_size = buf->f_bsize;
	p[nb].total = buf->f_blocks * 2048 / buf->f_bsize;
	p[nb].free = buf->f_bavail * 2048 / buf->f_bsize;
	return 0;
}
#endif /* defined(_volinfo_statvfs) */

/* volinfo_statvfs */
#if defined(_volinfo_statvfs)
# include <sys/statvfs.h>
static int _volinfo_statvfs_append(struct volinfo ** dev, struct statvfs * buf,
		int nb);
static int _volinfo_statvfs(struct volinfo ** dev)
{
	int ret;
	struct statvfs * buf;
	int cnt;
	int cnt2;

	if((cnt = getvfsstat(NULL, 0, ST_WAIT)) == -1)
		return _probe_perror("getvfsstat", -1);
	if((buf = malloc(sizeof(struct statvfs) * cnt)) == NULL)
		return _probe_perror(NULL, -1);
	if((cnt2 = getvfsstat(buf, sizeof(struct statvfs) * cnt, ST_WAIT))
			== -1)
	{
		free(buf);
		return _probe_perror("getvfsstat", -1);
	}
	for(ret = 0; ret < cnt && ret < cnt2; ret++)
	{
		if(_volinfo_statvfs_append(dev, &buf[ret], ret) == 0)
			continue;
		ret = -1;
		break;
	}
	free(buf);
	return ret;
}

static int _volinfo_statvfs_append(struct volinfo ** dev, struct statvfs * buf,
		int nb)
{
	struct volinfo * p;

	if((p = realloc(*dev, sizeof(*p) * (nb + 1))) == NULL)
		return _probe_perror(NULL, 1);
	*dev = p;
	strcpy(p[nb].name, buf->f_mntonname);
# if defined(DEBUG)
	fprintf(stderr, "_volinfo_append: %s\n", p[nb].name);
# endif
	p[nb].block_size = buf->f_bsize;
	p[nb].total = buf->f_blocks * 2048 / buf->f_bsize;
	p[nb].free = buf->f_bavail * 2048 / buf->f_bsize;
	return 0;
}
#endif /* defined(_volinfo_statvfs) */

/* volinfo generic */
#if defined(_volinfo_generic)
# warning Generic volume information is not supported
static int _volinfo_generic(struct volinfo ** dev)
{
	*dev = NULL;
	return 0;
}
#endif /* defined(_volinfo_generic) */


/* Probe */
/* private */
/* types */
typedef struct _App
{
	struct sysinfo sysinfo;
	unsigned int users;
	struct ifinfo * ifinfo;
	unsigned int ifinfo_cnt;
	struct volinfo * volinfo;
	unsigned int volinfo_cnt;
} Probe;


/* prototypes */
static int _probe_error(int ret);
static int _probe_perror(char const * message, int ret);
static int _probe_timeout(Probe * probe);


/* functions */
/* probe */
static int _probe(AppServerOptions options)
{
	Probe probe;
	AppServer * appserver;
	Event * event;
	struct timeval tv;

	memset(&probe, 0, sizeof(probe));
	if(_probe_timeout(&probe) != 0)
	{
		free(probe.ifinfo);
		free(probe.volinfo);
		return 1;
	}
	if((event = event_new()) == NULL)
	{
		free(probe.ifinfo);
		free(probe.volinfo);
		return _probe_error(1);
	}
	if((appserver = appserver_new_event(&probe, options,
					APPSERVER_PROBE_NAME, NULL, event))
			== NULL)
	{
		free(probe.ifinfo);
		free(probe.volinfo);
		event_delete(event);
		return _probe_error(1);
	}
	tv.tv_sec = PROBE_REFRESH;
	tv.tv_usec = 0;
	if(event_register_timeout(event, &tv, (EventTimeoutFunc)_probe_timeout,
			&probe) != 0)
		_probe_error(0);
	else
		event_loop(event);
	appserver_delete(appserver);
	event_delete(event);
	free(probe.ifinfo);
	free(probe.volinfo);
	return 1;
}


/* probe_error */
static int _probe_error(int ret)
{
	error_print(PROGNAME_PROBE);
	return ret;
}


/* probe_perror */
static int _probe_perror(char const * message, int ret)
{
	error_set_print(PROGNAME_PROBE, ret, "%s%s%s", message ? message : "",
			message ? ": " : "", strerror(errno));
	return ret;
}


/* probe_timeout */
static int _probe_timeout(Probe * probe)
{
	int i;
#if defined(DEBUG)
	static unsigned int count = 0;

	fprintf(stderr, "%s%d%s", "_probe_timeout(", count++, ")\n");
#endif
	if(_sysinfo(&probe->sysinfo) != 0)
		return _probe_perror("sysinfo", 1);
	if(_userinfo(&probe->users) != 0)
		return _probe_perror("userinfo", 1);
	if((i = _ifinfo(&probe->ifinfo)) < 0)
		return _probe_perror("ifinfo", 1);
	probe->ifinfo_cnt = i;
	if((i = _volinfo(&probe->volinfo)) < 0)
		return _probe_perror("volinfo", 1);
	probe->volinfo_cnt = i;
	return 0;
}


/* public */
/* functions */
/* AppInterface */
/* Probe_uptime */
uint32_t Probe_uptime(Probe * probe, AppServerClient * asc)
{
	(void) asc;

#if defined(DEBUG)
	fprintf(stderr, "%s() %ld\n", __func__, probe->sysinfo.uptime);
#endif
	return probe->sysinfo.uptime;
}


/* Probe_load */
int32_t Probe_load(Probe * probe, AppServerClient * asc, uint32_t * load1,
		uint32_t * load5, uint32_t * load15)
{
	(void) asc;

#if defined(DEBUG)
	fprintf(stderr, "%s() %lu %lu %lu\n", __func__, probe->sysinfo.loads[0],
			probe->sysinfo.loads[1], probe->sysinfo.loads[2]);
#endif
	*load1 = probe->sysinfo.loads[0];
	*load5 = probe->sysinfo.loads[1];
	*load15 = probe->sysinfo.loads[2];
	return 0;
}


/* Probe_ram */
int32_t Probe_ram(Probe * probe, AppServerClient * asc, uint32_t * total,
		uint32_t * free, uint32_t * shared, uint32_t * buffer)
{
	(void) asc;

#if defined(DEBUG)
	fprintf(stderr, "%s() total %lu, free %lu, shared %lu, buffered %lu\n",
			__func__, probe->sysinfo.totalram,
			probe->sysinfo.freeram, probe->sysinfo.sharedram,
			probe->sysinfo.bufferram);
#endif
	*total = probe->sysinfo.totalram;
	*free = probe->sysinfo.freeram;
	*shared = probe->sysinfo.sharedram;
	*buffer = probe->sysinfo.bufferram;
	return 0;
}


/* Probe_swap */
int32_t Probe_swap(Probe * probe, AppServerClient * asc, uint32_t * total,
		uint32_t * free)
{
	(void) asc;

#if defined(DEBUG)
	fprintf(stderr, "%s() %lu/%lu\n", __func__,
			probe->sysinfo.totalswap - probe->sysinfo.freeswap,
			probe->sysinfo.totalswap);
#endif
	*total = probe->sysinfo.totalswap;
	*free = probe->sysinfo.freeswap;
	return 0;
}


/* Probe_procs */
uint32_t Probe_procs(Probe * probe, AppServerClient * asc)
{
	(void) asc;

#if defined(DEBUG)
	fprintf(stderr, "%s() %u\n", __func__, probe->sysinfo.procs);
#endif
	return probe->sysinfo.procs;
}


/* Probe_users */
uint32_t Probe_users(Probe * probe, AppServerClient * asc)
{
	(void) asc;

#if defined(DEBUG)
	fprintf(stderr, "%s() %u\n", __func__, probe->users);
#endif
	return probe->users;
}


/* Probe_ifrxbytes */
uint32_t Probe_ifrxbytes(Probe * probe, AppServerClient * asc,
		String const * dev)
{
	unsigned int i;
	(void) asc;

	for(i = 0; i < probe->ifinfo_cnt
			&& string_compare(probe->ifinfo[i].name, dev) != 0; i++);
	if(i == probe->ifinfo_cnt)
		return -1;
#if defined(DEBUG)
	fprintf(stderr, "%s() %s %u\n", __func__, probe->ifinfo[i].name,
			probe->ifinfo[i].ibytes);
#endif
	return probe->ifinfo[i].ibytes;
}


/* Probe_iftxbytes */
uint32_t Probe_iftxbytes(Probe * probe, AppServerClient * asc,
		String const * dev)
{
	unsigned int i;
	(void) asc;

	for(i = 0; i < probe->ifinfo_cnt
			&& string_compare(probe->ifinfo[i].name, dev) != 0; i++);
	if(i == probe->ifinfo_cnt)
		return -1;
#if defined(DEBUG)
	fprintf(stderr, "%s() %s %u\n", __func__, probe->ifinfo[i].name,
			probe->ifinfo[i].obytes);
#endif
	return probe->ifinfo[i].obytes;
}


/* Probe_voltotal */
uint32_t Probe_voltotal(Probe * probe, AppServerClient * asc,
		String const * volume)
{
	unsigned int i;
	(void) asc;

	for(i = 0; i < probe->volinfo_cnt
			&& string_compare(probe->volinfo[i].name, volume) != 0;
			i++);
	if(i == probe->volinfo_cnt)
		return -1;
#if defined(DEBUG)
	fprintf(stderr, "%s() %s %lu\n", __func__, probe->volinfo[i].name,
			probe->volinfo[i].total);
#endif
	return probe->volinfo[i].total * (probe->volinfo[i].block_size / 1024);
}


/* Probe_volfree */
uint32_t Probe_volfree(Probe * probe, AppServerClient * asc,
		String const * volume)
{
	unsigned int i;
	(void) asc;

	for(i = 0; i < probe->volinfo_cnt
			&& string_compare(probe->volinfo[i].name, volume) != 0;
			i++);
	if(i == probe->volinfo_cnt)
		return -1;
#if defined(DEBUG)
	fprintf(stderr, "%s() %s %lu\n", __func__, probe->volinfo[i].name,
			probe->volinfo[i].free);
#endif
	return probe->volinfo[i].free * (probe->volinfo[i].block_size / 1024);
}


/* usage */
static int _usage(void)
{
	fputs("Usage: " PROGNAME_PROBE " [-R]\n", stderr);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	int o;
	AppServerOptions options = 0;

	while((o = getopt(argc, argv, "R")) != -1)
		switch(o)
		{
			case 'R':
				options = ASO_REGISTER;
				break;
			default:
				return _usage();
		}
	if(optind != argc)
		return _usage();
	return (_probe(options) == 0) ? 0 : 2;
}
