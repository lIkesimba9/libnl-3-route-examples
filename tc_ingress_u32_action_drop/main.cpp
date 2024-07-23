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
#include <netlink/route//act/gact.h>
#include <netlink/route/cls/u32.h>
#include <netlink/route/class.h>

#include <arpa/inet.h>

using namespace std;



void throw_err(int err) {
    if (err) {
        std::string error_str = "!!ERROR " + std::string(nl_geterror(err));
        throw std::runtime_error(error_str);
    }
}



void add_qdisc_ingress(struct rtnl_link *link, uint32_t parent_handle, uint32_t handle, struct nl_sock* sock) {
    int err;
    struct rtnl_qdisc *qdisc;
    qdisc = rtnl_qdisc_alloc();
    rtnl_tc_set_link(TC_CAST(qdisc), link);

    rtnl_tc_set_parent(TC_CAST(qdisc), parent_handle);
   // rtnl_qdisc_delete(sock, qdisc);
    rtnl_tc_set_handle(TC_CAST(qdisc), handle);
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "ingress");
    throw_err(err);


    err = rtnl_qdisc_add(sock, qdisc, NLM_F_CREATE);
    throw_err(err);
    rtnl_qdisc_put(qdisc);
}

struct u32key {
    uint32_t value;
    uint32_t mask;
    int offset;
    int keyoffmask;
};

void add_u32_filter_32key_with_action(struct rtnl_link *link, uint32_t handle, uint32_t flowid, struct nl_sock* sock, int prio, u32key key, struct rtnl_act* act) {
    struct rtnl_cls *filter;
    int err;
    filter = rtnl_cls_alloc();
    rtnl_tc_set_link(TC_CAST(filter), link);
    rtnl_tc_set_parent(TC_CAST(filter), handle);
    err = rtnl_tc_set_kind(TC_CAST(filter), "u32");

    throw_err(err);
    rtnl_cls_set_prio(filter, prio);
    rtnl_cls_set_protocol(filter, ETH_P_ALL);
    err = rtnl_u32_add_key_uint32(filter, key.value, key.mask, key.offset, key.keyoffmask);
    throw_err(err);

    err = rtnl_u32_set_classid(filter, flowid);
    throw_err(err);


    rtnl_u32_add_action(filter, act);
    throw_err(err);
    err = rtnl_u32_set_cls_terminal(filter);
    throw_err(err);

    err = rtnl_cls_add(sock, filter, NLM_F_CREATE);
    throw_err(err);
    rtnl_cls_put(filter);
}




int main(int argc, char **argv) {
    struct nl_cache *cache;
    struct rtnl_link *link;
    struct nl_sock *sock;
    int if_index;
    sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);
    rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);
    link = rtnl_link_get_by_name(cache, "lo");


    struct rtnl_qdisc *qdisc;
    if (!(qdisc = rtnl_qdisc_alloc())) {
        std::runtime_error("Can not allocate Qdisc");
    }
    rtnl_tc_set_link(TC_CAST(qdisc), link);
    rtnl_tc_set_parent(TC_CAST(qdisc), TC_HANDLE(0xffff, 0));

    //tc qdisc del dev lo ingress
    rtnl_qdisc_delete(sock, qdisc);
    free(qdisc);



    //tc qdisc add dev lo ingress
    add_qdisc_ingress(link, TC_H_INGRESS, TC_HANDLE(0xffff, 0), sock);

    struct rtnl_act *act = rtnl_act_alloc();
    if (!act) {
        printf("rtnl_act_alloc() returns %p\n", act);
        return -1;
    }
    rtnl_tc_set_kind(TC_CAST(act), "gact");

    rtnl_gact_set_action(act, TC_ACT_SHOT);


    u32key key;
    inet_pton(AF_INET, "127.0.0.1", &(key.value));
    inet_pton(AF_INET, "255.255.255.255", &(key.mask));
    key.value = ntohl(key.value);
    key.mask = ntohl(key.mask);
    key.offset = 16; // OFFSET_DESTINATION
    int prio = 1;

      //tc filter add dev lo parent 1: protocol ip prio 1 u32  match ip dst 127.0.0.1/32  flowid 1:10 action drop
    add_u32_filter_32key_with_action(link, TC_HANDLE(0xffff, 0), TC_HANDLE(0x1, 0xeeee), sock, prio, key, act);

    return 0;
}
