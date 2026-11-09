// Rng.h: xorshift64*, so a seed gives the same structure on every machine. No Rack dependency.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <cstdint>

namespace ss {

struct Rng
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 0x2545F4914F6CDD1Dull) { if (! s) s = 1; }
    uint64_t next()
    {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 0x2545F4914F6CDD1Dull;
    }
    float uniform() { return (float) ((next() >> 40) * (1.0 / 16777216.0)); }   // [0, 1)
};

}   // namespace ss
