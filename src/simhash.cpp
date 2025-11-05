#include "simhash.h"
#include <array>
#include <functional>

static inline uint64_t hashToken(const std::string &s) {
    // 使用 std::hash 作为示例，可改为 MurmurHash/CityHash
    return std::hash<std::string>{}(s);
}

uint64_t SimHasher::simhash64(const std::vector<std::string> &tokens) {
    std::array<long long, 64> bits{};
    for (const auto &t : tokens) {
        uint64_t h = hashToken(t);
        for (int i = 0; i < 64; ++i) {
            bits[i] += (h & (1ULL << i)) ? 1 : -1;
        }
    }
    uint64_t out = 0;
    for (int i = 0; i < 64; ++i) {
        if (bits[i] > 0) out |= (1ULL << i);
    }
    return out;
}

int SimHasher::hammingDistance(uint64_t a, uint64_t b) {
    uint64_t x = a ^ b;
    // builtin popcountll 等价实现
    int cnt = 0;
    while (x) { x &= (x - 1); ++cnt; }
    return cnt;
}


