// Exhaustive base-4 Keith search by branch and bound.
//
// Same equation as exhaustive.cpp, different engine. Weights are shifted
// nonnegative so the digit sum can be fixed, which makes the bound on each
// remaining suffix tight enough to prune without a lookup table. Memory is
// negligible, so this reaches widths the meet-in-the-middle cannot.
//
//   g++ -O3 -std=c++17 -pthread search.cpp -o search
//   ./search LOWER UPPER [THREADS]
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
    bool neg = x < 0;
    if (neg) x = -x;
    std::string s;
    while (x) { s += char('0' + int(x % 10)); x /= 10; }
    if (neg) s += '-';
    std::reverse(s.begin(), s.end());
    return s;
}

static Int pow4(int e) { Int r = 1; while (e--) r *= 4; return r; }

static std::vector<int> digits_of(Int n) {
    std::vector<int> d;
    while (n) { d.push_back(int(n % 4)); n /= 4; }
    if (d.empty()) d.push_back(0);
    std::reverse(d.begin(), d.end());
    return d;
}

// one-based index at which n appears in its own Keith sequence, else 0
static int hit_index(Int n) {
    std::vector<int> d = digits_of(n);
    int k = int(d.size());
    if (k < 2) return 0;
    std::vector<Int> w(d.begin(), d.end());
    Int s = 0;
    for (Int x : w) s += x;
    int idx = k, head = 0;
    while (true) {
        Int nxt = s;
        ++idx;
        if (nxt >= n) return nxt == n ? idx : 0;
        s += nxt - w[head];
        w[head] = nxt;
        head = (head + 1) % k;
    }
}

struct Equation { int k, m; Int lo, hi; std::vector<Int> coeff; };

static std::vector<Equation> equations(Int lower, Int upper) {
    std::vector<Equation> out;
    int ka = int(digits_of(lower).size()), kb = int(digits_of(upper).size());
    for (int k = ka; k <= kb; ++k) {
        Int lo = std::max(lower, pow4(k - 1)), hi = std::min(upper, pow4(k) - 1);
        if (lo > hi) continue;
        std::vector<std::vector<Int>> c;
        for (int i = 0; i < k; ++i) {
            std::vector<Int> e(k, 0);
            e[i] = 1;
            c.push_back(e);
        }
        for (int m = k + 1; ; ++m) {
            std::vector<Int> nc(k, 0);
            for (int b = 1; b <= k; ++b)
                for (int i = 0; i < k; ++i) nc[i] += c[c.size() - b][i];
            c.push_back(nc);
            if (nc[0] > hi) break;
            Int biggest = 0;
            for (Int x : nc) biggest += 3 * x;
            if (biggest >= lo) out.push_back({k, m, lo, hi, nc});
        }
    }
    return out;
}

struct Solver {
    const Equation& eq;
    int k;
    std::vector<Int> sw, place;
    std::vector<int> pos;
    Int shift = 0;
    std::vector<std::vector<Int>> lowB, highB;
    std::set<Int> found;
    unsigned long long nodes = 0;

    explicit Solver(const Equation& e) : eq(e), k(e.k) {}

    void walk(int at, int left_sum, Int target, Int value) {
        ++nodes;
        int left = k - at;
        if (left_sum < 0 || left_sum > 3 * left) return;
        if (target < lowB[at][left_sum] || target > highB[at][left_sum]) return;
        if (at == k) {
            if (left_sum == 0 && target == 0 && value >= eq.lo && value <= eq.hi) {
                if (int(digits_of(value).size()) != k)
                    throw std::runtime_error("width mismatch");
                if (hit_index(value) != eq.m)
                    throw std::runtime_error("hit-index mismatch");
                found.insert(value);
            }
            return;
        }
        int first = (pos[at] == 0) ? 1 : 0;
        for (int d = first; d <= 3; ++d)
            walk(at + 1, left_sum - d, target - d * sw[at], value + d * place[at]);
    }

    void run() {
        std::vector<Int> w(k);
        for (int i = 0; i < k; ++i) w[i] = pow4(k - 1 - i) - eq.coeff[i];

        Int lo = std::min(w[0], 3 * w[0]), hi = std::max(w[0], 3 * w[0]);
        for (int i = 1; i < k; ++i) {
            lo += std::min<Int>(0, 3 * w[i]);
            hi += std::max<Int>(0, 3 * w[i]);
        }
        if (lo > 0 || hi < 0) return;

        shift = -*std::min_element(w.begin(), w.end());
        if (shift < 0) shift = 0;

        pos.resize(k);
        std::iota(pos.begin(), pos.end(), 0);
        std::stable_sort(pos.begin(), pos.end(),
            [&](int a, int b) { return (w[a] + shift) > (w[b] + shift); });

        sw.resize(k);
        place.resize(k);
        for (int i = 0; i < k; ++i) {
            sw[i] = w[pos[i]] + shift;
            place[i] = pow4(k - 1 - pos[i]);
        }

        lowB.assign(k + 1, {});
        highB.assign(k + 1, {});
        lowB[k] = {0};
        highB[k] = {0};
        const Int BIG = Int(1) << 120;
        for (int i = k - 1; i >= 0; --i) {
            int cap = 3 * (k - i);
            lowB[i].assign(cap + 1, BIG);
            highB[i].assign(cap + 1, -BIG);
            for (int s = 0; s <= cap; ++s)
                for (int d = 0; d <= 3; ++d) {
                    int rest = s - d;
                    if (rest < 0 || rest >= int(lowB[i + 1].size())) continue;
                    lowB[i][s]  = std::min(lowB[i][s],  d * sw[i] + lowB[i + 1][rest]);
                    highB[i][s] = std::max(highB[i][s], d * sw[i] + highB[i + 1][rest]);
                }
        }
        for (int S = 1; S <= 3 * k; ++S) walk(0, S, shift * S, 0);
    }
};

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: search LOWER UPPER [THREADS]\n"; return 2; }
    auto parse = [](const std::string& t) {
        if (t.empty()) throw std::invalid_argument("empty bound");
        Int v = 0;
        for (char ch : t) {
            if (ch < '0' || ch > '9') throw std::invalid_argument("non-decimal bound");
            v = 10 * v + (ch - '0');
        }
        return v;
    };
    Int lower = 0, upper = 0;
    try { lower = parse(argv[1]); upper = parse(argv[2]); }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << "\n"; return 2; }
    if (lower < 4 || upper < lower) { std::cerr << "error: need 4 <= LOWER <= UPPER\n"; return 2; }
    unsigned nthreads = argc > 3 ? unsigned(std::stoul(argv[3])) : 4;

    auto eqs = equations(lower, upper);
    std::vector<std::set<Int>> hits(eqs.size());
    std::vector<unsigned long long> nodes(eqs.size(), 0);
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::string err;
    std::mutex errm;

    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, nthreads); ++t) pool.emplace_back([&] {
        for (;;) {
            size_t i = next.fetch_add(1);
            if (i >= eqs.size() || failed) return;
            try {
                Solver s(eqs[i]);
                s.run();
                hits[i] = s.found;
                nodes[i] = s.nodes;
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> g(errm);
                err = e.what();
                failed = true;
                return;
            }
        }
    });
    for (auto& th : pool) th.join();
    if (failed) { std::cerr << "internal check failed: " << err << "\n"; return 1; }

    std::set<Int> all;
    unsigned long long total = 0;
    std::cout << "k\tm\tlower\tupper\tnodes\tcandidates\n";
    for (size_t i = 0; i < eqs.size(); ++i) {
        total += nodes[i];
        std::string c;
        bool first = true;
        for (Int v : hits[i]) {
            if (!first) c += ',';
            c += show(v);
            first = false;
            all.insert(v);
        }
        std::cout << eqs[i].k << '\t' << eqs[i].m << '\t' << show(eqs[i].lo) << '\t'
                  << show(eqs[i].hi) << '\t' << nodes[i] << '\t' << c << '\n';
    }
    std::cerr << "equations=" << eqs.size() << " nodes=" << total << " found=" << all.size() << "\n";
    for (Int v : all) std::cerr << "  " << show(v) << "\n";
}
