#ifndef _NTFA_VMM_ENFORCER_UTIL_H_
#define _NTFA_VMM_ENFORCER_UTIL_H_
#include <stdint.h>
struct sockaddr_in;
bool get_port_from_ip(uint32_t ip);
bool check_sanity(const char* hostname, const sockaddr_in* addr);
const char* get_vlan_from_port(int port);
void stop_port(int port);
int get_port_from_mac(uint8_t mac[6]);
#endif  // _NTFA_VMM_ENFORCER_UTIL_H_
