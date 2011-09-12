#include "client.h"
#include <stdio.h>

using namespace Falcon;

void
my_cb(const LayerIdList& qh, void* cd, uint32_t fs, uint32_t rs) {
    LogCallbackData(qh, fs, rs);
    return;
}

int
main() {

    LayerIdList h;
    h.push_back("dvalet");
    h.push_back("dva");
    h.push_back("router");
    falcon_target* t = init(h, false, NULL);
    startMonitoring(t, my_cb, -1);

/*
    h[0] = "ntfa_spin_down";
    cl->StartMonitoring(h, false, my_cb, NULL);
    h[0] = "ntfa_spin_spin";
    cl->StartMonitoring(h, true, my_cb);

    h[0] = "ntfa_spin_stuck";
    cl->StartMonitoring(h, true, my_cb, 20);
*/
    for (;;) sleep(10);
    return 0;
}
