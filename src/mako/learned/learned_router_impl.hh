#pragma once

namespace Masstree {

template <typename P>
void LearnedRouter<P>::train() {
    // TODO: your training code
   trained_ = true;
}

template <typename P>
uint64_t LearnedRouter<P>::key_to_int(const Masstree::key<typename P::ikey_type>& k) const {
    // TODO: implement conversion based on Masstree key API
    // (see note below)
    return 0;
}

template <typename P>
bool LearnedRouter<P>::predict_leaf(const Masstree::key<typename P::ikey_type>& ka,
                                   Masstree::leaf<P>*& out_leaf) const {
    // TODO: your prediction code
    out_leaf = nullptr;
    return false;
}

template <typename P>
inline void record_prediction(LearnedRouter<P>& router,
                              const Masstree::leaf<P>* predicted,
                              const Masstree::leaf<P>* actual) {
    if (!router.enabled() || !router.is_trained()) return;

    if (!predicted || !actual) { router.record_miss(); return; }
    if (predicted == actual)   { router.record_hit();  return; }

    if (auto n = predicted->safe_next(); n && n == actual) {
        router.record_neighbor_hit();
        return;
    }
    router.record_miss();
}


} // namespace Masstree
