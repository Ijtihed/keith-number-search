#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using i128 = __int128_t;
using Clock = std::chrono::steady_clock;

static std::string show(i128 x) {
    if (x == 0) return "0";
    bool neg = x < 0;
    if (neg) x = -x;
    std::string s;
    while (x) { s.push_back(char('0' + x % 10)); x /= 10; }
    if (neg) s.push_back('-');
    std::reverse(s.begin(), s.end());
    return s;
}

static i128 pow4(int e) {
    i128 x = 1;
    while (e--) x *= 4;
    return x;
}

static std::vector<int> digits4(i128 n) {
    std::vector<int> d;
    while (n) { d.push_back(int(n % 4)); n /= 4; }
    if (d.empty()) d.push_back(0);
    std::reverse(d.begin(), d.end());
    return d;
}

static bool direct_keith(i128 n) {
    auto d = digits4(n);
    if (d.size() < 2) return false;
    const size_t k = d.size();
    std::vector<i128> q(d.begin(), d.end());
    i128 sum = 0;
    for (auto x : q) sum += x;
    size_t first = 0;
    while (true) {
        i128 next = sum;
        if (next >= n) return next == n;
        sum += next - q[first];
        q[first] = next;
        first = (first + 1) % k;
    }
}

struct Task {
    int k;
    int m;
    i128 lo;
    i128 hi;
    std::vector<i128> coeff;
};

struct Item {
    i128 equation_sum;
    i128 number_value;
};

struct Result {
    int k = 0;
    int m = 0;
    uint64_t right_items = 0;
    uint64_t left_items = 0;
    uint64_t equation_solutions = 0;
    std::set<i128> candidates;
    double seconds = 0;
    i128 max_abs_weight = 0;
    i128 max_abs_partial = 0;
};

static i128 abs128(i128 x) { return x < 0 ? -x : x; }

static Result solve_mitm(const Task& t) {
    auto start = Clock::now();
    Result out;
    out.k = t.k;
    out.m = t.m;
    std::vector<i128> place(t.k), weight(t.k);
    for (int i = 0; i < t.k; ++i) {
        place[i] = pow4(t.k - 1 - i);
        weight[i] = place[i] - t.coeff[i];
        out.max_abs_weight = std::max(out.max_abs_weight, abs128(weight[i]));
    }
    // Use a smaller sorted table for the 28-digit block.
    const int split = t.k >= 28 ? (t.k + 2) / 2 : (t.k + 1) / 2;
    std::vector<Item> right;
    uint64_t reserve = uint64_t(1) << (2 * (t.k - split));
    right.reserve(reserve);

    auto enum_right = [&](auto&& self, int pos, i128 es, i128 nv) -> void {
        out.max_abs_partial = std::max(out.max_abs_partial, abs128(es));
        if (pos == t.k) { right.push_back({es, nv}); return; }
        for (int d = 0; d < 4; ++d)
            self(self, pos + 1, es + d * weight[pos], nv + d * place[pos]);
    };
    enum_right(enum_right, split, 0, 0);
    out.right_items = right.size();
    std::sort(right.begin(), right.end(), [](const Item& a, const Item& b) {
        return a.equation_sum < b.equation_sum;
    });

    auto lower = [&](i128 key) {
        return std::lower_bound(right.begin(), right.end(), key,
            [](const Item& a, i128 b) { return a.equation_sum < b; });
    };
    auto upper = [&](i128 key) {
        return std::upper_bound(right.begin(), right.end(), key,
            [](i128 a, const Item& b) { return a < b.equation_sum; });
    };

    auto enum_left = [&](auto&& self, int pos, i128 es, i128 nv) -> void {
        out.max_abs_partial = std::max(out.max_abs_partial, abs128(es));
        if (pos == split) {
            ++out.left_items;
            i128 target = -es;
            auto a = lower(target), b = upper(target);
            out.equation_solutions += uint64_t(b - a);
            for (auto it = a; it != b; ++it) {
                i128 n = nv + it->number_value;
                if (t.lo <= n && n <= t.hi) {
                    if (!direct_keith(n)) {
                        std::cerr << "internal direct-verification failure at " << show(n) << "\n";
                        std::abort();
                    }
                    out.candidates.insert(n);
                }
            }
            return;
        }
        int first_digit = (pos == 0 ? 1 : 0);
        for (int d = first_digit; d < 4; ++d)
            self(self, pos + 1, es + d * weight[pos], nv + d * place[pos]);
    };
    enum_left(enum_left, 0, 0, 0);
    out.seconds = std::chrono::duration<double>(Clock::now() - start).count();
    return out;
}

static std::vector<Task> make_tasks(i128 lower, i128 upper) {
    std::vector<Task> tasks;
    int k0 = int(digits4(lower).size()), k1 = int(digits4(upper).size());
    for (int k = k0; k <= k1; ++k) {
        i128 lo = std::max(lower, pow4(k - 1));
        i128 hi = std::min(upper, pow4(k) - 1);
        if (lo > hi) continue;
        std::vector<std::vector<i128>> rows(k, std::vector<i128>(k));
        for (int i = 0; i < k; ++i) rows[i][i] = 1;
        for (int m = k + 1; ; ++m) {
            std::vector<i128> c(k);
            for (int back = 1; back <= k; ++back)
                for (int i = 0; i < k; ++i) c[i] += rows[rows.size() - back][i];
            rows.push_back(c);
            if (c[0] > hi) break;
            i128 max_term = 0;
            for (auto x : c) max_term += 3 * x;
            if (max_term >= lo) tasks.push_back({k, m, lo, hi, c});
        }
    }
    return tasks;
}

// Decimal only, no sign, no overflow past the supported ceiling.
static bool parse_bound(const std::string& text, i128 limit, i128& out) {
    if (text.empty()) return false;
    i128 v = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        if (v > (limit - (c - '0')) / 10) return false;
        v = 10 * v + (c - '0');
    }
    out = v;
    return true;
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: exhaustive LOWER UPPER [THREADS]\n";
        return 2;
    }
    // Below 4 there is no two-digit number and the k=1 coefficient never grows,
    // so make_tasks would not terminate. Above 4^28-1 the sorted table exceeds
    // 8 GB; use search.cpp for wider blocks.
    const i128 ceiling = pow4(28) - 1;
    i128 lower = 0, upper = 0;
    if (!parse_bound(argv[1], ceiling, lower) || !parse_bound(argv[2], ceiling, upper)) {
        std::cerr << "error: bounds must be decimal integers at most 4^28-1\n";
        return 2;
    }
    if (lower < 4 || upper < lower) {
        std::cerr << "error: need 4 <= LOWER <= UPPER <= 4^28-1\n";
        return 2;
    }
    unsigned workers = 4;
    if (argc == 4) {
        try { workers = unsigned(std::stoul(argv[3])); }
        catch (const std::exception&) { workers = 0; }
        if (workers < 1 || workers > 256) {
            std::cerr << "error: THREADS must be between 1 and 256\n";
            return 2;
        }
    }
    auto tasks = make_tasks(lower, upper);
    std::vector<Result> results(tasks.size());
    std::atomic<size_t> next{0};
    auto worker = [&]() {
        while (true) {
            size_t i = next.fetch_add(1);
            if (i >= tasks.size()) return;
            results[i] = solve_mitm(tasks[i]);
        }
    };
    std::vector<std::thread> pool;
    for (unsigned i = 0; i < std::max(1u, workers); ++i) pool.emplace_back(worker);
    for (auto& th : pool) th.join();

    std::cout << "method\tk\tm\tlower\tupper\tright_items\tleft_items\tequation_solutions"
                 "\tcandidates\tseconds\tmax_abs_weight\tmax_abs_partial\n";
    for (const auto& r : results) {
        std::ostringstream cand;
        bool first = true;
        for (auto n : r.candidates) { if (!first) cand << ','; cand << show(n); first = false; }
        std::cout << "mitm\t" << r.k << '\t' << r.m << '\t'
                  << show(tasks[&r - results.data()].lo) << '\t'
                  << show(tasks[&r - results.data()].hi) << '\t'
                  << r.right_items << '\t' << r.left_items << '\t'
                  << r.equation_solutions << '\t' << cand.str() << '\t'
                  << r.seconds << '\t' << show(r.max_abs_weight) << '\t'
                  << show(r.max_abs_partial) << '\n';
    }
}
