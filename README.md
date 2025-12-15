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

* Samples Masstree leaf boundaries during a training phase
* Predicts a candidate leaf for a query key at lookup time
* Speculatively jumps to that leaf before falling back to normal traversal if necessary

To further exploit locality, the implementation includes a **per-thread leaf cache** that remembers the most recently accessed leaf. Under monotonic access, this cache allows many lookups to bypass traversal entirely.

### Lookup Flow

1. Predict candidate leaf using the learned router
2. Attempt speculative access to the predicted leaf
3. Fall back to standard Masstree traversal if invalid
4. Cache successful leaf accesses per thread

---

## Implementation Details

* Learned routing logic is integrated into Masstree’s `find_unlocked` lookup path
* Routing is speculative and optimistic
* Correctness is preserved by falling back to Masstree traversal when needed
* Routing “hits” are defined as successful speculative routing attempts
* The system favors performance under structured workloads over strict adversarial robustness

---

## Evaluation

Evaluation is performed using Mako’s `simplebench` microbenchmark.

### Workloads Tested

* **Structured / sequential access** (keys queried in insertion order)

Randomized key access was intentionally removed in the final version to preserve locality assumptions and ensure stable learned routing behavior.

### Results Summary

* Under structured access, learned routing combined with per-thread caching achieves up to **4× speedup** over baseline Masstree
* Lookup latency is significantly reduced by bypassing internal node traversal
* Performance improvements are consistent across multiple workload variants when access locality is preserved

These results demonstrate that learned routing can substantially improve performance when workload structure aligns with model assumptions.

---

## How to Build and Run

### Build Requirements

* Linux (tested on Ubuntu 20.04+)
* GCC or Clang with C++17 support
* CMake
* pthreads
* Rust toolchain (required by Mako)

---

### 1. Install System Dependencies

```bash
bash apt_packages.sh
```

---

### 2. Install Rust Toolchain

```bash
source install_rustc.sh
```

Verify installation:

```bash
rustc --version
cargo --version
```

---

### 3. Build the Project

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

The `simplebench` benchmark binary will be generated in the `build/` directory.

---

## Running `simplebench`

### Usage

```bash
./simplebench <threads> <num_ops> <workload>
```

### Arguments

* `<threads>` – Number of worker threads
* `<num_ops>` – Total number of operations
* `<workload>` – Access pattern:

  * `B` – Bulk / sequential access
  * `C` – Clustered access
  * `D` – Structured variant with locality

---

### Example Runs

```bash
./simplebench 4 5000000 B
./simplebench 4 5000000 C
./simplebench 4 5000000 D
```

---

## Router Statistics

During benchmarking, the learned router reports:

* `hits` – Successful speculative routing attempts
* `misses` – Lookups that fell back to normal traversal
* `neighbor_hits` – Reserved for future extensions

A high hit count indicates that learned routing or caching was actively used. Correctness is always ensured by Masstree’s fallback traversal.

---

## Known Limitations

* Learned routing relies on structured access patterns
* Performance gains diminish if locality assumptions are violated
* The per-thread cache is optimistic and locality-dependent

These limitations reflect trade-offs between learned approximations and concurrent data structure safety.

---

## Future Work

* Neighbor-based verification
* More robust learned models
* Adaptive routing thresholds
* Hybrid learned / heuristic routing strategies

---

## Repository

[https://github.com/Eriks113470011/Mako_Masstree](https://github.com/Eriks113470011/Mako_Masstree)

---

## Author

Erik Swanson
MS Computer Science
Stony Brook University
