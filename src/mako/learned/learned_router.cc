#include "learned/learned_router.hh"
#include <algorithm>
#include <cmath>
#include <array>
#include "learned/learned_index.hh" 
#include "../masstree/masstree.hh" 
#include "../masstree/masstree_struct.hh" 
#include "../masstree/masstree_key.hh" 
#include "../masstree/kvrow.hh" 
#include "../masstree/query_masstree.hh"

namespace Masstree {



// learned_router.cc
#include <algorithm>
#include <numeric>  // for std::iota (if you keep fallback)
#include <cstdint>

namespace Masstree {

template <typename P>
void LearnedRouter<P>::train() {
    trained_ = false;

    const size_t n = leaf_keys_.size();
    if (n == 0) {
        reset_stats();
        return;
    }

    std::vector<uint64_t> keys2;
    std::vector<Masstree::leaf<P>*> ptrs2;
    keys2.reserve(n);
    ptrs2.reserve(n);

    keys2.push_back(leaf_keys_[0]);
    ptrs2.push_back(leaf_ptrs_[0]);

    bool monotonic = true;
    for (size_t i = 1; i < n; ++i) {
        uint64_t fk = leaf_keys_[i];
        if (fk < keys2.back()) monotonic = false;
        if (fk != keys2.back()) {
            keys2.push_back(fk);
            ptrs2.push_back(leaf_ptrs_[i]);
        }
    }

    if (!monotonic) {
        std::vector<size_t> idx(n);
        std::iota(idx.begin(), idx.end(), size_t(0));
        std::stable_sort(idx.begin(), idx.end(),
            [&](size_t a, size_t b) {
                return leaf_keys_[a] < leaf_keys_[b];
            });

        keys2.clear();
        ptrs2.clear();

        uint64_t prev = UINT64_MAX;
        for (size_t i : idx) {
            uint64_t fk = leaf_keys_[i];
            if (fk == prev) continue;
            keys2.push_back(fk);
            ptrs2.push_back(leaf_ptrs_[i]);
            prev = fk;
        }
    }

    leaf_keys_.swap(keys2);
    leaf_ptrs_.swap(ptrs2);

    // NEW: domain bounds for O(1) mapping
    min_key_ = leaf_keys_.front();
    max_key_ = leaf_keys_.back();

    clusters_.clear();

    trained_ = true;
    reset_stats();
}



template <typename P>
inline uint64_t LearnedRouter<P>::key_to_int(
    const Masstree::key<typename P::ikey_type>& k) const
{
    Masstree::Str fs = k.full_string();
    // fs = "k0000123"
    const char* p = fs.s + 1;          // skip 'k'
    uint64_t val = 0;
    for (int i = 1; i < fs.len; ++i) { // fixed-width digits
        val = val * 10 + uint64_t(p[i-1] - '0');
    }
    return val;
}




// learned_router.cc (or .hh if you inline it)
template<typename P>
inline bool LearnedRouter<P>::predict_leaf(
    const Masstree::key<typename P::ikey_type>& ka,
    Masstree::leaf<P>*& out_leaf) const
{
    if (!enabled_ || !trained_ || leaf_ptrs_.empty())
        return false;

    const uint64_t q = key_to_int(ka);

    // Fast clamps
    if (q <= min_key_) {
        out_leaf = leaf_ptrs_.front();
        return true;
    }
    if (q >= max_key_) {
        out_leaf = leaf_ptrs_.back();
        return true;
    }

    // O(1) learned mapping
    const size_t n = leaf_ptrs_.size();
    size_t idx = (size_t)((__uint128_t)(q - min_key_) * n
                          / (max_key_ - min_key_ + 1));

    // Safety clamp (virtually free)
    if (idx >= n) idx = n - 1;

    out_leaf = leaf_ptrs_[idx];
    return true;
}



template class LearnedRouter<Masstree::default_query_table_params>;

} // namespace Masstree
