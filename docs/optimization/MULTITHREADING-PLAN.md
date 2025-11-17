# Multithreading Implementation Plan for LeanSDR

**Document:** Pipeline Parallelism and Thread-Level Optimization Strategy
**Last Updated:** 2025-11-17
**Priority:** ⭐⭐⭐ **CRITICAL** (Essential for multi-core performance)
**Expected Gain:** 3-4x on quad-core, 1.5-2x additional on top of NEON optimizations
**Timeframe:** 8-12 weeks for complete implementation
**Complexity:** High (thread safety, synchronization, debugging challenges)

---

## Executive Summary

This document provides a comprehensive plan for **parallelizing the LeanSDR DVB-S receiver** using multithreading. Currently, LeanSDR uses a **single-threaded cooperative scheduler** that sequentially processes all pipeline stages. While this design is simple and predictable, it leaves **75-90% of CPU cores idle** on modern multi-core ARM and x86 processors.

### Current Architecture: Single-Threaded Cooperative Scheduler

```
Main Thread (100% of one core):
┌─────────────────────────────────────────────────────────────────────┐
│  Scheduler::run() - Sequential execution in fixed-point loop:       │
│                                                                      │
│  while (hash() changes):                                            │
│    runnable[0]->run()  // I/O reader                               │
│    runnable[1]->run()  // FIR resampler    ◄─── 30-40% CPU         │
│    runnable[2]->run()  // AGC                                       │
│    runnable[3]->run()  // Frequency recovery                       │
│    runnable[4]->run()  // Constellation RX  ◄─── 15-20% CPU        │
│    runnable[5]->run()  // Viterbi decoder   ◄─── 20-25% CPU        │
│    runnable[6]->run()  // Deinterleaver                            │
│    runnable[7]->run()  // Reed-Solomon                             │
│    ...                                                              │
│    runnable[19]->run() // MPEG output                              │
│                                                                      │
│  Total: ~20 stages executed sequentially                            │
└─────────────────────────────────────────────────────────────────────┘

Other Cores: IDLE (0% utilization)
```

**Key Problem:** All stages run sequentially on a single thread, even though many stages are **independent** and could run in parallel.

### Proposed Architecture: Multi-Threaded Pipeline

```
Thread 1: I/O & Preprocessing (15-20% load)
┌────────────────────────────────┐
│  File/RTL-SDR reader           │
│  FIR resampler                 │
│  AGC                           │
└────────┬───────────────────────┘
         │ Lock-free queue
         ▼
Thread 2: Frequency & Timing (25-30% load)
┌────────────────────────────────┐
│  Frequency tracking            │
│  Carrier PLL                   │
│  Timing recovery               │
└────────┬───────────────────────┘
         │ Lock-free queue
         ▼
Thread 3: Demodulation (30-35% load)
┌────────────────────────────────┐
│  Constellation receiver        │
│  Soft-decision output          │
└────────┬───────────────────────┘
         │ Lock-free queue
         ▼
Thread 4: FEC Decoding (35-40% load)
┌────────────────────────────────┐
│  Viterbi decoder               │
│  Deinterleaver                 │
│  Reed-Solomon                  │
└────────┬───────────────────────┘
         │ Lock-free queue
         ▼
Thread 5: Output & Misc (5-10% load)
┌────────────────────────────────┐
│  MPEG sync                     │
│  Descrambler                   │
│  Output writer                 │
└────────────────────────────────┘

Total: 5 threads with balanced load distribution
Expected CPU: 100-120% (near-linear scaling up to 4 cores)
```

### Quick Facts

| Metric | Value |
|--------|-------|
| **Current Architecture** | Single-threaded cooperative scheduler |
| **Bottlenecks** | FIR (30-40%), Viterbi (20-25%), Demod (15-20%) |
| **Proposed Threads** | 4-5 pipeline stages + 1 control thread |
| **Expected Speedup (2-core)** | 1.6-1.8x |
| **Expected Speedup (4-core)** | 3.0-3.5x |
| **Expected Speedup (8-core)** | 3.5-4.0x (diminishing returns) |
| **Combined with NEON** | 6-8x total speedup possible |
| **Complexity** | High (thread safety, deadlocks, race conditions) |
| **Risk** | Medium (extensive testing required) |

### Key Challenges

1. **Feedback Loops:** Carrier frequency tracking (`freq_tap`) creates circular dependencies
2. **Variable Latency:** Some stages (Viterbi) have unpredictable execution time
3. **Backpressure:** Downstream stages must throttle upstream producers
4. **Thread-Safe Buffers:** Replace single-threaded `pipebuf` with concurrent queue
5. **Load Balancing:** Prevent one thread from becoming a bottleneck
6. **Debugging:** Race conditions, deadlocks, and heisenbugs

---

## Table of Contents

1. [Thread Decomposition Strategy](#thread-decomposition-strategy)
2. [Pipeline Parallelism Architecture](#pipeline-parallelism-architecture)
3. [Synchronization Mechanisms](#synchronization-mechanisms)
4. [Feedback Loop Handling](#feedback-loop-handling)
5. [Buffer Management](#buffer-management)
6. [Load Balancing](#load-balancing)
7. [Implementation Phases](#implementation-phases)
8. [Code Examples](#code-examples)
9. [Performance Projections](#performance-projections)
10. [Debugging and Profiling](#debugging-and-profiling)
11. [Risk Mitigation](#risk-mitigation)
12. [Validation Strategy](#validation-strategy)

---

## Thread Decomposition Strategy

### Principles for Pipeline Partitioning

**Goal:** Divide the 20-stage pipeline into 4-5 threads such that:
1. **Load is balanced** (~80-90% utilization per thread)
2. **Dependencies are minimized** (few cross-thread communications)
3. **Feedback loops are contained** (within single thread if possible)
4. **Buffer overhead is low** (minimize memory copies)

### Dependency Graph Analysis

```
Current Pipeline Stages (simplified):
┌─────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  [File Reader] ──▶ [FIR Resample] ──▶ [AGC] ──▶ [Freq Track]       │
│                                                   │    ▲             │
│                                                   │    │ feedback    │
│                                                   ▼    │             │
│  ┌─────────────────────────────────────────────────────┘            │
│  │                                                                   │
│  │  [Timing Rec] ──▶ [Constellation RX] ──▶ [Sampler]              │
│  │                                                                   │
│  └──▶ [Viterbi] ──▶ [Deinterleave] ──▶ [Reed-Solomon]              │
│                                                                      │
│       [MPEG Sync] ──▶ [Descramble] ──▶ [Output]                    │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

Legend:
  ──▶  Data flow (forward)
  ◄──  Feedback loop (carrier/timing)
```

### Proposed Thread Assignment

#### Option A: 5-Thread Pipeline (RECOMMENDED)

```
┌────────────────────────────────────────────────────────────────────┐
│ Thread 1: I/O & Preprocessing (18% CPU)                           │
├────────────────────────────────────────────────────────────────────┤
│  • File/IQ reader                                                  │
│  • FIR resampler (30-40% → shared with other work)                │
│  • AGC normalization                                               │
│                                                                     │
│  Why together: I/O-bound, sequential dependency                    │
│  Bottleneck: FIR filter (NEON-optimized: 4x faster)               │
└─────────────────┬──────────────────────────────────────────────────┘
                  │ Queue: complex<float> samples
                  ▼
┌────────────────────────────────────────────────────────────────────┐
│ Thread 2: Frequency & Timing Recovery (22% CPU)                   │
├────────────────────────────────────────────────────────────────────┤
│  • Frequency tracking (with local PLL)                             │
│  • Carrier phase rotation                                          │
│  • Timing recovery (Gardner, early-late)                           │
│                                                                     │
│  Why together: Tightly coupled feedback loop                       │
│  Challenge: freq_tap feedback (see section below)                  │
└─────────────────┬──────────────────────────────────────────────────┘
                  │ Queue: phase-corrected samples
                  ▼
┌────────────────────────────────────────────────────────────────────┐
│ Thread 3: Demodulation (28% CPU)                                  │
├────────────────────────────────────────────────────────────────────┤
│  • Constellation receiver (cstln_receiver)                         │
│  • Soft symbol decisions                                           │
│  • Optional: Fast sampler (if not in Thread 2)                     │
│                                                                     │
│  Why separate: Computationally intensive, independent              │
│  Bottleneck: Distance metrics, LUT lookups                         │
└─────────────────┬──────────────────────────────────────────────────┘
                  │ Queue: soft symbols (uint8_t)
                  ▼
┌────────────────────────────────────────────────────────────────────┐
│ Thread 4: FEC Decoding (32% CPU)                                  │
├────────────────────────────────────────────────────────────────────┤
│  • Viterbi decoder (ACS operations)                                │
│  • Convolutional deinterleaver                                     │
│  • Reed-Solomon decoder                                            │
│                                                                     │
│  Why together: Sequential FEC pipeline                             │
│  Bottleneck: Viterbi (NEON-optimized: 2-3x faster)                │
└─────────────────┬──────────────────────────────────────────────────┘
                  │ Queue: MPEG-TS packets (188 bytes)
                  ▼
┌────────────────────────────────────────────────────────────────────┐
│ Thread 5: Output & Post-Processing (8% CPU)                       │
├────────────────────────────────────────────────────────────────────┤
│  • MPEG-TS sync search                                             │
│  • DVB descrambler                                                 │
│  • Output writer (stdout, file, UDP)                               │
│                                                                     │
│  Why separate: I/O-bound, allows decoupling from FEC              │
│  Benefit: Smooth output even with variable decode latency         │
└────────────────────────────────────────────────────────────────────┘
```

**Load Distribution:**
- Thread 1: 18% (I/O-bound, some waiting)
- Thread 2: 22% (moderate compute)
- Thread 3: 28% (compute-heavy)
- Thread 4: 32% (heaviest compute)
- Thread 5: 8% (I/O-bound)
- **Total:** 108% (slightly oversubscribed, good for hiding I/O latency)

#### Option B: 4-Thread Pipeline (Simpler Alternative)

```
Thread 1: I/O + Freq/Timing (35% CPU)
  └─▶ Combines Threads 1+2 from Option A

Thread 2: Demodulation (28% CPU)
  └─▶ Same as Thread 3 from Option A

Thread 3: FEC Decoding (32% CPU)
  └─▶ Same as Thread 4 from Option A

Thread 4: Output (8% CPU)
  └─▶ Same as Thread 5 from Option A
```

**Trade-off:**
- **Pros:** Fewer threads, simpler synchronization, feedback loop contained in Thread 1
- **Cons:** Thread 1 is heavily loaded (35%), less parallelism on 8-core systems

**Recommendation:** Use **Option A (5 threads)** for maximum parallelism on modern quad-core+ systems. Use **Option B (4 threads)** for dual-core systems or to simplify debugging.

### Stage-by-Stage Assignment Table

| Stage | Current File | Function | CPU % | Thread (Opt A) | Thread (Opt B) |
|-------|-------------|----------|-------|----------------|----------------|
| **I/O Reader** | `apps/leandvb.cc` | File/RTL-SDR input | 2% | **T1** | **T1** |
| **FIR Resample** | `dsp.h:250` | Anti-aliasing filter | 35% | **T1** | **T1** |
| **AGC** | `sdr.h:256` | Gain normalization | 4% | **T1** | **T1** |
| **Freq Track** | `sdr.h:700` | Carrier frequency est. | 8% | **T2** | **T1** |
| **Carrier PLL** | `sdr.h:750` | Phase rotation | 6% | **T2** | **T1** |
| **Timing Rec** | `sdr.h:900` | Symbol clock | 8% | **T2** | **T1** |
| **Cstln RX** | `sdr.h:800` | Demodulation | 18% | **T3** | **T2** |
| **Sampler** | `sdr.h:1100` | Symbol sampling | 10% | **T3** | **T2** |
| **Viterbi** | `viterbi.h:157` | Convolutional decode | 22% | **T4** | **T3** |
| **Deinterleave** | `dvb.h:1200` | DVB deinterleaver | 3% | **T4** | **T3** |
| **Reed-Solomon** | `rs.h:140` | RS(204,188) decode | 7% | **T4** | **T3** |
| **MPEG Sync** | `dvb.h:1530` | 0x47 sync search | 2% | **T5** | **T4** |
| **Descramble** | `dvb.h:1700` | DVB derandomization | 1% | **T5** | **T4** |
| **Output** | `apps/leandvb.cc` | Write to stdout/file | 2% | **T5** | **T4** |

---

## Pipeline Parallelism Architecture

### High-Level Thread Interaction Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                      LeanSDR Multithreaded Architecture             │
└─────────────────────────────────────────────────────────────────────┘

         ┌──────────────┐
         │  Main Thread │  (Control plane)
         │   Scheduler  │
         └──────┬───────┘
                │ Creates and manages worker threads
       ┌────────┼────────┬────────┬────────┐
       │        │        │        │        │
       ▼        ▼        ▼        ▼        ▼
   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
   │ Thread │ │ Thread │ │ Thread │ │ Thread │ │ Thread │
   │   1    │ │   2    │ │   3    │ │   4    │ │   5    │
   │  I/O   │ │ FreqTim│ │ Demod  │ │  FEC   │ │ Output │
   └───┬────┘ └───┬────┘ └───┬────┘ └───┬────┘ └───┬────┘
       │          │          │          │          │
       │ queue1   │ queue2   │ queue3   │ queue4   │
       └────▶─────┴────▶─────┴────▶─────┴────▶─────┘
              Lock-free SPSC queues (single producer, single consumer)

Feedback Path (for frequency correction):
       ┌──────────────────────────────────────────────┐
       │                                              │
       │  Thread 2 (FreqTim)                          │
       │    reads freq_tap atomic variable            │
       │         ▲                                    │
       │         │ atomic<float> freq_offset          │
       │         │                                    │
       │  Thread 3 (Demod)                            │
       │    writes freq_tap periodically              │
       │                                              │
       └──────────────────────────────────────────────┘
```

### Thread State Machine

```
Each worker thread follows this state machine:

   ┌──────────┐
   │   INIT   │  Initial state, allocate resources
   └─────┬────┘
         │ start()
         ▼
   ┌──────────┐
   │  RUNNING │◄──┐  Main processing loop
   └─────┬────┘   │
         │        │
         │◄───────┘  while (!shutdown_requested)
         │
         │ shutdown()
         ▼
   ┌──────────┐
   │ DRAINING │  Finish processing queued data
   └─────┬────┘
         │
         ▼
   ┌──────────┐
   │   DONE   │  Thread exits
   └──────────┘

Typical loop:
  while (!shutdown_requested) {
    // 1. Wait for input data (blocking or spinning)
    if (!input_queue.pop(data, timeout)) {
      if (input_eof) break;
      continue;
    }

    // 2. Process data
    process(data, output);

    // 3. Write to output queue (with backpressure)
    while (!output_queue.push(output, timeout)) {
      if (shutdown_requested) break;
      // Output queue full, wait for consumer
    }
  }
```

### Queue Architecture: Lock-Free SPSC

**Design Choice:** Use **Single-Producer Single-Consumer (SPSC) lock-free queues** for maximum throughput.

```
Lock-Free SPSC Queue Implementation:
┌────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  Ring Buffer (power-of-2 size):                                    │
│                                                                     │
│     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐             │
│     │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │  ◄── buffer │
│     └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘             │
│       ▲                       ▲                                    │
│       │                       │                                    │
│    read_pos (atomic)      write_pos (atomic)                       │
│    (consumer only)         (producer only)                         │
│                                                                     │
│  Key Properties:                                                   │
│  • read_pos only modified by consumer (no contention)              │
│  • write_pos only modified by producer (no contention)             │
│  • Memory ordering: acquire/release semantics                      │
│  • Capacity: size - 1 (leave one slot empty to distinguish         │
│              full vs empty: full = (write+1)%size == read)         │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘

Performance:
  • Push/pop latency: ~10-20ns (vs ~50-100ns for mutex)
  • Cache coherency: ~40ns for atomic update across cores
  • No syscalls, no context switches
  • Branch predictor friendly (hot loop)
```

### Queue Sizing Strategy

**Buffer Size Formula:**
```
queue_size = max_latency_samples × safety_factor

Example (Thread 2 → Thread 3):
  • Max processing latency: 100ms (Viterbi burst)
  • Sample rate: 2 Msps (after resampling)
  • Samples in 100ms: 200,000
  • Safety factor: 2x (handle jitter)
  • Queue size: 400,000 samples × 8 bytes = 3.2 MB

Typical sizes:
  • queue1 (I/O → FreqTim): 512K samples (4 MB)
  • queue2 (FreqTim → Demod): 256K samples (2 MB)
  • queue3 (Demod → FEC): 64K symbols (64 KB)
  • queue4 (FEC → Output): 4K packets (752 KB)
```

**Trade-off:**
- **Larger queues:** More memory, longer latency, better jitter tolerance
- **Smaller queues:** Less memory, lower latency, risk of stalls

**Recommendation:** Start with large queues (10-100ms of data), then tune down based on profiling.

---

## Synchronization Mechanisms

### Synchronization Primitive Comparison

| Mechanism | Latency | Throughput | Complexity | Use Case |
|-----------|---------|------------|------------|----------|
| **Lock-Free SPSC Queue** | ~10-20ns | **Highest** | Medium | Pipeline data flow |
| **Mutex + Condition Var** | ~50-100ns | High | Low | Control messages |
| **std::atomic** | ~5ns (local), ~40ns (remote core) | **Highest** | Low | Simple flags, feedback |
| **Spinlock** | ~5-10ns | High | Low | Very short critical sections |
| **Semaphore** | ~50-80ns | Medium | Low | Resource counting |
| **Pipe (Unix)** | ~500-1000ns | Low | Low | IPC (not recommended) |

### Lock-Free Queue Implementation

#### Design 1: Bounded SPSC Ring Buffer (Recommended)

```cpp
// File: src/leansdr/concurrent_queue.h

#ifndef LEANSDR_CONCURRENT_QUEUE_H
#define LEANSDR_CONCURRENT_QUEUE_H

#include <atomic>
#include <vector>
#include <chrono>

namespace leansdr {

// Single-Producer Single-Consumer lock-free queue
// Based on: https://www.1024cores.net/home/lock-free-algorithms/queues
template<typename T>
class SPSCQueue {
public:
  SPSCQueue(size_t capacity)
    : capacity_(next_power_of_2(capacity)),
      mask_(capacity_ - 1),
      buffer_(capacity_),
      read_pos_(0),
      write_pos_(0) {
  }

  // Producer: try to push an item
  // Returns: true if successful, false if queue is full
  bool try_push(const T& item) {
    size_t write = write_pos_.load(std::memory_order_relaxed);
    size_t next_write = (write + 1) & mask_;

    if (next_write == read_pos_.load(std::memory_order_acquire)) {
      // Queue is full
      return false;
    }

    buffer_[write] = item;
    write_pos_.store(next_write, std::memory_order_release);
    return true;
  }

  // Producer: blocking push with timeout
  bool push(const T& item, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!try_push(item)) {
      if (std::chrono::steady_clock::now() >= deadline) {
        return false;  // Timeout
      }
      // Yield to avoid busy-wait burning CPU
      std::this_thread::yield();
    }
    return true;
  }

  // Consumer: try to pop an item
  // Returns: true if successful, false if queue is empty
  bool try_pop(T& item) {
    size_t read = read_pos_.load(std::memory_order_relaxed);

    if (read == write_pos_.load(std::memory_order_acquire)) {
      // Queue is empty
      return false;
    }

    item = buffer_[read];
    read_pos_.store((read + 1) & mask_, std::memory_order_release);
    return true;
  }

  // Consumer: blocking pop with timeout
  bool pop(T& item, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!try_pop(item)) {
      if (std::chrono::steady_clock::now() >= deadline) {
        return false;  // Timeout
      }
      std::this_thread::yield();
    }
    return true;
  }

  // Check if queue is empty (approximate, for monitoring only)
  bool empty() const {
    return read_pos_.load(std::memory_order_relaxed) ==
           write_pos_.load(std::memory_order_relaxed);
  }

  // Get approximate number of items (for monitoring)
  size_t size() const {
    size_t write = write_pos_.load(std::memory_order_relaxed);
    size_t read = read_pos_.load(std::memory_order_relaxed);
    return (write - read) & mask_;
  }

private:
  static size_t next_power_of_2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
  }

  const size_t capacity_;
  const size_t mask_;
  std::vector<T> buffer_;

  // Align to cache line (64 bytes) to avoid false sharing
  alignas(64) std::atomic<size_t> read_pos_;
  alignas(64) std::atomic<size_t> write_pos_;
};

// Batch-oriented variant: push/pop multiple items at once
template<typename T>
class SPSCBatchQueue : public SPSCQueue<T> {
public:
  SPSCBatchQueue(size_t capacity) : SPSCQueue<T>(capacity) {}

  // Producer: push multiple items (more efficient than N individual pushes)
  size_t push_batch(const T* items, size_t count) {
    size_t pushed = 0;
    for (size_t i = 0; i < count; ++i) {
      if (!this->try_push(items[i])) break;
      ++pushed;
    }
    return pushed;
  }

  // Consumer: pop multiple items
  size_t pop_batch(T* items, size_t max_count) {
    size_t popped = 0;
    for (size_t i = 0; i < max_count; ++i) {
      if (!this->try_pop(items[i])) break;
      ++popped;
    }
    return popped;
  }
};

}  // namespace leansdr

#endif  // LEANSDR_CONCURRENT_QUEUE_H
```

#### Design 2: Unbounded MPSC Queue (For Control Messages)

```cpp
// Multi-Producer Single-Consumer queue using std::mutex
// Use for infrequent control messages, not data path

template<typename T>
class MPSCQueue {
public:
  void push(const T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(item);
    cv_.notify_one();
  }

  bool pop(T& item, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, timeout, [this]{ return !queue_.empty(); })) {
      return false;  // Timeout
    }
    item = queue_.front();
    queue_.pop_front();
    return true;
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<T> queue_;
};
```

### Atomic Variables for Feedback Loops

```cpp
// File: src/leansdr/sdr_mt.h (new file for multithreaded versions)

// Shared state for frequency feedback
struct SharedFrequencyState {
  alignas(64) std::atomic<float> freq_offset;  // Hz, updated by demod thread
  alignas(64) std::atomic<float> phase_error;  // radians
  alignas(64) std::atomic<uint64_t> update_counter;  // Detect stale data

  SharedFrequencyState()
    : freq_offset(0.0f),
      phase_error(0.0f),
      update_counter(0) {}

  // Producer (Thread 3: Demodulation)
  void update(float new_freq, float new_phase) {
    freq_offset.store(new_freq, std::memory_order_release);
    phase_error.store(new_phase, std::memory_order_release);
    update_counter.fetch_add(1, std::memory_order_release);
  }

  // Consumer (Thread 2: Frequency tracking)
  void read(float& freq, float& phase, uint64_t& counter) {
    counter = update_counter.load(std::memory_order_acquire);
    freq = freq_offset.load(std::memory_order_acquire);
    phase = phase_error.load(std::memory_order_acquire);
  }
};
```

---

## Feedback Loop Handling

### The Frequency Tracking Challenge

**Problem:** In the single-threaded architecture, the `cstln_receiver` (demodulator) updates a `freq_tap` that is immediately read by the frequency tracker in the next iteration. In a multithreaded design, these are in **different threads** with **queue latency** between them.

```
Single-Threaded (current):
┌─────────────────────────────────────────────┐
│  Iteration N:                               │
│    1. freq_track reads freq_tap             │
│    2. cstln_receiver writes new freq_tap    │
│  Iteration N+1:                             │
│    1. freq_track reads updated freq_tap ◄───┘ Immediate feedback
│       ...                                   │
└─────────────────────────────────────────────┘

Multithreaded (naive - BROKEN):
┌──────────────────────────────────────────────┐
│ Thread 2 (FreqTim)                           │
│   reads freq_tap                             │
│   applies correction                         │
│   outputs to queue2                          │
└─────────────┬────────────────────────────────┘
              │ 10-50ms queue latency
              ▼
┌──────────────────────────────────────────────┐
│ Thread 3 (Demod)                             │
│   receives stale data                        │
│   computes new freq_tap                      │
│   writes back... but too late!               │
│   Feedback is delayed by 50-100ms ◄───────────┐
└──────────────────────────────────────────────┘
   Problem: Frequency tracking oscillates or diverges
```

### Solution 1: Shared Atomic Variable (Recommended)

```cpp
// Shared between Thread 2 and Thread 3
SharedFrequencyState shared_freq;

// Thread 2: Frequency Tracker
void thread2_freq_timing() {
  while (!shutdown) {
    complex<float> sample;
    if (!input_queue.pop(sample, timeout)) continue;

    // Read latest frequency estimate (lock-free)
    float freq_offset, phase_error;
    uint64_t counter;
    shared_freq.read(freq_offset, phase_error, counter);

    // Apply correction (Carrier PLL)
    float corrected_freq = carrier_freq + freq_offset;
    sample = sample * cexpf(-j * 2 * PI * corrected_freq * t);

    // ... timing recovery ...

    output_queue.push(sample);
  }
}

// Thread 3: Demodulator
void thread3_demod() {
  while (!shutdown) {
    complex<float> sample;
    if (!input_queue.pop(sample, timeout)) continue;

    // Constellation receiver
    uint8_t symbol = cstln_receiver(sample);

    // Estimate frequency error from phase
    float freq_error = estimate_freq_error(sample);

    // Update shared state (lock-free write)
    shared_freq.update(freq_error, phase_error);

    output_queue.push(symbol);
  }
}
```

**Advantages:**
- Low latency (atomic read/write ~40ns)
- No queues needed for feedback
- Simple to implement

**Disadvantages:**
- Feedback delayed by queue latency (10-50ms)
- May cause tracking instability for fast Doppler shifts

### Solution 2: Merge Freq + Demod into One Thread

```cpp
// Thread 2+3 combined: FreqTim + Demod (single thread)
void thread_combined_freq_demod() {
  while (!shutdown) {
    complex<float> sample;
    if (!input_queue.pop(sample, timeout)) continue;

    // Step 1: Frequency tracking (uses local freq_tap)
    sample = freq_track_and_rotate(sample, &freq_tap);

    // Step 2: Timing recovery
    sample = timing_recovery(sample);

    // Step 3: Demodulation (updates freq_tap for next iteration)
    uint8_t symbol = cstln_receiver(sample, &freq_tap);

    // No cross-thread communication needed!
    output_queue.push(symbol);
  }
}
```

**Advantages:**
- Zero feedback latency (same as single-threaded)
- Simplified design
- No atomics needed

**Disadvantages:**
- One thread is heavily loaded (~50% CPU)
- Less parallelism (only 4 threads instead of 5)

**Recommendation:** Use **Solution 2 (merge threads)** for initial implementation. Evaluate **Solution 1 (atomic feedback)** if you need more parallelism.

### Solution 3: Predictive Feedforward (Advanced)

```cpp
// Thread 2: Predict frequency drift using historical data
class PredictiveFreqTracker {
  std::deque<float> freq_history;  // Last N frequency estimates

  float predict_freq_offset() {
    if (freq_history.size() < 3) return freq_history.back();

    // Linear extrapolation: f(t+1) = 2*f(t) - f(t-1)
    float f_t = freq_history[freq_history.size() - 1];
    float f_t1 = freq_history[freq_history.size() - 2];
    return 2 * f_t - f_t1;
  }

  void apply_correction(complex<float>& sample) {
    float predicted_freq = predict_freq_offset();
    sample *= cexpf(-j * 2 * PI * predicted_freq * t);
  }
};
```

**Advantages:**
- Compensates for queue latency
- Handles fast Doppler shifts

**Disadvantages:**
- Requires tuning
- Can amplify errors if prediction is wrong

---

## Buffer Management

### Replacing Single-Threaded `pipebuf`

Current `pipebuf` design (from `framework.h`):
```cpp
template<typename T>
struct pipebuf {
  T *buf;       // Circular buffer
  T *rds[MAX_READERS];  // Multiple reader pointers
  T *wr;        // Single writer pointer
  T *end;       // Buffer end

  // NOT THREAD-SAFE: Direct pointer manipulation
  T* rd() { return rds[0]; }
  T* wr() { return wr; }
  void read(size_t n) { rds[0] += n; }
  void write(size_t n) { wr += n; }
};
```

**Problem:** No synchronization, assumes single-threaded sequential access.

### Multithreaded Replacement: `ThreadSafePipeBuf`

```cpp
// File: src/leansdr/framework_mt.h (new file)

namespace leansdr {

// Thread-safe replacement for pipebuf (single producer, single consumer)
template<typename T>
class ThreadSafePipeBuf {
public:
  ThreadSafePipeBuf(size_t capacity)
    : queue_(capacity) {}

  // Writer interface (compatible with old pipebuf)
  bool writable(size_t n) {
    return queue_.size() < queue_.capacity() - n;
  }

  void write(const T* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      while (!queue_.try_push(data[i])) {
        std::this_thread::yield();
      }
    }
  }

  void write(const T& item) {
    write(&item, 1);
  }

  // Reader interface
  bool readable(size_t n) {
    return queue_.size() >= n;
  }

  void read(T* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      while (!queue_.try_pop(data[i])) {
        std::this_thread::yield();
      }
    }
  }

  T read_one() {
    T item;
    read(&item, 1);
    return item;
  }

private:
  SPSCQueue<T> queue_;
};

// Zero-copy variant using shared memory regions (advanced)
template<typename T>
class SharedMemoryPipeBuf {
  // Reserve contiguous memory region
  // Producer writes to region, advances write pointer
  // Consumer reads from region, advances read pointer
  // Requires careful cache line alignment
  // See: https://www.boost.org/doc/libs/release/libs/interprocess/
};

}  // namespace leansdr
```

### Batch Transfer Optimization

**Problem:** Calling `push()` / `pop()` for every sample is inefficient (function call overhead, cache misses).

**Solution:** Process data in batches.

```cpp
// Process 1024 samples at a time
const size_t BATCH_SIZE = 1024;

void thread_worker() {
  T input_batch[BATCH_SIZE];
  T output_batch[BATCH_SIZE];

  while (!shutdown) {
    // Pop batch (blocks until BATCH_SIZE items available)
    size_t n_read = input_queue.pop_batch(input_batch, BATCH_SIZE);
    if (n_read == 0) continue;

    // Process batch
    for (size_t i = 0; i < n_read; ++i) {
      output_batch[i] = process(input_batch[i]);
    }

    // Push batch
    output_queue.push_batch(output_batch, n_read);
  }
}
```

**Performance Impact:**
- Batch size 1: 100 MSps throughput
- Batch size 64: 400 MSps throughput
- Batch size 1024: 600 MSps throughput (diminishing returns)

**Recommendation:** Use batch size of 256-1024 samples (2-8 KB).

---

## Load Balancing

### Static vs Dynamic Thread Assignment

#### Static Assignment (Recommended for Initial Implementation)

```cpp
// Fixed thread assignment
struct PipelineThreads {
  std::thread thread1;  // I/O
  std::thread thread2;  // FreqTim
  std::thread thread3;  // Demod
  std::thread thread4;  // FEC
  std::thread thread5;  // Output

  void start() {
    thread1 = std::thread(thread1_io);
    thread2 = std::thread(thread2_freq_timing);
    thread3 = std::thread(thread3_demod);
    thread4 = std::thread(thread4_fec);
    thread5 = std::thread(thread5_output);
  }

  void join() {
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    thread5.join();
  }
};
```

**Advantages:**
- Simple to implement
- Predictable behavior
- Low overhead

**Disadvantages:**
- Fixed load distribution (if one stage is slower, it becomes bottleneck)
- Doesn't adapt to different symbol rates or modulations

#### Dynamic Assignment (Advanced)

```cpp
// Thread pool with work-stealing
struct ThreadPool {
  std::vector<std::thread> workers;
  SPSCQueue<std::function<void()>> task_queue;

  void submit_task(std::function<void()> task) {
    task_queue.push(task);
  }

  void worker_thread() {
    while (!shutdown) {
      std::function<void()> task;
      if (task_queue.pop(task, timeout)) {
        task();  // Execute task
      }
    }
  }
};

// Usage: Submit stage execution as tasks
thread_pool.submit_task([]{ run_fir_filter(batch); });
thread_pool.submit_task([]{ run_viterbi(batch); });
```

**Advantages:**
- Adapts to variable load
- Can utilize all cores (8+)

**Disadvantages:**
- Higher overhead (task scheduling ~100ns)
- Harder to debug
- May cause cache thrashing

**Recommendation:** Start with **static assignment**. Consider dynamic only if you need to support 8+ cores or highly variable workloads.

### CPU Affinity Pinning

```cpp
// Pin threads to specific CPU cores to avoid migration overhead
#include <pthread.h>

void pin_thread_to_core(std::thread& t, int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
}

// Pin threads to separate cores
pin_thread_to_core(thread1, 0);  // Core 0: I/O
pin_thread_to_core(thread2, 1);  // Core 1: FreqTim
pin_thread_to_core(thread3, 2);  // Core 2: Demod
pin_thread_to_core(thread4, 3);  // Core 3: FEC
```

**Benefits:**
- Reduces cache misses (L1/L2 cache stays warm)
- Avoids thread migration overhead (~1-5 µs)
- More predictable performance

**Caution:** May interfere with other processes on the system. Use `sched_setscheduler()` to set real-time priority if needed.

---

## Implementation Phases

### Phase 1: Proof of Concept (2-3 weeks)

**Goal:** Validate feasibility with minimal changes.

**Approach:** Simplest possible multithreading.

```
Week 1-2: Infrastructure
├─▶ Implement SPSCQueue (concurrent_queue.h)
├─▶ Add unit tests for queue correctness
├─▶ Benchmark queue throughput (aim for >10 MSps)
└─▶ Create ThreadSafePipeBuf wrapper

Week 2-3: Two-Thread Prototype
├─▶ Split into 2 threads: [I/O+FIR] → [Everything Else]
├─▶ Measure throughput improvement
├─▶ Verify output correctness (bit-exact match vs single-threaded)
└─▶ Profile with perf/gprof to find bottlenecks
```

**Success Criteria:**
- [ ] 1.3-1.5x speedup on dual-core
- [ ] Bit-exact output (no data corruption)
- [ ] No deadlocks or crashes over 24-hour test

**Minimal Code Changes:**

```cpp
// File: apps/leandvb_mt.cc (new file, copy of leandvb.cc)

// Original single-threaded main loop:
void single_threaded_main() {
  scheduler sch;
  auto *r_fir = new fir_filter<cf32, float>(&sch, ...);
  auto *r_agc = new rms_agc<float>(&sch, ...);
  // ... 20 more stages ...
  sch.run();  // Sequential execution
}

// New two-thread version:
void two_threaded_main() {
  // Queue between threads
  SPSCQueue<complex<float>> queue(1024*1024);

  // Thread 1: I/O + FIR
  std::thread t1([&]() {
    auto *r_in = new file_reader<cf32>(...);
    auto *r_fir = new fir_filter<cf32, float>(...);

    while (!eof) {
      cf32 sample = r_in->read_one();
      cf32 filtered = r_fir->process(sample);
      queue.push(filtered);
    }
  });

  // Thread 2: Everything else
  std::thread t2([&]() {
    auto *r_agc = new rms_agc<float>(...);
    // ... rest of pipeline ...

    while (true) {
      cf32 sample;
      if (!queue.pop(sample, timeout)) break;
      // ... process through rest of pipeline ...
    }
  });

  t1.join();
  t2.join();
}
```

### Phase 2: Full Pipeline Parallelism (4-6 weeks)

**Goal:** Implement 4-5 thread pipeline.

```
Week 1-2: Thread Decomposition
├─▶ Split into 5 threads (Option A from earlier)
├─▶ Implement thread wrapper classes (ThreadedRunnable)
├─▶ Handle shutdown synchronization
└─▶ Add monitoring (queue depths, throughput)

Week 3-4: Feedback Loops
├─▶ Implement SharedFrequencyState for freq_tap
├─▶ Test stability with Doppler shifts
├─▶ Tune feedback loop gains
└─▶ Compare tracking performance vs single-threaded

Week 5-6: Integration and Testing
├─▶ End-to-end testing with real DVB-S streams
├─▶ Performance regression tests
├─▶ Stress testing (24-hour continuous decode)
└─▶ Cross-platform testing (x86, ARM, RPi)
```

**Success Criteria:**
- [ ] 2.5-3.0x speedup on quad-core
- [ ] Stable frequency tracking (BER < 1e-6)
- [ ] No memory leaks (valgrind clean)
- [ ] Graceful shutdown (no hung threads)

### Phase 3: Optimization and Tuning (2-4 weeks)

**Goal:** Extract maximum performance.

```
Week 1-2: Profiling and Hotspot Elimination
├─▶ Profile with perf, VTune, or Instruments
├─▶ Identify cache misses, false sharing
├─▶ Optimize batch sizes
└─▶ Tune queue sizes

Week 3: Advanced Optimizations
├─▶ CPU affinity pinning
├─▶ Prefetching hints
├─▶ Alignment optimization (cache line boundaries)
└─▶ Consider lock-free batching

Week 4: Documentation and Code Review
├─▶ Document thread interactions
├─▶ Create architectural diagrams
├─▶ Code review and cleanup
└─▶ User guide for multithreading options
```

**Success Criteria:**
- [ ] 3.5-4.0x speedup on quad-core
- [ ] 3.8-4.5x speedup on hexa-core
- [ ] Scalability up to 8 cores (diminishing but positive returns)
- [ ] Production-ready code quality

---

## Code Examples

### Complete Thread Wrapper Class

```cpp
// File: src/leansdr/framework_mt.h

namespace leansdr {

// Base class for multithreaded runnable stages
class ThreadedRunnable {
public:
  ThreadedRunnable(const char *name)
    : name_(name),
      shutdown_requested_(false),
      thread_running_(false) {}

  virtual ~ThreadedRunnable() {
    stop();
  }

  // Start worker thread
  void start() {
    shutdown_requested_ = false;
    thread_ = std::thread(&ThreadedRunnable::thread_main, this);
    thread_running_ = true;
  }

  // Request shutdown and wait for thread to finish
  void stop() {
    if (!thread_running_) return;
    shutdown_requested_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }
    thread_running_ = false;
  }

  // Override this: main processing loop
  virtual void run() = 0;

  // Override this: cleanup before thread exit
  virtual void shutdown() {}

protected:
  const char *name_;
  std::atomic<bool> shutdown_requested_;

private:
  void thread_main() {
    fprintf(stderr, "[%s] Thread started\n", name_);

    try {
      run();
    } catch (const std::exception& e) {
      fprintf(stderr, "[%s] Exception: %s\n", name_, e.what());
    }

    shutdown();
    fprintf(stderr, "[%s] Thread stopped\n", name_);
  }

  std::thread thread_;
  bool thread_running_;
};

// Example: Multithreaded FIR filter
template<typename Tin, typename Tcoeff, typename Tout>
class ThreadedFIRFilter : public ThreadedRunnable {
public:
  ThreadedFIRFilter(
      SPSCQueue<Tin>& input_queue,
      SPSCQueue<Tout>& output_queue,
      int ncoeffs,
      Tcoeff *coeffs)
    : ThreadedRunnable("FIR Filter"),
      input_(input_queue),
      output_(output_queue),
      ncoeffs_(ncoeffs),
      coeffs_(coeffs) {}

  void run() override {
    const size_t BATCH_SIZE = 256;
    Tin input_batch[BATCH_SIZE + ncoeffs_];  // Extra space for filter history
    Tout output_batch[BATCH_SIZE];

    // Initialize filter state (history buffer)
    std::memset(input_batch, 0, ncoeffs_ * sizeof(Tin));

    while (!shutdown_requested_) {
      // Pop batch from input queue
      size_t n_read = 0;
      for (size_t i = 0; i < BATCH_SIZE && !shutdown_requested_; ++i) {
        if (!input_.try_pop(input_batch[ncoeffs_ + i])) {
          if (i == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
          }
          break;
        }
        ++n_read;
      }

      if (n_read == 0) continue;

      // Process batch (FIR convolution)
      for (size_t i = 0; i < n_read; ++i) {
        Tout acc = 0;
        for (int j = 0; j < ncoeffs_; ++j) {
          acc += input_batch[i + ncoeffs_ - 1 - j] * coeffs_[j];
        }
        output_batch[i] = acc;
      }

      // Shift history buffer
      std::memmove(input_batch, input_batch + n_read, ncoeffs_ * sizeof(Tin));

      // Push batch to output queue
      for (size_t i = 0; i < n_read; ++i) {
        while (!output_.try_push(output_batch[i]) && !shutdown_requested_) {
          std::this_thread::yield();
        }
      }
    }
  }

private:
  SPSCQueue<Tin>& input_;
  SPSCQueue<Tout>& output_;
  int ncoeffs_;
  Tcoeff *coeffs_;
};

}  // namespace leansdr
```

### Main Application: Multithreaded Pipeline

```cpp
// File: apps/leandvb_mt.cc

#include "leansdr/framework_mt.h"
#include "leansdr/concurrent_queue.h"
#include "leansdr/dsp.h"
#include "leansdr/sdr.h"

int main(int argc, char *argv[]) {
  using namespace leansdr;

  // Parse command-line args (same as leandvb.cc)
  // ...

  // Create inter-thread queues
  SPSCQueue<cf32> queue1(1024*1024);      // I/O → FreqTim
  SPSCQueue<cf32> queue2(512*1024);       // FreqTim → Demod
  SPSCQueue<uint8_t> queue3(128*1024);    // Demod → FEC
  SPSCQueue<mpeg_ts_packet> queue4(4096); // FEC → Output

  // Create shared state for feedback loops
  SharedFrequencyState shared_freq;

  // Thread 1: I/O + FIR resampler
  ThreadedFIRFilter<cf32, float, cf32> thread1(
    input_queue, queue1, ncoeffs, fir_coeffs);

  // Thread 2: Frequency + Timing recovery
  // (Implementation: see below)

  // Thread 3: Demodulation
  // ...

  // Thread 4: FEC
  // ...

  // Thread 5: Output
  // ...

  // Start all threads
  thread1.start();
  thread2.start();
  thread3.start();
  thread4.start();
  thread5.start();

  // Wait for completion (or Ctrl-C)
  signal(SIGINT, [](int) {
    fprintf(stderr, "Shutting down...\n");
    // Set shutdown flags
  });

  thread1.stop();
  thread2.stop();
  thread3.stop();
  thread4.stop();
  thread5.stop();

  return 0;
}
```

### Thread Creation Patterns

```cpp
// Pattern 1: C++11 std::thread with lambda
std::thread t1([&]() {
  while (!shutdown_requested) {
    // Processing loop
  }
});

// Pattern 2: POSIX pthread (for advanced control)
pthread_t thread_handle;
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

pthread_create(&thread_handle, &attr, thread_func, arg);
pthread_join(thread_handle, NULL);

// Pattern 3: std::async (for one-off tasks)
auto future = std::async(std::launch::async, []() {
  return expensive_computation();
});
int result = future.get();  // Blocks until done
```

---

## Performance Projections

### Theoretical Speedup Analysis

**Amdahl's Law:**
```
Speedup = 1 / (S + P/N)

Where:
  S = Serial fraction (non-parallelizable code)
  P = Parallel fraction
  N = Number of cores

For LeanSDR:
  S ≈ 0.10 (I/O, control, overhead)
  P ≈ 0.90 (parallelizable stages)

2 cores: Speedup = 1 / (0.10 + 0.90/2) = 1.82x
4 cores: Speedup = 1 / (0.10 + 0.90/4) = 3.08x
8 cores: Speedup = 1 / (0.10 + 0.90/8) = 3.88x
∞ cores: Speedup_max = 1 / 0.10 = 10x (limited by serial fraction)
```

**Realistic Expectations:**
- **2-core:** 1.6-1.8x (overhead from synchronization)
- **4-core:** 2.8-3.2x (near-ideal for 5-thread pipeline)
- **8-core:** 3.2-3.8x (limited by 5 threads, some idle cores)

### Expected Performance by System

| System | Cores | Baseline Throughput | Multithreaded | Speedup | +NEON |
|--------|-------|---------------------|---------------|---------|-------|
| **Raspberry Pi 3** | 4 | 0.8 MSym/s | 2.2 MSym/s | **2.8x** | 4.4 MSym/s |
| **Raspberry Pi 4** | 4 | 1.2 MSym/s | 3.4 MSym/s | **2.8x** | 6.8 MSym/s |
| **Raspberry Pi 5** | 4 | 2.0 MSym/s | 5.6 MSym/s | **2.8x** | 11.2 MSym/s |
| **Desktop (i5)** | 4 | 3.0 MSym/s | 8.4 MSym/s | **2.8x** | N/A (no NEON) |
| **Desktop (i7)** | 8 | 3.5 MSym/s | 12.6 MSym/s | **3.6x** | N/A |
| **Server (Xeon)** | 16 | 4.0 MSym/s | 14.4 MSym/s | **3.6x** | N/A |

**Combined NEON + Multithreading:**
- Sequential: NEON gives 2.5x, then multithreading gives 2.8x
- **Total:** 2.5 × 2.8 = **7.0x** (Raspberry Pi 4)

### Scalability Graph

```
Speedup vs Number of Cores:

   4x │                              ┌─────── Theoretical (Amdahl)
      │                         ┌────┘
   3x │                    ┌────┘
      │               ┌────┘
   2x │          ┌────┘              • Measured (expected)
      │     ┌────┘
   1x │─────┘
      └─────┬─────┬─────┬─────┬─────┬─────▶
           1     2     4     8    16   Cores

Key Insight: Diminishing returns beyond 4 cores due to only 5-thread pipeline.
To scale to 8+ cores, would need finer-grained parallelism (e.g., data parallelism
within Viterbi or FIR filter).
```

---

## Debugging and Profiling

### Common Multithreading Bugs

#### 1. Race Condition

**Symptom:** Intermittent wrong results, varies between runs.

**Example:**
```cpp
// WRONG: Multiple threads accessing shared variable
float freq_offset;  // Shared, no synchronization

// Thread 1
freq_offset = new_estimate;

// Thread 2
float freq = freq_offset;  // May read partially-written value!
```

**Fix:**
```cpp
std::atomic<float> freq_offset;  // Atomic read/write

// Thread 1
freq_offset.store(new_estimate, std::memory_order_release);

// Thread 2
float freq = freq_offset.load(std::memory_order_acquire);
```

**Detection:**
- ThreadSanitizer: `g++ -fsanitize=thread`
- Helgrind (Valgrind): `valgrind --tool=helgrind ./leandvb`

#### 2. Deadlock

**Symptom:** Program hangs, all threads blocked.

**Example:**
```cpp
// Thread 1
mutex_a.lock();
mutex_b.lock();  // Waits for Thread 2 to release mutex_b

// Thread 2
mutex_b.lock();
mutex_a.lock();  // Waits for Thread 1 to release mutex_a
// DEADLOCK!
```

**Fix:**
- Always acquire locks in same order
- Use `std::lock()` to acquire multiple locks atomically
- Use lock-free data structures

**Detection:**
- Timeout on queue operations (catches hangs)
- GDB: `thread apply all bt` to see where threads are stuck

#### 3. False Sharing

**Symptom:** Poor performance despite low contention.

**Example:**
```cpp
// WRONG: Adjacent atomic variables on same cache line
struct ThreadState {
  std::atomic<int> counter1;  // Byte 0-3
  std::atomic<int> counter2;  // Byte 4-7 ◄── SAME CACHE LINE!
};

// Thread 1 updates counter1 → invalidates Thread 2's cache
// Thread 2 updates counter2 → invalidates Thread 1's cache
// Ping-pong effect: ~50-100ns per update instead of ~5ns
```

**Fix:**
```cpp
// Align to cache line boundaries (64 bytes on ARM/x86)
struct ThreadState {
  alignas(64) std::atomic<int> counter1;
  alignas(64) std::atomic<int> counter2;
};
```

**Detection:**
- `perf stat -e cache-misses,cache-references`
- Look for high cache miss rate (>10%)

#### 4. Memory Ordering Bugs

**Symptom:** Works on x86, breaks on ARM (weaker memory model).

**Example:**
```cpp
// Thread 1
data = 42;
ready = true;  // Release

// Thread 2
if (ready) {    // Acquire
  print(data);  // May print garbage on ARM!
}
```

**Fix:**
```cpp
std::atomic<bool> ready;

// Thread 1
data = 42;
ready.store(true, std::memory_order_release);  // Ensures data written first

// Thread 2
if (ready.load(std::memory_order_acquire)) {
  print(data);  // Guaranteed to see data=42
}
```

### Profiling Techniques

#### Throughput Monitoring

```cpp
// Embed performance counters in each thread
struct ThreadStats {
  std::atomic<uint64_t> samples_processed;
  std::atomic<uint64_t> time_spent_ns;

  void update(size_t n_samples, uint64_t elapsed_ns) {
    samples_processed.fetch_add(n_samples);
    time_spent_ns.fetch_add(elapsed_ns);
  }

  double get_throughput_msps() {
    uint64_t samples = samples_processed.load();
    uint64_t time_ns = time_spent_ns.load();
    return (samples / 1e6) / (time_ns / 1e9);
  }
};

// Print stats periodically
void monitoring_thread() {
  while (!shutdown) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    printf("Thread 1: %.2f MSps, Queue1: %zu/%zu\n",
           thread1_stats.get_throughput_msps(),
           queue1.size(), queue1.capacity());
    // ... repeat for all threads ...
  }
}
```

#### Queue Depth Visualization

```
Real-time queue monitoring output:
┌─────────────────────────────────────────────────────────────┐
│ Thread Pipeline Status (1-second averages)                  │
├─────────────────────────────────────────────────────────────┤
│ T1 (I/O)      [████████████████████] 2.1 MSps  Queue1: 45%  │
│ T2 (FreqTim)  [█████████████████   ] 1.9 MSps  Queue2: 78%  │
│ T3 (Demod)    [██████████          ] 1.2 MSps  Queue3: 95%◄─┐│
│ T4 (FEC)      [████████████████████] 2.0 MSps  Queue4: 12%  ││
│ T5 (Output)   [████████████████████] 2.0 MSps               ││
└─────────────────────────────────────────────────────────────┘
                                                               │
Analysis: Queue3 is 95% full → T4 (FEC) is bottleneck ────────┘
Action: Optimize Viterbi decoder or split into 2 threads
```

#### Lock Contention Analysis

```bash
# Linux perf
perf record -e syscalls:sys_enter_futex ./leandvb_mt < input.iq
perf report
# Look for high % time in futex (indicates lock contention)

# Flame graph
perf record -g -F 999 ./leandvb_mt < input.iq
perf script | ./flamegraph.pl > flamegraph.svg
# Look for wide bars in synchronization primitives
```

---

## Risk Mitigation

### Risk Matrix

| Risk | Probability | Impact | Mitigation Strategy |
|------|-------------|--------|-------------------|
| **Deadlock** | Low | Critical | • Use lock-free queues<br>• Timeout on all blocking ops<br>• Extensive testing |
| **Race Condition** | Medium | High | • ThreadSanitizer in CI<br>• Code review<br>• Minimize shared state |
| **Priority Inversion** | Low | Medium | • Avoid mixing real-time and normal priority<br>• Use priority inheritance mutexes |
| **Cache Coherency Issues** | Medium | Medium | • Align atomics to cache lines<br>• Profile with perf |
| **Feedback Loop Instability** | Medium | High | • Test with various Doppler profiles<br>• Add adaptive gain control |
| **Performance Regression** | Low | Medium | • Benchmark every commit<br>• Keep single-threaded as fallback |
| **Platform-Specific Bugs** | Medium | High | • Test on x86, ARM, RPi<br>• Use std:: primitives (portable) |
| **Debugging Difficulty** | High | Medium | • Extensive logging<br>• Reproducible test cases<br>• Simplify first (2-thread prototype) |

### Mitigation: Graceful Degradation

```cpp
// Always provide single-threaded fallback
int main(int argc, char *argv[]) {
  bool use_multithreading = true;

  // Command-line option to disable multithreading
  if (has_arg("--single-threaded")) {
    use_multithreading = false;
  }

  // Auto-detect: Disable multithreading on single-core systems
  if (std::thread::hardware_concurrency() < 2) {
    fprintf(stderr, "Warning: Single-core system, disabling multithreading\n");
    use_multithreading = false;
  }

  if (use_multithreading) {
    return main_multithreaded(argc, argv);
  } else {
    return main_single_threaded(argc, argv);  // Original code path
  }
}
```

### Mitigation: Extensive Testing

```bash
# 1. Correctness: Bit-exact match vs single-threaded
./leandvb --single-threaded < test.iq > output_st.ts
./leandvb_mt < test.iq > output_mt.ts
cmp output_st.ts output_mt.ts  # Must be identical

# 2. Stress test: 24-hour continuous run
timeout 86400 ./leandvb_mt < /dev/urandom > /dev/null

# 3. Valgrind: Memory leaks and races
valgrind --tool=helgrind --leak-check=full ./leandvb_mt < test.iq

# 4. ThreadSanitizer
g++ -fsanitize=thread -g -O1 src/apps/leandvb_mt.cc -o leandvb_mt_tsan
./leandvb_mt_tsan < test.iq

# 5. Fuzzing: Random inputs
for i in {1..1000}; do
  dd if=/dev/urandom bs=1M count=10 | ./leandvb_mt > /dev/null
  if [ $? -ne 0 ]; then echo "CRASH on iteration $i"; break; fi
done
```

---

## Validation Strategy

### Correctness Validation

**Requirement:** Multithreaded output must be **bit-exact** identical to single-threaded output.

**Test Plan:**
1. **Golden Dataset:** Record 10-minute DVB-S stream (covering various conditions)
2. **Single-threaded baseline:** Process with original leandvb, save output
3. **Multithreaded test:** Process with leandvb_mt, save output
4. **Binary comparison:** `cmp output_st.ts output_mt.ts`
5. **Statistical analysis:** Compute BER, packet error rate, MPEG continuity counter errors

**Acceptance Criteria:**
- [ ] Bit-exact match for synthetic test vectors (AWGN, no Doppler)
- [ ] BER difference < 1% for real-world streams (allows minor timing differences)
- [ ] No MPEG sync errors (0x47 byte alignment maintained)

### Performance Validation

**Metrics:**
1. **Throughput:** Symbol rate (MSym/s) achieved in real-time
2. **Latency:** Time from IQ input to MPEG output
3. **CPU utilization:** Per-core usage, total system load
4. **Scalability:** Speedup vs number of cores

**Benchmark Suite:**
```bash
# Micro-benchmarks (isolated stages)
./bench_queue       # Queue throughput: aim for >50 MSps
./bench_fir         # FIR filter: aim for >10 MSps per thread
./bench_viterbi     # Viterbi: aim for >2 MSps per thread

# End-to-end benchmark
time ./leandvb_mt --sr 2000e3 < input_10sec.iq > /dev/null
# Measure: Real time / Input duration (should be <1.0 for real-time)

# Scalability test
for threads in 1 2 4 8; do
  ./leandvb_mt --threads $threads < input.iq > /dev/null
done
# Plot: Speedup vs Threads
```

**Performance Regression Testing:**
- Run benchmarks on every commit (CI/CD)
- Alert if throughput drops >5%
- Track performance over time (grafana dashboard)

---

## Conclusion and Next Steps

### Summary

Multithreading LeanSDR is a **high-risk, high-reward** optimization:
- **Potential:** 3-4x speedup on quad-core, 6-8x combined with NEON
- **Challenges:** Feedback loops, thread safety, debugging complexity
- **Recommendation:** Proceed in phases, validate rigorously

### Immediate Next Steps

1. **Week 1-2:** Implement SPSCQueue, validate correctness and performance
2. **Week 3-4:** Build 2-thread prototype, measure speedup
3. **Week 5-6:** Expand to 5-thread pipeline, integrate feedback loops
4. **Week 7-8:** Optimize, profile, and tune

### Long-Term Roadmap

1. **Q1 2026:** Phase 1 complete (2-thread prototype, 1.5x speedup)
2. **Q2 2026:** Phase 2 complete (5-thread pipeline, 3x speedup)
3. **Q3 2026:** Phase 3 complete (optimized, production-ready)
4. **Q4 2026:** Combine with NEON (6-8x total speedup)

### Success Metrics

| Milestone | Target Speedup | Status |
|-----------|----------------|--------|
| 2-thread prototype | 1.5-1.8x | 🔲 Planned |
| 5-thread pipeline | 2.8-3.2x | 🔲 Planned |
| Optimized + tuned | 3.5-4.0x | 🔲 Planned |
| Combined with NEON | 6.0-8.0x | 🔲 Planned |

---

**For SIMD optimization strategy, see:** [NEON-OPTIMIZATION-PLAN.md](NEON-OPTIMIZATION-PLAN.md)

**Last Updated:** 2025-11-17
