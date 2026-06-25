#include "orderbook.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>

struct LatencyStats {
    double averageNs_;
    double p50Ns_;
    double p95Ns_;
    double p99Ns_;
};

int main() {
    constexpr std::size_t eventCount = 1'000'000;

    OrderBook book;

    std::vector<long long> latencies;
    latencies.reserve(eventCount);

    auto benchmarkStart = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < eventCount; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // generate synthetic order event
        // book.addOrder(...)

        auto end = std::chrono::high_resolution_clock::now();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start
        ).count();

        latencies.push_back(ns);
    }

    auto benchmarkEnd = std::chrono::high_resolution_clock::now();

    double totalSeconds = std::chrono::duration<double>(
        benchmarkEnd - benchmarkStart
    ).count();

    double ordersPerSecond = eventCount / totalSeconds;

    std::sort(latencies.begin(), latencies.end());

    auto percentile = [&](double p) {
        std::size_t index = static_cast<std::size_t>(p * latencies.size());
        return latencies[index];
    };

    std::cout << "Events: " << eventCount << '\n';
    std::cout << "Orders/sec: " << ordersPerSecond << '\n';
    std::cout << "p50 latency ns: " << percentile(0.50) << '\n';
    std::cout << "p95 latency ns: " << percentile(0.95) << '\n';
    std::cout << "p99 latency ns: " << percentile(0.99) << '\n';
}