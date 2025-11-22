// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_CONFIG_H
#define LEANSDR_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// ======================================================================
// Platform and Architecture Detection
// ======================================================================

// Detect ARM architecture
#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
  #define LEANSDR_ARM 1

  #if defined(__aarch64__) || defined(_M_ARM64)
    #define LEANSDR_ARM64 1
    #define LEANSDR_ARM32 0
  #else
    #define LEANSDR_ARM64 0
    #define LEANSDR_ARM32 1
  #endif
#else
  #define LEANSDR_ARM 0
  #define LEANSDR_ARM32 0
  #define LEANSDR_ARM64 0
#endif

// Detect x86/x64 architecture
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || \
    defined(__i386__) || defined(_M_IX86)
  #define LEANSDR_X86 1

  #if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
    #define LEANSDR_X86_64 1
    #define LEANSDR_X86_32 0
  #else
    #define LEANSDR_X86_64 0
    #define LEANSDR_X86_32 1
  #endif
#else
  #define LEANSDR_X86 0
  #define LEANSDR_X86_32 0
  #define LEANSDR_X86_64 0
#endif

// Detect operating system
#if defined(__linux__)
  #define LEANSDR_OS_LINUX 1
  #define LEANSDR_OS_ANDROID 0
#elif defined(__ANDROID__)
  #define LEANSDR_OS_LINUX 0
  #define LEANSDR_OS_ANDROID 1
#else
  #define LEANSDR_OS_LINUX 0
  #define LEANSDR_OS_ANDROID 0
#endif

// ======================================================================
// NEON Support Detection (Compile-Time)
// ======================================================================

// ARM NEON compile-time detection
#if LEANSDR_ARM
  // Check for explicit NEON flags
  #if defined(__ARM_NEON__) || defined(__ARM_NEON)
    #define LEANSDR_HAS_NEON_COMPILETIME 1
  #else
    #define LEANSDR_HAS_NEON_COMPILETIME 0
  #endif

  // NEON intrinsics availability
  #if LEANSDR_HAS_NEON_COMPILETIME
    #if LEANSDR_ARM64
      #define LEANSDR_USE_NEON_INTRINSICS 1
    #elif LEANSDR_ARM32 && defined(__ARM_NEON__)
      #define LEANSDR_USE_NEON_INTRINSICS 1
    #else
      #define LEANSDR_USE_NEON_INTRINSICS 0
    #endif
  #else
    #define LEANSDR_USE_NEON_INTRINSICS 0
  #endif
#else
  #define LEANSDR_HAS_NEON_COMPILETIME 0
  #define LEANSDR_USE_NEON_INTRINSICS 0
#endif

// ======================================================================
// SIMD Feature Detection
// ======================================================================

// SSE/AVX support for x86
#if LEANSDR_X86
  #if defined(__SSE__)
    #define LEANSDR_HAS_SSE 1
  #else
    #define LEANSDR_HAS_SSE 0
  #endif

  #if defined(__SSE2__)
    #define LEANSDR_HAS_SSE2 1
  #else
    #define LEANSDR_HAS_SSE2 0
  #endif

  #if defined(__AVX__)
    #define LEANSDR_HAS_AVX 1
  #else
    #define LEANSDR_HAS_AVX 0
  #endif

  #if defined(__AVX2__)
    #define LEANSDR_HAS_AVX2 1
  #else
    #define LEANSDR_HAS_AVX2 0
  #endif
#else
  #define LEANSDR_HAS_SSE 0
  #define LEANSDR_HAS_SSE2 0
  #define LEANSDR_HAS_AVX 0
  #define LEANSDR_HAS_AVX2 0
#endif

// ======================================================================
// Runtime CPU Feature Detection
// ======================================================================

namespace leansdr {

#if LEANSDR_ARM && (LEANSDR_OS_LINUX || LEANSDR_OS_ANDROID)

// Runtime NEON detection on Linux/Android ARM systems
inline bool has_neon_runtime() {
  static int neon_available = -1;

  if (neon_available == -1) {
    #if LEANSDR_ARM64
      // NEON is mandatory on ARMv8/AArch64
      neon_available = 1;
    #else
      // ARMv7: Check /proc/cpuinfo
      neon_available = 0;
      FILE *fp = fopen("/proc/cpuinfo", "r");
      if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
          if (strncmp(line, "Features", 8) == 0) {
            if (strstr(line, "neon")) {
              neon_available = 1;
              break;
            }
          }
        }
        fclose(fp);
      }
    #endif
  }

  return neon_available == 1;
}

#else

// Fallback for non-Linux ARM or other architectures
inline bool has_neon_runtime() {
  #if LEANSDR_HAS_NEON_COMPILETIME
    return true;
  #else
    return false;
  #endif
}

#endif

// Combined compile-time and runtime NEON check
inline bool use_neon() {
  #if LEANSDR_HAS_NEON_COMPILETIME
    return has_neon_runtime();
  #else
    return false;
  #endif
}

} // namespace leansdr

// ======================================================================
// SIMD Width and Alignment Configuration
// ======================================================================

#if LEANSDR_USE_NEON_INTRINSICS
  // NEON operates on 128-bit vectors
  #define LEANSDR_SIMD_WIDTH 128
  #define LEANSDR_SIMD_ALIGN 16
  #define LEANSDR_FLOAT32_PER_SIMD 4
  #define LEANSDR_INT32_PER_SIMD 4
  #define LEANSDR_INT16_PER_SIMD 8
  #define LEANSDR_INT8_PER_SIMD 16
#elif LEANSDR_HAS_AVX2
  #define LEANSDR_SIMD_WIDTH 256
  #define LEANSDR_SIMD_ALIGN 32
  #define LEANSDR_FLOAT32_PER_SIMD 8
  #define LEANSDR_INT32_PER_SIMD 8
  #define LEANSDR_INT16_PER_SIMD 16
  #define LEANSDR_INT8_PER_SIMD 32
#elif LEANSDR_HAS_SSE2
  #define LEANSDR_SIMD_WIDTH 128
  #define LEANSDR_SIMD_ALIGN 16
  #define LEANSDR_FLOAT32_PER_SIMD 4
  #define LEANSDR_INT32_PER_SIMD 4
  #define LEANSDR_INT16_PER_SIMD 8
  #define LEANSDR_INT8_PER_SIMD 16
#else
  #define LEANSDR_SIMD_WIDTH 0
  #define LEANSDR_SIMD_ALIGN 8
  #define LEANSDR_FLOAT32_PER_SIMD 1
  #define LEANSDR_INT32_PER_SIMD 1
  #define LEANSDR_INT16_PER_SIMD 1
  #define LEANSDR_INT8_PER_SIMD 1
#endif

// ======================================================================
// Compiler Hints and Attributes
// ======================================================================

// Force inline
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
  #define LEANSDR_FORCE_INLINE __forceinline
#else
  #define LEANSDR_FORCE_INLINE inline
#endif

// No inline
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
  #define LEANSDR_NOINLINE __declspec(noinline)
#else
  #define LEANSDR_NOINLINE
#endif

// Memory alignment
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_ALIGNED(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
  #define LEANSDR_ALIGNED(x) __declspec(align(x))
#else
  #define LEANSDR_ALIGNED(x)
#endif

// Restrict keyword
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_RESTRICT __restrict__
#elif defined(_MSC_VER)
  #define LEANSDR_RESTRICT __restrict
#else
  #define LEANSDR_RESTRICT
#endif

// Likely/unlikely branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_LIKELY(x) __builtin_expect(!!(x), 1)
  #define LEANSDR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define LEANSDR_LIKELY(x) (x)
  #define LEANSDR_UNLIKELY(x) (x)
#endif

// Pure function (no side effects, result depends only on arguments)
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_PURE __attribute__((pure))
#else
  #define LEANSDR_PURE
#endif

// Const function (stricter than pure - doesn't read global memory)
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_CONST __attribute__((const))
#else
  #define LEANSDR_CONST
#endif

// ======================================================================
// Debug and Profiling Configuration
// ======================================================================

// Debug mode detection
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
  #define LEANSDR_DEBUG 1
#else
  #define LEANSDR_DEBUG 0
#endif

// Debug assertions
#if LEANSDR_DEBUG
  #include <assert.h>
  #define LEANSDR_ASSERT(cond) assert(cond)
  #define LEANSDR_ASSERT_MSG(cond, msg) assert((cond) && (msg))
#else
  #define LEANSDR_ASSERT(cond) ((void)0)
  #define LEANSDR_ASSERT_MSG(cond, msg) ((void)0)
#endif

// Static assertions (compile-time)
#if defined(__cplusplus) && __cplusplus >= 201103L
  #define LEANSDR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
  #define LEANSDR_STATIC_ASSERT(cond, msg) typedef char static_assertion_##__LINE__[(cond)?1:-1]
#endif

// Performance profiling markers
#ifndef LEANSDR_ENABLE_PROFILING
  #define LEANSDR_ENABLE_PROFILING 0
#endif

#if LEANSDR_ENABLE_PROFILING
  #include <stdio.h>
  #include <time.h>

  #define LEANSDR_PROFILE_START(name) \
    struct timespec _prof_start_##name; \
    clock_gettime(CLOCK_MONOTONIC, &_prof_start_##name);

  #define LEANSDR_PROFILE_END(name) \
    do { \
      struct timespec _prof_end_##name; \
      clock_gettime(CLOCK_MONOTONIC, &_prof_end_##name); \
      double _prof_elapsed = (_prof_end_##name.tv_sec - _prof_start_##name.tv_sec) + \
                             (_prof_end_##name.tv_nsec - _prof_start_##name.tv_nsec) * 1e-9; \
      fprintf(stderr, "[PROFILE] %s: %.6f seconds\n", #name, _prof_elapsed); \
    } while(0)

  #define LEANSDR_PROFILE_SCOPE(name) \
    struct timespec _prof_scope_start_##name; \
    clock_gettime(CLOCK_MONOTONIC, &_prof_scope_start_##name); \
    auto _prof_scope_guard_##name = [&]() { \
      struct timespec _prof_scope_end; \
      clock_gettime(CLOCK_MONOTONIC, &_prof_scope_end); \
      double _prof_elapsed = (_prof_scope_end.tv_sec - _prof_scope_start_##name.tv_sec) + \
                             (_prof_scope_end.tv_nsec - _prof_scope_start_##name.tv_nsec) * 1e-9; \
      fprintf(stderr, "[PROFILE] %s: %.6f seconds\n", #name, _prof_elapsed); \
    }; \
    (void)_prof_scope_guard_##name;
#else
  #define LEANSDR_PROFILE_START(name) ((void)0)
  #define LEANSDR_PROFILE_END(name) ((void)0)
  #define LEANSDR_PROFILE_SCOPE(name) ((void)0)
#endif

// Debug logging
#if LEANSDR_DEBUG
  #include <stdio.h>
  #define LEANSDR_LOG_DEBUG(fmt, ...) \
    fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
  #define LEANSDR_LOG_INFO(fmt, ...) \
    fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)
  #define LEANSDR_LOG_WARN(fmt, ...) \
    fprintf(stderr, "[WARN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
  #define LEANSDR_LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
  #define LEANSDR_LOG_DEBUG(fmt, ...) ((void)0)
  #define LEANSDR_LOG_INFO(fmt, ...) ((void)0)
  #define LEANSDR_LOG_WARN(fmt, ...) ((void)0)
  #define LEANSDR_LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

// ======================================================================
// Optimization Hints
// ======================================================================

// Prefetch hints for memory access
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
  #define LEANSDR_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#else
  #define LEANSDR_PREFETCH_READ(addr) ((void)0)
  #define LEANSDR_PREFETCH_WRITE(addr) ((void)0)
#endif

// Assume alignment (optimization hint to compiler)
#if defined(__GNUC__) || defined(__clang__)
  #define LEANSDR_ASSUME_ALIGNED(ptr, align) \
    __builtin_assume_aligned((ptr), (align))
#else
  #define LEANSDR_ASSUME_ALIGNED(ptr, align) (ptr)
#endif

// ======================================================================
// Fallback Mechanism Configuration
// ======================================================================

// Enable automatic fallback from SIMD to scalar code
#ifndef LEANSDR_ENABLE_AUTO_FALLBACK
  #define LEANSDR_ENABLE_AUTO_FALLBACK 1
#endif

// Enable runtime dispatch between SIMD and scalar implementations
#ifndef LEANSDR_ENABLE_RUNTIME_DISPATCH
  #define LEANSDR_ENABLE_RUNTIME_DISPATCH 1
#endif

// ======================================================================
// Version and Build Information
// ======================================================================

#define LEANSDR_CONFIG_VERSION_MAJOR 1
#define LEANSDR_CONFIG_VERSION_MINOR 0
#define LEANSDR_CONFIG_VERSION_PATCH 0

namespace leansdr {

// Get build configuration as a string
inline const char* get_build_config() {
  static const char* config_str =
    "LeanSDR Build Configuration:\n"
    #if LEANSDR_ARM
      "  Architecture: ARM"
      #if LEANSDR_ARM64
        " (64-bit)\n"
      #else
        " (32-bit)\n"
      #endif
    #elif LEANSDR_X86
      "  Architecture: x86"
      #if LEANSDR_X86_64
        " (64-bit)\n"
      #else
        " (32-bit)\n"
      #endif
    #else
      "  Architecture: Unknown\n"
    #endif
    #if LEANSDR_HAS_NEON_COMPILETIME
      "  NEON: Available (compile-time)\n"
    #else
      "  NEON: Not available\n"
    #endif
    #if LEANSDR_HAS_SSE2
      "  SSE2: Available\n"
    #endif
    #if LEANSDR_HAS_AVX2
      "  AVX2: Available\n"
    #endif
    #if LEANSDR_DEBUG
      "  Build: Debug\n"
    #else
      "  Build: Release\n"
    #endif
  ;

  return config_str;
}

} // namespace leansdr

#endif // LEANSDR_CONFIG_H
