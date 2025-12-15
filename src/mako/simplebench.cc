#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <cstring>
#include <random>
#include <cstdio>
#include <limits>

#include "masstree/query_masstree.hh"
#include "masstree/masstree.hh"
#include "masstree/masstree_tcursor.hh"
#include "masstree/masstree_insert.hh"
#include "masstree/masstree_get.hh"
#include "masstree/masstree_scan.hh"
#include "masstree/masstree_key.hh"

#include "learned/learned_runtime.hh"
#include "learned/learned_runtime_flags.hh"
#include "learned/learned_router.hh"

using namespace Masstree;

// --------------------------------------------------------
// UTIL
// --------------------------------------------------------

inline double ms_since(const std::chrono::high_resolution_clock::time_point& start) {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

static inline std::string make_key(size_t i) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "k%07zu", i);
    return std::string(buf);
}

// --------------------------------------------------------
// CONFIG
// --------------------------------------------------------

struct BenchConfig {
    size_t      num_threads = 1;
    size_t      num_ops     = 100000;
    std::string workload    = "A";   // A, B, C, D
};

// --------------------------------------------------------
// SCAN VISITOR
// --------------------------------------------------------

struct ScanVisitor {
    int remain;
    threadinfo* ti;

    ScanVisitor(int n, threadinfo& t) : remain(n), ti(&t) {}

    template <typename SS, typename K>
    inline void visit_leaf(const SS&, const K&, threadinfo&) {}

    inline bool visit_value(Str, row_type*, threadinfo&) {
        return --remain > 0;
    }
};

// --------------------------------------------------------
// OPERATIONS
// --------------------------------------------------------

void do_insert(threadinfo& ti,
               basic_table<default_query_table_params>& table,
               size_t idx)
{
    using Params = default_query_table_params;
    std::string key = make_key(idx);

    tcursor<Params> c(table, Str(key.data(), key.size()));
    c.find_insert(ti);

    row_type* row = row_type::create1(Str("v"), ti.update_timestamp(), ti);
    c.value() = row;
    c.finish(1, ti);
}

bool do_read(threadinfo& ti,
             basic_table<default_query_table_params>& table,
             size_t idx)
{
    using Params = default_query_table_params;
    std::string key = make_key(idx);

#if MAKO_LEARNED_INDEX
    if (Masstree::RuntimeFlags::learned) {
        auto& router = LearnedRouter<Params>::instance();
        if (router.enabled() && router.is_trained()) {
            Masstree::key<typename Params::ikey_type> ka(Str(key.data(), key.size()));
            Masstree::leaf<Params>* leaf = nullptr;
            router.predict_leaf(ka, leaf);
            if (leaf)
                __builtin_prefetch(leaf, 0, 2);
        }
    }
#endif

    row_type* out = nullptr;
    return table.get(Str(key.data(), key.size()), out, ti);
}

bool do_scan(threadinfo& ti,
             basic_table<default_query_table_params>& table,
             const std::string& prefix)
{
    ScanVisitor visitor(100, ti);
    table.scan(Str(prefix), true, visitor, ti);
    return true;
}

// --------------------------------------------------------
// WORKER
// --------------------------------------------------------

struct WorkerArgs {
    query_table<default_query_table_params>* qtable;
    size_t start;
    size_t end;
    const BenchConfig* cfg;
    size_t ops_done = 0;
};

void worker(WorkerArgs* args)
{
    auto& table = args->qtable->table();
    threadinfo* ti = threadinfo::make(threadinfo::TI_PROCESS, args->start % 1000);

    std::mt19937_64 rng(args->start + 0xdeadbeef);
    std::uniform_int_distribution<size_t> dist(args->start, args->end - 1);

    for (size_t i = args->start; i < args->end; ++i) {
        size_t k = dist(rng);

        if (args->cfg->workload == "A") {
            if (rng() & 1) do_read(*ti, table, k);
            else           do_insert(*ti, table, k);
        }
        else if (args->cfg->workload == "B") {
            if ((rng() % 100) < 95) do_read(*ti, table, k);
            else                    do_insert(*ti, table, k);
        }
        else if (args->cfg->workload == "C") {
            do_read(*ti, table, k);
        }
        else if (args->cfg->workload == "D") {
            if (rng() & 1) do_read(*ti, table, k);
            else           do_scan(*ti, table, make_key(k));
        }
        else {
            do_read(*ti, table, k);
        }

        args->ops_done++;
    }
}

// --------------------------------------------------------
// BENCHMARK
// --------------------------------------------------------

void run_benchmark(const BenchConfig& cfg)
{
    using Params = default_query_table_params;

    threadinfo* ti0 = threadinfo::make(threadinfo::TI_MAIN, -1);
    query_table<Params> qtable;
    qtable.initialize(*ti0);

    auto& table = qtable.table();

    // ----------------------------------------------------
    // PRELOAD
    // ----------------------------------------------------
    if (cfg.workload == "B" || cfg.workload == "C" || cfg.workload == "D") {
        std::cout << "Preloading " << cfg.num_ops << " keys...\n";
        for (size_t i = 0; i < cfg.num_ops; ++i)
            do_insert(*ti0, table, i);
    }

#if MAKO_LEARNED_INDEX
    Masstree::RuntimeFlags::learned = true;
    Masstree::RuntimeFlags::debug   = false;
#else
    Masstree::RuntimeFlags::learned = false;
#endif

#if MAKO_LEARNED_INDEX
    // ----------------------------------------------------
    // LEARNED ROUTER TRAINING (MISSING PIECE — NOW FIXED)
    // ----------------------------------------------------
    if (Masstree::RuntimeFlags::learned &&
        (cfg.workload == "B" || cfg.workload == "C" || cfg.workload == "D")) {

        auto& router = LearnedRouter<Params>::instance();
        router.clear_samples();
        router.set_enabled(true);
        router.set_online_sampling(false);

        std::cout << "Collecting leaf samples...\n";

        std::string k0 = make_key(0);
        unlocked_tcursor<Params> uc(table, Str(k0.data(), k0.size()));
        uc.find_unlocked(*ti0);

        Masstree::leaf<Params>* leaf = uc.node();
        while (leaf && leaf->prev_) leaf = leaf->prev_;

        size_t leaf_count = 0;
        while (leaf) {
            router.add_leaf(leaf->first_key_uint64(), leaf);
            leaf = leaf->safe_next();
            leaf_count++;
        }

        std::cout << "Collected " << leaf_count << " leaves.\n";
        std::cout << "Training learned index...\n";

        router.train();

        if (!router.is_trained()) {
            std::cerr << "[LEARNED] ERROR: router failed to train\n";
            std::abort();
        }
    }
#endif

    // ----------------------------------------------------
    // EXECUTION
    // ----------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads(cfg.num_threads);
    std::vector<WorkerArgs> args(cfg.num_threads);

    size_t per = cfg.num_ops / cfg.num_threads;

    for (size_t t = 0; t < cfg.num_threads; ++t) {
        args[t] = { &qtable,
                    t * per,
                    (t == cfg.num_threads - 1) ? cfg.num_ops : (t + 1) * per,
                    &cfg };
        threads[t] = std::thread(worker, &args[t]);
    }

    size_t total = 0;
    for (auto& th : threads) th.join();
    for (auto& a : args) total += a.ops_done;

    double ms = ms_since(start);

    std::cout << "=====================\n";
    std::cout << "   Masstree Benchmark\n";
    std::cout << "=====================\n";
    std::cout << "Workload: " << cfg.workload << "\n";
    std::cout << "Threads:  " << cfg.num_threads << "\n";
    std::cout << "Ops:      " << total << "\n";
    std::cout << "Time:     " << ms << " ms\n";
    std::cout << "Ops/sec:  " << (total / (ms / 1000.0)) << "\n\n";

#if MAKO_LEARNED_INDEX
    LearnedRouter<Params>::instance().print_stats();
#endif

#if MAKO_LEARNED_INDEX
    // ----------------------------------------------------
    // LEARNED INDEX MICROBENCHMARK (POINT LOOKUPS ONLY)
    // ----------------------------------------------------
    {
        auto& router = LearnedRouter<Params>::instance();
        std::cout << "=== Learned Index Microbenchmark ===\n";

        threadinfo* ti = threadinfo::make(threadinfo::TI_PROCESS, 777);

        const size_t N = 50000;
        std::vector<std::string> keys(N);
        for (size_t i = 0; i < N; ++i)
            keys[i] = make_key(i);

        auto run_gets = [&](double& ms_out) {
            auto begin = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < N; ++i) {
                row_type* out = nullptr;
                table.get(Str(keys[i].data(), keys[i].size()), out, *ti);
            }
            auto end = std::chrono::high_resolution_clock::now();
            ms_out = std::chrono::duration<double, std::milli>(end - begin).count();
        };

        double baseline_ms = 0.0;
        double learned_ms  = 0.0;

        router.set_enabled(true);
        router.reset_stats();
        run_gets(learned_ms);

        router.set_enabled(false);
        router.reset_stats();
        run_gets(baseline_ms);


        std::cout << "Trained?     " << router.is_trained() << "\n";
        std::cout << "#Samples     " << router.num_samples() << "\n";
        std::cout << "Baseline GET " << baseline_ms << " ms\n";
        std::cout << "Learned GET  " << learned_ms  << " ms\n";
        std::cout << "Speedup      " << (baseline_ms / learned_ms) << "x\n";
    }
#endif

}

// --------------------------------------------------------
// MAIN
// --------------------------------------------------------

int main(int argc, char** argv)
{
    BenchConfig cfg;
    if (argc >= 2) cfg.num_threads = std::stoul(argv[1]);
    if (argc >= 3) cfg.num_ops     = std::stoul(argv[2]);
    if (argc >= 4) cfg.workload    = argv[3];

    std::cout << "Running benchmark:\n";
    std::cout << "Threads = " << cfg.num_threads
              << " Ops = " << cfg.num_ops
              << " Workload = " << cfg.workload << "\n\n";

    run_benchmark(cfg);
    return 0;
}



