#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
    #pragma warning(disable: 4324) // structure was padded due to alignment specifier (intended for cache line alignment)
#endif

namespace mmb {

/**
 * @brief Standard L1/L2 cache line size for modern x86-64 and ARM64 architectures.
 * 
 * Aligning per-worker data structures to 64 bytes prevents the False Sharing phenomenon,
 * where two threads writing to independent variables located in the same 64-byte cache
 * line invalidate each other's L1/L2 caches, devastating parallel scaling.
 */
inline constexpr std::size_t CACHE_LINE_SIZE = 64;

} // namespace mmb
