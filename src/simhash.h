#pragma once
#include <string>
#include <vector>
#include <cstdint>

// SimHash 实现（64 位）：
// 1) 对每个 token 进行 64 位哈希
// 2) 对每一位累加权重（默认 1）
// 3) 正值置 1，负值置 0，得到 simhash
class SimHasher {
public:
    static uint64_t simhash64(const std::vector<std::string> &tokens);
    static int hammingDistance(uint64_t a, uint64_t b);
};


