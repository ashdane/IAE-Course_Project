/*
 * Cache-Aware and Cache-Oblivious Sorting Algorithms
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <sys/mman.h>
#include <vector>

// Hardware constants (tune per platform; target x86-64 Skylake)
static constexpr std::size_t CACHE_LINE      = 64;
static constexpr std::size_t L1_DATA_BYTES   = 32 * 1024;
static constexpr std::size_t ELEM_BYTES      = 8;
static constexpr std::size_t BLOCK_SIZE      = CACHE_LINE / ELEM_BYTES;
static constexpr int         K_WAY           = static_cast<int>((L1_DATA_BYTES / 4) / (BLOCK_SIZE * ELEM_BYTES));
static constexpr std::size_t BASE_CASE_ELEMS = 64;
static constexpr int         PREFETCH_DIST   = 60;

using Key = int64_t;
static constexpr Key INF_KEY = INT64_MAX;

// Arena for pre-allocated memory
struct Arena {
    Key*        base   = nullptr;
    std::size_t total  = 0;
    std::size_t cursor = 0;

    explicit Arena(std::size_t n_elements) : total(n_elements) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, CACHE_LINE, n_elements * sizeof(Key)) != 0) {
            std::cerr << "posix_memalign failed\n"; std::exit(1);
        }
        base = reinterpret_cast<Key*>(ptr);
        madvise(base, n_elements * sizeof(Key), MADV_HUGEPAGE);
    }

    Key* alloc(std::size_t n) {
        if (cursor + n > total) { std::cerr << "Arena exhausted\n"; std::exit(1); }
        Key* p = base + cursor; cursor += n; return p;
    }
    void reset() { cursor = 0; }
    ~Arena() { free(base); }
};

// Branchless insertion sort
inline void insertion_sort(Key* a, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i) {
        Key key = a[i];
        std::size_t j = i;
        while (j > 0 && a[j-1] > key) { a[j] = a[j-1]; --j; }
        a[j] = key;
    }
}

// Winner Tree for k-way merge
struct WinnerTree {
    static constexpr int MAXK = 256;
    int  tree[2 * MAXK];
    Key  head[MAXK + 1];
    int  k;

    void init(int nk, Key** rp, const std::size_t* rl, const std::size_t* rpos) {
        k = nk;
        for (int r = 0; r < k; ++r)
            head[r] = (rpos[r] < rl[r]) ? rp[r][rpos[r]] : INF_KEY;
        for (int r = 0; r < k; ++r) tree[k + r] = r;
        for (int i = k - 1; i >= 1; --i) {
            int l = tree[2*i], ri = tree[2*i+1];
            tree[i] = (head[l] <= head[ri]) ? l : ri;
        }
    }

    int  winner()     const { return tree[1]; }
    Key  winner_key() const { return head[tree[1]]; }

    void update(int w) {
        int i = (w + k) >> 1;
        while (i >= 1) {
            int l = tree[2*i], ri = tree[2*i+1];
            tree[i] = (head[l] <= head[ri]) ? l : ri;
            i >>= 1;
        }
    }
};

// Cache-Aware: k-Way Merge Sort
void cache_aware_sort(Key* data, std::size_t n, Arena& arena) {
    if (n <= BASE_CASE_ELEMS) { insertion_sort(data, n); return; }

    const std::size_t RUN_LEN  = L1_DATA_BYTES / (2 * sizeof(Key));
    const int         num_runs = static_cast<int>((n + RUN_LEN - 1) / RUN_LEN);

    for (int r = 0; r < num_runs; ++r) {
        std::size_t off = static_cast<std::size_t>(r) * RUN_LEN;
        std::size_t len = std::min(RUN_LEN, n - off);
        std::sort(data + off, data + off + len);
    }
    if (num_runs == 1) return;

    Key* out = arena.alloc(n);
    Key* tmp = arena.alloc(n);

    auto do_merge = [](int nruns, Key** sp, std::size_t* sl, Key* dst) -> std::size_t {
        std::size_t rpos[WinnerTree::MAXK] = {};
        WinnerTree wt;
        wt.init(nruns, sp, sl, rpos);
        std::size_t oi = 0;
        while (wt.winner_key() != INF_KEY) {
            int w  = wt.winner();
            int pf = static_cast<int>(rpos[w]) + PREFETCH_DIST;
            if (static_cast<std::size_t>(pf) < sl[w])
                __builtin_prefetch(&sp[w][pf], 0, 3);
            dst[oi++] = wt.head[w];
            ++rpos[w];
            wt.head[w] = (rpos[w] < sl[w]) ? sp[w][rpos[w]] : INF_KEY;
            wt.update(w);
        }
        return oi;
    };

    if (num_runs <= K_WAY) {
        Key*        sp[WinnerTree::MAXK];
        std::size_t sl[WinnerTree::MAXK];
        for (int r = 0; r < num_runs; ++r) {
            std::size_t off = static_cast<std::size_t>(r) * RUN_LEN;
            sp[r] = data + off;
            sl[r] = std::min(RUN_LEN, n - off);
        }
        do_merge(num_runs, sp, sl, out);
    } else {
        int         num_groups = (num_runs + K_WAY - 1) / K_WAY;
        Key*        gptr[WinnerTree::MAXK];
        std::size_t gsz[WinnerTree::MAXK];
        std::size_t tc = 0;
        for (int g = 0; g < num_groups; ++g) {
            int         first_run = g * K_WAY;
            int         nruns     = std::min(K_WAY, num_runs - first_run);
            Key*        sp[WinnerTree::MAXK];
            std::size_t sl[WinnerTree::MAXK];
            for (int r = 0; r < nruns; ++r) {
                std::size_t off = static_cast<std::size_t>(first_run + r) * RUN_LEN;
                sp[r] = data + off;
                sl[r] = std::min(RUN_LEN, n - off);
            }
            gptr[g] = tmp + tc;
            gsz[g]  = do_merge(nruns, sp, sl, gptr[g]);
            tc += gsz[g];
        }
        do_merge(num_groups, gptr, gsz, out);
    }
    std::memcpy(data, out, n * sizeof(Key));
}

// Cache-Oblivious: Recursive Merge Sort
static void merge_branchless(const Key* __restrict__ la, std::size_t la_n,
                              const Key* __restrict__ lb, std::size_t lb_n,
                              Key* __restrict__ out) {
    std::size_t ia = 0, ib = 0, io = 0;
    while (ia < la_n && ib < lb_n) {
        int pick_a  = static_cast<int>(la[ia] <= lb[ib]);
        out[io++]   = pick_a ? la[ia] : lb[ib];
        ia += static_cast<std::size_t>(pick_a);
        ib += static_cast<std::size_t>(1 - pick_a);
    }
    while (ia < la_n) out[io++] = la[ia++];
    while (ib < lb_n) out[io++] = lb[ib++];
}

static void co_sort_impl(Key* a, std::size_t n, Key* tmp) {
    if (n <= BASE_CASE_ELEMS) { insertion_sort(a, n); return; }
    std::size_t mid = n / 2;
    co_sort_impl(a,       mid,     tmp);
    co_sort_impl(a + mid, n - mid, tmp);
    merge_branchless(a, mid, a + mid, n - mid, tmp);
    std::memcpy(a, tmp, n * sizeof(Key));
}

void cache_oblivious_sort(Key* data, std::size_t n, Arena& arena) {
    Key* tmp = arena.alloc(n);
    co_sort_impl(data, n, tmp);
}

// Benchmarking harness
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;
struct Result { double ms; bool correct; };

template <typename Fn>
Result benchmark(const std::vector<Key>& src, std::size_t n,
                 const std::vector<Key>& ref, Fn fn, Arena& arena,
                 int warmup = 1, int runs = 3) {
    double total = 0.0; bool ok = true;
    std::vector<Key> buf(n);
    for (int r = -warmup; r < runs; ++r) {
        std::copy(src.begin(), src.end(), buf.begin());
        arena.reset();
        auto t0 = Clock::now();
        fn(buf.data(), n, arena);
        auto t1 = Clock::now();
        if (r >= 0) { total += Ms(t1 - t0).count(); if (buf != ref) ok = false; }
    }
    return { total / runs, ok };
}

int main(int argc, char** argv) {
    std::vector<std::size_t> sizes = {100'000, 1'000'000, 5'000'000, 10'000'000, 50'000'000};
    if (argc > 1) {
        sizes.clear();
        for (int i = 1; i < argc; ++i) {
            sizes.push_back(std::atoll(argv[i]));
        }
    }

    std::cout << "Cache-Aware vs Cache-Oblivious Sorting\n";
    std::mt19937_64 rng(42);

    auto evaluate_distribution = [&](const std::string& dist_name, bool nearly_sorted) {
        std::cout << "\n--- " << dist_name << " Distribution ---\n";
        std::cout << std::left << std::setw(15) << "N" 
                  << std::right << std::setw(20) << "Cache-Aware (ms)"
                  << std::setw(25) << "Cache-Oblivious (ms)"
                  << std::setw(20) << "std::sort (ms)" << "\n";
        std::cout << std::string(80, '-') << "\n";

        for (std::size_t N : sizes) {
            std::uniform_int_distribution<Key> dist(0, static_cast<Key>(N) * 10);
            std::vector<Key> data(N);
            for (auto& x : data) x = dist(rng);
            
            std::vector<Key> ref_data = data;
            std::sort(ref_data.begin(), ref_data.end());

            if (nearly_sorted) {
                data = ref_data;
                std::uniform_int_distribution<std::size_t> idx_dist(0, N - 1);
                std::size_t num_swaps = std::max<std::size_t>(1, N / 100); // 1% swaps
                for (std::size_t i = 0; i < num_swaps; ++i) {
                    std::swap(data[idx_dist(rng)], data[idx_dist(rng)]);
                }
                ref_data = data;
                std::sort(ref_data.begin(), ref_data.end());
            }

            Arena arena(4 * N);

            auto r1 = benchmark(data, N, ref_data,
                [](Key* d, std::size_t n, Arena& a){ cache_aware_sort(d, n, a); }, arena);
            
            auto r2 = benchmark(data, N, ref_data,
                [](Key* d, std::size_t n, Arena& a){ cache_oblivious_sort(d, n, a); }, arena);
            
            auto r3 = benchmark(data, N, ref_data,
                [](Key* d, std::size_t n, Arena&){ std::sort(d, d + n); }, arena);

            std::string err = "";
            if (!r1.correct || !r2.correct || !r3.correct) {
                err = " [WRONG]";
            }

            std::cout << std::left << std::setw(15) << N 
                      << std::right << std::setw(20) << std::fixed << std::setprecision(2) << r1.ms
                      << std::setw(25) << std::fixed << std::setprecision(2) << r2.ms
                      << std::setw(20) << std::fixed << std::setprecision(2) << r3.ms
                      << err << "\n";
        }
    };

    evaluate_distribution("Random", false);
    evaluate_distribution("Nearly-Sorted", true);

    return 0;
}
