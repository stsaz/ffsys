/** ffsys: network configuration
2020, Simon Zolin */

/*
ffnetconf_destroy
ffnetconf_get
ffnetconf_error
*/

#pragma once
#include <ffsys/base.h>
#include <ffbase/vector.h>

enum FFNETCONF_F {
	FFNETCONF_DNS_ADDR = 1, // list of DNS server addresses
	FFNETCONF_INTERFACES = 2, // Get properties of all network interfaces (`struct ffnetconf_ifinfo`)
};

typedef struct {
	ffvec ifs; // struct ffnetconf_ifinfo*[]

	char **dns_addrs; // char*[]
	ffuint dns_addrs_num;

	int err;
	const char *err_func;
	char err_buf[100];
} ffnetconf;

#define _FFNC_ERR(nc, f, e) \
do { \
	nc->err_func = f; \
	nc->err = e; \
} while (0)

/** Get last error message */
static inline char* ffnetconf_error(ffnetconf *nc)
{
	ffsz_format(nc->err_buf, sizeof(nc->err_buf), "%s: %E"
		, nc->err_func, nc->err);
	return nc->err_buf;
}

static void _ffnc_ifi_destroy(ffvec *ifs)
{
	struct ffnetconf_ifinfo **it;
	FFSLICE_WALK(ifs, it) {
		ffmem_free(*it);
	}
	ffvec_free(ifs);
}

struct ffnetconf_ifinfo {
	ffuint	index;
	ffuint	mtu;
	char	name[64];
	ffbyte	hwaddr[6];
	ffuint	ip_n;
	ffbyte	ip[0][16];
};

static struct ffnetconf_ifinfo* _ffnc_ifi_new(ffvec *ifs, ffuint n_ip)
{
	struct ffnetconf_ifinfo *nif = (struct ffnetconf_ifinfo*)ffmem_calloc(1, sizeof(struct ffnetconf_ifinfo) + n_ip * 16);
	*ffvec_pushT(ifs, struct ffnetconf_ifinfo*) = nif;
	return nif;
}

static inline struct ffnetconf_ifinfo* _ffnc_ifi_expand(struct ffnetconf_ifinfo *nif)
{
	return (struct ffnetconf_ifinfo*)ffmem_realloc(nif, sizeof(struct ffnetconf_ifinfo) + (nif->ip_n + 1) * 16);
}

/** Map IPv4 address to IPv6. */
static inline void _ffnc_ip6_v4mapped_set(void *ip6, const void *ip4)
{
	ffuint *i = (ffuint*)ip6;
	i[3] = *(ffuint*)ip4;
	i[0] = i[1] = 0;
	i[2] = ffint_be_cpu32(0x0000ffff);
}


#ifdef FF_WIN

#include <iphlpapi.h>

static FIXED_INFO* _ffnetconf_info()
{
	FIXED_INFO *info = NULL;
	DWORD sz = sizeof(FIXED_INFO);
	ffbool first = 1;
	for (;;) {
		if (NULL == (info = (FIXED_INFO*)ffmem_alloc(sz)))
			return NULL;
		ffuint r = GetNetworkParams(info, &sz);
		if (r == ERROR_BUFFER_OVERFLOW) {
			if (!first)
				goto end;
			first = 0;
			ffmem_free(info);
			info = NULL;
			continue;
		} else if (r != 0)
			goto end;
		break;
	}
	return info;

end:
	ffmem_free(info);
	return NULL;
}

/** Get DNS server addresses */
static int _ffnetconf_dns(ffnetconf *nc, const FIXED_INFO *info)
{
	const IP_ADDR_STRING *ip;
	ffsize cap = 0;
	ffuint n = 0;
	for (ip = &info->DnsServerList;  ip != NULL;  ip = ip->Next) {
		cap += ffsz_len(ip->IpAddress.String) + 1;
		n++;
	}
	if (NULL == (nc->dns_addrs = (char**)ffmem_alloc(cap + n * sizeof(void*))))
		return -1;

	// ptr1 ptr2 ... data1 data2 ...
	char **ptr = nc->dns_addrs;
	char *data = (char*)nc->dns_addrs + n * sizeof(void*);
	for (ip = &info->DnsServerList;  ip != NULL;  ip = ip->Next) {
		ffsize len = ffsz_len(ip->IpAddress.String) + 1;
		ffmem_copy(data, ip->IpAddress.String, len);
		*(ptr++) = data;
		data += len;
	}
	nc->dns_addrs_num = n;
	return 0;
}

/** Allocate IP_ADAPTER_ADDRESSES* object. */
static int _ffnc_GetAdaptersAddresses(IP_ADAPTER_ADDRESSES **aa, ffuint flags)
{
	int r, i = 0;
	ULONG cap = 16*1024;
	do {
		if (NULL == (*aa = (IP_ADAPTER_ADDRESSES*)ffmem_alloc(cap)))
			return -1;
		if (0 == (r = GetAdaptersAddresses(0, flags, NULL, *aa, &cap)))
			return 0;
		ffmem_free(*aa);
		i++;
	} while (r == ERROR_BUFFER_OVERFLOW || i < 2);
	*aa = NULL;
	return r;
}

static int _ffnc_ifi_read(ffnetconf *nc, IP_ADAPTER_ADDRESSES *a)
{
	for (;  a != NULL;  a = a->Next) {

		ffuint n = 0;
		for (const IP_ADAPTER_UNICAST_ADDRESS_LH *ua = a->FirstUnicastAddress;  ua != NULL;  ua = ua->Next) {
			n++;
		}

		struct ffnetconf_ifinfo *nif;
		if (NULL == (nif = _ffnc_ifi_new(&nc->ifs, n))) {
			_FFNC_ERR(nc, "memory alloc", GetLastError());
			return -1;
		}

		nif->index = a->IfIndex;
		nif->mtu = a->Mtu;
		ffsz_wtou(nif->name, sizeof(nif->name), a->FriendlyName);
		if (a->PhysicalAddressLength == 6)
			ffmem_copy(nif->hwaddr, a->PhysicalAddress, 6);

		n = 0;
		for (const IP_ADAPTER_UNICAST_ADDRESS_LH *ua = a->FirstUnicastAddress;  ua != NULL;  ua = ua->Next) {

			if (ua->Address.lpSockaddr->sa_family == AF_INET) {
				const struct sockaddr_in *si = (struct sockaddr_in*)ua->Address.lpSockaddr;
				_ffnc_ip6_v4mapped_set(nif->ip[n++], &si->sin_addr);

			} else if (ua->Address.lpSockaddr->sa_family == AF_INET6) {
				const struct sockaddr_in6 *si6 = (struct sockaddr_in6*)ua->Address.lpSockaddr;
				ffmem_copy(nif->ip[n++], (char*)&si6->sin6_addr, 16);
			}
		}
		nif->ip_n = n;
	}

	return 0;
}

static inline int ffnetconf_get(ffnetconf *nc, ffuint flags)
{
	int rc = -1;
	switch (flags) {
	case FFNETCONF_DNS_ADDR: {
		FIXED_INFO *info = _ffnetconf_info();
		if (info == NULL) {
			_FFNC_ERR(nc, "GetNetworkParams", 0);
			break;
		}

		rc = _ffnetconf_dns(nc, info);

		ffmem_free(info);
		break;
	}

	case FFNETCONF_INTERFACES: {
		IP_ADAPTER_ADDRESSES *aa = NULL;
		ffuint flags = GAA_FLAG_SKIP_DNS_SERVER;
		if (0 != (rc = (_ffnc_GetAdaptersAddresses(&aa, flags)))) {
			_FFNC_ERR(nc, "GetAdaptersAddresses", rc);
			break;
		}

		rc = _ffnc_ifi_read(nc, aa);
		ffmem_free(aa);
		break;
	}
	}

	return rc;
}

#elif __ANDROID_API__ >= 24

#include <ifaddrs.h>

static int _ffnc_ifi_read(ffnetconf *nc)
{
	struct ifaddrs *ifa, *a;
	if (0 != getifaddrs(&ifa)) {
		_FFNC_ERR(nc, "getifaddrs", errno);
		return -1;
	}

	struct ffnetconf_ifinfo *nif = NULL, **pnif;
	for (a = ifa;  a != NULL;  a = a->ifa_next) {

		if (nif == NULL
			|| !ffsz_eq(a->ifa_name, nif->name)) {
			if (NULL == (nif = _ffnc_ifi_new(&nc->ifs, 1))) {
				return -1;
			}
			ffsz_copyz(nif->name, sizeof(nif->name), a->ifa_name);
			pnif = ffslice_lastT(&nc->ifs, struct ffnetconf_ifinfo*);
		}

		if (a->ifa_addr == NULL)
			continue;

		if (NULL == (nif = _ffnc_ifi_expand(nif)))
			return -1;
		*pnif = nif;

		if (a->ifa_addr->sa_family == AF_INET) {
			const struct sockaddr_in *sin = (struct sockaddr_in*)a->ifa_addr;
			_ffnc_ip6_v4mapped_set(nif->ip[nif->ip_n++], &sin->sin_addr);

		} else if (a->ifa_addr->sa_family == AF_INET6) {
			const struct sockaddr_in6 *sin = (struct sockaddr_in6*)a->ifa_addr;
			ffmem_copy(nif->ip[nif->ip_n++], (char*)&sin->sin6_addr, 16);
		}
	}

	freeifaddrs(ifa);
	return 0;
}

#elif defined FF_LINUX

#include <ffsys/netlink.h>

static struct ffnetconf_ifinfo** _ffnc_ifi_find(const ffslice *ifs, ffuint index)
{
	struct ffnetconf_ifinfo **it;
	FFSLICE_WALK(ifs, it) {
		if (index == (*it)->index)
			return it;
	}
	return NULL;
}

static int _ffnc_ifi_read(ffnetconf *nc)
{
	int rc = -1;
	ffsock nl = FFSOCK_NULL;
	ffuint buf_cap = 1*1024*1024, state = 0;
	void *buf = NULL;
	struct rtgenmsg gen = {
		.rtgen_family = AF_UNSPEC,
	};

	if (FFSOCK_NULL == (nl = ffnetlink_create(NETLINK_ROUTE, RTMGRP_LINK | RTMGRP_IPV4_IFADDR, 0))) {
		_FFNC_ERR(nc, "ffnetlink_create", errno);
		goto end;
	}

	if (NULL == (buf = ffmem_alloc(buf_cap))) {
		_FFNC_ERR(nc, "ffmem_alloc", errno);
		goto end;
	}

	for (;;) {
		switch (state) {
		case 0:
		case 1: {
			static const ffuint types[] = { RTM_GETLINK, RTM_GETADDR };
			if (0 != ffnetlink_send(nl, types[state], &gen, sizeof(gen), state)) {
				_FFNC_ERR(nc, "ffnetlink_send", errno);
				goto end;
			}
			state |= 0x80;
			break;
		}
		}

		int r = ffnetlink_recv(nl, buf, buf_cap);
		if (r <= 0) {
			_FFNC_ERR(nc, "ffnetlink_recv", errno);
			goto end;
		}
		ffstr resp = FFSTR_INITN(buf, (ffsize)r);

		ffstr body, val;
		const struct nlmsghdr *nh;
		const struct rtattr *attr;
		while (NULL != (nh = ffnetlink_next(&resp, &body))) {

			switch (nh->nlmsg_type) {

			case RTM_NEWLINK: {
				if (sizeof(struct ifinfomsg) > body.len)
					continue;
				const struct ifinfomsg *ifi = (struct ifinfomsg*)body.ptr;
				ffstr_shift(&body, sizeof(struct ifinfomsg));

				struct ffnetconf_ifinfo *nif;
				if (NULL == (nif = _ffnc_ifi_new(&nc->ifs, 1))) {
					goto end;
				}
				nif->index = ifi->ifi_index;

				while (NULL != (attr = ffnetlink_rtattr_next(&body, &val))) {

					switch (attr->rta_type) {
					case IFLA_IFNAME:
						ffsz_copyn(nif->name, sizeof(nif->name), val.ptr, val.len);
						break;

					case IFLA_ADDRESS:
						if (val.len == 6)
							ffmem_copy(nif->hwaddr, val.ptr, 6);
						break;
					}
				}
				break;
			}

			case RTM_NEWADDR: {
				if (sizeof(struct ifaddrmsg) > body.len)
					continue;
				const struct ifaddrmsg *ifa = (struct ifaddrmsg*)body.ptr;
				ffstr_shift(&body, sizeof(struct ifaddrmsg));

				struct ffnetconf_ifinfo *nif, **pnif;
				if (NULL == (pnif = _ffnc_ifi_find((ffslice*)&nc->ifs, ifa->ifa_index)))
					continue; // unknown interface
				nif = *pnif;

				while (NULL != (attr = ffnetlink_rtattr_next(&body, &val))) {

					switch (attr->rta_type) {
					case IFA_ADDRESS:
						if (NULL == (nif = _ffnc_ifi_expand(nif)))
							goto end;
						*pnif = nif;

						if (ifa->ifa_family == AF_INET)
							_ffnc_ip6_v4mapped_set(nif->ip[nif->ip_n++], val.ptr);
						else if (ifa->ifa_family == AF_INET6)
							ffmem_copy(nif->ip[nif->ip_n++], val.ptr, val.len);
					}
				}
				break;
			}

			case NLMSG_ERROR: {
				int err = 0;
				if (sizeof(struct nlmsgerr) < body.len) {
					const struct nlmsgerr *e = (struct nlmsgerr*)body.ptr;
					err = -e->error;
				}
				_FFNC_ERR(nc, "NLMSG_ERROR", err);
				goto end;
			}

			case NLMSG_DONE:
				goto msg_done;
			}
		}

		continue;

	msg_done:
		state &= ~0x80;
		state++;
		if (state == 2)
			break;
	}

	rc = 0;

end:
	if (rc)
		_ffnc_ifi_destroy(&nc->ifs);
	ffmem_free(buf);
	ffnetlink_close(nl);
	return rc;
}

#endif

#ifdef FF_UNIX

#include <fcntl.h>

/** Read system DNS configuration file and get DNS server addresses */
static int _ffnetconf_dns(ffnetconf *nc)
{
	int rc = -1, fd = -1;
	ffstr fdata = {};
	ffvec servers = {}; //ffstr[]
	ffsize cap = 4*1024;
	char **ptr;
	char *data;
	const char *fn = "/etc/resolv.conf";
	ffstr in, line, word, skip = FFSTR_INIT(" \t\r"), *el;

	if (NULL == ffstr_alloc(&fdata, cap))
		goto end;

	if (-1 == (fd = open(fn, O_RDONLY)))
		goto end;

	ffssize n;
	if (0 >= (n = read(fd, fdata.ptr, cap)))
		goto end;
	fdata.len = n;

	cap = 0;
	in = fdata;
	while (in.len != 0) {
		ffstr_splitby(&in, '\n', &line, &in);
		ffstr_skipany(&line, &skip);

		ffstr_splitbyany(&line, " \t", &word, &line);
		if (ffstr_eqz(&word, "nameserver")) {
			ffstr_splitbyany(&line, " \t", &word, NULL);
			if (NULL == (el = ffvec_pushT(&servers, ffstr)))
				goto end;
			*el = word;
			cap += word.len + 1;
		}
	}

	if (NULL == (nc->dns_addrs = (char**)ffmem_alloc(cap + servers.len * sizeof(void*))))
		goto end;

	// ptr1 ptr2 ... data1 data2 ...
	ptr = nc->dns_addrs;
	data = (char*)nc->dns_addrs + servers.len * sizeof(void*);
	FFSLICE_WALK_T(&servers, el, ffstr) {
		ffmem_copy(data, el->ptr, el->len);
		*(ptr++) = data;
		data += el->len;
		*(data++) = '\0';
	}
	nc->dns_addrs_num = servers.len;
	rc = 0;

end:
	close(fd);
	ffvec_free(&servers);
	ffstr_free(&fdata);
	return rc;
}

static inline int ffnetconf_get(ffnetconf *nc, ffuint flags)
{
	int rc = -1;
	switch (flags) {
	case FFNETCONF_DNS_ADDR:
		rc = _ffnetconf_dns(nc);
		break;

	case FFNETCONF_INTERFACES:
		rc = _ffnc_ifi_read(nc);
		break;
	}
	return rc;
}

#endif


/** Destroy ffnetconf object */
static inline void ffnetconf_destroy(ffnetconf *nc)
{
	_ffnc_ifi_destroy(&nc->ifs);

	ffmem_free(nc->dns_addrs);
	nc->dns_addrs = NULL;
}

/** Get system network configuration
flags: enum FFNETCONF_F
Return 0 on success */
static int ffnetconf_get(ffnetconf *nc, ffuint flags);
