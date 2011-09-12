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
