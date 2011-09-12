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

#include "util.h"
#include <stdio.h>
#include "binary_libs/libbridge.h"

unsigned char mac[6] = {0x00, 0x1c, 0x25, 0xbc, 0x5b, 0x7c};

int
main() {
  br_init();
  printf("port is %d\n", get_port_from_mac(mac));
}
