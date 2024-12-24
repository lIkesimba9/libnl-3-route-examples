#include <iostream>
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
#include <netlink/route/cls/flower.h>
#include <netlink/route/cls/u32.h>
#include <netlink/route/class.h>




void throw_err(int err) {
    if (err) {
        std::string error_str = "!!ERROR " + std::string(nl_geterror(err));
        throw std::runtime_error(error_str);
    }
}


void add_flower_filter(int interface_index, uint32_t handle, struct nl_sock* sock, int prio) {
    struct rtnl_cls *filter;
    int err;
    filter = rtnl_cls_alloc();

    rtnl_tc_set_ifindex(TC_CAST(filter), interface_index);
    err = rtnl_tc_set_kind(TC_CAST(filter), "flower");
    throw_err(err);
    rtnl_cls_set_prio(filter, prio);
    rtnl_cls_set_protocol(filter, ETH_P_8021Q); // vlan
    err = rtnl_flower_set_vlan_id(filter, 100);
    throw_err(err);
    uint32_t  flowid = TC_HANDLE(0x1, 0x11);

    err = rtnl_cls_add(sock, filter, NLM_F_CREATE);
    throw_err(err);
    rtnl_cls_put(filter);
}


int main() {

    struct nl_sock *sock;
    struct rtnl_link *link;
    int if_index;

   // nl_connect(sock_, NETLINK_ROUTE);
   // rtnl_link_alloc_cache(sock_, AF_UNSPEC, &cache_);
   // link_ = rtnl_link_get_by_name(cache_, "eth1");


    int err;

    //uint64_t rate=0, ceil=0;

    struct nl_cache *link_cache;

    if (!(sock = nl_socket_alloc())) {
        printf("Unable to allocate netlink socket\n");
        exit(1);
    }

    if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0 ) {
        printf("Nu s-a putut conecta la NETLINK!\n");
        nl_socket_free(sock);
        exit(1);
    }


    if ((err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache)) < 0) {
        printf("Unable to allocate link cache: %s\n",
               nl_geterror(err));
        nl_socket_free(sock);
        exit(1);
    }

    /* lookup interface index of eth0 */
    if (!(link = rtnl_link_get_by_name(link_cache, "eth1"))) {
        /* error */
        printf("Interface not found\n");
        nl_socket_free(sock);
        exit(1);
    }

    if_index = rtnl_link_get_ifindex(link);

    //int interface_index, uint32_t handle, struct nl_sock* sock, int prio
    add_flower_filter(if_index, 0, sock, 1);
    return 0;
}
