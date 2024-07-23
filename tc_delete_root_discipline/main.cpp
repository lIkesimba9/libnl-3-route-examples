#include <iostream>
#include <string>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/tc.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/qdisc/htb.h>
#include <netlink/route/qdisc/sfq.h>
#include <netlink/route/classifier.h>
#include <netlink/route/qdisc.h>
#include <linux/if_ether.h>
#include <linux/pkt_cls.h>
#include <linux/netlink.h>
#include <netlink/route/cls/u32.h>
#include <netlink/route/class.h>

using namespace std;

void delete_root_discipline(string interface) {
    struct nl_sock *sock_;
    struct rtnl_link *link_;
    struct nl_cache *cache_;

    sock_ = nl_socket_alloc();
    nl_connect(sock_, NETLINK_ROUTE);
    rtnl_link_alloc_cache(sock_, AF_UNSPEC, &cache_);
    link_ = rtnl_link_get_by_name(cache_, interface.c_str());

    // delete old qdisc
    struct rtnl_qdisc *qdisc;
    if (!(qdisc = rtnl_qdisc_alloc())) {
        std::runtime_error("can not allocate qdisc");
    }
    rtnl_tc_set_link(TC_CAST(qdisc), link_);
    rtnl_tc_set_parent(TC_CAST(qdisc), TC_H_ROOT);

    //Delete current qdisc
    rtnl_qdisc_delete(sock_, qdisc);
    free(qdisc);
}

int main(int argc, char **argv) {
    if (argc == 1) {
        cout << "Usage " << argv[0] << " interface name\n";
    } else {
        delete_root_discipline(argv[1]);
    }
    return 0;
}
