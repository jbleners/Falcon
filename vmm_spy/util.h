/* 
 * Copyright (C) 2011 Joshua B. Leners <leners@cs.utexas.edu> and
 * the University of Texas at Austin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

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
