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
#ifndef _NTFA_FALCON_CLIENT_GENERATION_H_
#define _NTFA_FALCON_CLIENT_GENERATION_H_

#include <stdint.h>

#include <vector>

struct target_t;

// This class is a convenience wrapper around a gen_vec_
class Generation {
    public:
        Generation(const uint32_t len, const uint32_t* vals);
        void SetGen(target_t* target) const;
        ~Generation();
    private:
        std::vector<uint32_t> gen_vec_;


friend bool operator!=(const Generation& g1, const Generation& g2);
friend bool operator==(const Generation& g1, const Generation& g2);
friend bool IsChild(const Generation& g1, const Generation& g2);
};

bool operator!=(const Generation& g1, const Generation& g2);
bool operator==(const Generation& g1, const Generation& g2);
bool IsChild(const Generation& g1, const Generation& g2);

#endif  // _NTFA_FALCON_CLIENT_GENERATION_H_
