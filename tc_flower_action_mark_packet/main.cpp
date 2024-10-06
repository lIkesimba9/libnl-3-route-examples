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
#include <netlink/route/act/skbedit.h>




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
    rtnl_tc_set_handle(TC_CAST(qdisc), handle);
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "ingress");
    throw_err(err);


    err = rtnl_qdisc_add(sock, qdisc, NLM_F_CREATE);
    throw_err(err);
    rtnl_qdisc_put(qdisc);
}

void add_flower_filter(struct rtnl_link *link, uint32_t parent_handle, struct nl_sock* sock, int prio, struct rtnl_act* act, uint16_t vlan_id) {
    struct rtnl_cls *filter;
    int err;
    filter = rtnl_cls_alloc();
    rtnl_tc_set_link(TC_CAST(filter), link);

    rtnl_tc_set_parent(TC_CAST(filter), parent_handle);
    err = rtnl_tc_set_kind(TC_CAST(filter), "flower");
    throw_err(err);

    rtnl_cls_set_prio(filter, prio);
    rtnl_cls_set_protocol(filter, ETH_P_8021Q);
    err = rtnl_flower_set_vlan_id(filter, vlan_id);

    throw_err(err);

    err = rtnl_flower_append_action(filter, act);
    throw_err(err);

    err = rtnl_cls_add(sock, filter, NLM_F_CREATE);
    throw_err(err);
    rtnl_cls_put(filter);
}



int main() {

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
    rtnl_tc_set_kind(TC_CAST(act), "skbedit");

    rtnl_skbedit_set_action(act, TC_ACT_PIPE);
    rtnl_skbedit_set_mark(act, 11);


   add_flower_filter(link, TC_HANDLE(0xffff, 0), sock, 1, act, 101);
    return 0;
}