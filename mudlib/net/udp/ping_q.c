#include <standard.h>
#include "udp.h"
/*
 * Someone wants us to ping them.
 */

void incoming_request(mapping info) {
  if (info["PORTUDP"]) {
    if (!NAMESERVER_CD->query_mud_info(info["NAME"]))
      PING_Q->send_ping_q(info["HOSTADDRESS"], info["PORTUDP"]);
    NAMESERVER_CD->send_udp(info["HOSTADDRESS"], info["PORTUDP"],
                            "@@@"+UDP_PING_A+NAMESERVER_CD->start_message()+
                            "@@@\n");
  }
} /* incoming_request() */

void send_ping_q(string host, int port) {
  NAMESERVER_CD->send_udp(host, port, "@@@"+UDP_PING_Q+
                          "||NAME:"+mud_name()+
                          "||PORTUDP:"+NAMESERVER_CD->query_my_port()+
                          "@@@\n");
} /* send_ping_q() */

void create() {
  seteuid(getuid());
} /* create() */

void dest_me() { destruct(TO); }
