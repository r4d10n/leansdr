# LeanSDR System Architecture Overview

**Document:** System-Level Architecture
**Last Updated:** 2025-11-17
**Complexity:** High-Level

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [System Architecture Diagram](#system-architecture-diagram)
3. [Component Hierarchy](#component-hierarchy)
4. [Data Flow Model](#data-flow-model)
5. [Processing Stages](#processing-stages)
6. [Memory Architecture](#memory-architecture)
7. [Thread Model](#thread-model)
8. [Performance Characteristics](#performance-characteristics)

---

## Executive Summary

**LeanSDR** is a lightweight, portable software-defined radio framework designed for **DVB-S/DVB-S2 satellite television reception**. The architecture prioritizes:

- **Simplicity:** Header-only C++ library with minimal dependencies
- **Portability:** Runs on x86, ARM, and embedded systems
- **Speed:** Optimized for real-time demodulation of QPSK signals
- **Modularity:** Composable DSP components in a dataflow framework

### Key Statistics
| Metric | Value |
|--------|-------|
| **Total LOC** | ~10,000 lines |
| **Header Files** | 13 core libraries |
| **Applications** | 10 utilities |
| **Processing Stages** | 20 (standard mode) / 9 (high-speed) |
| **Buffer Memory** | ~200 KB typical |
| **Thread Model** | Single-threaded cooperative |

---

## System Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           LEANSDR ARCHITECTURE                                │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                         APPLICATION LAYER                                │ │
│  │                                                                          │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │ │
│  │  │ leandvb  │  │leandvbtx │  │ leanmlm  │  │ leanscan │  │  lean... │ │ │
│  │  │  (RX)    │  │  (TX)    │  │   (RX)   │  │(scanner) │  │ (utils)  │ │ │
│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘ │ │
│  └───────┼─────────────┼─────────────┼─────────────┼─────────────┼────────┘ │
│          │             │             │             │             │           │
│  ┌───────┴─────────────┴─────────────┴─────────────┴─────────────┴────────┐ │
│  │                      COMPONENT LIBRARY (src/leansdr/*.h)                │ │
│  │                                                                          │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐│ │
│  │  │ DVB Layer    │  │  SDR Layer   │  │  DSP Layer   │  │ FEC Layer   ││ │
│  │  │──────────────│  │──────────────│  │──────────────│  │─────────────││ │
│  │  │• dvb.h       │  │• sdr.h       │  │• dsp.h       │  │• viterbi.h  ││ │
│  │  │• iess.h      │  │• math.h      │  │• filtergen.h │  │• convolve.h ││ │
│  │  │• hdlc.h      │  │              │  │              │  │• rs.h       ││ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘│ │
│  │                                                                          │ │
│  │  ┌───────────────────────────────────────────────────────────────────┐ │ │
│  │  │                  FRAMEWORK LAYER (framework.h)                     │ │ │
│  │  │                                                                    │ │ │
│  │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │ │ │
│  │  │  │  Scheduler   │  │   pipebuf<T> │  │  runnable    │           │ │ │
│  │  │  │──────────────│  │──────────────│  │──────────────│           │ │ │
│  │  │  │• Fixpoint    │  │• Multi-reader│  │• Base class  │           │ │ │
│  │  │  │  execution   │  │• FIFO queue  │  │• run() hook  │           │ │ │
│  │  │  │• Hash-based  │  │• Compaction  │  │• Cooperative │           │ │ │
│  │  │  └──────────────┘  └──────────────┘  └──────────────┘           │ │ │
│  │  └───────────────────────────────────────────────────────────────────┘ │ │
│  │                                                                          │ │
│  │  ┌───────────────────────────────────────────────────────────────────┐ │ │
│  │  │                     I/O LAYER (generic.h, gui.h)                  │ │ │
│  │  │  • file_reader/writer  • buffer_reader/writer  • X11 scopes      │ │ │
│  │  └───────────────────────────────────────────────────────────────────┘ │ │
│  └──────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                         RUNTIME SYSTEM                                   │ │
│  │  ┌────────────────┐  ┌────────────────┐  ┌─────────────┐               │ │
│  │  │ Operating System│  │  C++ Runtime   │  │   libm      │               │ │
│  │  │  (Linux/Unix)   │  │   (stdlib)     │  │ (math lib)  │               │ │
│  │  └────────────────┘  └────────────────┘  └─────────────┘               │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Hierarchy

### Layer 1: Framework (foundation.h)
**Purpose:** Provides the execution engine and data flow primitives

```
framework.h (290 lines)
├── scheduler
│   ├── run() - Main execution loop
│   ├── step() - Execute all runnables once
│   └── hash() - Detect fixpoint convergence
│
├── pipebuf<T>
│   ├── Multi-reader FIFO
│   ├── Single-writer interface
│   └── Automatic compaction
│
├── runnable (base class)
│   └── run() - Override for processing
│
└── pipewriter<T> / pipereader<T>
    └── Type-safe buffer access
```

### Layer 2: DSP Primitives (dsp.h, math.h, filtergen.h)
**Purpose:** Core signal processing algorithms

```
dsp.h (2,500+ lines)
├── Filters
│   ├── fir_filter<T> - Finite impulse response (BOTTLENECK: 30-40% CPU)
│   ├── iir_filter<T> - Infinite impulse response
│   └── decimator<T> - Downsample with filtering
│
├── FFT
│   ├── cfft_engine<T> - Complex FFT (BOTTLENECK: 3-5% CPU)
│   └── rfft_engine<T> - Real FFT
│
├── Resamplers
│   ├── sampler<T> - Arbitrary rate conversion
│   └── fir_resampler<T> - FIR-based resampling (MAJOR BOTTLENECK)
│
└── AGC
    ├── auto_gain_control<T> - Amplitude stabilization
    └── rms_agc<T> - RMS-based AGC

math.h (116 lines)
├── complex<T> - Complex number arithmetic
├── trig16 - Trigonometric LUTs (256 KB)
└── Constellation LUTs - Symbol mapping (390 KB)

filtergen.h (119 lines)
├── lowpass() - Sinc filter generation
├── root_raised_cosine() - Pulse shaping
├── normalize_dcgain() - DC normalization
└── normalize_power() - Power normalization
```

### Layer 3: SDR Components (sdr.h)
**Purpose:** Radio-specific signal processing

```
sdr.h (3,000+ lines)
├── Demodulators
│   ├── cstln_receiver<T> - Constellation demodulator (BOTTLENECK: 15-20% CPU)
│   ├── psk_receiver<T> - PSK demodulation
│   └── qam_receiver<T> - QAM demodulation
│
├── Synchronization
│   ├── clock_recovery<T> - Symbol timing
│   ├── cstln_lut<256> - Constellation lookup
│   └── fir_sampler<T> - Symbol interpolation
│
├── Carrier Recovery
│   ├── pll<T> - Phase-locked loop
│   ├── costas_loop<T> - Carrier tracking
│   └── freq_control<T> - Frequency offset correction
│
└── Utilities
    ├── auto_notch<T> - Interference rejection
    ├── cnr_estimator<T> - Carrier-to-noise ratio
    └── spectrum_analyzer<T> - FFT-based spectral display
```

### Layer 4: DVB Implementation (dvb.h, convolutional.h, viterbi.h, rs.h)
**Purpose:** DVB-S/DVB-S2 specific processing

```
dvb.h (2,000+ lines)
├── Framing
│   ├── dvb_framelock<T> - Frame synchronization
│   ├── mpeg_sync<T> - MPEG-TS sync (0x47 detection)
│   └── plslot<T> - Physical layer slot processing
│
├── Deinterleaving
│   ├── deinterleaver<T> - Forney interleaving (I=12)
│   └── byte_deinterleaver<T> - Byte-level deinterleaving
│
└── Descrambling
    ├── dvb_derand<T> - Randomization removal
    └── prbs_generator - Pseudo-random sequence

viterbi.h (350 lines)
├── viterbi_dec<> - Generic Viterbi decoder (BOTTLENECK: 20-25% CPU)
│   ├── trellis - State transition graph
│   ├── update() - ACS (Add-Compare-Select) operations
│   └── traceback() - Path reconstruction
│
└── dvb_deconvol_sync<T> - Multi-sync Viterbi wrapper

convolutional.h (275 lines)
├── convol_poly2<> - QPSK 1/2 encoder
├── convol_multipoly<> - Multi-rate encoder
├── deconvol_poly<> - Reference decoder
└── deconvol_sync<> - Production decoder

rs.h (276 lines)
├── gf_element<GF,POLYNOMIAL> - Galois field arithmetic (BOTTLENECK: 5-10% CPU)
├── rs_encoder<> - Reed-Solomon encoder
└── rs_decoder<> - Reed-Solomon decoder
    ├── syndromes() - Error detection
    ├── berlekamp_massey() - Error locator polynomial
    └── forney() - Error magnitude computation
```

### Layer 5: I/O and GUI (generic.h, gui.h, hdlc.h, iess.h)
**Purpose:** Input/output and visualization

```
generic.h (380 lines)
├── file_reader<T> - Read from file descriptor
├── file_writer<T> - Write to file descriptor
├── decimator<T> - Sample decimation
└── serializer<T> - Type conversion

gui.h (609 lines) [Optional - #ifdef GUI]
├── cscope<T> - Constellation diagram
├── wavescope<T> - Time-domain waveform
├── spectrumscope<T> - Frequency-domain spectrum
├── slowmultiscope<T> - Multi-channel oscilloscope
└── rfscope<T> - RF spectrum analyzer

hdlc.h (311 lines)
├── hdlc_dec - HDLC frame decoder
└── hdlc_sync - Synchronization and polarity detection

iess.h (76 lines)
└── etr192_descrambler - IESS descrambling (ETSI TR 192)
```

---

## Data Flow Model

### Execution Model: Cooperative Fixpoint Iteration

```
                     ┌───────────────────────┐
                     │    Main Program       │
                     │  (leandvb.cc, etc.)   │
                     └───────────┬───────────┘
                                 │
                                 │ Constructs pipeline
                                 ▼
                     ┌───────────────────────┐
                     │     scheduler         │
                     │───────────────────────│
                     │ runnables[64]         │
                     │ pipes[64]             │
                     └───────────┬───────────┘
                                 │
                                 │ scheduler.run()
                                 ▼
        ┌────────────────────────────────────────────┐
        │        FIXPOINT EXECUTION LOOP             │
        │                                            │
        │  while(true) {                             │
        │    prev_hash = hash()                      │
        │                                            │
        │    for each runnable:                      │
        │      runnable->run()  ◄─┐                  │
        │                         │                  │
        │    new_hash = hash()    │                  │
        │                         │                  │
        │    if (new_hash == prev_hash)              │
        │      break  // Fixpoint reached            │
        │  }                      │                  │
        └─────────────────────────┼──────────────────┘
                                  │
                   Each runnable processes data
                   from input pipes to output pipes
                                  │
                                  ▼
        ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
        │ pipebuf<T>   │───▶│  runnable A  │───▶│ pipebuf<T>   │
        │  (input)     │    │──────────────│    │  (output)    │
        │              │    │• Read N      │    │              │
        │wr ──────┐   │    │• Process     │    │   ┌────── wr │
        │rd[0] ────┼───┼───▶│• Write M     │────┼───┤         │
        │rd[1] ────┘   │    │              │    │   └────── rd[0]
        └──────────────┘    └──────────────┘    └──────────────┘
                                  │
                   Multiple readers can consume
                   from same buffer (fan-out)
                                  │
                                  ▼
                        ┌─────────┴─────────┐
                        │                   │
                   ┌──────────┐       ┌──────────┐
                   │runnable B│       │runnable C│
                   └──────────┘       └──────────┘
```

### Data Flow Characteristics

| Characteristic | Description |
|----------------|-------------|
| **Model** | Synchronous dataflow (SDF) |
| **Execution** | Cooperative (no preemption) |
| **Blocking** | Implicit (runnables yield when no data) |
| **Buffering** | Multi-reader FIFO with compaction |
| **Synchronization** | Single-threaded (no locks needed) |
| **Termination** | Fixpoint detection via hash |

---

## Processing Stages

### Complete leandvb Processing Pipeline (Standard Mode)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        LEANDVB SIGNAL PROCESSING CHAIN                       │
└─────────────────────────────────────────────────────────────────────────────┘

STDIN (raw IQ samples)
  │
  ├─▶ [1] file_reader<cf32>                      Read complex float samples
  │        └─▶ Format conversion (u8/s16 → cf32)
  │
  ├─▶ [2] Optional: adder<cf32>                  Add noise (testing)
  │        └─▶ White Gaussian noise injection
  │
  ├─▶ [3] Optional: auto_notch<f32>              ⚡ NEON CANDIDATE
  │        └─▶ Adaptive notch filter (remove interference)
  │              Complexity: O(N) per sample
  │              CPU: 2-3%
  │
  ├─▶ [4] rotator<f32>                            Frequency derotation
  │        └─▶ exp(j*2π*offset*t) multiplication
  │              Complexity: O(N)
  │              CPU: 1-2%
  │
  ├─▶ [5] cnr_fft<f32>                           CNR estimation
  │        └─▶ FFT-based carrier-to-noise ratio
  │              Requires Fs > 3*Fm
  │              CPU: 1-2%
  │
  ├─▶ [6] spectrum_analyzer<f32>                 ⚡ NEON CANDIDATE (FFT)
  │        └─▶ 1024-bin FFT for visualization
  │              Complexity: O(N log N)
  │              CPU: 1-2%
  │
  ├─▶ [7] fir_filter<cf32, float>               ⚡⚡⚡ PRIMARY BOTTLENECK
  │        └─▶ Anti-aliasing lowpass + decimation
  │              Order: 100-200 taps
  │              Complexity: O(N × M) where M = taps
  │              CPU: 30-40%
  │              NEON potential: 4x speedup
  │
  ├─▶ [8] Optional: cscope<f32>                  Constellation scope (GUI)
  │        └─▶ X11 visualization
  │
  ├─▶ [9] cstln_receiver<f32>                    ⚡⚡ MAJOR BOTTLENECK
  │        └─▶ Symbol synchronization + demodulation
  │              • Timing recovery (Mueller-Müller)
  │              • Carrier recovery (Costas PLL)
  │              • Soft-decision generation
  │              • FIR symbol interpolation
  │              Complexity: O(N × K) where K = samples/symbol
  │              CPU: 15-20%
  │              NEON potential: 2x speedup
  │              Outputs: symbols, frequency offset, MER
  │
  ├─▶ [10] Feedback loops
  │        ├─▶ freq_tap → resampler (Doppler compensation)
  │        └─▶ freq_tap → CNR estimator
  │
  ├─▶ [11] viterbi_sync<T> OR deconvol_sync<T>  ⚡⚡ MAJOR BOTTLENECK
  │        └─▶ Convolutional decoding
  │              [A] viterbi_sync: Optimal but slow
  │                  • 64-state trellis
  │                  • ACS operations: 256 per symbol
  │                  Complexity: O(64 × 256) = 16,384 ops/symbol
  │                  CPU: 20-25%
  │                  NEON potential: 2-3x speedup
  │              [B] deconvol_sync: Faster but suboptimal
  │                  • Algebraic decoder
  │                  • Multi-sync (4 rotations × 2 polarities)
  │                  Complexity: O(puncture_period)
  │                  CPU: 8-12%
  │                  NEON potential: 2x speedup
  │
  ├─▶ [12] mpeg_sync<u8>                         MPEG-TS synchronization
  │        └─▶ Find 0x47 sync byte
  │              Scan up to 1632 bytes
  │              Complexity: O(13,056) comparisons worst-case
  │              CPU: 2-3%
  │              NEON potential: 4-8x speedup (SIMD search)
  │
  ├─▶ [13] deinterleaver<u8>                     Byte deinterleaving
  │        └─▶ Forney interleaving (I=12)
  │              Complexity: O(N) with scattered access
  │              CPU: 3-5%
  │              NEON potential: 1.5x speedup
  │
  ├─▶ [14] rs_decoder<u8, 0x11D>                ⚡ OPTIMIZATION TARGET
  │        └─▶ Reed-Solomon RS(204, 188, 8)
  │              • Syndrome calculation
  │              • Berlekamp-Massey algorithm
  │              • Forney error magnitude
  │              • Chien search
  │              Complexity: O(3,200-6,000) GF operations
  │              CPU: 5-10%
  │              NEON potential: 2x speedup
  │
  ├─▶ [15] dvb_derand<u8>                        Derandomization
  │        └─▶ XOR with PRBS
  │              Complexity: O(N)
  │              CPU: <1%
  │              NEON potential: 4x speedup (batch XOR)
  │
  └─▶ [16] file_writer<u8>                       Output clean MPEG-TS
           └─▶ Write to STDOUT

STDOUT (MPEG transport stream)
```

### Performance Summary by Stage

| Stage | Component | CPU % | Complexity | NEON Speedup | Priority |
|-------|-----------|-------|------------|--------------|----------|
| 7 | FIR Resampler | 30-40% | O(N×M) | **4x** | **CRITICAL** |
| 11 | Viterbi Decoder | 20-25% | O(64×256) | **2-3x** | **CRITICAL** |
| 9 | Constellation RX | 15-20% | O(N×K) | **2x** | **HIGH** |
| 14 | Reed-Solomon | 5-10% | O(6000) | **2x** | **MEDIUM** |
| 3 | Auto-Notch | 2-3% | O(N) | **2x** | **LOW** |
| 6 | FFT/Spectrum | 1-2% | O(N log N) | **2-3x** | **LOW** |
| 12 | MPEG Sync | 2-3% | O(13K) | **4-8x** | **MEDIUM** |

**Total Optimization Potential:** 2-4x with comprehensive NEON implementation

---

## Memory Architecture

### Buffer Organization

```
┌────────────────────────────────────────────────────────────────┐
│                    MEMORY LAYOUT (typical)                      │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  SCHEDULER OVERHEAD: ~5 KB                                     │
│  ├─ runnables[64] × 80 bytes = 5.1 KB                         │
│  └─ pipes[64] × 80 bytes = 5.1 KB                              │
│                                                                 │
│  PIPELINE BUFFERS: ~200 KB (configurable via --buf-factor)    │
│  ├─ BUF_BASEBAND: 16,384 × cf32 = 128 KB                      │
│  ├─ BUF_SYMBOLS: 4,096 × softsymbol = 16 KB                   │
│  ├─ BUF_BYTES: 8,192 × u8 = 8 KB                              │
│  ├─ BUF_MPEGBYTES: 9,792 × u8 = 10 KB                         │
│  ├─ BUF_PACKETS: 4 × 204 bytes = 816 bytes                    │
│  └─ Misc buffers: ~40 KB                                       │
│                                                                 │
│  LOOKUP TABLES: ~1.5 MB                                        │
│  ├─ trig16 (sin/cos): 65,536 × cf32 = 512 KB                  │
│  ├─ Constellation LUTs: ~390 KB                                │
│  ├─ Reed-Solomon GF tables: 768 bytes                         │
│  ├─ Viterbi trellis: ~16 KB                                   │
│  └─ Other LUTs: ~600 KB                                        │
│                                                                 │
│  WORKING SET (stack/heap): ~500 KB                             │
│  ├─ FFT scratch buffers: ~100 KB                              │
│  ├─ Filter coefficient storage: ~50 KB                        │
│  └─ Component state: ~350 KB                                   │
│                                                                 │
│  TOTAL MEMORY FOOTPRINT: ~2.2 MB                               │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

### Buffer Sizing Strategy

```cpp
// From leandvb.cc - Buffer size calculations

// BUF_BASEBAND: Enough for ANF (4096) + cstln_receiver (128+1)
#define BUF_BASEBAND (4096 * cfg.buf_factor)

// BUF_SYMBOLS: Symbol buffer for deconvolution + sync
#define BUF_SYMBOLS (1024 * cfg.buf_factor)

// BUF_BYTES: Enough for MPEG sync (1632) + deinterleaver (204×12)
#define BUF_BYTES (4096 * cfg.buf_factor)

// BUF_MPEGBYTES: Deinterleaver frame (2448) + margin
#define BUF_MPEGBYTES ((204*8+2) * cfg.buf_factor)

// BUF_PACKETS: Small packet buffer
#define BUF_PACKETS 4
```

**Key Insight:** Buffer sizes are carefully chosen to accommodate the most demanding component in each stage. Increasing `buf_factor` reduces compaction overhead but increases memory usage.

---

## Thread Model

### Current: Single-Threaded Cooperative Scheduler

```
┌─────────────────────────────────────────────────────────────┐
│              SINGLE-THREADED EXECUTION                       │
│                                                              │
│  Main Thread                                                │
│  ├─▶ scheduler.run()                                        │
│  │    └─▶ while (progress):                                │
│  │         ├─▶ runnable[0].run()  ◄─ Process data, yield   │
│  │         ├─▶ runnable[1].run()  ◄─ Process data, yield   │
│  │         ├─▶ runnable[2].run()  ◄─ Process data, yield   │
│  │         │   ...                                          │
│  │         ├─▶ runnable[N].run()  ◄─ Process data, yield   │
│  │         └─▶ check_fixpoint()   ◄─ Hash comparison       │
│  │              └─▶ if no progress: exit                    │
│  └─▶ exit                                                   │
│                                                              │
│  Advantages:                                                │
│  ✓ Simple, deterministic                                   │
│  ✓ No synchronization overhead                             │
│  ✓ Cache-friendly sequential access                        │
│  ✓ Easy to debug                                           │
│                                                              │
│  Disadvantages:                                             │
│  ✗ Single-core utilization only                            │
│  ✗ Slow stage blocks entire pipeline                       │
│  ✗ Cannot overlap I/O and compute                          │
│  ✗ No load balancing                                       │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Proposed: Multi-Threaded Pipeline (See MULTITHREADING-PLAN.md)

```
┌─────────────────────────────────────────────────────────────┐
│            MULTI-THREADED PIPELINE (PROPOSED)                │
│                                                              │
│  Thread 1 [I/O]        ┌──────────────┐                     │
│  ├─▶ Input reader      │ Lock-free    │                     │
│  └─▶ Format conversion │ Queue 1      │                     │
│                        └──────┬───────┘                     │
│  Thread 2 [CPU-Heavy]          │                            │
│  ├─▶ Preprocessing    ┌────────▼──────┐                     │
│  ├─▶ ANF              │ Lock-free     │                     │
│  └─▶ FIR Resampler    │ Queue 2       │                     │
│       (BOTTLENECK)    └────────┬──────┘                     │
│                                 │                            │
│  Thread 3 [CPU-Heavy]           │                            │
│  ├─▶ Constellation RX  ┌────────▼──────┐                     │
│  ├─▶ Timing recovery   │ Lock-free     │                     │
│  └─▶ Carrier recovery  │ Queue 3       │                     │
│       (BOTTLENECK)     └────────┬──────┘                     │
│                                  │                            │
│  Thread 4 [CPU-Heavy]            │                            │
│  ├─▶ Viterbi decoder   ┌─────────▼─────┐                     │
│  ├─▶ MPEG sync         │ Lock-free     │                     │
│  ├─▶ Deinterleaver     │ Queue 4       │                     │
│  ├─▶ Reed-Solomon      └─────────┬─────┘                     │
│  └─▶ Derandomization             │                            │
│       (BOTTLENECK)                │                            │
│                                   │                            │
│  Thread 5 [I/O]                   │                            │
│  ├─▶ Output writer     ┌──────────▼────┐                     │
│  └─▶ Monitoring        │ STDOUT        │                     │
│                        └───────────────┘                     │
│                                                              │
│  Expected Speedup: 3-4x on quad-core ARM Cortex-A          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Performance Characteristics

### Current Performance (Baseline)

| Platform | Architecture | Clock | Throughput | Modulation |
|----------|-------------|-------|------------|------------|
| **Raspberry Pi 4** | ARM Cortex-A72 | 1.5 GHz | ~1.2 MSym/s | QPSK FEC1/2 |
| **Desktop x86** | Intel i7 | 3.5 GHz | ~3-5 MSym/s | QPSK FEC1/2 |
| **Embedded ARM** | Cortex-A9 | 1.0 GHz | ~0.5 MSym/s | QPSK FEC1/2 |

### Projected Performance with Optimizations

| Optimization Phase | Raspberry Pi 4 | Desktop x86 | Embedded ARM |
|-------------------|----------------|-------------|--------------|
| **Baseline** | 1.2 MSym/s | 3-5 MSym/s | 0.5 MSym/s |
| **+Phase 1 NEON** | 3-4 MSym/s | 6-10 MSym/s | 1-1.5 MSym/s |
| **+Phase 2 Threading** | 6-8 MSym/s | 15-25 MSym/s | 2-3 MSym/s |
| **+Phase 3 Advanced** | 10-15 MSym/s | 30-50 MSym/s | 4-6 MSym/s |

### Real-World Use Cases

| Application | Symbol Rate | Required Throughput | Feasibility |
|-------------|-------------|---------------------|-------------|
| **ISS DATV** | 2 MSym/s | 8 Msps IQ | ✓ Achievable (Phase 1) |
| **QO-100 Narrowband** | 500 kSym/s | 2 Msps IQ | ✓ Already works |
| **QO-100 Wideband** | 2 MSym/s | 8 Msps IQ | ✓ Achievable (Phase 1) |
| **Commercial DVB-S** | 27.5 MSym/s | 110 Msps IQ | ✗ Requires HW accel |
| **DVB-S2 HD** | 10 MSym/s | 40 Msps IQ | ⚠ Marginal (Phase 3) |

**Conclusion:** With full optimization, LeanSDR can handle amateur satellite and low-rate commercial DVB on modern ARM platforms.

---

## Next Steps

For detailed optimization strategies, see:
- [NEON Optimization Plan](../optimization/NEON-OPTIMIZATION-PLAN.md) - ⭐ SIMD vectorization
- [Multithreading Strategy](../optimization/MULTITHREADING-PLAN.md) - ⭐ Pipeline parallelization
- [Performance Analysis](../optimization/PERFORMANCE-ANALYSIS.md) - Bottleneck deep-dive

For component-level details, see:
- [DSP Components](../components/DSP-COMPONENTS.md)
- [SDR Components](../components/SDR-COMPONENTS.md)
- [DVB Implementation](../components/DVB-IMPLEMENTATION.md)
- [FEC Systems](../components/FEC-SYSTEMS.md)

---

**Last Updated:** 2025-11-17
