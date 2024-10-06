#include <iostream>
#include <string>

#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/pkt_cls.h>
#include <netlink/netlink.h>
#include <netlink/route/class.h>
#include <netlink/route/classifier.h>
#include <netlink/route/cls/fw.h>
#include <netlink/route/link.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/qdisc/htb.h>
#include <netlink/route/tc.h>

#include <arpa/inet.h>

using namespace std;

void throw_err(int err) {
  if (err) {
    std::string error_str = "!!ERROR " + std::string(nl_geterror(err));
    throw std::runtime_error(error_str);
  }
}

void add_qdisc_htb(struct rtnl_link *link, uint32_t parent_handle,
                   uint32_t handle, struct nl_sock *sock,
                   uint32_t default_class) {
  int err;
  struct rtnl_qdisc *qdisc;
  qdisc = rtnl_qdisc_alloc();
  rtnl_tc_set_link(TC_CAST(qdisc), link);

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

void add_htb_class(struct rtnl_link *link, uint32_t parent_handle,
                   uint32_t handle, struct nl_sock *sock, uint32_t rate,
                   uint32_t ceil = 0, uint32_t prio = 0, uint32_t quantum = 0) {
  int err;

  struct rtnl_class *cl;

  cl = rtnl_class_alloc();
  rtnl_tc_set_link(TC_CAST(cl), link);
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

void add_fw_filter_by_mark(struct rtnl_link *link, uint32_t handle,
                           uint32_t flowid, struct nl_sock *sock, int prio,
                           uint32_t mark) {
  struct rtnl_cls *filter;
  int err;
  filter = rtnl_cls_alloc();
  rtnl_tc_set_link(TC_CAST(filter), link);
  rtnl_tc_set_parent(TC_CAST(filter), handle);
  err = rtnl_tc_set_kind(TC_CAST(filter), "fw");

  throw_err(err);
  rtnl_cls_set_prio(filter, prio);
  rtnl_cls_set_protocol(filter, ETH_P_ALL);
  rtnl_tc_set_handle(TC_CAST(filter), mark);

  err = rtnl_fw_set_classid(filter, flowid);
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
  rtnl_tc_set_parent(TC_CAST(qdisc), TC_H_ROOT);

  // Delete current qdisc
  rtnl_qdisc_delete(sock, qdisc);
  free(qdisc);

  // tc qdisc add dev lo root handle 1: htb default 20
  add_qdisc_htb(link, TC_H_ROOT, TC_HANDLE(0x1, 0), sock, TC_HANDLE(0, 0x20));

  // tc class add dev lo parent 1: classid 1:1 htb rate 10240kbit
  add_htb_class(link, TC_HANDLE(0x1, 0), TC_HANDLE(0x1, 0x1), sock, 1250000);

  // tc class add dev lo parent 1:1 classid 1:10 htb rate 7168kbit
  add_htb_class(link, TC_HANDLE(0x1, 0x1), TC_HANDLE(0x1, 0x10), sock, 12500,
                1250000, 1);

  // tc class add dev lo parent 1:1 classid 1:20 htb rate 3072kbit ceil
  // 10240kbit
  add_htb_class(link, TC_HANDLE(0x1, 0x1), TC_HANDLE(0x1, 0x20), sock, 12500,
                1250000, 2);

  // tc filter add dev lo parent 1: protocol all prio 3 handle 11 fw flowid
  // 1:10
  add_fw_filter_by_mark(link, TC_HANDLE(0x1, 0), TC_HANDLE(0x1, 0x10), sock, 3,
                        11);

  return 0;
}
