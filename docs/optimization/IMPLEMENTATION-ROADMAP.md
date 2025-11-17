# LeanSDR Optimization Implementation Roadmap

**Document:** Comprehensive 18-Week Implementation Plan
**Last Updated:** 2025-11-17
**Project Goal:** Achieve 6-8x overall performance improvement through NEON SIMD and Multithreading
**Target Platform:** ARM-based systems (Raspberry Pi 3/4/5)
**Estimated Duration:** 18 weeks (4.5 months)
**Team Size:** 1-2 developers

---

## Executive Summary

This roadmap provides a detailed, week-by-week plan for optimizing LeanSDR through a phased approach:

1. **Phase 1 (Weeks 1-4):** NEON Quick Wins - Target highest-ROI components with SIMD optimizations
2. **Phase 2 (Weeks 5-10):** Pipeline Threading - Implement lock-free multithreaded pipeline
3. **Phase 3 (Weeks 11-18):** Advanced Optimizations - Polish, tune, and validate for production

**Expected Results:**
- Phase 1: 2.0-2.5x speedup (NEON on critical path)
- Phase 2: Additional 2.8-3.2x speedup (4-thread pipeline parallelism)
- Phase 3: Final tuning to 6-8x total speedup
- **Target:** Real-time decoding of 3-6 MSym/s on Raspberry Pi 4

---

## ASCII Gantt Chart

```
Week  │ Phase 1: NEON Quick Wins │ Phase 2: Pipeline Threading │ Phase 3: Advanced Optimizations
──────┼──────────────────────────┼─────────────────────────────┼──────────────────────────────────
  1   │ ████ FIR Filter NEON     │                             │
  2   │ ████ FIR + Testing       │                             │
  3   │      ████ FIR Sampler    │                             │
  4   │      ████ AGC + Bench    │                             │
      │           ▼ Milestone 1  │                             │
  5   │                          │ ████ Queue Impl             │
  6   │                          │ ████ Queue Tests            │
  7   │                          │      ████ 2-Thread Proto    │
  8   │                          │      ████ Integration       │
  9   │                          │           ████ 5-Thread     │
 10   │                          │           ████ Feedback     │
      │                          │                ▼ M2         │
 11   │                          │                             │ ████ Viterbi NEON
 12   │                          │                             │ ████ Viterbi Test
 13   │                          │                             │      ████ FFT+RS
 14   │                          │                             │      ████ MPEG Sync
 15   │                          │                             │           ████ Load Bal
 16   │                          │                             │           ████ Tuning
 17   │                          │                             │                ████ Validation
 18   │                          │                             │                ████ Polish
      │                          │                             │                     ▼ M3

Legend: ████ = Active work    ▼ = Milestone    M1/M2/M3 = Major deliverables
```

### Parallel Work Streams

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        Concurrent Activities Timeline                        │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  Weeks 1-4:  [NEON Implementation] ─────────────────────────┐                │
│              [Unit Testing]        ─────────────────────────┤                │
│              [Benchmarking]                 ────────────────┘                │
│                                                                               │
│  Weeks 5-10: [Queue Implementation]     ────────┐                            │
│              [2-Thread Prototype]            ────────┐                        │
│              [5-Thread Pipeline]                  ────────┐                   │
│              [Integration Testing]                     ────────┐              │
│                                                                               │
│  Weeks 11-18:[Viterbi NEON]         ────────┐                                │
│              [FFT/RS Optimization]       ────────┐                            │
│              [Load Balancing]                 ────────┐                       │
│              [Performance Tuning]                  ────────────┐              │
│              [Cross-platform Testing]                   ───────────┐          │
│              [Documentation]                                 ──────────┐      │
│                                                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: NEON Quick Wins (Weeks 1-4)

**Objective:** Achieve 2-2.5x speedup by optimizing the three highest-ROI bottlenecks with ARM NEON SIMD instructions.

**Target Components:**
1. FIR Filter (30-40% CPU → 8% CPU)
2. FIR Sampler (8-12% CPU → 2-3% CPU)
3. AGC Power Measurement (3-5% CPU → 1% CPU)

### Week 1: FIR Filter NEON Optimization

**Primary Goal:** Implement NEON-optimized FIR filter with 4x speedup

#### Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Set up NEON build environment<br>• Create `dsp_neon.h` skeleton<br>• Review ARM NEON intrinsics documentation | Build system with NEON flags | Dev1 |
| **Tue-Wed** | • Implement NEON FIR filter template specialization<br>• Handle 4-way SIMD MAC operations<br>• Implement horizontal sum reduction<br>• Add scalar tail processing | Working NEON FIR filter | Dev1 |
| **Thu** | • Create unit test suite<br>• Test against scalar reference<br>• Verify bit-exact correctness (tolerance: 1e-5)<br>• Test edge cases (non-multiple-of-4 coefficients) | Unit tests passing | Dev1/Dev2 |
| **Fri** | • Micro-benchmark FIR filter<br>• Measure isolated speedup<br>• Profile with perf/gprof<br>• Document results | Benchmark report | Dev1 |

**Success Criteria:**
- [ ] NEON FIR filter compiles on ARMv7/ARMv8
- [ ] Correctness: Max error < 1e-5 vs scalar
- [ ] Performance: 3.5-4.5x speedup vs scalar
- [ ] Fallback: Compiles on x86 (uses scalar path)

**Dependencies:**
- Compiler: GCC 7.5+ or Clang 10+
- Hardware: ARM development board or QEMU emulator

**Risk Mitigation:**
- **Risk:** NEON intrinsics unfamiliar → **Mitigation:** Study example code from ARM documentation
- **Risk:** Horizontal sum is slow → **Mitigation:** Use pairwise adds, accept 1.3x overhead

**Deliverables:**
- `src/leansdr/dsp_neon.h` - NEON optimized FIR filter
- `test/test_fir_neon.cc` - Unit tests
- `docs/benchmarks/week1_fir_benchmark.txt` - Performance data

---

### Week 2: FIR Filter Integration and Testing

**Primary Goal:** Integrate NEON FIR filter into leandvb and validate end-to-end

#### Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Integrate NEON FIR into leandvb<br>• Add compile-time feature detection<br>• Test with real DVB-S streams | Working integration | Dev1 |
| **Tue** | • End-to-end correctness testing<br>• Compare MPEG-TS output (NEON vs scalar)<br>• Test with various symbol rates (0.5-4 MSym/s) | Correctness validation | Dev1/Dev2 |
| **Wed** | • Performance regression testing<br>• Measure actual speedup in leandvb<br>• Profile to confirm FIR CPU reduction | Performance report | Dev1 |
| **Thu** | • Cross-platform testing<br>• Test on RPi3, RPi4, RPi5<br>• Test x86 fallback (should use scalar) | Platform compatibility | Dev2 |
| **Fri** | • Bug fixes and polish<br>• Code review<br>• Update documentation | Code review complete | Team |

**Success Criteria:**
- [ ] End-to-end: Bit-exact MPEG-TS output vs baseline
- [ ] Performance: 1.25-1.35x overall speedup in leandvb
- [ ] Stability: No crashes in 1-hour continuous run
- [ ] Portability: Builds and runs on ARM and x86

**Testing Checkpoints:**
1. **Unit Test:** `make test_neon` passes
2. **Integration Test:** `./leandvb --neon < test.iq | md5sum` matches baseline
3. **Stress Test:** 1-hour run with no errors
4. **Platform Test:** Works on 3+ ARM platforms

**Decision Point:**
- **IF** speedup < 1.2x → Profile deeper, check compiler flags (-O3, -ftree-vectorize)
- **IF** output differs → Check rounding errors, increase tolerance to 1e-4
- **ELSE** → Proceed to Week 3

**Deliverables:**
- Updated `src/apps/leandvb.cc` with NEON integration
- `test/integration/test_fir_integration.sh` - Integration test script
- `docs/WEEK2_INTEGRATION_REPORT.md` - Test results and performance data

---

### Week 3: FIR Sampler and AGC NEON Optimization

**Primary Goal:** Apply NEON optimizations to FIR sampler and AGC (second/third highest ROI targets)

#### Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Implement NEON FIR sampler<br>• Reuse FIR filter NEON code<br>• Handle interpolation indices | NEON FIR sampler | Dev1 |
| **Tue** | • Implement NEON AGC power measurement<br>• Vectorize power calculation (re² + im²)<br>• Batch process 4 samples at a time | NEON AGC | Dev1 |
| **Wed** | • Unit testing for both components<br>• Verify correctness vs scalar<br>• Edge case testing | Unit tests passing | Dev1/Dev2 |
| **Thu** | • Integrate into leandvb<br>• End-to-end testing<br>• Measure cumulative speedup | Integration complete | Dev1 |
| **Fri** | • Performance benchmarking<br>• Profile end-to-end pipeline<br>• Document cumulative results | Benchmark report | Dev1 |

**Success Criteria:**
- [ ] FIR Sampler: 3.5-4.0x speedup
- [ ] AGC: 3.0-4.0x speedup
- [ ] Cumulative: 1.7-2.0x overall speedup
- [ ] Correctness: Bit-exact match (tolerance: 1e-5)

**Dependencies:**
- Week 1-2 FIR filter implementation (reuse code patterns)

**Deliverables:**
- `src/leansdr/dsp_neon.h` - Added FIR sampler NEON code
- `src/leansdr/sdr_neon.h` - NEON AGC implementation
- `docs/benchmarks/week3_cumulative_benchmark.txt`

---

### Week 4: Integration, Benchmarking, and Phase 1 Wrap-up

**Primary Goal:** Validate Phase 1 results, establish baseline for Phase 2

#### Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Comprehensive integration testing<br>• Test all NEON components together<br>• Validate with 10+ DVB-S streams | Test results | Dev1/Dev2 |
| **Tue** | • Performance benchmarking suite<br>• Measure on RPi3, RPi4, RPi5<br>• Compare vs baseline (scalar) | Performance matrix | Dev1 |
| **Wed** | • Code review and cleanup<br>• Remove debug code<br>• Add inline documentation<br>• Check for memory leaks (valgrind) | Clean codebase | Team |
| **Thu** | • Cross-platform validation<br>• Test ARM32, ARM64, x86-64<br>• Test with GCC and Clang<br>• Verify fallback paths | Platform matrix | Dev2 |
| **Fri** | • **MILESTONE 1 REVIEW**<br>• Present results to stakeholders<br>• Document lessons learned<br>• Plan adjustments for Phase 2 | Phase 1 report | Team |

**Success Criteria (Phase 1 Complete):**
- [ ] Overall speedup: 1.8-2.2x on Raspberry Pi 4
- [ ] Throughput: 2.2-2.6 MSym/s (from 1.2 MSym/s baseline)
- [ ] Correctness: 100% MPEG-TS packet match vs baseline
- [ ] Stability: 24-hour continuous decode without crashes
- [ ] Portability: Works on 5+ platforms

**Phase 1 Milestone 1 Deliverables:**
- [ ] Working NEON-optimized leandvb binary
- [ ] Comprehensive test suite (unit + integration)
- [ ] Performance benchmark report
- [ ] Updated documentation (README, build instructions)
- [ ] Git tag: `v1.0-neon-phase1`

**Go/No-Go Decision:**
- **GO to Phase 2 IF:**
  - Speedup ≥ 1.8x
  - Zero correctness failures
  - Stable for ≥ 24 hours
- **ADJUST IF:**
  - Speedup < 1.8x → Profile more, add more NEON optimizations (Constellation RX)
  - Stability issues → Debug and fix before proceeding

---

## Phase 2: Pipeline Threading (Weeks 5-10)

**Objective:** Implement 4-5 thread pipeline parallelism to achieve 2.8-3.2x additional speedup on quad-core systems.

**Strategy:** Lock-free SPSC (Single Producer Single Consumer) queues between pipeline stages

**Expected Cumulative Result:** 1.8x (Phase 1) × 2.8x (Phase 2) = **5.0x total speedup**

### Week 5-6: Thread-Safe Queue Implementation

**Primary Goal:** Build and validate high-performance lock-free queue infrastructure

#### Week 5 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Design lock-free SPSC queue API<br>• Review literature (1024cores.net)<br>• Define interface and memory model | Design doc | Dev1 |
| **Tue** | • Implement SPSCQueue template<br>• Ring buffer with atomic read/write pointers<br>• Memory ordering (acquire/release semantics) | Working queue | Dev1 |
| **Wed** | • Implement blocking push/pop with timeout<br>• Add batch transfer methods<br>• Cache line alignment (avoid false sharing) | Complete API | Dev1 |
| **Thu** | • Unit tests for correctness<br>• Test concurrent push/pop<br>• Test edge cases (empty, full, wrap-around) | Unit tests | Dev1/Dev2 |
| **Fri** | • Micro-benchmark throughput<br>• Measure latency (push/pop time)<br>• Test with various buffer sizes | Benchmark data | Dev2 |

**Week 5 Success Criteria:**
- [ ] Queue correctness: 10M push/pop operations without data loss
- [ ] Throughput: ≥ 50 MSps (samples/second)
- [ ] Latency: < 20ns per push/pop (uncontended)
- [ ] Thread-safe: Passes ThreadSanitizer (`-fsanitize=thread`)

#### Week 6 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Implement MPSCQueue for control messages<br>• Use mutex for multi-producer case | MPSC queue | Dev2 |
| **Tue** | • Create ThreadSafePipeBuf wrapper<br>• Make compatible with existing pipebuf API<br>• Minimize code changes needed | Wrapper class | Dev1 |
| **Wed** | • Stress testing<br>• 24-hour concurrent test<br>• Memory leak check (valgrind) | Stress test results | Dev1/Dev2 |
| **Thu** | • Performance tuning<br>• Optimize batch sizes<br>• Test alignment and padding | Tuned implementation | Dev1 |
| **Fri** | • Documentation and code review<br>• Document thread safety guarantees<br>• Add usage examples | Code review done | Team |

**Week 6 Success Criteria:**
- [ ] Zero memory leaks in 24-hour test
- [ ] Zero data corruption in 100M sample stress test
- [ ] API compatibility with existing pipebuf
- [ ] Clean ThreadSanitizer report

**Deliverables (Weeks 5-6):**
- `src/leansdr/concurrent_queue.h` - Lock-free SPSC/MPSC queues
- `src/leansdr/framework_mt.h` - Thread-safe wrappers
- `test/test_queue.cc` - Comprehensive queue tests
- `docs/QUEUE_DESIGN.md` - Architecture documentation

**Risk Mitigation:**
- **Risk:** Queue deadlock → **Mitigation:** Always use timeouts, never infinite blocking
- **Risk:** False sharing → **Mitigation:** Cache-line align atomic variables (64 bytes)
- **Risk:** Memory ordering bugs on ARM → **Mitigation:** Use acquire/release, test on ARM hardware

---

### Week 7-8: Two-Thread Prototype

**Primary Goal:** Prove feasibility with simplest possible multithreading (2 threads)

**Thread Assignment:**
- Thread 1: I/O + FIR resampler + AGC (50% CPU)
- Thread 2: Everything else (50% CPU)

#### Week 7 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Create `leandvb_mt.cc` skeleton<br>• Set up 2-thread architecture<br>• Define queue between threads | Prototype skeleton | Dev1 |
| **Tue** | • Implement Thread 1 (I/O + FIR + AGC)<br>• Adapt existing code to push to queue<br>• Handle EOF gracefully | Thread 1 working | Dev1 |
| **Wed** | • Implement Thread 2 (rest of pipeline)<br>• Pop from queue, process sequentially<br>• Maintain single-threaded scheduler | Thread 2 working | Dev1 |
| **Thu** | • Integration and basic testing<br>• Verify output correctness<br>• Test with simple DVB-S stream | Basic integration | Dev1/Dev2 |
| **Fri** | • Debugging and stabilization<br>• Fix race conditions<br>• Handle edge cases (slow consumer) | Stable prototype | Team |

**Week 7 Success Criteria:**
- [ ] Prototype compiles and runs
- [ ] Produces some output (may have errors)
- [ ] No deadlocks or hangs
- [ ] Baseline performance measured

#### Week 8 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Correctness validation<br>• Compare output vs single-threaded<br>• Debug any differences | Correct output | Dev1/Dev2 |
| **Tue** | • Performance measurement<br>• Benchmark on dual-core and quad-core<br>• Measure speedup vs single-threaded | Performance data | Dev1 |
| **Wed** | • Profiling and analysis<br>• Use perf/gprof to find bottlenecks<br>• Analyze queue depths | Profile report | Dev1 |
| **Thu** | • Optimization<br>• Tune queue sizes<br>• Adjust batch sizes<br>• Test different buffer configurations | Optimized config | Dev1 |
| **Fri** | • Documentation and review<br>• Document thread architecture<br>• Code review | Review complete | Team |

**Week 8 Success Criteria:**
- [ ] Correctness: Bit-exact match vs single-threaded
- [ ] Performance: 1.5-1.7x speedup on dual-core
- [ ] Performance: 1.3-1.5x speedup on quad-core (not optimal yet)
- [ ] Stability: 1-hour continuous run without issues

**Deliverables (Weeks 7-8):**
- `src/apps/leandvb_mt.cc` - Two-thread prototype
- `test/test_2thread.sh` - Integration test
- `docs/2THREAD_ARCHITECTURE.md` - Design documentation
- `docs/benchmarks/week8_2thread_benchmark.txt`

**Decision Point:**
- **IF** speedup < 1.3x → Debug queue overhead, check context switching
- **IF** output incorrect → Fix synchronization bugs before proceeding
- **ELSE** → Proceed to 5-thread implementation

---

### Week 9-10: Full 5-Thread Pipeline

**Primary Goal:** Implement production-ready 5-thread pipeline with balanced load

**Thread Architecture:**
- Thread 1: I/O + FIR resampler + AGC (18% load)
- Thread 2: Frequency tracking + Timing recovery (22% load)
- Thread 3: Constellation receiver + Demodulation (28% load)
- Thread 4: Viterbi + Deinterleaver + Reed-Solomon (32% load)
- Thread 5: MPEG sync + Descrambler + Output (8% load)

#### Week 9 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Design 5-thread architecture<br>• Define queues and interfaces<br>• Create thread wrapper classes | Design document | Dev1 |
| **Tue-Wed** | • Implement all 5 thread workers<br>• Create ThreadedRunnable base class<br>• Implement start/stop/join logic | 5 thread workers | Dev1 |
| **Thu** | • Integrate and debug<br>• Fix compilation errors<br>• Basic smoke testing | Compiling prototype | Team |
| **Fri** | • Initial testing and debugging<br>• Fix obvious bugs<br>• Measure baseline performance | Initial results | Team |

**Week 9 Success Criteria:**
- [ ] All 5 threads start and stop cleanly
- [ ] No immediate crashes
- [ ] Some correct output produced
- [ ] Queue depths can be monitored

#### Week 10 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • **Feedback loop handling**<br>• Implement SharedFrequencyState<br>• Connect freq_tap between Thread 2 and Thread 3<br>• Test tracking stability | Working feedback | Dev1 |
| **Tue** | • Correctness validation<br>• Compare vs single-threaded baseline<br>• Debug and fix synchronization issues | Correct output | Dev1/Dev2 |
| **Wed** | • Performance benchmarking<br>• Measure on 2-core, 4-core, 8-core<br>• Analyze load distribution | Benchmark matrix | Dev1 |
| **Thu** | • Load balancing<br>• Profile per-thread CPU usage<br>• Adjust thread assignments if needed<br>• Tune queue sizes | Balanced load | Dev1 |
| **Fri** | • **MILESTONE 2 REVIEW**<br>• Validate Phase 2 goals<br>• Document results<br>• Plan Phase 3 priorities | Phase 2 report | Team |

**Week 10 Success Criteria:**
- [ ] Correctness: Bit-exact output vs baseline
- [ ] Performance: 2.5-3.0x speedup on quad-core vs single-threaded
- [ ] Stability: Frequency tracking remains stable (BER < 1e-6)
- [ ] Load balance: No thread >90% utilized, no thread <10%

**Phase 2 Milestone 2 Deliverables:**
- [ ] Working 5-thread leandvb_mt binary
- [ ] Lock-free queue infrastructure
- [ ] Feedback loop implementation (freq_tap)
- [ ] Performance benchmark report
- [ ] Updated documentation
- [ ] Git tag: `v2.0-multithreading`

**Cumulative Performance Target:**
- Single-threaded baseline: 1.2 MSym/s
- After Phase 1 (NEON): 2.4 MSym/s (2.0x)
- After Phase 2 (Threading): 6.0 MSym/s (5.0x total)

**Go/No-Go Decision:**
- **GO to Phase 3 IF:**
  - Speedup ≥ 4.5x cumulative (Phase 1 + Phase 2)
  - Frequency tracking stable
  - Zero data corruption
  - 24-hour stress test passes
- **ADJUST IF:**
  - Speedup < 4.0x → Profile bottlenecks, consider 4-thread architecture instead
  - Tracking instability → Tune feedback loop gains, consider merging Thread 2+3

---

## Phase 3: Advanced Optimizations (Weeks 11-18)

**Objective:** Optimize remaining bottlenecks, tune for production, achieve 6-8x total speedup

**Focus Areas:**
1. Viterbi decoder NEON optimization (Weeks 11-12)
2. FFT and Reed-Solomon optimizations (Weeks 13-14)
3. Load balancing and tuning (Weeks 15-16)
4. Validation and production readiness (Weeks 17-18)

### Week 11-12: Viterbi NEON Optimization

**Primary Goal:** Apply NEON to Viterbi Add-Compare-Select (ACS) operations

**Challenge:** Random access patterns make full vectorization difficult

**Strategy:**
1. Vectorize branch metric computation
2. Vectorize normalization step (subtract min cost)
3. Dual decoder interleaving (process 2 streams in parallel)

#### Week 11 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Profile Viterbi decoder in detail<br>• Identify vectorizable sections<br>• Design NEON strategy | Profile report | Dev1 |
| **Tue** | • Implement NEON normalization<br>• Vectorize min/subtract operations<br>• Use NEON min/sub intrinsics | NEON normalization | Dev1 |
| **Wed** | • Implement NEON branch metrics<br>• Vectorize soft-symbol distance calc<br>• Batch process 8 symbols at a time | NEON branch metrics | Dev1 |
| **Thu** | • Unit testing<br>• Verify correctness vs scalar Viterbi<br>• Test with various code rates | Unit tests | Dev1/Dev2 |
| **Fri** | • Micro-benchmark<br>• Measure isolated Viterbi speedup<br>• Profile hotspots | Benchmark data | Dev1 |

**Week 11 Success Criteria:**
- [ ] NEON normalization: 4-8x speedup
- [ ] NEON branch metrics: 2-4x speedup
- [ ] Overall Viterbi: 2.0-2.5x speedup
- [ ] Correctness: BER matches scalar implementation

#### Week 12 Tasks

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Integrate NEON Viterbi into multithreaded pipeline<br>• Test with real DVB-S streams | Integration complete | Dev1 |
| **Tue** | • End-to-end performance testing<br>• Measure impact on overall throughput<br>• Verify load balance (Thread 4 load) | Performance data | Dev1 |
| **Wed** | • Optimization and tuning<br>• Experiment with dual decoder<br>• Tune batch sizes | Optimized config | Dev1 |
| **Thu** | • Stability testing<br>• Long-duration decode tests<br>• Verify BER across SNR range | Stability report | Dev1/Dev2 |
| **Fri** | • Documentation and code review<br>• Document Viterbi NEON optimizations<br>• Code review | Review complete | Team |

**Week 12 Success Criteria:**
- [ ] Viterbi CPU usage: 22% → 10-12% (Thread 4)
- [ ] Overall speedup: +0.3-0.5x additional
- [ ] Cumulative speedup: 5.3-5.5x
- [ ] BER performance maintained across SNR range

**Deliverables (Weeks 11-12):**
- `src/leansdr/viterbi_neon.h` - NEON Viterbi implementation
- `test/test_viterbi_neon.cc` - Unit tests
- `docs/VITERBI_NEON.md` - Optimization documentation

---

### Week 13-14: FFT and Reed-Solomon Optimizations

**Primary Goal:** Optimize secondary bottlenecks for incremental gains

#### Week 13 Tasks - FFT Optimization

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Evaluate ARM NE10 library<br>• Benchmark ne10 FFT vs current<br>• Check license compatibility | Evaluation report | Dev2 |
| **Tue** | • Integrate ne10 or implement custom NEON FFT<br>• Radix-4 butterfly with NEON | NEON FFT | Dev2 |
| **Wed** | • Test and validate FFT<br>• Verify spectrum display correctness<br>• Benchmark performance | FFT validation | Dev2 |
| **Thu** | • MPEG sync byte search NEON<br>• Vectorize 0x47 byte comparison<br>• Process 16 bytes at a time | NEON MPEG sync | Dev1 |
| **Fri** | • Integration and testing<br>• Measure overall impact<br>• Document results | Integration done | Team |

**Week 13 Success Criteria:**
- [ ] FFT: 2-3x speedup
- [ ] MPEG sync: 4-8x speedup
- [ ] Overall impact: +0.1-0.2x additional speedup
- [ ] No regressions in spectrum display

#### Week 14 Tasks - Reed-Solomon Optimization

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Profile Reed-Solomon decoder<br>• Identify Galois Field bottlenecks<br>• Design optimization strategy | Profile report | Dev1 |
| **Tue** | • Optimize GF multiplication tables<br>• Cache-friendly data layout<br>• Prefetch hints | Optimized tables | Dev1 |
| **Wed** | • NEON syndrome calculation<br>• Vectorize polynomial evaluation<br>• Batch process symbols | NEON syndrome | Dev1 |
| **Thu** | • Testing and validation<br>• Verify error correction capability<br>• Test with injected errors | Validation done | Dev1/Dev2 |
| **Fri** | • Integration and benchmarking<br>• Measure end-to-end impact<br>• Document results | Integration done | Team |

**Week 14 Success Criteria:**
- [ ] Reed-Solomon: 1.5-2.0x speedup
- [ ] Error correction: Maintains FEC performance
- [ ] Overall impact: +0.1-0.15x additional speedup
- [ ] Cumulative speedup: 5.5-6.0x

**Deliverables (Weeks 13-14):**
- `src/leansdr/dsp_neon.h` - NEON FFT butterflies
- `src/leansdr/rs_neon.h` - NEON Reed-Solomon
- `src/leansdr/dvb_neon.h` - NEON MPEG sync
- `docs/benchmarks/week14_secondary_optimizations.txt`

---

### Week 15-16: Load Balancing and Performance Tuning

**Primary Goal:** Fine-tune all parameters for maximum performance and stability

#### Week 15 Tasks - Load Balancing

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Comprehensive profiling<br>• Profile with perf on all platforms<br>• Identify per-thread bottlenecks | Profile report | Dev1 |
| **Tue** | • Analyze load distribution<br>• Check for imbalanced threads<br>• Identify opportunities for reassignment | Analysis report | Dev1 |
| **Wed** | • Rebalance thread assignments<br>• Move stages between threads if needed<br>• Test alternative configurations | Rebalanced config | Dev1 |
| **Thu** | • CPU affinity tuning<br>• Pin threads to specific cores<br>• Test with and without affinity | Affinity config | Dev2 |
| **Fri** | • Queue size tuning<br>• Experiment with buffer sizes<br>• Find optimal memory/latency trade-off | Tuned queue sizes | Dev2 |

**Week 15 Success Criteria:**
- [ ] All threads: 60-85% CPU utilization (balanced)
- [ ] No thread idle >20% of time
- [ ] No thread saturated >95% of time
- [ ] Queue depths: Average 20-40% full

#### Week 16 Tasks - Performance Tuning

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Batch size optimization<br>• Test batch sizes: 64, 128, 256, 512, 1024<br>• Find sweet spot for throughput | Optimal batch sizes | Dev1 |
| **Tue** | • Cache optimization<br>• Analyze cache miss rates<br>• Improve data layout and alignment | Cache-optimized code | Dev1 |
| **Wed** | • Compiler flag optimization<br>• Test: -O2, -O3, -Ofast, -march=native<br>• Test: -flto (link-time optimization) | Optimal build flags | Dev2 |
| **Thu** | • Memory allocation optimization<br>• Reduce malloc/free in hot paths<br>• Use object pools if beneficial | Optimized allocation | Dev1 |
| **Fri** | • Final benchmarking<br>• Comprehensive performance matrix<br>• All platforms, all symbol rates | Final benchmarks | Team |

**Week 16 Success Criteria:**
- [ ] Throughput: 6.5-7.5 MSym/s on Raspberry Pi 4
- [ ] Cumulative speedup: 6.0-7.0x vs original baseline
- [ ] Latency: < 100ms from IQ to MPEG-TS
- [ ] CPU efficiency: > 75% time in productive work

**Deliverables (Weeks 15-16):**
- `docs/TUNING_GUIDE.md` - Performance tuning documentation
- `scripts/benchmark_suite.sh` - Comprehensive benchmarking scripts
- `docs/benchmarks/week16_final_tuning.txt` - Performance data

---

### Week 17-18: Validation, Testing, and Production Readiness

**Primary Goal:** Ensure production quality, validate all success criteria

#### Week 17 Tasks - Comprehensive Validation

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Cross-platform testing<br>• Test on RPi3, RPi4, RPi5<br>• Test on ARM32, ARM64<br>• Test on x86-64 (fallback) | Platform matrix | Dev2 |
| **Tue** | • Compiler testing<br>• Test with GCC 7, 8, 9, 10, 11<br>• Test with Clang 10, 11, 12<br>• Fix any warnings/errors | Compiler matrix | Dev2 |
| **Wed** | • Stress testing<br>• 24-hour continuous decode<br>• Test with 20+ DVB-S streams<br>• Memory leak check (valgrind) | Stress test report | Dev1/Dev2 |
| **Thu** | • Edge case testing<br>• Low SNR streams (3-5 dB)<br>• High symbol rates (5+ MSym/s)<br>• Fast Doppler shifts | Edge case results | Dev1 |
| **Fri** | • Regression testing<br>• Run full test suite<br>• Compare all metrics vs baseline<br>• Verify no functionality lost | Regression report | Team |

**Week 17 Success Criteria:**
- [ ] Zero crashes in 24-hour test
- [ ] Zero memory leaks (valgrind clean)
- [ ] Works on 8+ platform/compiler combinations
- [ ] Handles edge cases gracefully

#### Week 18 Tasks - Production Readiness and Launch

| Day | Task | Deliverable | Owner |
|-----|------|-------------|-------|
| **Mon** | • Code cleanup and polish<br>• Remove debug code and comments<br>• Ensure consistent style<br>• Final code review | Clean codebase | Team |
| **Tue** | • Documentation completion<br>• Update README with performance data<br>• Write user guide<br>• Document build/run instructions | Complete docs | Team |
| **Wed** | • Performance report<br>• Create comprehensive benchmark report<br>• Generate graphs and charts<br>• Document all optimizations | Performance report | Dev1 |
| **Thu** | • Packaging and release prep<br>• Create release notes<br>• Tag release version<br>• Build binaries for distribution | Release package | Dev2 |
| **Fri** | • **MILESTONE 3 FINAL REVIEW**<br>• Present to stakeholders<br>• Demo real-time performance<br>• Celebrate success! 🎉 | Project complete | Team |

**Week 18 Success Criteria:**
- [ ] All Phase 3 goals met
- [ ] Production-quality code
- [ ] Complete documentation
- [ ] Ready for public release

**Phase 3 Milestone 3 Deliverables:**
- [ ] Production-ready leandvb binary (6-8x faster)
- [ ] Comprehensive test suite (unit + integration + stress)
- [ ] Complete documentation suite
- [ ] Performance benchmark report with graphs
- [ ] Release notes and changelog
- [ ] Git tag: `v3.0-optimized`

---

## Dependencies and Critical Path

### Dependency Graph

```
Week 1-2: FIR NEON ────────────────────────────┐
                                                │
Week 3: FIR Sampler + AGC ──────────────────────┼─▶ Week 4: Phase 1 Complete
                                                │
Week 5-6: Queue Infrastructure ─────────────────┘

Week 7-8: 2-Thread Prototype ────────────────────┐
  ▲                                              │
  │                                              │
  └─ Depends on Week 5-6                         │
                                                 │
Week 9-10: 5-Thread Pipeline ────────────────────┼─▶ Week 10: Phase 2 Complete
  ▲                                              │
  │                                              │
  └─ Depends on Week 7-8                         │
                                                 │
Week 11-12: Viterbi NEON ────────────────────────┘
  ▲
  │
  └─ Can start in parallel with Week 9-10

Week 13-14: FFT/RS Optimization
  ▲
  │
  └─ Independent, can parallelize

Week 15-16: Tuning ──────────────────────────────┐
  ▲                                              │
  │                                              │
  └─ Depends on all previous optimizations       │
                                                 │
Week 17-18: Validation ──────────────────────────┼─▶ Week 18: Project Complete
  ▲                                              │
  │                                              │
  └─ Depends on Week 15-16                       │
```

### Critical Path (Longest Dependency Chain)

```
Week 1 → Week 2 → Week 5 → Week 6 → Week 7 → Week 8 → Week 9 → Week 10 → Week 15 → Week 16 → Week 17 → Week 18

Critical path duration: 12 weeks
Total project duration: 18 weeks
Parallelization efficiency: 67% (some weeks have parallel work streams)
```

### Parallel Work Opportunities

| Weeks | Primary Track | Parallel Track | Sync Point |
|-------|--------------|----------------|------------|
| 1-4 | NEON FIR (Dev1) | Testing/Validation (Dev2) | Week 4 Review |
| 5-6 | Queue Impl (Dev1) | Testing/Benchmarking (Dev2) | Week 6 Review |
| 11-14 | Viterbi NEON (Dev1) | FFT/RS Optimization (Dev2) | Week 14 Integration |
| 17-18 | Platform Testing (Dev2) | Documentation (Dev1) | Week 18 Release |

---

## Milestones and Deliverables

### Milestone 1: NEON Quick Wins Complete (End of Week 4)

**Date:** End of Week 4
**Review Meeting:** Friday Week 4, 2pm

**Success Criteria:**
- [ ] Overall speedup: 1.8-2.2x on Raspberry Pi 4
- [ ] FIR filter: 3.5-4.0x speedup
- [ ] FIR sampler: 3.5-4.0x speedup
- [ ] AGC: 3.0-4.0x speedup
- [ ] Bit-exact correctness vs baseline
- [ ] 24-hour stability test passed
- [ ] Works on ARM and x86 platforms

**Deliverables:**
- [ ] `dsp_neon.h` - NEON FIR filter and sampler
- [ ] `sdr_neon.h` - NEON AGC
- [ ] Test suite with >95% coverage
- [ ] Benchmark report with graphs
- [ ] Updated README and build docs
- [ ] Git tag: `v1.0-neon-phase1`

**Decision Gate:**
- **GREEN:** Speedup ≥ 1.8x, zero failures → Proceed to Phase 2
- **YELLOW:** Speedup 1.5-1.8x → Add more NEON optimizations before Phase 2
- **RED:** Speedup < 1.5x or correctness issues → Debug and fix before proceeding

---

### Milestone 2: Multithreading Complete (End of Week 10)

**Date:** End of Week 10
**Review Meeting:** Friday Week 10, 2pm

**Success Criteria:**
- [ ] Cumulative speedup: 4.5-5.5x on quad-core
- [ ] 5-thread pipeline working correctly
- [ ] Frequency tracking stable (BER < 1e-6)
- [ ] Load balanced across threads
- [ ] 24-hour stress test passed
- [ ] Lock-free queues validated

**Deliverables:**
- [ ] `leandvb_mt` - Multithreaded version
- [ ] `concurrent_queue.h` - Lock-free queue library
- [ ] `framework_mt.h` - Threading infrastructure
- [ ] Comprehensive test suite
- [ ] Architecture documentation
- [ ] Benchmark report (2-core, 4-core, 8-core)
- [ ] Git tag: `v2.0-multithreading`

**Decision Gate:**
- **GREEN:** Speedup ≥ 4.5x, stable tracking → Proceed to Phase 3
- **YELLOW:** Speedup 4.0-4.5x → Profile and optimize before Phase 3
- **RED:** Speedup < 4.0x or instability → Redesign thread architecture

---

### Milestone 3: Production Release (End of Week 18)

**Date:** End of Week 18
**Review Meeting:** Friday Week 18, 2pm (Final Presentation)

**Success Criteria:**
- [ ] Overall speedup: 6.0-8.0x on Raspberry Pi 4
- [ ] Real-time decode: 5+ MSym/s on RPi4
- [ ] All optimizations integrated and working
- [ ] Production-quality code (clean, documented)
- [ ] Zero known critical bugs
- [ ] Cross-platform validated (5+ platforms)
- [ ] Complete documentation suite

**Deliverables:**
- [ ] Production leandvb binary (optimized)
- [ ] Complete source code (all NEON + threading)
- [ ] Comprehensive test suite (unit + integration + stress)
- [ ] Performance benchmark report with graphs
- [ ] User guide and tuning documentation
- [ ] Developer documentation
- [ ] Release notes and changelog
- [ ] Git tag: `v3.0-optimized`

**Acceptance Criteria:**
- [ ] Stakeholder approval
- [ ] All success criteria met
- [ ] Ready for public release or deployment

---

## Testing Checkpoints

### Continuous Testing Strategy

**Daily:**
- [ ] Unit tests run on every commit
- [ ] Quick smoke test (5-minute DVB-S stream)
- [ ] Compiler warnings checked (zero tolerance)

**Weekly:**
- [ ] Full test suite execution
- [ ] 1-hour stability test
- [ ] Performance regression check
- [ ] Code review of week's changes

**Phase Gates (Weeks 4, 10, 18):**
- [ ] 24-hour stress test
- [ ] Cross-platform validation
- [ ] Memory leak check (valgrind)
- [ ] ThreadSanitizer check
- [ ] Performance benchmark suite
- [ ] Correctness validation (bit-exact comparison)

### Testing Pyramid

```
                    ┌─────────────────┐
                    │  E2E Tests      │  (Weekly: 24-hour stress test)
                    │  (Hours)        │
                    └─────────────────┘
                 ┌─────────────────────────┐
                 │  Integration Tests      │  (Daily: Real DVB-S streams)
                 │  (Minutes)              │
                 └─────────────────────────┘
            ┌──────────────────────────────────┐
            │  Component Tests                 │  (On commit: Isolated stages)
            │  (Seconds)                       │
            └──────────────────────────────────┘
       ┌──────────────────────────────────────────────┐
       │  Unit Tests                                  │  (On commit: Functions/classes)
       │  (Milliseconds)                              │
       └──────────────────────────────────────────────┘

Target: 80% unit, 15% component, 4% integration, 1% E2E
```

### Test Categories

| Category | Test Count | Frequency | Duration | Owner |
|----------|-----------|-----------|----------|-------|
| **Unit Tests** | 100+ | On every commit | < 5s | Developer |
| **Component Tests** | 30+ | On every commit | < 30s | Developer |
| **Integration Tests** | 10+ | Daily | < 5min | CI System |
| **Performance Benchmarks** | 5 | Weekly | < 10min | Developer |
| **Stress Tests** | 3 | Phase gates | 24 hours | QA |
| **Platform Tests** | 8 configs | Phase gates | 1 hour | QA |

### Automated Test Suite

```bash
# scripts/run_tests.sh

#!/bin/bash
set -e

echo "=== LeanSDR Test Suite ==="

# 1. Unit tests
echo "[1/6] Running unit tests..."
make test_unit
# Expected: >100 tests, 100% pass

# 2. NEON correctness tests
echo "[2/6] Running NEON correctness tests..."
./test_fir_neon
./test_viterbi_neon
# Expected: Max error < 1e-5 vs scalar

# 3. Queue correctness tests
echo "[3/6] Running queue tests..."
./test_queue
# Expected: 10M ops, zero data loss

# 4. Integration tests
echo "[4/6] Running integration tests..."
./test_integration.sh
# Expected: Bit-exact match vs baseline

# 5. Performance benchmarks
echo "[5/6] Running performance benchmarks..."
./benchmark_suite.sh
# Expected: Speedup meets targets

# 6. Memory checks
echo "[6/6] Running memory checks..."
valgrind --leak-check=full --error-exitcode=1 ./leandvb_mt < test.iq > /dev/null
# Expected: Zero leaks, zero errors

echo "✓ All tests passed!"
```

---

## Risk Mitigation Points

### High-Risk Items and Mitigation Strategies

| Risk | Probability | Impact | Mitigation | Contingency Plan |
|------|-------------|--------|------------|------------------|
| **NEON speedup lower than expected** | Medium | High | Profile early (Week 1), validate assumptions | Add more NEON targets (Constellation RX) |
| **Threading introduces deadlocks** | Low | Critical | Always use timeouts, extensive testing | Use simpler 4-thread architecture |
| **Feedback loop instability** | Medium | High | Test with various Doppler profiles | Merge Thread 2+3 (sacrifice parallelism) |
| **Cross-platform issues** | Medium | Medium | Test on multiple platforms weekly | Maintain single-threaded fallback |
| **Performance regression** | Low | High | Benchmark on every commit, CI checks | Revert changes, debug before proceeding |
| **Schedule slip (team unavailable)** | Medium | Medium | 2-week buffer built into schedule | Prioritize critical path, defer nice-to-haves |
| **Memory corruption bugs** | Low | Critical | Use ThreadSanitizer, valgrind weekly | Extensive debugging, add assertions |
| **Cache coherency issues on ARM** | Medium | Medium | Test on real ARM hardware (not emulator) | Use stronger memory ordering (seq_cst) |

### Risk Monitoring

**Weekly Risk Review (Fridays):**
- Review progress vs plan
- Identify new risks
- Update mitigation strategies
- Adjust schedule if needed

**Risk Escalation Criteria:**
- **Yellow:** 1+ week behind schedule → Reallocate resources
- **Red:** Critical bug blocking progress → All hands on deck

---

## Resource Requirements

### Team

**Primary Team:**
- **Dev1:** Senior developer (NEON, multithreading expertise)
- **Dev2:** Developer (testing, benchmarking)
- **Reviewer:** Code reviewer (can be external, part-time)

**Estimated Effort:**
- Dev1: 18 weeks × 40 hours = 720 hours
- Dev2: 18 weeks × 30 hours = 540 hours (part-time during some weeks)
- Reviewer: 10 hours (code reviews at milestones)
- **Total:** ~1270 hours (32 person-weeks)

### Hardware

**Development:**
- 1× Raspberry Pi 4 (4GB RAM, quad-core) - Primary development target
- 1× Raspberry Pi 3 (1GB RAM, quad-core) - Legacy testing
- 1× Raspberry Pi 5 (8GB RAM, quad-core) - Future platform
- 1× x86-64 desktop (for cross-platform testing, fallback validation)

**Testing:**
- 1× RTL-SDR or similar for live DVB-S testing
- Test DVB-S streams (10+ streams, various SNR, symbol rates)

### Software

**Development Tools:**
- GCC 7.5+ or Clang 10+ (ARM cross-compiler if developing on x86)
- Git for version control
- Make or CMake for build system
- Valgrind (memory debugging)
- ThreadSanitizer (race condition detection)
- Perf or gprof (profiling)

**Libraries:**
- ARM NE10 library (optional, for optimized FFT)
- Existing LeanSDR dependencies

### Infrastructure

**Continuous Integration:**
- CI server (GitHub Actions, GitLab CI, or Jenkins)
- Automated test runs on every commit
- Performance regression tracking
- Multi-platform builds (ARM32, ARM64, x86-64)

**Version Control:**
- Git repository with feature branches
- Branch strategy:
  - `main` - stable releases
  - `develop` - integration branch
  - `feature/neon-fir` - feature branches
  - `feature/multithreading` - feature branches

---

## Success Criteria

### Phase 1 Success Criteria (Week 4)

**Performance:**
- [ ] FIR filter speedup: 3.5-4.5x
- [ ] FIR sampler speedup: 3.5-4.5x
- [ ] AGC speedup: 3.0-4.0x
- [ ] **Overall speedup: 1.8-2.2x** ⭐

**Correctness:**
- [ ] Bit-exact match vs baseline (tolerance: 1e-5)
- [ ] BER performance maintained (±5%)
- [ ] Zero data corruption in 24-hour test

**Quality:**
- [ ] Code review approved
- [ ] Test coverage >90%
- [ ] Documentation complete

**Portability:**
- [ ] Compiles on ARM32, ARM64, x86-64
- [ ] Works with GCC and Clang
- [ ] Fallback path tested on x86

---

### Phase 2 Success Criteria (Week 10)

**Performance:**
- [ ] Threading speedup: 2.5-3.0x (vs single-threaded)
- [ ] **Cumulative speedup: 4.5-5.5x** ⭐
- [ ] Throughput on RPi4: 5.5-6.5 MSym/s

**Correctness:**
- [ ] Bit-exact match vs baseline
- [ ] Frequency tracking stable (BER < 1e-6)
- [ ] Zero deadlocks in 24-hour test

**Load Balance:**
- [ ] All threads: 60-85% CPU utilization
- [ ] No thread idle >20% of time
- [ ] Queue depths: 20-60% average fill

**Quality:**
- [ ] Zero memory leaks (valgrind clean)
- [ ] Zero race conditions (ThreadSanitizer clean)
- [ ] Graceful shutdown (no hung threads)

---

### Phase 3 Success Criteria (Week 18) - FINAL

**Performance:**
- [ ] Viterbi speedup: 2.0-2.5x
- [ ] FFT speedup: 2-3x
- [ ] Reed-Solomon speedup: 1.5-2.0x
- [ ] **Overall speedup: 6.0-8.0x** ⭐⭐⭐
- [ ] **Throughput on RPi4: 7-9 MSym/s** ⭐⭐⭐

**Correctness:**
- [ ] Bit-exact match vs baseline
- [ ] BER performance maintained across SNR range (0-15 dB)
- [ ] Handles edge cases: Low SNR, high symbol rate, Doppler

**Quality:**
- [ ] Production-quality code (clean, documented)
- [ ] Zero critical bugs
- [ ] Complete test suite (>95% coverage)
- [ ] Complete documentation

**Portability:**
- [ ] Validated on 5+ platform/compiler combinations
- [ ] Works on RPi3, RPi4, RPi5
- [ ] Fallback works on x86

**Stability:**
- [ ] 24-hour continuous decode without crashes
- [ ] Memory usage stable (no leaks)
- [ ] CPU usage stable (no runaway threads)

---

## Performance Tracking Dashboard

### Weekly Performance Metrics

```
┌─────────────────────────────────────────────────────────────────────────┐
│                  LeanSDR Optimization Progress Dashboard                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Baseline (Week 0):  1.2 MSym/s (100%)                                  │
│  Target (Week 18):   7.2 MSym/s (600%) ◄── 6x speedup goal              │
│                                                                          │
│  Progress:                                                               │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ Week 4:  [████████████░░░░░░░░░░░░░░░░░░░░░░] 2.4 MSym/s (200%)   │ │
│  │ Week 10: [████████████████████████████░░░░░░░░] 5.8 MSym/s (483%)   │ │
│  │ Week 18: [████████████████████████████████████] 7.8 MSym/s (650%) ✓│ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  Component Speedups:                                                     │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ FIR Filter:      [████████] 4.0x                                    │ │
│  │ FIR Sampler:     [████████] 4.0x                                    │ │
│  │ AGC:             [███████░] 3.5x                                    │ │
│  │ Viterbi:         [█████░░░] 2.5x                                    │ │
│  │ Threading:       [███████░] 3.0x                                    │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  Key Metrics:                                                            │
│  • Speedup: 6.5x  (Target: 6.0-8.0x) ✓                                  │
│  • Throughput: 7.8 MSym/s  (Target: 7+ MSym/s) ✓                        │
│  • Latency: 85ms  (Target: <100ms) ✓                                    │
│  • CPU Usage: 380% on quad-core  (Efficient use of 4 cores) ✓           │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Performance Tracking Table

| Week | Milestone | Throughput (MSym/s) | Speedup | Status |
|------|-----------|---------------------|---------|--------|
| 0 | Baseline | 1.2 | 1.0x | Baseline |
| 4 | Phase 1 Complete | 2.4 | 2.0x | 🎯 Target: 1.8-2.2x |
| 10 | Phase 2 Complete | 5.8 | 4.8x | 🎯 Target: 4.5-5.5x |
| 18 | Phase 3 Complete | 7.8 | 6.5x | 🎯 Target: 6.0-8.0x |

---

## Decision Points and Contingency Plans

### Week 4 Decision Point: Phase 1 Review

**Decision Criteria:**

1. **IF speedup ≥ 2.0x:**
   - ✅ **GO:** Proceed to Phase 2 (multithreading)
   - Document lessons learned
   - Celebrate Phase 1 success

2. **IF speedup 1.5-2.0x:**
   - ⚠️ **ADJUST:** Add more NEON optimizations before Phase 2
   - Optimize Constellation RX (15-20% CPU)
   - Optimize MPEG sync (2-3% CPU)
   - Reassess after additional optimizations

3. **IF speedup < 1.5x:**
   - 🛑 **STOP:** Deep dive analysis required
   - Profile in detail (perf, gprof)
   - Check compiler optimization flags
   - Verify NEON code is actually being used (objdump)
   - Consider alternative optimization strategies

---

### Week 10 Decision Point: Phase 2 Review

**Decision Criteria:**

1. **IF cumulative speedup ≥ 4.5x AND stable tracking:**
   - ✅ **GO:** Proceed to Phase 3 (advanced optimizations)
   - Focus on polish and production readiness

2. **IF cumulative speedup 4.0-4.5x OR minor tracking issues:**
   - ⚠️ **ADJUST:** Tune before Phase 3
   - Profile thread load distribution
   - Optimize queue sizes
   - Fix tracking issues (tune feedback loop)
   - Spend 1 extra week on tuning

3. **IF cumulative speedup < 4.0x OR major tracking instability:**
   - 🛑 **REASSESS:** Major redesign needed
   - Consider 4-thread architecture (merge Thread 2+3)
   - Consider reducing queue overhead
   - Validate lock-free queue performance
   - May extend Phase 2 by 2 weeks

---

### Week 18 Decision Point: Production Release

**Decision Criteria:**

1. **IF all success criteria met:**
   - ✅ **RELEASE:** Tag v3.0, prepare release notes
   - Publish performance benchmarks
   - Deploy to production or release publicly

2. **IF minor issues remain:**
   - ⚠️ **SOFT LAUNCH:** Beta release (v3.0-beta)
   - Address remaining issues
   - Full release in 1-2 weeks

3. **IF major issues found:**
   - 🛑 **DEFER RELEASE:** Continue development
   - Fix critical bugs
   - Re-validate
   - Reassess release timeline

---

## Appendix

### A. Build and Test Commands

```bash
# Build with NEON optimizations
make clean
CXXFLAGS="-march=armv8-a+simd -O3 -ftree-vectorize -ffast-math" make leandvb

# Build multithreaded version
make leandvb_mt

# Run unit tests
make test

# Run integration tests
./test/integration/test_all.sh

# Run performance benchmark
./scripts/benchmark.sh > results.txt

# Check for memory leaks
valgrind --leak-check=full ./leandvb_mt < test.iq > /dev/null

# Check for race conditions
./leandvb_mt_tsan < test.iq > /dev/null  # Built with -fsanitize=thread

# Profile with perf
perf record -g ./leandvb_mt < input.iq > /dev/null
perf report
```

### B. Benchmarking Scripts

```bash
#!/bin/bash
# scripts/benchmark.sh

# Comprehensive benchmarking for all platforms

echo "=== LeanSDR Performance Benchmark ==="
echo "Platform: $(uname -m)"
echo "Cores: $(nproc)"
echo "Date: $(date)"
echo ""

# Test file
INPUT="test_data/dvb-s_2msps.iq"

# Baseline (single-threaded, no NEON)
echo "[1/4] Baseline (scalar, single-threaded)..."
time ./leandvb_baseline < $INPUT > /dev/null

# Phase 1 (NEON only)
echo "[2/4] Phase 1 (NEON, single-threaded)..."
time ./leandvb_neon < $INPUT > /dev/null

# Phase 2 (NEON + multithreading)
echo "[3/4] Phase 2 (NEON + 5 threads)..."
time ./leandvb_mt < $INPUT > /dev/null

# Phase 3 (all optimizations)
echo "[4/4] Phase 3 (all optimizations)..."
time ./leandvb_final < $INPUT > /dev/null

echo ""
echo "✓ Benchmark complete!"
```

### C. Continuous Integration Configuration

```yaml
# .github/workflows/ci.yml

name: LeanSDR CI

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        arch: [armv7, armv8, x86_64]
        compiler: [gcc-10, gcc-11, clang-12]

    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y ${{ matrix.compiler }}

      - name: Build
        run: |
          make clean
          CXX=${{ matrix.compiler }} make

      - name: Run tests
        run: make test

      - name: Run integration tests
        run: ./test/integration/test_all.sh

      - name: Check for memory leaks
        run: |
          sudo apt-get install -y valgrind
          valgrind --leak-check=full --error-exitcode=1 ./leandvb_mt < test/data/test.iq

  benchmark:
    runs-on: self-hosted  # ARM runner
    steps:
      - uses: actions/checkout@v2

      - name: Build optimized
        run: make clean && make CXXFLAGS="-march=native -O3" leandvb_mt

      - name: Run benchmark
        run: ./scripts/benchmark.sh

      - name: Check performance regression
        run: |
          # Compare vs baseline, fail if >5% slower
          ./scripts/check_regression.sh
```

### D. Performance Visualization

```python
# scripts/plot_performance.py

import matplotlib.pyplot as plt

weeks = [0, 4, 10, 18]
throughput = [1.2, 2.4, 5.8, 7.8]
target = [1.2, 2.0, 5.0, 7.0]

plt.figure(figsize=(10, 6))
plt.plot(weeks, throughput, 'o-', label='Actual', linewidth=2, markersize=10)
plt.plot(weeks, target, 's--', label='Target', linewidth=2, markersize=8)
plt.xlabel('Week', fontsize=14)
plt.ylabel('Throughput (MSym/s)', fontsize=14)
plt.title('LeanSDR Optimization Progress', fontsize=16)
plt.grid(True, alpha=0.3)
plt.legend(fontsize=12)
plt.tight_layout()
plt.savefig('optimization_progress.png', dpi=150)
print("✓ Plot saved to optimization_progress.png")
```

---

## Summary

This 18-week implementation roadmap provides a structured, phased approach to optimizing LeanSDR:

1. **Phase 1 (Weeks 1-4):** Quick wins with NEON SIMD → 2x speedup
2. **Phase 2 (Weeks 5-10):** Multithreading pipeline → 5x cumulative speedup
3. **Phase 3 (Weeks 11-18):** Polish and production readiness → 6-8x total speedup

**Key Success Factors:**
- Incremental approach with clear milestones
- Continuous testing and validation
- Risk mitigation at every phase
- Clear decision points and contingency plans
- Comprehensive performance tracking

**Expected Outcome:**
- **6-8x performance improvement** on Raspberry Pi 4
- Real-time decode of **5-9 MSym/s** DVB-S streams
- Production-ready, portable codebase
- Complete test and benchmark suite

**Next Steps:**
1. Review this roadmap with stakeholders
2. Set up development environment (Week 0)
3. Begin Week 1: FIR Filter NEON optimization
4. Track progress weekly against this plan

---

**Document Version:** 1.0
**Last Updated:** 2025-11-17
**Author:** LeanSDR Optimization Team
**Status:** 🎯 Ready for Implementation

**For detailed technical plans, see:**
- [NEON-OPTIMIZATION-PLAN.md](NEON-OPTIMIZATION-PLAN.md)
- [MULTITHREADING-PLAN.md](MULTITHREADING-PLAN.md)
