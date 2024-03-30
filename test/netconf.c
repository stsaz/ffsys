/** ffsys: netconf.h tester
2024, Simon Zolin */

#include <ffsys/error.h>
#include <ffsys/netconf.h>
#include <ffsys/test.h>

void test_netconf()
{
	ffnetconf nc = {};

	x(0 == ffnetconf_get(&nc, FFNETCONF_INTERFACES));
	struct ffnetconf_ifinfo **pifi, *ifi;
	FFSLICE_WALK(&nc.ifs, pifi) {
		ifi = *pifi;
		fflog("[%u] %s %6xb"
			, ifi->index, ifi->name, ifi->hwaddr);
		for (ffuint i = 0;  i < ifi->ip_n;  i++) {
			fflog("%16xb", ifi->ip[i]);
		}
	}

	x(0 == ffnetconf_get(&nc, FFNETCONF_DNS_ADDR));
	x(nc.dns_addrs_num != 0);
	x(nc.dns_addrs[0] != NULL);
	fflog("dns_addrs[0]: '%s'", nc.dns_addrs[0]);

	ffnetconf_destroy(&nc);
}
