// Многопоточность и прогресс. Не зависит от Qt.
#pragma once
#include <algorithm>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace ip {

// Счётчик прогресса: алгоритм вызывает reset(total) и затем step() из любых потоков.
class Progress {
public:
    Progress() : m_done(0), m_total(1) {}
    void reset(int total) { m_total = total > 0 ? total : 1; m_done = 0; }
    void step(int n = 1) { m_done += n; }
    int percent() const {
        const int p = static_cast<int>(100.0 * m_done.load() / m_total.load());
        return p > 100 ? 100 : p;
    }
private:
    std::atomic<int> m_done, m_total;
};
inline void stepProgress(Progress* p, int n = 1) { if (p) p->step(n); }

// Число потоков: 0 = автоматически (hardware_concurrency), 1 = однопоточный режим.
inline std::atomic<int>& threadSetting() { static std::atomic<int> n(0); return n; }
inline void setThreadCount(int n) { threadSetting() = n; }
inline int effectiveThreads() {
    int n = threadSetting().load();
    if (n <= 0) n = static_cast<int>(std::thread::hardware_concurrency());
    return n <= 0 ? 2 : n;
}

// Делит диапазон [begin, end) на блоки и выполняет body(b, e) в отдельных потоках.
inline void parallelFor(int begin, int end, const std::function<void(int, int)>& body) {
    const int count = end - begin;
    if (count <= 0) return;
    const int nt = std::min(effectiveThreads(), count);
    if (nt <= 1) { body(begin, end); return; }
    std::vector<std::thread> pool;
    const int chunk = (count + nt - 1) / nt;
    for (int t = 0; t < nt; ++t) {
        const int b = begin + t * chunk, e = std::min(end, b + chunk);
        if (b >= e) break;
        pool.emplace_back(body, b, e);
    }
    for (size_t i = 0; i < pool.size(); ++i) pool[i].join();
}

}  // namespace ip
