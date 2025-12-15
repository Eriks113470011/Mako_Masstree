
#pragma once

#include <vector>
#include <cstdint>
#include <mutex>
#include "../masstree/masstree.hh"
#include "../masstree/masstree_struct.hh"
#include "../masstree/masstree_key.hh"
#include "../masstree/kpermuter.hh"

namespace Masstree {

template<typename P>
class LearnedRouter {
public:
    static LearnedRouter& instance() {
        static LearnedRouter inst;
        return inst;
    }

    void clear_samples() {
        leaf_keys_.clear();
        leaf_ptrs_.clear();
        clusters_.clear();
        trained_ = false;

        hits_ = 0;
        misses_ = 0;
        neighbor_hits_ = 0;
    }

    bool fast_path_ = true;  // DEADLINE MODE

    // ---- Counters (NON-ATOMIC) ---------------------------------
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;
    uint64_t neighbor_hits_ = 0;

    inline void reset_stats() {
        hits_ = 0;
        misses_ = 0;
        neighbor_hits_ = 0;
    }

    inline void record_hit()          { ++hits_; }
    inline void record_miss()         { ++misses_; }
    inline void record_neighbor_hit() { ++neighbor_hits_; }

    inline void print_stats() const {
        const uint64_t total = hits_ + misses_;
        const double acc = total ? (100.0 * double(hits_) / double(total)) : 0.0;

        printf("[Router] hits=%lu misses=%lu neighbor_hits=%lu accuracy=%.2f%%\n",
               (unsigned long)hits_,
               (unsigned long)misses_,
               (unsigned long)neighbor_hits_,
               acc);
    }

    // ---- Model + samples ----------------------------------------
    struct Cluster {
        uint64_t min_key, max_key;
        size_t start_idx, end_idx; // (unused currently)
        Masstree::leaf<P>* rep;
    };

    const std::vector<Cluster>& clusters() const { return clusters_; }
    bool is_trained() const { return trained_; }
    size_t num_samples() const { return leaf_keys_.size(); }

    inline void reset_model() {
        leaf_keys_.clear();
        leaf_ptrs_.clear();
        clusters_.clear();
        trained_ = false;
        reset_stats();
    }

    void add_leaf(uint64_t fk, Masstree::leaf<P>* leaf) {
        leaf_keys_.push_back(fk);
        leaf_ptrs_.push_back(leaf);
    }

    void add_sample(uint64_t fk, Masstree::leaf<P>* leaf) {
        // if (online_sampling_) {
        //     std::lock_guard<std::mutex> g(sample_mu_);
        //     leaf_keys_.push_back(fk);
        //     leaf_ptrs_.push_back(leaf);
        // } else {
        //     leaf_keys_.push_back(fk);
        //     leaf_ptrs_.push_back(leaf);
        // }
    }

    void train();
    bool predict_leaf(const Masstree::key<typename P::ikey_type>& ka,
                      Masstree::leaf<P>*& out_leaf) const;

    uint64_t key_to_int(const Masstree::key<typename P::ikey_type>& k) const;

    void set_enabled(bool e) { enabled_ = e; }
    bool enabled() const { return enabled_; }

    void set_online_sampling(bool) {}
    bool online_sampling() const { return false; }

    // Return a const reference to the sorted first keys (for interval checks)
    const std::vector<uint64_t>& leaf_keys() const { return leaf_keys_; }

    // Check if a query ikey q falls into the predicted interval using leaf_keys_.
    // Also return the predecessor index pos that we used (so you can hedge neighbors).
    bool interval_hit(uint64_t q, size_t& pos) const {
        if (!enabled_ || !is_trained() || leaf_keys_.empty()) { pos = 0; return false; }
        // predecessor: last first-key <= q
        auto it = std::upper_bound(leaf_keys_.begin(), leaf_keys_.end(), q);
        if (it == leaf_keys_.begin())      pos = 0;
        else if (it == leaf_keys_.end())   pos = leaf_keys_.size() - 1;
        else                               pos = size_t(it - leaf_keys_.begin() - 1);

        const uint64_t min = leaf_keys_[pos];
        const uint64_t max = (pos + 1 < leaf_keys_.size())
                        ? leaf_keys_[pos + 1] - 1
                        : std::numeric_limits<uint64_t>::max();
        return (q >= min && q <= max);
    }

    uint64_t min_key_ = 0;
    uint64_t max_key_ = 0;

    #ifndef LEARNED_DEBUG
    #define LEARNED_DEBUG 0
    #endif


    inline void debug_interval(const char* tag,
                            uint64_t q,
                            size_t pos,
                            const std::vector<uint64_t>& keys,
                            bool hit, bool neighbor)
    {
    #if LEARNED_DEBUG
        const uint64_t min = keys[pos];
        const uint64_t max = (pos + 1 < keys.size())
                        ? keys[pos + 1] - 1
                        : std::numeric_limits<uint64_t>::max();
        std::fprintf(stderr,
            "[LEARNED] %s q=%llu pos=%zu min=%llu max=%llu result=%s\n",
            tag,
            (unsigned long long)q,
            (unsigned long long)pos,
            (unsigned long long)min,
            (unsigned long long)max,
            hit ? "HIT" : (neighbor ? "NEIGHBOR" : "MISS"));
    #endif
    }

private:
    LearnedRouter() = default;

    std::vector<uint64_t> leaf_keys_;
    std::vector<Masstree::leaf<P>*> leaf_ptrs_;
    std::vector<Cluster> clusters_;

    bool trained_ = false;
    bool enabled_ = true;

    // Only used when online_sampling_ == true
    // mutable std::mutex sample_mu_;
    // bool online_sampling_ = false;
};

} // namespace Masstree

#include "learned_router_impl.hh"


