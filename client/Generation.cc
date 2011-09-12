#include "Generation.h"
#include "spy_prot.h"

Generation::Generation(const uint32_t len, const uint32_t* vals) {
    gen_vec_ = std::vector<uint32_t>(vals, vals + len);
}

void
Generation::SetGen(target_t* target) const {
    target->generation.generation_len = gen_vec_.size();
    target->generation.generation_val = reinterpret_cast<uint32_t *>(
                                        malloc(sizeof(uint32_t) *
                                               gen_vec_.size()));
    memcpy(target->generation.generation_val, &gen_vec_[0],
           gen_vec_.size() * sizeof(gen_vec_[0]));
}

bool operator==(const Generation& g1, const Generation& g2) {
    if (g1.gen_vec_.size() != g2.gen_vec_.size()) {
        return false;
    }
    for (uint32_t i = 0; i < g1.gen_vec_.size(); i++) {
        if (g1.gen_vec_[i] != g1.gen_vec_[i]) {
            return false;
        }
    }
    return true;
}

bool operator!=(const Generation& g1, const Generation& g2) {
    return !(g1 == g2);
}


bool IsChild(const Generation& g1, const Generation& g2) {
    if (g1.gen_vec_.size() != g2.gen_vec_.size() - 1) {
        return false;
    }
    for (uint32_t i = 0; i < g1.gen_vec_.size(); i++) {
        if (g1.gen_vec_[i] != g1.gen_vec_[i]) {
            return false;
        }
    }
    return true;
}

Generation::~Generation() {
    gen_vec_.clear();
}
