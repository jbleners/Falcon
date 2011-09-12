#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "binary_libs/libbridge.h"
#include "common.h"


#define LINE_BUF_SIZE 512
static bool
get_mac_from_ip(const char *ip, uint8_t* mac) {
    char *line_buf = new char[LINE_BUF_SIZE];
    FILE *arp_table = fopen("/proc/net/arp", "r");
    CHECK(arp_table);
    for (;;) {
        if (NULL == fgets(line_buf, LINE_BUF_SIZE, arp_table)) {
            delete [] line_buf;
            fclose(arp_table);
            return false;
        }
        char **sep_start;
        char *sep_string = line_buf;
        sep_start = &sep_string;
        char *arp_ip = strsep(sep_start, " ");
        char *arp_mac;
        if (!strcmp(arp_ip, ip)) {
            while (sep_start != NULL) {
                arp_mac = strsep(sep_start, " ");
                // Length of the MAC string
                if (strlen(arp_mac) == 17) {
                    unsigned int tmp[6];
                    sscanf(arp_mac, "%2x:%2x:%2x:%2x:%2x:%2x", &tmp[0], &tmp[1],
                           &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
                    for (int i = 0; i < 6; i++) {
                        mac[i] = static_cast<uint8_t>(tmp[i]);
                    }
                    delete [] line_buf;
                    fclose(arp_table);
                    return true;
                }
            }
        }
    }
    CHECK(0);  // Shouldn't ever reach here...
    fclose(arp_table);
    return false;
}


// Code for this function largely comes from bridge/brctl.c
int
get_port_from_mac(uint8_t mac[6]) {
    const char *brname = "br0";
#define CHUNK 128
    int i, j, n;
    fdb_entry *fdb = NULL;
    int offset = 0;

    for (;;) {
        fdb = reinterpret_cast<fdb_entry *>(realloc(fdb, (offset + CHUNK) *
                                                    sizeof(*fdb)));
        if (!fdb) {
            fprintf(stderr, "Out of memory\n");
            free(fdb);
            return -1;
        }

        n = br_read_fdb(brname, fdb+offset, offset, CHUNK);
        if (n == 0)
            break;

        if (n < 0) {
            fprintf(stderr, "read of forward table failed: %s\n",
                    strerror(errno));
            return -1;
        }
        offset += n;
    }
    for (i = 0; i < offset; i++) {
        const struct fdb_entry *f = fdb + i;
        bool mac_match = true;
        for (j = 0; j < 6; j++) {
            if (mac[j] != f->mac_addr[j]) {
                mac_match = false;
                break;
            }
        }
        if (mac_match) {
            // The bridge and the switch interact in an unknown but 
            // particular way.
            // /proc/bridge/port starts at 0
            // fdb starts at 2
            return f->port_no - 2;
        }
    }
    return -1;
}

int
get_port_from_ip(uint32_t ip) {
    in_addr addr;
    addr.s_addr = ip;
    // Hooray single threaded!
    const char* ip_str = inet_ntoa(addr);
    uint8_t mac[6];
    if (get_mac_from_ip(ip_str, mac)) {
        return get_port_from_mac(mac);
    }
    return -1;
}

const char*
get_vlan_from_port(int port) {
    static char buf[IFNAMSIZ];
    memset(&buf, 0, IFNAMSIZ);
    snprintf(buf, IFNAMSIZ, "vlan%d", port + 2);
    return buf;
}

bool
check_sanity(const char* hostname, const sockaddr_in* addr) {
    return true;
}

#ifndef PATH_MAX
#define PATH_MAX 512
#endif
void
stop_port(int port) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/proc/switch/eth0/port/%d/enable", port);
    FILE *port_file = fopen(path, "w");
    CHECK(port_file);
    fputc('0', port_file);
    fputc('\n', port_file);
    fclose(port_file);
    return;
}
