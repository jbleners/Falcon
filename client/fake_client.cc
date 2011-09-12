/* 
 * Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *
 */
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
