#include "util.h"
#include <stdio.h>
#include "binary_libs/libbridge.h"

unsigned char mac[6] = {0x00, 0x1c, 0x25, 0xbc, 0x5b, 0x7c};

int
main() {
  br_init();
  printf("port is %d\n", get_port_from_mac(mac));
}
