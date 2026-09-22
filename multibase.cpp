// Exhaustive Keith-number search in an arbitrary base by branch and bound.
//
//   g++ -O3 -std=c++17 -pthread keithb.cpp -o keithb
//   ./keithb BASE LOWER UPPER [THREADS] [--parity]
//
// --parity enables the mod-2 periodicity filter (Theorem A) so its effect can
// be measured by ablation.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Int = __int128_t;

static std::string show(Int x) {
    if (x == 0) return "0";
    bool neg = x < 0; if (neg) x = -x;
    std::string s;
    while (x) { s += char('0' + int(x % 10)); x /= 10; }
    if (neg) s += '-';
    std::reverse(s.begin(), s.end());
    return s;
}

static int BASE = 4;
static bool USE_PARITY = false;
static bool PARITY_FIRST = false;
static bool USE_MOD = false;
static bool USE_MOD2 = false;
static const int MOD2 = 63;   // coprime to 64; bits 0..62 of a word
static const int MOD = 64;   // residues tracked as a 64-bit mask
static inline uint64_t rot63(uint64_t x, int r) {
    r %= MOD2; if (r < 0) r += MOD2;
    const uint64_t m = (1ULL << MOD2) - 1ULL;
    return ((x << r) | (x >> (MOD2 - r ? MOD2 - r : MOD2))) & m;
}
static inline uint64_t rotl64(uint64_t x, int r) {
    r &= 63;
    return r ? ((x << r) | (x >> (64 - r))) : x;   // shifting by 64 is undefined
}

static Int powb(int e) { Int r = 1; while (e--) r *= BASE; return r; }

static std::vector<int> digits_of(Int n) {
    std::vector<int> d;
    while (n) { d.push_back(int(n % BASE)); n /= BASE; }
    if (d.empty()) d.push_back(0);
    std::reverse(d.begin(), d.end());
    return d;
}

static int hit_index(Int n) {
    std::vector<int> d = digits_of(n);
    int k = int(d.size());
    if (k < 2) return 0;
    std::vector<Int> w(d.begin(), d.end());
    Int s = 0; for (Int x : w) s += x;
    int idx = k, head = 0;
    while (true) {
        Int nxt = s; ++idx;
        if (nxt >= n) return nxt == n ? idx : 0;
        s += nxt - w[head]; w[head] = nxt; head = (head + 1) % k;
    }
}

struct Equation { int k, m; Int lo, hi; std::vector<Int> coeff; };

static std::vector<Equation> equations(Int lower, Int upper) {
    std::vector<Equation> out;
    int ka = int(digits_of(lower).size()), kb = int(digits_of(upper).size());
    for (int k = std::max(2, ka); k <= kb; ++k) {
        Int lo = std::max(lower, powb(k - 1)), hi = std::min(upper, powb(k) - 1);
        if (lo > hi) continue;
        std::vector<std::vector<Int>> c;
        for (int i = 0; i < k; ++i) { std::vector<Int> e(k, 0); e[i] = 1; c.push_back(e); }
        for (int m = k + 1; ; ++m) {
            std::vector<Int> nc(k, 0);
            for (int b = 1; b <= k; ++b)
                for (int i = 0; i < k; ++i) nc[i] += c[c.size() - b][i];
            c.push_back(nc);
            if (nc[0] > hi) break;
            Int biggest = 0;
            for (Int x : nc) biggest += (BASE - 1) * x;
            if (biggest >= lo) out.push_back({k, m, lo, hi, nc});
        }
    }
    return out;
}

struct Solver {
    const Equation& eq;
    int k, maxd;
    std::vector<Int> sw, place;
    std::vector<int> pos;
    Int shift = 0;
    std::vector<std::vector<Int>> lowB, highB;
    std::vector<std::vector<uint64_t>> reach;   // residues mod 64
    std::vector<std::vector<uint64_t>> reach2;  // residues mod 63
    std::set<Int> found;
    unsigned long long nodes = 0;
    // Theorem A: parity of a_j has period k+1 with pattern (d_1..d_k, S).
    // A hit at m forces  parity(N) == p_i  with  i = ((m-1) mod (k+1)) + 1,
    // where p_i = d_i for i <= k and p_{k+1} = S. Since parity(N) = d_k when the
    // base is even and S when it is odd, this becomes one or two unary parity
    // constraints once S (and, for even bases, the parity of d_k) is fixed.
    std::vector<int> req;      // required parity per digit index, -1 = free
    int par_i = -1;            // 0-based index of the pinned digit, -1 if none
    bool par_is_sum = false;   // the pinned value is S rather than a digit

    explicit Solver(const Equation& e) : eq(e), k(e.k), maxd(BASE - 1) {}

    void walk(int at, int left_sum, Int target, Int value) {
        ++nodes;
        int left = k - at;
        if (left_sum < 0 || left_sum > maxd * left) return;
        if (target < lowB[at][left_sum] || target > highB[at][left_sum]) return;
        if (USE_MOD) {
            Int r = target % MOD; if (r < 0) r += MOD;
            if (!((reach[at][left_sum] >> int(r)) & 1ULL)) return;
        }
        if (USE_MOD2) {
            Int r = target % MOD2; if (r < 0) r += MOD2;
            if (!((reach2[at][left_sum] >> int(r)) & 1ULL)) return;
        }
        if (at == k) {
            if (left_sum == 0 && target == 0 && value >= eq.lo && value <= eq.hi) {
                if (int(digits_of(value).size()) != k) throw std::runtime_error("width mismatch");
                if (hit_index(value) != eq.m) throw std::runtime_error("hit-index mismatch");
                found.insert(value);
            }
            return;
        }
        int lo_d = (pos[at] == 0) ? 1 : 0;
        int want = req[pos[at]];
        for (int d = lo_d; d <= maxd; ++d) {
            if (want >= 0 && (d & 1) != want) continue;
            walk(at + 1, left_sum - d, target - d * sw[at], value + d * place[at]);
        }
    }

    void run() {
        std::vector<Int> w(k);
        for (int i = 0; i < k; ++i) w[i] = powb(k - 1 - i) - eq.coeff[i];

        Int lo = std::min(w[0], Int(maxd) * w[0]), hi = std::max(w[0], Int(maxd) * w[0]);
        for (int i = 1; i < k; ++i) {
            lo += std::min<Int>(0, Int(maxd) * w[i]);
            hi += std::max<Int>(0, Int(maxd) * w[i]);
        }
        if (lo > 0 || hi < 0) return;

        shift = -*std::min_element(w.begin(), w.end());
        if (shift < 0) shift = 0;

        pos.resize(k); std::iota(pos.begin(), pos.end(), 0);
        std::stable_sort(pos.begin(), pos.end(),
            [&](int a, int b) { return (w[a] + shift) > (w[b] + shift); });

        int idx = ((eq.m - 1) % (k + 1)) + 1;       // 1-based position in the parity cycle
        if (idx == k + 1) par_is_sum = true; else par_i = idx - 1;

        if (USE_PARITY && PARITY_FIRST) {
            // Visit the parity-constrained digits first so the filter cuts the
            // whole subtree rather than only its last level.
            std::vector<int> pin;
            if (par_i >= 0) pin.push_back(par_i);
            if (BASE % 2 == 0) pin.push_back(k - 1);
            std::stable_sort(pos.begin(), pos.end(), [&](int a, int b) {
                bool pa = std::find(pin.begin(), pin.end(), a) != pin.end();
                bool pb = std::find(pin.begin(), pin.end(), b) != pin.end();
                return pa && !pb;
            });
        }

        // sw and place must be built from the FINAL visit order: the DP bounds
        // below are indexed by depth, so any later permutation of pos would
        // silently desynchronise them and prune valid branches.
        sw.resize(k); place.resize(k);
        for (int i = 0; i < k; ++i) { sw[i] = w[pos[i]] + shift; place[i] = powb(k - 1 - pos[i]); }

        lowB.assign(k + 1, {}); highB.assign(k + 1, {});
        lowB[k] = {0}; highB[k] = {0};
        const Int BIG = Int(1) << 120;
        for (int i = k - 1; i >= 0; --i) {
            int cap = maxd * (k - i);
            lowB[i].assign(cap + 1, BIG); highB[i].assign(cap + 1, -BIG);
            for (int s = 0; s <= cap; ++s)
                for (int d = 0; d <= maxd; ++d) {
                    int rest = s - d;
                    if (rest < 0 || rest >= int(lowB[i + 1].size())) continue;
                    lowB[i][s]  = std::min(lowB[i][s],  d * sw[i] + lowB[i + 1][rest]);
                    highB[i][s] = std::max(highB[i][s], d * sw[i] + highB[i + 1][rest]);
                }
        }
        if (USE_MOD) {
            reach.assign(k + 1, {});
            reach[k] = {1ULL};                       // empty suffix makes only 0
            for (int i = k - 1; i >= 0; --i) {
                int cap = maxd * (k - i);
                reach[i].assign(cap + 1, 0ULL);
                int off = int(((sw[i] % MOD) + MOD) % MOD);
                for (int sm = 0; sm <= cap; ++sm) {
                    uint64_t acc = 0;
                    for (int d = 0; d <= maxd; ++d) {
                        int rest = sm - d;
                        if (rest < 0 || rest >= int(reach[i + 1].size())) continue;
                        acc |= rotl64(reach[i + 1][rest], (d * off) % MOD);
                    }
                    reach[i][sm] = acc;
                }
            }
        }

        if (USE_MOD2) {
            reach2.assign(k + 1, {});
            reach2[k] = {1ULL};
            for (int i = k - 1; i >= 0; --i) {
                int cap = maxd * (k - i);
                reach2[i].assign(cap + 1, 0ULL);
                int off = int(((sw[i] % MOD2) + MOD2) % MOD2);
                for (int sm = 0; sm <= cap; ++sm) {
                    uint64_t acc = 0;
                    for (int d = 0; d <= maxd; ++d) {
                        int rest = sm - d;
                        if (rest < 0 || rest >= int(reach2[i + 1].size())) continue;
                        acc |= rot63(reach2[i + 1][rest], (d * off) % MOD2);
                    }
                    reach2[i][sm] = acc;
                }
            }
        }

        req.assign(k, -1);
        const bool even_base = (BASE % 2 == 0);
        // A binary relation d_k == d_i (mod 2) only arises for an even base with
        // i <= k; splitting the outer loop on the parity of d_k makes both unary.
        const bool needs_split = USE_PARITY && even_base && !par_is_sum && par_i != k - 1;
        for (int S = 1; S <= maxd * k; ++S) {
            if (!USE_PARITY) { walk(0, S, shift * S, 0); continue; }
            if (needs_split) {
                for (int pk = 0; pk < 2; ++pk) {
                    std::fill(req.begin(), req.end(), -1);
                    req[k - 1] = pk; req[par_i] = pk;
                    walk(0, S, shift * S, 0);
                }
            } else {
                std::fill(req.begin(), req.end(), -1);
                if (even_base && par_is_sum)        req[k - 1] = S & 1;   // d_k == S
                else if (!even_base && !par_is_sum) req[par_i]  = S & 1;   // d_i == S
                // even_base && par_i==k-1, or odd_base && par_is_sum: vacuous
                walk(0, S, shift * S, 0);
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 4) { std::cerr << "usage: keithb BASE LOWER UPPER [THREADS] [--parity]\n"; return 2; }
    auto parse = [](const std::string& t) {
        if (t.empty()) throw std::invalid_argument("empty");
        Int v = 0;
        for (char ch : t) {
            if (ch < '0' || ch > '9') throw std::invalid_argument("non-decimal");
            v = 10 * v + (ch - '0');
        }
        return v;
    };
    Int lower = 0, upper = 0;
    unsigned nthreads = 4;
    try {
        BASE = std::stoi(argv[1]);
        lower = parse(argv[2]); upper = parse(argv[3]);
        if (argc > 4 && argv[4][0] != '-') nthreads = unsigned(std::stoul(argv[4]));
    } catch (const std::exception& e) { std::cerr << "error: " << e.what() << "\n"; return 2; }
    for (int i = 4; i < argc; ++i) if (std::string(argv[i]) == "--parity") USE_PARITY = true;
        else if (std::string(argv[i]) == "--parity-first") { USE_PARITY = true; PARITY_FIRST = true; }
        else if (std::string(argv[i]) == "--mod") USE_MOD = true;
        else if (std::string(argv[i]) == "--mod2") { USE_MOD = true; USE_MOD2 = true; }
    if (BASE < 2 || BASE > 16) { std::cerr << "error: need 2 <= BASE <= 16\n"; return 2; }
    if (lower < BASE || upper < lower) { std::cerr << "error: need BASE <= LOWER <= UPPER\n"; return 2; }

    auto eqs = equations(lower, upper);
    std::vector<std::set<Int>> hits(eqs.size());
    std::vector<unsigned long long> nodes(eqs.size(), 0);
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::string err; std::mutex errm;

    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, nthreads); ++t) pool.emplace_back([&] {
        for (;;) {
            size_t i = next.fetch_add(1);
            if (i >= eqs.size() || failed) return;
            try { Solver s(eqs[i]); s.run(); hits[i] = s.found; nodes[i] = s.nodes; }
            catch (const std::exception& e) {
                std::lock_guard<std::mutex> g(errm); err = e.what(); failed = true; return;
            }
        }
    });
    for (auto& th : pool) th.join();
    if (failed) { std::cerr << "internal check failed: " << err << "\n"; return 1; }

    std::set<Int> all; unsigned long long total = 0;
    std::cout << "base\tk\tm\tlower\tupper\tnodes\tcandidates\n";
    for (size_t i = 0; i < eqs.size(); ++i) {
        total += nodes[i];
        std::string c; bool first = true;
        for (Int v : hits[i]) { if (!first) c += ','; c += show(v); first = false; all.insert(v); }
        std::cout << BASE << '\t' << eqs[i].k << '\t' << eqs[i].m << '\t' << show(eqs[i].lo) << '\t'
                  << show(eqs[i].hi) << '\t' << nodes[i] << '\t' << c << '\n';
    }
    std::cerr << "base=" << BASE << " equations=" << eqs.size()
              << " nodes=" << total << " found=" << all.size() << "\n";
}
