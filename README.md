# Learned Routing for Masstree in Mako

This project explores integrating a learned routing mechanism into **Masstree**, the in-memory index used by **Mako**, to accelerate point lookups under structured access patterns.

The goal is to evaluate whether learned techniques can reduce Masstree traversal overhead while preserving practical system behavior.

---

## Motivation

Masstree provides excellent performance across general workloads, but each lookup still requires traversing multiple internal nodes. Under structured or monotonic access patterns, this traversal cost can become redundant.

Recent work on learned indexes suggests that approximate models can predict where keys reside in an index. This project investigates whether a learned router can speculatively direct lookups to the correct Masstree leaf, reducing traversal overhead and improving performance.

---

## Design Overview

The system introduces a **LearnedRouter** that:
- Samples Masstree leaf boundaries during a training phase
- Predicts a candidate leaf for a query key at lookup time
- Speculatively jumps to that leaf before falling back to normal traversal if necessary

To further exploit locality, the implementation includes a **per-thread leaf cache** that remembers the most recently accessed leaf. Under monotonic access, this cache allows many lookups to bypass traversal entirely.

### Lookup Flow
1. Predict candidate leaf using the learned router
2. Attempt speculative access to the predicted leaf
3. Fall back to standard Masstree traversal if invalid
4. Cache successful leaf accesses per thread

---

## Implementation Details

- Learned routing logic is integrated into Masstree’s `find_unlocked` lookup path
- Routing is speculative and optimistic
- Correctness is preserved by falling back to Masstree traversal when needed
- Routing “hits” are defined as successful speculative routing attempts
- The system favors performance under structured workloads over strict adversarial robustness

---

## Evaluation

Evaluation is performed using Mako’s `simplebench` microbenchmark.

### Workloads Tested
- **Structured access** (keys queried in insertion order)
- **Random access** (keys shuffled)

### Results Summary
- Under structured access, learned routing combined with per-thread caching achieves up to **4× speedup** over baseline Masstree
- Under random access, benefits diminish and performance approaches baseline
- Speedups rely heavily on access locality and speculative execution

These results demonstrate that learned routing is effective when workload structure aligns with model assumptions, but does not universally outperform traditional indexing.

---

## How to Build and Run

### Build
```bash
make -j

## Build Requirements

- Linux (tested on Ubuntu 20.04+)
- GCC / Clang with C++17 support
- CMake
- pthreads
- Python 3 (optional, for auxiliary scripts)

The project builds as part of the existing Mako build system and does not require additional external libraries.

## Configuration Options

The learned routing logic can be enabled or disabled at compile time and runtime.

### Compile-Time Flags
- `MAKO_LEARNED_INDEX`  
  Enables learned routing integration into Masstree.

### Runtime Flags
- `RuntimeFlags::learned`  
  Toggles learned routing during execution.

These flags allow direct comparison between baseline Masstree and the learned routing implementation without rebuilding the system.


## Router Statistics

During benchmarking, the learned router reports the following statistics:

- `hits`  
  Number of lookups where speculative learned routing was attempted.
- `misses`  
  Number of lookups where learned routing was not used.
- `neighbor_hits`  
  Reserved for future work involving neighbor-based verification.

A high hit count indicates that learned routing or caching was actively used during lookup execution. Hits do not imply strict correctness guarantees; correctness is ensured by Masstree’s fallback traversal when necessary.

## Experimental Methodology

All experiments were conducted using the `simplebench` microbenchmark included with Mako.

For each configuration:
1. Keys are preloaded into the Masstree index
2. Leaf samples are collected for training
3. The learned router is trained
4. Baseline and learned lookups are timed independently

Structured workloads issue queries in insertion order, while randomized workloads shuffle query keys to remove locality.

## Known Limitations

- Learned routing does not provide universal speedups
- Benefits diminish under randomized access patterns
- Strict correctness enforcement significantly reduces performance gains
- The per-thread cache relies on locality and optimistic assumptions

These limitations reflect fundamental trade-offs between learned approximations and concurrent data structure correctness.

## Future Work

Potential extensions include:
- Neighbor-based leaf verification
- More robust learned models
- Adaptive fallback thresholds
- Hybrid routing strategies combining learned and heuristic methods

Exploring these directions may improve robustness while retaining performance benefits.

## Author

Erik Swanson  
MS Computer Science  
Stony Brook University
