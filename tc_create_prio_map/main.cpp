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
#include <netlink/route/qdisc/prio.h>
#include <linux/if_ether.h>
#include <linux/pkt_cls.h>
#include <linux/netlink.h>
#include <netlink/route/cls/u32.h>
#include <netlink/route/class.h>

using namespace std;

int get_tc_classid(__u32 *h, const char *str)
{
    __u32 maj, min;
    char *p;

    maj = TC_H_ROOT;
    if (strcmp(str, "root") == 0)
        goto ok;
    maj = TC_H_UNSPEC;
    if (strcmp(str, "none") == 0)
        goto ok;
    maj = strtoul(str, &p, 16);
    if (p == str) {
        maj = 0;
        if (*p != ':')
            return -1;
    }
    if (*p == ':') {
        if (maj >= (1<<16))
            return -1;
        maj <<= 16;
        str = p+1;
        min = strtoul(str, &p, 16);
        if (*p != 0)
            return -1;
        if (min >= (1<<16))
            return -1;
        maj |= min;
    } else if (*p != 0)
        return -1;

    ok:
    *h = maj;
    return 0;
}

uint32_t get_tc_classid_wrap(const std::string& handle) {
    uint32_t id;
    get_tc_classid(&id, handle.c_str());
    return id;
}

void throw_err(int err) {
    if (err) {
        std::string error_str = "!!ERROR " + std::string(nl_geterror(err));
        throw std::runtime_error(error_str);
    }
}



void add_qdisc_prio(int interface_index, uint32_t parent_handle, uint32_t handle, struct nl_sock* sock,
        uint8_t* default_priomap = NULL, int count_bands = 2, int size_priomap = 16) {
    int err;
    struct rtnl_qdisc *qdisc;
    qdisc = rtnl_qdisc_alloc();
    rtnl_tc_set_ifindex(TC_CAST(qdisc), interface_index);

    rtnl_tc_set_parent(TC_CAST(qdisc), parent_handle);
    rtnl_tc_set_handle(TC_CAST(qdisc), handle);
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "prio");
    throw_err(err);
  /*  if (default_priomap) {

        rtnl_qdisc_prio_set_priomap(qdisc, default_priomap, size_priomap);
        throw_err(err);
    }*/
    uint8_t map[] = QDISC_PRIO_DEFAULT_PRIOMAP;
    for (int i = 0; i < 16; ++i) {
        map[i] = 1;
    }

    rtnl_qdisc_prio_set_bands(qdisc, 2);
    rtnl_qdisc_prio_set_priomap(qdisc, map, 16);

   throw_err(err);

    
    err = rtnl_qdisc_add(sock, qdisc, NLM_F_CREATE | NLM_F_REPLACE);
    throw_err(err);
    rtnl_qdisc_put(qdisc);
}

void add_qdisc_pfast(int interface_index, uint32_t parent_handle, uint32_t handle, struct nl_sock *sock) {
    int err;

    struct rtnl_qdisc *qdisc;

    if (!(qdisc = rtnl_qdisc_alloc())) {
        printf("Can not allocate qdisc object\n");
        return;
    }
    rtnl_tc_set_ifindex(TC_CAST(qdisc), interface_index);
    rtnl_tc_set_parent(TC_CAST(qdisc), parent_handle);

    rtnl_tc_set_handle(TC_CAST(qdisc), TC_HANDLE(handle,0));
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "pfifo_fast");
    throw_err(err);


    uint8_t map[] = QDISC_PRIO_DEFAULT_PRIOMAP;

    rtnl_qdisc_prio_set_bands(qdisc, 3);
    rtnl_qdisc_prio_set_priomap(qdisc, map, 16);

    err = rtnl_qdisc_add(sock, qdisc, NLM_F_CREATE | NLM_F_REPLACE);
    throw_err(err);

    /* Return the qdisc object to free memory resources */
    rtnl_qdisc_put(qdisc);
}

void add_qdisc_htb(int interface_index, uint32_t parent_handle, uint32_t handle, struct nl_sock* sock, uint32_t default_class) {
    int err;
    struct rtnl_qdisc *qdisc;
    qdisc = rtnl_qdisc_alloc();
    rtnl_tc_set_ifindex(TC_CAST(qdisc), interface_index);

    rtnl_tc_set_parent(TC_CAST(qdisc), parent_handle);
    rtnl_tc_set_handle(TC_CAST(qdisc), handle);
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "htb");
    throw_err(err);

    err = rtnl_htb_set_defcls(qdisc, default_class);
    throw_err(err);

    err = rtnl_qdisc_add(sock, qdisc, NLM_F_CREATE);
    throw_err(err);
    rtnl_qdisc_put(qdisc);
}

void add_qdisc_sfq(int interface_index, uint32_t parent_handle, uint32_t handle, struct nl_sock *sock) {
    int err;

    struct rtnl_qdisc *qdisc;

    if (!(qdisc = rtnl_qdisc_alloc())) {
        printf("Can not allocate qdisc object\n");
        return;
    }
    rtnl_tc_set_ifindex(TC_CAST(qdisc), interface_index);
    rtnl_tc_set_parent(TC_CAST(qdisc), parent_handle);

    rtnl_tc_set_handle(TC_CAST(qdisc), TC_HANDLE(handle,0));
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "sfq");
    throw_err(err);

    err = rtnl_qdisc_add(sock, qdisc, NLM_F_CREATE | NLM_F_REPLACE);
    throw_err(err);

    /* Return the qdisc object to free memory resources */
    rtnl_qdisc_put(qdisc);
}


void add_htb_class(int interface_index, uint32_t parent_handle, uint32_t handle, struct nl_sock* sock,
                   uint32_t rate, uint32_t ceil = 0, uint32_t prio = 0, uint32_t quantum = 0) {
    int err;

    struct rtnl_class *cl;

    cl = rtnl_class_alloc();
    rtnl_tc_set_ifindex(TC_CAST(cl), interface_index);
    rtnl_tc_set_parent(TC_CAST(cl), parent_handle);
    rtnl_tc_set_handle(TC_CAST(cl), handle); // 1:1

    err = rtnl_tc_set_kind(TC_CAST(cl), "htb");
    throw_err(err);
    err = rtnl_htb_set_rate(cl, rate);
    throw_err(err);

    if (ceil) {
        err = rtnl_htb_set_ceil(cl, ceil);
        throw_err(err);
    }
    if (prio) {
        err = rtnl_htb_set_prio(cl, prio);
        throw_err(err);
    }
    if (quantum) {
        err = rtnl_htb_set_quantum(cl, quantum);
        throw_err(err);
    }

    err = rtnl_class_add(sock, cl, NLM_F_CREATE);
    throw_err(err);
    rtnl_class_put(cl);
}



int main() {
    uint8_t priomap[] = { 1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 };

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
    rtnl_tc_set_parent(TC_CAST(qdisc), TC_H_ROOT);

    //Delete current qdisc
    rtnl_qdisc_delete(sock, qdisc);
    free(qdisc);

    if_index = rtnl_link_get_ifindex(link);


    //tc qdisc add dev lo root handle 1: prio bands 2 priomap 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
    add_qdisc_prio(if_index, TC_H_ROOT, get_tc_classid_wrap("1:0"), sock, priomap);

    //tc qdisc add dev lo parent 1:1 handle 10: htb default 999
    add_qdisc_htb(if_index, get_tc_classid_wrap("1:1"), get_tc_classid_wrap("10:"), sock, get_tc_classid_wrap("999"));

    //tc qdisc add dev lo parent 1:2 handle 20: htb default 999
    add_qdisc_htb(if_index, get_tc_classid_wrap("1:2"), get_tc_classid_wrap("20:"), sock, get_tc_classid_wrap("999"));

    //tc class add dev lo parent 10: classid 10:1 htb rate 10Mbit
    add_htb_class(if_index, get_tc_classid_wrap("10:"), get_tc_classid_wrap("10:1"), sock, 1250000);

    //tc class add dev lo parent 10:1 classid 10:999 htb rate 500kbit ceil 500kbit prio 999
    add_htb_class(if_index, get_tc_classid_wrap("10:1"), get_tc_classid_wrap("10:999"), sock, 62500, 62500, 999);

    //tc class add dev lo parent 10:1 classid 10:999 htb rate 500kbit ceil 500kbit prio 999
    add_htb_class(if_index, get_tc_classid_wrap("10:1"), get_tc_classid_wrap("10:2"), sock, 125000, 1250000);
    //tc qdisc add dev lo parent 10:2 handle 11:2 sfq
    add_qdisc_sfq(if_index, get_tc_classid_wrap("10:2"), get_tc_classid_wrap("11"), sock);


    //tc class add dev lo parent 20: classid 20:1 htb rate 20Mbit
    add_htb_class(if_index, get_tc_classid_wrap("20:"), get_tc_classid_wrap("20:1"), sock, 2500000);

    //tc class add dev lo parent 20:1 classid 20:999 htb rate 500kbit ceil 500kbit prio 999
    add_htb_class(if_index, get_tc_classid_wrap("20:1"), get_tc_classid_wrap("20:999"), sock, 62500, 62500, 999);

    //tc class add dev lo parent 20:1 classid 20:2 htb rate 2Mbit ceil 20Mbit
    add_htb_class(if_index, get_tc_classid_wrap("20:1"), get_tc_classid_wrap("20:2"), sock, 250000, 2500000);
    //tc qdisc add dev lo parent 20:2 handle 21:2 pfifo_fast
    add_qdisc_pfast(if_index, get_tc_classid_wrap("20:2"), get_tc_classid_wrap("21"), sock);

    return 0;
}
