
#pragma once
namespace Masstree {

// Process-wide runtime switches (C++17 inline variables avoid ODR issues).
struct RuntimeFlags {
    static inline bool learned = false;  // set from simplebench (--learned)
    static inline bool debug   = false;  // set from simplebench (--debug)
};

} // namespace Masstree
