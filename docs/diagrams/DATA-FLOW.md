# LeanSDR Data Flow Diagram Documentation

**Document:** Comprehensive Data Flow Architecture
**Last Updated:** 2025-11-17
**Scope:** Signal processing pipeline, buffer management, scheduling, and thread architecture

---

## Table of Contents

1. [Complete Signal Processing Chain (20 Stages)](#complete-signal-processing-chain)
2. [Pipebuf Data Flow Mechanism](#pipebuf-data-flow-mechanism)
3. [Scheduler Fixpoint Execution Loop](#scheduler-fixpoint-execution-loop)
4. [Feedback Loops and Frequency Tap](#feedback-loops-and-frequency-tap)
5. [Buffer Memory Layout](#buffer-memory-layout)
6. [Thread Communication Architecture](#thread-communication-architecture)

---

## Complete Signal Processing Chain

### Overview: IQ Samples to MPEG-TS in 20 Stages

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                    LEANDVB SIGNAL PROCESSING PIPELINE                                            │
│                      INPUT: IQ SAMPLES → OUTPUT: MPEG-TS PACKETS                                │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 1: INPUT CONVERSION
┌─────────────────┐
│  File Input     │
│  (u8/s8/u16/   │
│   s16/f32)      │
└────────┬────────┘
         │
         ├──→ file_reader<u8>      [8-bit samples]
         ├──→ file_reader<s16>     [16-bit samples]
         └──→ file_reader<cf32>    [32-bit float IQ]
         │
    [I/O Bound]
         │
    Buffer: stdin (BUF_BASEBAND + input_buffer)
    Size: 4096 * buf_factor samples
    │
    ▼
    ┌──────────────────────────────────┐
    │  cconverter<in,out>              │
    │  Format normalization to cf32    │
    │  Data Type: cu8 → cf32           │
    │           cs8 → cf32             │
    │           cu16 → cf32            │
    │           cs16 → cf32            │
    │           cf32 → cf32 (scaled)   │
    └────────┬─────────────────────────┘
             │ Output: p_rawiq (cf32)
             ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 2: GUI SCOPE (Optional - Display Only)
┌────────────────────────┬────────────────────────┐
│  cscope<f32>           │  spectrumscope<f32>    │
│  (IQ Constellation)    │  (FFT - Raw Signal)    │
└────────────────────────┴────────────────────────┘
           │
           │ [No data flow impact - monitoring only]
           │
           ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 3: NOISE INJECTION (Optional - Testing Only)
         ┌────────────────────────┐
    ┌─→  │   wgn_c<f32>           │  ──→ p_noise (cf32)
    │    │  (White Gaussian       │
    │    │   Noise Generator)     │
    │    └────────────────────────┘
    │
    └── [STDIN] ──→ p_preprocessed ──┐
                                     │
                                     ├──→ adder<cf32> ──→ p_noisy
                                     │
                                     └──→ (if --awgn specified)
    │
    ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 4: AUTO-NOTCH FILTER (Removes Interference)
         p_preprocessed (cf32)
         │
         ├──→ auto_notch<f32>
         │    • Detects narrow-band interference (birdies)
         │    • Adaptive IIR filter (~2nd order each)
         │    • Configuration: --anf NUM (default: 1)
         │    [CPU Intensive - SIMD Target]
         │
         ├─ Buffer: reads/writes BUF_BASEBAND
         ├─ Minimum: 4096 samples
         │
         ▼ Output: p_autonotched (cf32)
         │
         │ [If --anf disabled, uses original p_preprocessed]
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 5: FREQUENCY DEROTATION (Static Frequency Shift)
         p_preprocessed (cf32)
         │
         ├─ IF: --Fderot <frequency>
         │
         ├──→ rotator<f32>
         │    • Complex multiplication by exp(-j*2*pi*Fderot*t/Fs)
         │    • Fixed frequency shift (not adaptive)
         │    [Lightweight computation]
         │
         ├─ Buffer: reads 1, writes 1 per sample
         │
         ▼ Output: p_derot (cf32)
         │
         │ [If --Fderot not specified, uses original p_preprocessed]
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 6: CNR ESTIMATION (1 Hz Updates)
         p_preprocessed (cf32)
         │
         ├──→ cnr_fft<f32>
         │    • Measures Carrier-to-Noise Ratio
         │    • Uses FFT-based signal/noise estimation
         │    • Requires: Fs > 3*Fm
         │    • Decimates to 1 Hz output rate
         │    [Lightweight - decimated]
         │
         ├─ Output: p_cnr (f32) @ 1 Hz
         │
         ├─ Connections:
         │  • r_cnr->freq_tap = &demod.freq_tap  [FEEDBACK]
         │  • r_cnr->tap_multiplier = 1.0/decim
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 7: SPECTRUM ANALYZER (1 Hz Updates)
         p_preprocessed (cf32)
         │
         ├──→ spectrum<f32>
         │    • 1024-bin FFT of preprocessed signal
         │    • Decimates to 1 Hz output rate
         │    • Used for monitoring/display
         │    [Lightweight - decimated]
         │
         ├─ Output: p_spectrum (f32[1024]) @ 1 Hz
         │
         ├─ Data Type: float[1024] FFT bins
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 8-9: RESAMPLING / DECIMATION
         p_preprocessed (cf32)
         │
         ├─ IF --resample:
         │    │
         │    ├──→ fir_filter<cf32,float>
         │    │    • High-order lowpass FIR filter
         │    │    • Decimation ratio: decim (computed or specified)
         │    │    • Typical order: 100-300 taps
         │    │    • Cutoff: (Fm/2)*(1+rolloff/2)
         │    │    • Filter rejection: 10-30 dB
         │    │    [CPU INTENSIVE - PRIMARY BOTTLENECK]
         │    │    [SIMD Parallelization Target: 4x speedup]
         │    │
         │    ├─ Connections:
         │    │  • r_resample->freq_tap = &demod.freq_tap  [FEEDBACK]
         │    │  • r_resample->tap_multiplier = 1.0/decim
         │    │  • r_resample->freq_tol = Fm/(Fs*decim)*0.1
         │    │
         │    ├─ Output sampling rate: Fs' = Fs/decim
         │    │
         │    ▼ Output: p_resampled (cf32)
         │
         ├─ ELSE IF --decim:
         │    │
         │    ├──→ decimator<cf32>
         │    │    • Simple down-sampling (no filtering)
         │    │    • ★ WARNING: Causes aliasing!
         │    │    • Fast but low quality
         │    │
         │    ▼ Output: p_decimated (cf32)
         │
         └─ ELSE:
              No resampling, use original p_preprocessed
         │
         ├─ Updated Fs: Fs' = Fs/decim
         │ Buffer: BUF_BASEBAND
         │ Decimation factor: decim ∈ [1, Fs/Fm_min]
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 10: PREPROCESSED OUTPUT (Optional File Dump)
         p_preprocessed (cf32)
         │
         ├──→ file_writer<cf32>
         │    • Writes intermediate preprocessed signal to file
         │    • Debug/analysis purposes
         │    • --pp-fd <file descriptor>
         │    [I/O Bound]
         │
         ├─ Data Type: cf32 (complex float)
         │ Size: (end_time - start_time) * Fs' samples
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 11: CONSTELLATION RECEIVER (Core Demodulator)
         p_preprocessed' (cf32)
         │ [After resampling, at rate Fs']
         │
         ├──→ cstln_receiver<f32>
         │    ★ CORE DEMODULATION ENGINE
         │
         │    Internal Operations:
         │    ├─ Symbol Timing Recovery (Gardner/Mueller-Muller)
         │    ├─ Carrier Phase Tracking (Costas PLL)
         │    ├─ Frequency Offset Correction
         │    ├─ Soft Symbol Decisions (decision feedback)
         │    └─ IQ constellation mapping
         │
         │    Sampler Types:
         │    ├─ SAMP_NEAREST: Fast, poor quality
         │    ├─ SAMP_LINEAR: Balanced (default)
         │    └─ SAMP_RRC: Best quality, CPU-intensive
         │           (Root-Raised-Cosine interpolation)
         │
         │    [CPU INTENSIVE - BOTTLENECK #3]
         │    [SIMD Target: Soft-decision calculations]
         │
         ├─ Input: p_preprocessed (cf32) @ Fs'
         ├─ Output channels:
         │  • p_symbols (softsymbol) @ Fm  [FEC input]
         │  • p_freq (f32) @ decimation   [Carrier estimate]
         │  • p_ss (f32) @ decimation     [Signal strength]
         │  • p_mer (f32) @ decimation    [Modulation error ratio]
         │  • p_sampled (cf32) @ Fm       [For GUI display]
         │
         ├─ Configuration:
         │  • demod.set_omega(Fs'/Fm)     [Samples per symbol]
         │  • demod.set_freq(Ftune/Fs')   [Initial frequency bias]
         │  • demod.set_allow_drift()     [Enable frequency tracking]
         │  • demod.meas_decimation       [Measurement rate]
         │
         ├─ Buffer: BUF_SYMBOLS
         │  • Reads: 128+1 samples per run
         │  • Writes: 128/omega symbols
         │
         ▼ Output: p_symbols (softsymbol)
         │
         │ SOFTSYMBOL data type:
         │  struct softsymbol {
         │    s16 I, Q;         // In-phase & Quadrature soft bits
         │  };
         │  Value range: -128 to +127
         │  Signed for decision feedback in FEC
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 12: TRACKING FEEDBACK LOOPS

         Feedback Connection:
         ┌─────────────────────────────────────────────────┐
         │  demod.freq_tap (output: carrier frequency)     │
         │  Points to demod's internal frequency state     │
         └────────────────────────────────────────────────┬┘
              ▲                                            │
              │                                            │
              │ Feedback:                                  │ Feedback:
              │ • Doppler compensation                     │ • CNR correction
              │ • Resampler coefficient updates            │
              │                                            │
              ├─ r_resample->freq_tap = &demod.freq_tap  │
              │  r_resample->tap_multiplier = 1.0/decim  │
              │  Updates filter coefficients based on freq │
              │                                            │
              └─ r_cnr->freq_tap = &demod.freq_tap       │
                 r_cnr->tap_multiplier = 1.0/decim       │
                 Corrects measurement reference           │

         [No additional buffers - pure control signals]

         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 13: DECONVOLUTION & SYNC (FEC Hard Decisions)
         p_symbols (softsymbol)
         │
         ├─ IF --viterbi:
         │    │
         │    ├──→ viterbi_sync
         │    │    • Maximum likelihood sequence estimation
         │    │    • Convolutional code decoder (rate 1/2, K=7)
         │    │    • Trellis decoder with ACS operations
         │    │    [CPU INTENSIVE - BOTTLENECK #2]
         │    │    [SIMD Target: Branch metrics (4x speedup)]
         │    │
         │    ├─ Buffer: 2048 * buf_factor bytes
         │    ├─ Data Type: u8 (hard decisions)
         │    │
         │    ▼ Output: p_bytes (u8)
         │
         ├─ ELSE:
         │    │
         │    ├──→ deconvol_sync_simple
         │    │    • Algebraic deconvolution
         │    │    • Faster than Viterbi, worse performance
         │    │    • Suitable for high SNR
         │    │
         │    ├─ Buffer: BUF_BYTES
         │    │
         │    ▼ Output: p_bytes (u8)
         │
         ├─ Alternative (HDLC mode):
         │    • etr192_descrambler + hdlc_sync
         │    • For non-MPEG data
         │
         ├─ Output: Bit stream (descrambled raw bits)
         │ Format: u8 (0x00 or 0xFF per bit)
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 14: MPEG SYNCHRONIZATION (Find 0x47 Sync Markers)
         p_bytes (u8)
         │
         ├──→ mpeg_sync<u8,0>
         │    • Finds 0x47 MPEG sync byte
         │    • Byte alignment (if not already aligned)
         │    • Skips bad data between sync markers
         │    [Lightweight - pattern matching]
         │
         ├─ Searches for byte patterns: 0x47 at regular intervals
         ├─ Scans up to 204 bytes for next sync
         │
         ├─ Output channels:
         │  • p_mpegbytes (u8) @ 204-byte chunks
         │  • p_lock (int) @ measurement rate [0=unlocked, 1=locked]
         │  • p_locktime (u32) @ packet rate [milliseconds locked]
         │
         ├─ Buffer: BUF_MPEGBYTES (2448 * buf_factor)
         │ Requirement: 17*11*12+204 = 2448 bytes minimum
         │
         ▼ Output: p_mpegbytes (u8)
         │
         │ [204-byte packets including RS parity bytes]
         │ Format: [MPEG data (188 bytes) + RS parity (16 bytes)]
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 15: DEINTERLEAVING (Undo Forney I=12 Interleaving)
         p_mpegbytes (u8)
         │
         ├──→ deinterleaver<u8>
         │    • Reverses row-column interleaving
         │    • DVB-S standard: I=12 (12-row interleaver)
         │    • Memory block: 17*11*12+204 = 2448 bytes
         │    [Memory Bound - moderate CPU]
         │
         ├─ Internal permutation:
         │  • Reads 2448 bytes (interleaved)
         │  • Outputs 204-byte RS packets (deinterleaved)
         │  • Pattern: Multiple readers, single write
         │
         ├─ Buffer: BUF_PACKETS (1 packet minimum, typically 4)
         │
         ▼ Output: p_rspackets (rspacket<u8>)
         │
         │ Data Type: struct rspacket<T> {
         │   T data[204];  // 188 data + 16 RS parity bytes
         │ };
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 16: REED-SOLOMON FEC (Error Correction)
         p_rspackets (rspacket<u8>)
         │
         ├──→ rs_decoder<u8,0>
         │    • RS(204,188) Reed-Solomon decoder
         │    • Corrects up to 8-byte errors per packet
         │    • Syndrome calculation + Error location polynomial
         │    • Chien search + Forney formula
         │    [Moderate CPU - polynomial arithmetic]
         │    [SIMD Target: Syndrome calculation (2x speedup)]
         │
         ├─ Input: 204-byte packets (188 data + 16 parity)
         ├─ Output: 188-byte packets (corrected or uncorrected)
         │
         ├─ Error tracking:
         │  • p_vbitcount: Bits processed per packet
         │  • p_verrcount: Bits corrected per packet
         │
         ├─ Buffer: BUF_PACKETS
         │
         ▼ Output: p_rtspackets (tspacket)
         │
         │ Data Type: struct tspacket {
         │   u8 data[188];  // MPEG-TS packet (randomized)
         │ };
         │
         │ Note: Still randomized (needs derandomization)
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 17: BER ESTIMATION (Statistics on Error Rates)
         p_verrcount (int), p_vbitcount (int)
         │
         ├──→ rate_estimator<float>
         │    • Measures Viterbi Bit Error Rate (VBER)
         │    • Samples errors over sliding window
         │    • Sample size: max(Fm/2, 50000) bits
         │    [Lightweight - statistical accumulation]
         │
         ├─ Output: p_vber (f32) @ reduced rate
         │
         ├─ Used for monitoring/display only
         │ Does NOT affect data flow
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 18: DERANDOMIZATION (Remove PRBS Scrambling)
         p_rtspackets (tspacket)
         │
         ├──→ derandomizer
         │    • Removes PRBS-15 scrambling pattern
         │    • DVB-S standard scrambling
         │    • PRBS polynomial: X^15 + X^14 + 1
         │    • Initial state: 0x00FF
         │    [Lightweight - XOR operations]
         │
         ├─ Input: 188-byte randomized packets
         ├─ Output: 188-byte descrambled packets
         │
         ├─ Buffer: BUF_PACKETS
         │
         ▼ Output: p_tspackets (tspacket)
         │
         │ Data Type: struct tspacket {
         │   u8 data[188];  // Final MPEG-TS packet
         │ };
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 19: OUTPUT (Write MPEG-TS Packets)
         p_tspackets (tspacket)
         │
         ├──→ file_writer<tspacket>
         │    • Writes 188-byte MPEG-TS packets to STDOUT
         │    • Standard MPEG-TS format
         │    [I/O Bound]
         │
         ├─ Output: File descriptor 1 (STDOUT)
         │ Data size: 188 bytes per packet
         │ Bit rate: (188 bytes * Fm / 8) bits/sec
         │
         ├─ Example:
         │  leandvb ... | ffplay -f mpegts -
         │
         ▼
         STDOUT: MPEG-TS packets
         │
         │ (Binary output - not human-readable)
         │ Use: ffplay, VLC, or other MPEG-TS player
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┘

STAGE 20: AUXILIARY OUTPUTS (Monitoring & Diagnostics)
         p_freq, p_ss, p_mer, p_cnr, p_lock, p_locktime, p_vber
         │
         ├──→ file_printer (Text Output)
         │    • Output channels:
         │      - FREQ: Carrier frequency offset (Hz)
         │      - SS: Signal strength (0-128 normalized)
         │      - MER: Modulation Error Ratio (dB)
         │      - CNR: Carrier-to-Noise Ratio (dB)
         │      - LOCK: Lock status (0/1)
         │      - LOCKTIME: Duration locked (ms)
         │      - VBER: Viterbi Bit Error Rate (1e-5 format)
         │
         │    • Output: File descriptor (--info-fd)
         │      Line-based text format or JSON
         │
         ├──→ file_carrayprinter (Symbol Output)
         │    • Constellation symbols in real-time
         │    • Output: File descriptor (--const-fd)
         │    • Format: "SYMBOLS [real,imag]" per symbol
         │
         ├──→ file_vectorprinter (Spectrum Output)
         │    • 1024-bin FFT of preprocessed signal
         │    • Output: File descriptor (--spectrum-fd)
         │    • Format: "SPECTRUM [bin1,bin2,...,bin1024]"
         │
         ├──→ slowmultiscope (GUI Timeline - if enabled)
         │    • Multi-channel display of metrics over time
         │    • Channels:
         │      - Frequency offset (with wrap display)
         │      - Signal strength
         │      - MER
         │      - CNR
         │      - TS recovery percentage
         │
         ├─ All outputs are asynchronous
         ├─ Decimation/sampling configurable (--info-rate)
         │
         ├─ Examples:
         │   --info-fd 2 --info-rate 5   [stderr, 5 Hz updates]
         │   --const-fd 3 --spectrum-fd 4 [Constellation+spectrum on fds 3,4]
         │   --gui [Enable GUI with X11 display]
         │
         ▼
         Monitoring output (real-time statistics)
         │
         │ Used for:
         │   - Receiver tuning/diagnostics
         │   - Signal quality assessment
         │   - Automated control (AUC)
         │   - User feedback in real-time
         │

└─────────────────────────────────────────────────────────────────────────────────────────────────┘

SUMMARY: Data Type Transformations

   cu8/cs8/cu16/cs16/cf32                    [Input formats]
      ↓
   cf32 (rawiq)                              [Normalized IQ]
      ↓
   cf32 (preprocessed: ANF/derot/decimated)  [Baseband samples]
      ↓
   softsymbol (symbols)                      [Soft decisions: s16 I/Q]
      ↓
   u8 (bytes)                                [Hard bits]
      ↓
   u8 (mpegbytes - deinterleaved)            [188+16 byte packets]
      ↓
   tspacket (randomized)                     [Raw MPEG-TS packets]
      ↓
   tspacket (final)                          [Descrambled MPEG-TS]
      ↓
   STDOUT                                    [Output stream]

```

---

## Pipebuf Data Flow Mechanism

### Multi-Reader FIFO with Compaction

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PIPEBUF<T> DATA STRUCTURE                            │
│              Multi-Reader FIFO Buffer with Automatic Packing             │
└─────────────────────────────────────────────────────────────────────────┘

STRUCTURE DEFINITION:
━━━━━━━━━━━━━━━━━━━━

template<typename T>
struct pipebuf {
    T *buf;              // Allocated buffer memory
    T *rds[MAX_READERS]; // Read pointers (8 max)
    int nrd;             // Number of active readers
    T *wr;               // Write pointer
    T *end;              // End of allocated buffer

    unsigned long min_write;    // Minimum writable space before pack()
    unsigned long total_written;// Total items written (ever)
    unsigned long total_read;   // Total items read (ever)
};


CIRCULAR BUFFER LAYOUT:
━━━━━━━━━━━━━━━━━━━━━━

    Memory Addresses Increase →

┌────────────────────────────────────────────────────────────────────────┐
│ buf                                                               end   │
│  ↓                                                                 ↓   │
│ [Already Read] [Unread Data] [Writable Space] [Reserved for safety]   │
│  ↑                ↑                  ↑                                 │
│ rds[0]           rds[1]              wr                               │
│ (slowest)        (fastest)       (write head)                          │
│                                                                        │
│ rds[0] is SLOWEST reader (determines pack point)                       │
│ rds[1] is faster reader                                                │
│ wr is write head (moving forward)                                      │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘


PACKING OPERATION (Compaction):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

When: wr approaches end OR writable() < min_write

BEFORE PACK:
───────────
    [Consumed] [Reader1 Data] [Reader0 Data] [Unwritten]
                               ↑              ↑
                              rds[0]          wr


ACTION: memmove(buf, rd_slowest, unread_bytes)
  1. Find slowest reader position: rd = min(rds[0], rds[1], ...)
  2. Move unread data to buffer start: memmove(buf, rd, (wr-rd)*sizeof(T))
  3. Adjust all pointers backwards: offset = rd - buf
     - wr -= offset
     - For each reader i: rds[i] -= offset


AFTER PACK:
──────────
    [Reader1 Data] [Reader0 Data] [Writable Space]
                   ↑               ↑
                  rds[0]           wr

    buf ──→ new read head = 0
    (wr is now < end, more space available)


EXAMPLE WITH NUMBERS:
━━━━━━━━━━━━━━━━━━━

Buffer: T[100] (size=100)

Initial state:
  buf=0, wr=40, rds[0]=10, rds[1]=30, end=100

  [0-10: consumed] [10-30: reader1] [30-40: reader0] [40-100: writable=60]

Write 50 more items (wr → 90):
  [0-10: old] [10-30: r1] [30-40: r0] [40-90: new] [90-100: writable=10]

writable() = 10 < min_write (say 20), so trigger pack()

PACK:
  rd_slowest = min(10, 30) = 10
  offset = 10
  memmove(buf, buf+10, 30 items)

  wr = 90 - 10 = 80
  rds[0] = 10 - 10 = 0
  rds[1] = 30 - 10 = 20

After pack:
  [0-20: reader1] [20-30: reader0] [30-80: new data] [80-100: writable=20]

  buf → new starting point
  wr = 80
  writable() = 100 - 80 = 20


MULTIPLE READERS:
━━━━━━━━━━━━━━━━

Pipeline example:

  ┌─────────────┐         ┌─────────────┐         ┌─────────────┐
  │  Producer   │──→ buf ←─│  Reader 1   │─────────│  Reader 2   │
  │             │  p_data  │  (slower)   │ p_data2 │  (faster)   │
  └─────────────┘         └─────────────┘         └─────────────┘

  Writer:  pipewriter<T> object
    • wr() - Get current write position
    • writable() - Check space available (triggers pack if needed)
    • written(n) - Advance write pointer n items

  Readers: pipereader<T> objects (nrd instances)
    • rd() - Get current read position for this reader
    • readable() - Unread items available
    • read(n) - Advance read pointer n items

  Problem:
    • Reader1 is slow (reads: pos=10)
    • Reader2 is fast (reads: pos=40)
    • Data [10-40] held in buffer until Reader1 catches up

  Solution:
    • pack() moves data to front when Reader1 falls behind
    • Frees memory for Writer


WRITER OPERATIONS:
━━━━━━━━━━━━━━━━━

struct pipewriter<T> {
    pipebuf<T> &buf;

    // Non-blocking check for space
    unsigned long writable() {
        if (buf.end - buf.wr < buf.min_write)
            buf.pack();  // Try to reclaim space
        return buf.end - buf.wr;
    }

    // Get write position
    T *wr() { return buf.wr; }

    // Advance write pointer
    void written(unsigned long n) {
        buf.wr += n;
        buf.total_written += n;
    }

    // Convenience: write single item
    void write(const T &e) {
        *wr() = e;
        written(1);
    }
};


READER OPERATIONS:
━━━━━━━━━━━━━━━━━

struct pipereader<T> {
    pipebuf<T> &buf;
    int id;  // Reader ID (0..7)

    // How many items available
    unsigned long readable() {
        return buf.wr - buf.rds[id];
    }

    // Get read position
    T *rd() { return buf.rds[id]; }

    // Advance read pointer
    void read(unsigned long n) {
        buf.rds[id] += n;
        buf.total_read += n;
    }
};


USAGE PATTERN IN PIPELINE:
━━━━━━━━━━━━━━━━━━━━━━━━━

Producer runnable:
    pipewriter<cf32> writer(p_rawiq, 128);  // Min write 128 samples

    void run() {
        unsigned long n = writer.writable();  // Space available
        if (n > 0) {
            cf32 *wr = writer.wr();
            // Fill buffer
            for (int i = 0; i < n; ++i)
                wr[i] = ... data from input ...;
            writer.written(n);
        }
    }

Consumer runnable:
    pipereader<cf32> reader(p_rawiq);

    void run() {
        unsigned long n = reader.readable();  // Data available
        if (n >= 128) {
            cf32 *rd = reader.rd();
            // Process buffer
            for (int i = 0; i < 128; ++i)
                process(rd[i]);
            reader.read(128);
        }
    }


DATA FLOW DIAGRAM:
━━━━━━━━━━━━━━━━━

    ┌──────────────┐
    │  Producer    │
    │  run()       │
    └──────┬───────┘
           │ pipewriter<cf32>
           │ .writable() → get space
           │ .written(n) → advance wr
           │
           ▼
    ┌──────────────────────────────────┐
    │        pipebuf<cf32>             │
    │  [Circular buffer with pack]    │
    │  buf, wr, rds[0], rds[1]        │
    └──────┬───────────────┬──────────┘
           │               │
      Reader1          Reader2
    pipereader<cf32> pipereader<cf32>
    .readable()       .readable()
           │               │
           ▼               ▼
    ┌──────────────┐ ┌──────────────┐
    │ Consumer1    │ │ Consumer2    │
    │ (Slower)     │ │ (Faster)     │
    │ run()        │ │ run()        │
    └──────────────┘ └──────────────┘


HASH-BASED FIXPOINT DETECTION:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

pipebuf tracks:
    • total_written (cumulative count)
    • total_read (cumulative count)

hash() = total_written + total_read

Scheduler uses hash to detect stagnation:
    unsigned long long prev_hash = 0;
    while (1) {
        step();  // Run all runnables once
        unsigned long long h = hash_all_pipes();
        if (h == prev_hash) break;  // Fixpoint reached
        prev_hash = h;
    }


BUFFER SIZE REQUIREMENTS:
━━━━━━━━━━━━━━━━━━━━━━━━━

Pipeline buffers are sized to accommodate:

BUF_BASEBAND = 4096 * buf_factor
    • Input samples (cf32)
    • Preprocessed IQ data
    • Resampled output
    • Spectrum FFT input
    Buffer strategy: Keep 4096 samples minimum
    Typical: 16,384 samples (buf_factor=4)
    Memory: cf32 = 8 bytes → 128 KB per buffer

BUF_SYMBOLS = 1024 * buf_factor
    • Soft symbols (softsymbol)
    • From cstln_receiver
    • To Viterbi/deconvol
    Typical: 4,096 symbols
    Memory: softsymbol = 4 bytes → 16 KB

BUF_BYTES = 2048 * buf_factor
    • Raw bytes from deconvol
    • To MPEG sync
    Typical: 8,192 bytes = 8 KB

BUF_MPEGBYTES = 2448 * buf_factor
    • MPEG packets (188 + RS parity bytes)
    Minimum: 17*11*12+204 = 2,448 bytes (one full deinterleaver block)
    Typical: 9,792 bytes ≈ 10 KB

BUF_PACKETS = buf_factor
    • MPEG-TS packets (tspacket)
    • Small, mainly for synchronization
    Typical: 4 packets = 752 bytes

BUF_SLOW = buf_factor
    • Measurement outputs (freq, SS, MER, CNR)
    • Updated at reduced rate (1 Hz typical)
    Typical: 4 samples


TOTAL MEMORY: ~200 KB (buf_factor=4)
────────────

128 KB (rawiq) + 16 KB (symbols) + 10 KB (bytes) + 10 KB (MPEG) + 1 KB (packets)
+ 1 KB (slow) + misc = ~200 KB

Scales linearly with buf_factor:
    buf_factor=1 → ~50 KB (minimal latency)
    buf_factor=4 → ~200 KB (default)
    buf_factor=16 → ~800 KB (for bursty networks)

```

---

## Scheduler Fixpoint Execution Loop

### Cooperative Multi-Tasking with Hash-Based Convergence

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   SCHEDULER FIXPOINT EXECUTION LOOP                      │
│        Cooperative scheduling with automatic convergence detection       │
└─────────────────────────────────────────────────────────────────────────┘


HIGH-LEVEL ALGORITHM:
━━━━━━━━━━━━━━━━━━━━

scheduler.run():
    prev_hash = 0
    loop:
        step()                   // Run all runnables once
        h = hash_all_buffers()   // Sum of (total_written + total_read)
        if h == prev_hash:
            break                // No data moved - fixpoint
        prev_hash = h


DETAILED FLOW:
━━━━━━━━━━━━━

    ┌─────────────────────────────────────────────────────┐
    │ START: scheduler.run()                              │
    └────────────────────┬────────────────────────────────┘
                         │
                         ▼
              ┌──────────────────────┐
              │ prev_hash = 0        │
              │ iteration = 0        │
              └────────────┬─────────┘
                           │
            ┌──────────────▼──────────────┐
            │  Main Loop: while (1)       │
            └──────────────┬──────────────┘
                           │
        ┌──────────────────▼─────────────────────┐
        │  STEP 1: Run All Runnables             │
        ├────────────────────────────────────────┤
        │  for i = 0 to nrunnables-1:            │
        │      runnables[i]->run()               │
        │                                        │
        │  Order of execution:                   │
        │    1. file_reader (input)              │
        │    2. preprocessing (ANF, filters)     │
        │    3. cstln_receiver (demod)           │
        │    4. deconvol/viterbi                 │
        │    5. MPEG sync/RS decoder             │
        │    6. output (file_writer)             │
        │    7. monitoring (printers, scopes)    │
        │                                        │
        │  Each runnable processes available     │
        │  data and advances its read/write      │
        │  pointers in pipebuf                   │
        └──────────────────┬────────────────────┘
                           │
        ┌──────────────────▼─────────────────────┐
        │  STEP 2: Hash All Buffers              │
        ├────────────────────────────────────────┤
        │  h = 0                                 │
        │  for i = 0 to npipes-1:                │
        │      h += (1+i) * pipes[i]->hash()    │
        │                                        │
        │  Each pipe hash = total_written +     │
        │                    total_read          │
        │                                        │
        │  Weighted sum accounts for buffer     │
        │  order (earlier buffers weighted      │
        │  less to break ties)                  │
        │                                        │
        │  Example:                              │
        │    h = 1*pipe[0].hash() +              │
        │        2*pipe[1].hash() +              │
        │        3*pipe[2].hash() + ...          │
        └──────────────────┬────────────────────┘
                           │
        ┌──────────────────▼─────────────────────┐
        │  STEP 3: Check Convergence             │
        ├────────────────────────────────────────┤
        │  if h == prev_hash:                    │
        │      break  (FIXPOINT REACHED)         │
        │  else:                                 │
        │      prev_hash = h                    │
        │      iteration++                       │
        │      continue loop                     │
        └──────────────────┬────────────────────┘
                           │
            ┌──────────────▼──────────────┐
            │  Either:                    │
            │  • Fixpoint → Exit loop     │
            │  • Data changed → Next iter │
            └──────────────┬──────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │ scheduler.shutdown()   │
              │ call run() on all      │
              │ runnables              │
              └────────────────────────┘
                           │
                           ▼
                      ┌────────┐
                      │  DONE  │
                      └────────┘


FIXPOINT SEMANTICS:
━━━━━━━━━━━━━━━━━━

Fixpoint = No runnable made progress in last iteration

Conditions at fixpoint:
    ✓ All input consumed
    ✓ All processing complete
    ✓ All output written
    ✗ Waiting for more input (EOF)

Example 1 - Normal completion:
    Iteration 1: file_reader writes 4096 → hash changes
    Iteration 2: filters process 4096 → hash changes
    ...
    Iteration N: output writes final data → hash changes
    Iteration N+1: no one has data to process → hash same → STOP


Example 2 - Backpressure:
    Iteration 1: producer wants to write 1000 items
    Iteration 2: consumer reads 500 items
    Iteration 3: producer writes 500 items
    Iteration 4: consumer tries to read but input exhausted
    Iteration 5: producer blocked on input (EOF)
    Iteration 6: hash unchanged → STOP


RUNNABLE INTERFACE:
━━━━━━━━━━━━━━━━━━

struct runnable_common {
    const char *name;
    virtual void run() = 0;
    virtual void shutdown() = 0;
};

run() contract:
    • Process available data (check readable() > 0)
    • Write results (check writable() > min_write)
    • Advance read/write pointers (affects hash)
    • Non-blocking (don't wait for more data)
    • May process partial data or nothing

shutdown() contract:
    • Flush any buffered data
    • Close files/resources
    • Called once at program exit


EXAMPLE RUNNABLE: file_reader<T>
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct file_reader : runnable {
    FILE *f;
    pipewriter<T> &writer;

    void run() {
        unsigned long n = writer.writable();  // Space available?
        if (n == 0) return;  // Full, try later

        T *wr = writer.wr();
        int nread = fread(wr, sizeof(T), n, f);  // Non-blocking?

        if (nread > 0) {
            writer.written(nread);  // Advance pointer
            // HASH CHANGES - loop continues
        }
        if (nread < n || feof(f)) {
            // Input exhausted
        }
    }
};


EXAMPLE RUNNABLE: cstln_receiver<T>
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct cstln_receiver : runnable {
    pipereader<cf32> reader;
    pipewriter<softsymbol> writer;

    void run() {
        unsigned long avail = reader.readable();
        if (avail < 128+1) return;  // Not enough data

        unsigned long nwrite = writer.writable();
        if (nwrite < 128) return;  // No space

        cf32 *rd = reader.rd();
        softsymbol *wr = writer.wr();

        // Process 128 IQ samples → N symbols
        unsigned long nsym = demodulate(rd, wr, 128);

        reader.read(128);
        writer.written(nsym);
        // HASH CHANGES - both read and write advanced
    }
};


SCHEDULER STATE:
━━━━━━━━━━━━━━━

struct scheduler {
    pipebuf_common *pipes[MAX_PIPES];  // All buffers
    int npipes;
    runnable_common *runnables[MAX_RUNNABLES];  // All components
    int nrunnables;
    window_placement *windows;  // GUI hints
    bool verbose;  // Logging
    bool debug;    // Debug output

    void run() {
        unsigned long long prev_hash = 0;
        while (1) {
            step();  // Run runnables[0..nrunnables-1]->run()
            unsigned long long h = hash();
            if (h == prev_hash) break;
            prev_hash = h;
        }
    }

    unsigned long long hash() {
        unsigned long long h = 0;
        for (int i = 0; i < npipes; ++i) {
            h += (1+i) * pipes[i]->hash();
        }
        return h;
    }

    void dump() {
        // Print buffer statistics
    }
};


ITERATION EXAMPLE: DVB-S Demodulation
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Data flowing through pipeline:

Iteration 1:
    file_reader: reads 4096 cu8 → writes 4096 cf32 (rawiq)
                 hash_change = true
    ANF: reads 4096 cf32 → writes 4096 cf32 (preprocessed)
         hash_change = true
    Other runnables: no input available (empty)
    ──→ hash changed, continue

Iteration 2:
    file_reader: reads 4096 cu8 → writes 4096 cf32
                 hash_change = true
    ANF: reads 4096 cf32 → writes 4096 cf32
         hash_change = true
    cstln_receiver: reads 128 cf32 → writes ~64 softsymbols
                    hash_change = true
    ──→ hash changed, continue

Iteration 3-10: Similar progressive pipeline filling
    ──→ hash changes each iteration

Iteration 11:
    file_reader: EOF reached, no write
    ANF: reads 4096, writes 4096
    cstln_receiver: reads 128, writes ~64
    deconvol: reads 64 soft-symbols → writes 64 bytes
    mpeg_sync: reads 204 bytes → writes 0 (need 204 min)
    ...
    ──→ hash changed (preprocessing still has data)

Iteration 12:
    file_reader: EOF, nothing
    ANF: no input
    cstln_receiver: no input
    deconvol: processes buffered data
    mpeg_sync: processes bytes
    ...
    ──→ hash changed (FEC still processing)

Iteration 100:
    All runnables: no input, no output possible
    Hash = same as iteration 99
    ──→ FIXPOINT: exit loop


VERBOSE OUTPUT:
━━━━━━━━━━━━━━

With --verbose:
    Adding pipe "stdin" (cu8)
    Adding pipe "rawiq" (cf32)
    ...
    Adding runnable "file_reader"
    Adding runnable "auto_notch"
    ...
    Buffer statistics:
    .rawiq             : 4090/65536  4016 writable, unread (4090)
    .preprocessed      : 4090/65536  4016 writable, unread (0 4090)
    .symbols           : 2000/4096   2096 writable, unread (0 2000)
    Total buffer memory: 234 KiB


TIMING:
━━━━━━

- Single iteration ≈ 1-10 ms (depends on workload)
- Fixpoint reached ≈ 100-1000 iterations (until EOF)
- Real-time: Fs=2.4 MHz → ~4096/2.4M = 1.7 ms per iteration
- Total latency: 100 iterations × 1.7 ms = 170 ms end-to-end

```

---

## Feedback Loops and Frequency Tap

### Carrier Tracking with Resampler and CNR Feedback

```
┌─────────────────────────────────────────────────────────────────────────┐
│                  FEEDBACK LOOPS IN LEANDVB ARCHITECTURE                  │
│              Frequency tap connecting resampler and demodulator          │
└─────────────────────────────────────────────────────────────────────────┘


FEEDBACK LOOP OVERVIEW:
━━━━━━━━━━━━━━━━━━━━━

Problem: Satellite signals experience Doppler shift
         cstln_receiver estimates carrier frequency
         But resampler needs updated frequency info

Solution: freq_tap pointer passed from demod to filters

Data flows forward: IQ → Resample → Demod → Symbols
Control flows back: Demod.freq_tap → Resample (updates coefficients)


ARCHITECTURE DIAGRAM:
━━━━━━━━━━━━━━━━━━━

    ┌─────────────────────────────────────────────────────┐
    │           FORWARD DATA PATH (Serial)                 │
    │                                                      │
    │  p_preprocessed (cf32 @ Fs)                          │
    │       │                                              │
    │       ├──→ fir_filter<cf32>                          │
    │       │    (decimate, apply freq correction)         │
    │       │                                              │
    │       ├─ Output: p_resampled (cf32 @ Fs/decim)      │
    │       │                                              │
    │       ├──→ cstln_receiver<f32>                       │
    │       │    (demodulate, track PLL)                   │
    │       │                                              │
    │       ├─ Output: p_symbols (softsymbol)              │
    │       │         p_freq (f32) ← Carrier estimate      │
    │       │                                              │
    │       ▼                                              │
    │    (to FEC)                                          │
    │                                                      │
    └──────────────────────┬───────────────────────────────┘
                           │
    ┌──────────────────────▼───────────────────────────────┐
    │        FEEDBACK CONTROL PATH (Real-time)            │
    │                                                      │
    │   demod.freq_tap (pointer to float)                  │
    │        ▲                                              │
    │        │ Feedback: demod's frequency estimate        │
    │        │                                              │
    │        ├─ r_resample->freq_tap = &demod.freq_tap    │
    │        │  r_resample->tap_multiplier = 1.0/decim    │
    │        │                                              │
    │        └─ r_cnr->freq_tap = &demod.freq_tap         │
    │           r_cnr->tap_multiplier = 1.0/decim         │
    │                                                      │
    └─────────────────────────────────────────────────────┘


DETAILED FREQUENCY TAP MECHANISM:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Stage: RESAMPLER (fir_filter)
───────────────────────────────

struct fir_filter : runnable {
    pipewriter<cf32> writer;
    pipereader<cf32> reader;

    float *freq_tap;          // Pointer to external frequency
    float tap_multiplier;     // 1.0/decim
    float freq_tol;           // Tolerance for frequency changes

    void run() {
        if (freq_tap) {
            // Read external frequency (from demod)
            float new_freq = *freq_tap * tap_multiplier;

            // Update filter coefficients if frequency changed
            if (fabs(new_freq - current_freq) > freq_tol) {
                current_freq = new_freq;

                // Recompute filter phase advance
                // h(n) -> h(n) * exp(j*2*pi*freq*n/Fs)

                // Apply frequency shift in FIR taps
                for (int k = 0; k < ncoeffs; ++k) {
                    phase = 2*pi*freq*k/Fs;
                    coeffs[k] *= exp(j*phase);  // Modulate taps
                }
            }
        }

        // Process: Read input → Apply FIR → Write output
        unsigned long avail = reader.readable();
        if (avail < decim) return;

        unsigned long out_space = writer.writable();
        if (out_space < 1) return;

        cf32 *in = reader.rd();
        cf32 *out = writer.wr();

        // FIR convolution with frequency-adjusted taps
        cf32 sum = 0;
        for (int k = 0; k < ncoeffs; ++k)
            sum += coeffs[k] * in[k];

        *out = sum;
        reader.read(decim);
        writer.written(1);
    }
};


Stage: CNR ESTIMATOR (cnr_fft)
──────────────────────────────

struct cnr_fft : runnable {
    pipereader<cf32> reader;
    pipewriter<f32> writer;

    float *freq_tap;          // Pointer to external frequency
    float tap_multiplier;     // 1.0/decim (or 1.0 if no resampler)

    void run() {
        if (freq_tap) {
            // Read external frequency (from demod)
            float new_freq = *freq_tap * tap_multiplier;

            // Use frequency estimate to set measurement bandwidth
            // or to derotate signal before FFT

            // Example: Measure noise power shifted by frequency
            int bin = (new_freq * Fs / fft_size);
            // Measure power around bin
        }

        // Process: Read IQ → FFT → Estimate CNR → Write result
        unsigned long avail = reader.readable();
        if (avail < fft_size) return;

        unsigned long out_space = writer.writable();
        if (out_space < 1) return;

        cf32 *in = reader.rd();

        // FFT + power spectrum
        // Identify signal peak vs noise floor
        float signal_power = ...;
        float noise_power = ...;
        float cnr = signal_power / noise_power;

        writer.write(cnr);  // Output CNR in dB
        reader.read(fft_size);
    }
};


Stage: CONSTELLATION RECEIVER (cstln_receiver)
──────────────────────────────────────────────

struct cstln_receiver : runnable {
    pipereader<cf32> reader;
    pipewriter<softsymbol> writer;

    // Internal frequency tracking (Costas PLL)
    float freq_tap;           // ★ THIS IS THE FEEDBACK SOURCE

    void run() {
        // Demodulation: timing recovery + PLL

        // Costas PLL updates frequency estimate:
        float error = ... (cross-product of I/Q);
        freq_tap += pll_gain * error;  // Update frequency

        // Frequency tracking within symbol timing
        // (Gardner timing recovery + PLL carrier tracking)

        // Output soft symbols
    }
};


FEEDBACK SIGNAL PATH:
━━━━━━━━━━━━━━━━━━━

TIMELINE:
    t=0: User starts leandvb with --Fs 2.4M --Fm 2M
         │
    t=1: file_reader starts streaming IQ samples
         │ fir_filter sees freq_tap = &demod.freq_tap (NULL initially)
         │ CNR estimator sees freq_tap = &demod.freq_tap
         │
    t=2: cstln_receiver begins demodulation
         │ Costas PLL adjusts freq_tap internally
         │ freq_tap now has meaningful value (carrier estimate)
         │
    t=3: fir_filter runs AFTER cstln_receiver
         │ if (freq_tap) reads current value
         │ Adjusts filter taps to match demod's frequency
         │
    t=4: CNR estimator runs
         │ Uses freq_tap to set measurement center frequency
         │
    t=5+: Loop continues, frequency estimate continuously updated


EXAMPLE SCENARIO: Doppler Shift
━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Initial: Satellite at rest → no Doppler
    demod.freq_tap = 0 (Hz)
    fir_filter: uses nominal filter coefficients
    cnr_fft: measures power at band center

Step 1: Satellite approaches Earth
    Doppler shift: +500 Hz (example)

Step 2: cstln_receiver Costas PLL detects offset
    Adjusts demod.freq_tap → +500 Hz

Step 3: fir_filter frequency compensation
    On next run:
    new_freq = 500 * (1.0/decim) = 500 Hz (if decim=1)
    Updates FIR coefficients:
        h'[k] = h[k] * exp(j*2*pi*500*k/Fs)

Step 4: Output of fir_filter now pre-compensates for Doppler
    Reduced frequency offset in demod's input

Step 5: Feedback loop stabilizes
    Error decreases in next PLL iteration
    Frequency tap converges to true Doppler shift


IMPLEMENTATION DETAILS: Configuration in leandvb.cc
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Code (line 506-515):
    // TRACKING FILTERS

    if (r_resample) {
        r_resample->freq_tap = &demod.freq_tap;
        r_resample->tap_multiplier = 1.0 / decim;
        r_resample->freq_tol = cfg.Fm/(cfg.Fs*decim) * 0.1;
    }

    if (r_cnr) {
        r_cnr->freq_tap = &demod.freq_tap;
        r_cnr->tap_multiplier = 1.0 / decim;
    }


Explanation:
    • r_resample: FIR filter after resampling
    • freq_tap: Points to demodulator's internal frequency
    • tap_multiplier: Scales frequency if decimation changed sample rate
    • freq_tol: Only update coefficients if change > 0.1*Fm
                (avoids thrashing on noise)
    • r_cnr: CNR estimator also uses freq_tap
    • Both tap_multiplier values are 1.0/decim

Effect:
    • Resampler has current frequency offset
    • Can adjust filter to match demod's PLL
    • Reduces frequency error in demod input
    • Improves lock time and tracking ability


TIMING OF FEEDBACK:
━━━━━━━━━━━━━━━━

Scheduler iteration order:
    1. file_reader: Input → p_rawiq
    2. ANF filter: p_rawiq → p_preprocessed
    3. fir_filter: p_preprocessed → p_resampled (READS freq_tap)
    4. cstln_receiver: p_resampled → p_symbols (UPDATES freq_tap)

Problem: fir_filter runs BEFORE cstln_receiver each iteration!
Solution: Pipelined execution
    ├─ Iteration N: fir_filter uses freq_tap from iteration N-1
    │               (one sample-block of lag)
    │
    └─ This is ACCEPTABLE because:
       • Frequency changes slowly (over 100s of symbols)
       • One-iteration lag ≈ 1 ms (negligible)
       • Negative feedback loop stabilizes


FEEDBACK STABILITY:
━━━━━━━━━━━━━━━━

System block diagram:

    Input IQ ──→ [Resampler] ──→ [Demod] ──→ Symbols
                      ▲           │
                      └───────────┘
                     freq_tap feedback

    Negative feedback loop:
    • Error (frequency offset) reduces over time
    • Integrator (resampler uses new freq every iteration)
    • Proportional gain (PLL gain in demod)

    Stability conditions:
    ✓ Slow feedback (decimation reduces feedback rate)
    ✓ Bounded frequency range (min/max freqw limits)
    ✓ Gain carefully tuned (pll_adjustment parameter)


CONTROL FLOW CODE:
━━━━━━━━━━━━━━━━

In dsp.h - fir_filter run() method:

    void run() {
        unsigned long avail = reader.readable();
        if (avail < decim) return;

        unsigned long out = writer.writable();
        if (out < min_out) return;

        // ★ FEEDBACK: Check if frequency changed
        if (freq_tap) {
            float new_freq = *freq_tap * tap_multiplier;  // Read external value
            if (fabsf(new_freq - current_freq) > freq_tol) {
                current_freq = new_freq;
                update_filter_coefficients();  // Adjust taps
            }
        }

        // Process: decimate by factor and apply FIR
        const cf32 *in = reader.rd();
        cf32 *out = writer.wr();

        while (decim_count < decim) {
            // FIR convolution with possibly updated coefficients
            cf32 sum = 0;
            for (int k = 0; k < ncoeffs; ++k)
                sum += coeffs[k] * in[decim_count + k];

            decim_count += decim;
        }

        *out = sum;
        reader.read(decim);
        writer.written(1);
    }


ALTERNATIVE ARCHITECTURES:
━━━━━━━━━━━━━━━━━━━━━━━━

Current (single-threaded):
    • One freq_tap pointer
    • Feedback read after demod writes
    • Simple, deterministic

Multi-threaded proposal:
    • freq_tap becomes atomic variable
    • Mutex-protected or lock-free
    • Resampler thread reads asynchronously
    • Demod thread updates asynchronously
    • Race conditions possible but acceptable


PERFORMANCE IMPLICATIONS:
━━━━━━━━━━━━━━━━━━━━━━

Benefit:
    + Doppler tracking without manual retune
    + Frequency offset corrected before soft-decision stage
    + Improves SNR in demodulation
    + Faster lock acquisition

Cost:
    - Resampler must recalculate filter taps on freq change
    - Adds ~5-10% CPU overhead (freq checks on each iteration)
    - Complexity in understanding feedback loop

Real-world impact:
    • Reduces initial lock time: 1-2 seconds → 0.5-1 second
    • Enables passive reception (no manual tuning)
    • Essential for satellite reception

```

---

## Buffer Memory Layout

### Complete Memory Architecture and Sizing Strategy

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 BUFFER MEMORY LAYOUT ARCHITECTURE                        │
│              Heap allocation, buffer sizing, and packing strategy        │
└─────────────────────────────────────────────────────────────────────────┘


MEMORY ALLOCATION OVERVIEW:
━━━━━━━━━━━━━━━━━━━━━━━━━

Heap Memory Regions (typical buf_factor=4):

┌─────────────────────────────────────────────────────────────────────────┐
│                                                                          │
│  Stack (↓)                                                               │
│  ├─ Local variables                                                     │
│  ├─ Function parameters                                                │
│  └─ ~1 MB available (usually)                                          │
│                                                                          │
│  [Free space]                                                            │
│                                                                          │
│  Heap (↑)                                                                │
│  ├─ Scheduler object (~100 bytes)                                       │
│  ├─ Runnable objects (~50 bytes each × 20 = 1 KB)                      │
│  ├─ Pipebuf objects (~200 bytes each × 10 = 2 KB)                      │
│  │                                                                       │
│  └─ Pipebuf DATA BUFFERS (★ MAIN ALLOCATION)                           │
│     │                                                                    │
│     ├─ stdin (cu8): 4096 + input_buffer [optional]                     │
│     ├─ rawiq (cf32): 4096*4 = 16,384 items = 128 KB                    │
│     ├─ preprocessed (cf32): 16,384 items = 128 KB                      │
│     ├─ resampled (cf32): 16,384 items = 128 KB                         │
│     ├─ symbols (softsymbol): 4,096 items = ~16 KB                      │
│     ├─ bytes (u8): 8,192 items = 8 KB                                  │
│     ├─ mpegbytes (u8): 9,792 items = ~10 KB                            │
│     ├─ rspackets (rspacket<u8>): 4 packets = 816 B                     │
│     ├─ tspackets (tspacket): 4 packets = 752 B                         │
│     ├─ freq, SS, MER, CNR (f32): 4 items each = 64 B                   │
│     └─ [Other small buffers for GUI, measurements]                     │
│                                                                          │
│     Total data: ~200 KB (with default buf_factor=4)                    │
│                 Scales linearly with buf_factor                         │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘


DETAILED BUFFER SIZES:
━━━━━━━━━━━━━━━━━━━━

buf_factor multiplier controls memory/latency tradeoff

buf_factor=1:  Minimal (50 KB) - high latency, real-time
buf_factor=4:  Default (200 KB) - balanced
buf_factor=16: Large (800 KB) - low latency, high memory

Computation from leandvb.cc (line 180-202):

    unsigned long BUF_BASEBAND = 4096 * cfg.buf_factor;
    unsigned long BUF_SYMBOLS = 1024 * cfg.buf_factor;
    unsigned long BUF_BYTES = 2048 * cfg.buf_factor;
    unsigned long BUF_MPEGBYTES = 2448 * cfg.buf_factor;
    unsigned long BUF_PACKETS = cfg.buf_factor;
    unsigned long BUF_SLOW = cfg.buf_factor;


╔════════════════════════════════════════════════════════════════════════╗
║ BUFFER SIZE TABLE (bytes)                                              ║
╠════════════╦═════════════╦═════════════╦═════════════╦════════════════╣
║ Buffer     ║ buf_factor=1║ buf_factor=4║ buf_factor=16║ Notes          ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ stdin      ║ 4,096 (u8)  ║ 4,096 (u8)  ║ 4,096 (u8)  ║ Input format   ║
║            ║ = 4 KB      ║ = 4 KB      ║ = 4 KB      ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ rawiq      ║ 4,096 cf32  ║ 16,384 cf32 ║ 65,536 cf32 ║ IQ samples     ║
║            ║ = 32 KB     ║ = 128 KB    ║ = 512 KB    ║ (8 B/item)     ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ preproc'd  ║ 4,096 cf32  ║ 16,384 cf32 ║ 65,536 cf32 ║ After ANF      ║
║            ║ = 32 KB     ║ = 128 KB    ║ = 512 KB    ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ resampled  ║ 4,096 cf32  ║ 16,384 cf32 ║ 65,536 cf32 ║ If --resample  ║
║            ║ = 32 KB     ║ = 128 KB    ║ = 512 KB    ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ symbols    ║ 1,024 SS    ║ 4,096 SS    ║ 16,384 SS   ║ Soft symbols   ║
║            ║ = 4 KB      ║ = 16 KB     ║ = 64 KB     ║ (4 B/item)     ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ bytes      ║ 2,048 u8    ║ 8,192 u8    ║ 32,768 u8   ║ From deconv    ║
║            ║ = 2 KB      ║ = 8 KB      ║ = 32 KB     ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ mpegbytes  ║ 2,448 u8    ║ 9,792 u8    ║ 39,168 u8   ║ With RS parity ║
║            ║ = 2.4 KB    ║ = 9.6 KB    ║ = 38 KB     ║ (one block min)║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ rspackets  ║ 4 packets   ║ 4 packets   ║ 4 packets   ║ 204 B each     ║
║            ║ = 816 B     ║ = 816 B     ║ = 816 B     ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ tspackets  ║ 4 packets   ║ 4 packets   ║ 4 packets   ║ 188 B each     ║
║            ║ = 752 B     ║ = 752 B     ║ = 752 B     ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ Monitoring ║ ~1 KB       ║ ~1 KB       ║ ~1 KB       ║ freq, SS, MER  ║
║ (slow)     ║             ║             ║             ║                ║
╠════════════╬═════════════╬═════════════╬═════════════╬════════════════╣
║ ★ TOTAL    ║ ~50 KB      ║ ~200 KB     ║ ~800 KB     ║ Typical system ║
╚════════════╩═════════════╩═════════════╩═════════════╩════════════════╝


PACKING MEMORY MANAGEMENT:
━━━━━━━━━━━━━━━━━━━━━━━━━

Each pipebuf<T> has automatic memory compaction (pack):

Before pack():
┌────────────────────────────────────────────────────────────────┐
│ buf                  Unread data        end                     │
│  ↓                                       ↓                      │
│ [Already read] [Reader1] [Reader0] [Writable space] [Reserved]│
│  ↑             ↑          ↑                                     │
│  0             rd1        rd0                                   │
│
│ Memory waste: "Already read" region = rd0 - buf bytes


After pack():
┌────────────────────────────────────────────────────────────────┐
│ buf             Unread data        end                          │
│  ↓                                  ↓                           │
│ [Reader1] [Reader0] [Writable space] [Reserved]               │
│  ↑        ↑                                                     │
│  0        rd0-offset                                           │
│
│ Memory recovery: (rd0 - buf) bytes now writable


PACKING TRIGGERS:
━━━━━━━━━━━━━━━━

Called automatically when:
    writable() < min_write

Inside pipewriter.writable():
    if (buf.end - buf.wr < buf.min_write)
        buf.pack();

Strategy:
    • min_write set per pipeline stage
    • Examples:
      - file_reader: min_write=1 (can write single item)
      - cstln_receiver: min_write=128 (needs block of 128)
      - fir_filter: min_write=1 (outputs single sample per decim input)

Cost of pack():
    • memmove() for unread data: O(unread_bytes)
    • Pointer updates: O(nreaders)
    • ~1% CPU overhead (happens ~10x per second)


CIRCULAR BUFFER INVARIANTS:
━━━━━━━━━━━━━━━━━━━━━━━

Always maintained:
    ✓ buf ≤ rds[i] ≤ wr ≤ end (for all i)
    ✓ wr - rds[i] = readable(i) (data available to reader i)
    ✓ end - wr = writable (space for writer)
    ✓ After pack: buf = rd_slowest position before pack


EXAMPLE MEMORY SNAPSHOT: DVB-S Reception
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Steady state at t=10 seconds:

┌─────────────────────────────────────────────────────────────────┐
│ HEAP MEMORY LAYOUT                                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│ 0x0: [Scheduler object: 100 B]                                 │
│      [Runnable objects: 1 KB]                                  │
│      [Pipebuf struct array: 500 B]                             │
│      [Free space: 4 KB]                                        │
│                                                                  │
│ 0x10000: p_rawiq buffer (cf32[16384])                          │
│          ├─ [Consumed: 0-2000]                                │
│          ├─ [Reader1 (preprocessing): 2000-4000]             │
│          ├─ [Reader2 (measurement): 4000-4050]               │
│          ├─ [Writer: 4050]                                   │
│          └─ [Writable: 4050-16384] = 12,334 items           │
│          Memory used: 128 KB                                  │
│                                                                  │
│ 0x30000: p_preprocessed buffer (cf32[16384])                 │
│          ├─ [Consumed: 0-1000]                               │
│          ├─ [Reader (resampler): 1000-3000]                 │
│          ├─ [Writer: 3000]                                   │
│          └─ [Writable: 3000-16384] = 13,384 items           │
│          Memory used: 128 KB                                  │
│                                                                  │
│ 0x50000: p_resampled buffer (cf32[16384])                    │
│          ├─ [Reader (demod): 0-500]                         │
│          ├─ [Writer: 500]                                    │
│          └─ [Writable: 500-16384] = 15,884 items            │
│          Memory used: 128 KB                                  │
│                                                                  │
│ 0x70000: p_symbols buffer (softsymbol[4096])                 │
│          ├─ [Reader (deconv): 0-200]                        │
│          ├─ [Writer: 250]                                    │
│          └─ [Writable: 250-4096] = 3,846 items              │
│          Memory used: 16 KB                                   │
│                                                                  │
│ 0x80000: p_bytes buffer (u8[8192])                           │
│          ├─ [Reader (MPEG sync): 0-512]                     │
│          ├─ [Writer: 1024]                                   │
│          └─ [Writable: 1024-8192] = 7,168 items             │
│          Memory used: 8 KB                                    │
│                                                                  │
│ 0x90000: p_mpegbytes buffer (u8[9792])                       │
│          ├─ [Reader (deinterleaver): 0-2448]               │
│          ├─ [Writer: 2450]                                   │
│          └─ [Writable: 2450-9792] = 7,342 items             │
│          Memory used: 10 KB                                   │
│                                                                  │
│ 0xA0000: Other buffers (monitoring, packets)                 │
│          Memory used: ~2 KB                                    │
│                                                                  │
│ 0xA1000: [Free space for growth]                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

Total heap used: ~200 KB + 2 KB overhead = 202 KB


BUFFER SIZING GUIDELINES:
━━━━━━━━━━━━━━━━━━━━━━

Input throughput:
    Fs = 2.4 MHz (typical satellite)
    Data rate = Fs * sizeof(item)
    = 2.4M * 8 bytes = 19.2 MB/sec (IQ samples)

Minimum buffer to avoid stalls:
    BUF_BASEBAND = 4096 samples
    = 4096 * 8 bytes = 32 KB
    Hold time: 4096 / 2.4M = 1.7 ms

    If I/O is bursty (USB, network):
    Consider input_buffer: adds extra delay

Symbol processing:
    BUF_SYMBOLS = 1024 * buf_factor
    = 1024 * 4 = 4,096 symbols

    Processing rate: Fm / log2(nsymbols) symbols/sec
    = 2M / 2 = 1M symbols/sec (QPSK)
    Hold time: 4096 / 1M = 4 ms

FEC buffers:
    BUF_MPEGBYTES = 2448 bytes minimum
    = one complete deinterleaver block
    (cannot process less due to Forney interleaving)


MEMORY OPTIMIZATION:
━━━━━━━━━━━━━━━━━━

Current strategy (implicit):
    • Use buf_factor=1 for low-latency systems
    • Use buf_factor=4 for bursty I/O
    • Use buf_factor=16 for ultra-low-latency with buffering

    Command line:
    leandvb --buf-factor 1 input.iq    # Minimal memory
    leandvb --buf-factor 16 input.iq   # Max buffering

For embedded systems (e.g., Raspberry Pi):
    • Set --buf-factor 1 (50 KB total)
    • Disable GUI (saves X11 library)
    • No monitoring (saves output buffers)
    • Minimal resampling (ANF only)

    Predicted footprint: ~30 KB + program (~100 KB) = ~130 KB


MULTI-THREADED IMPACT (Future):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Proposed thread-local buffers:

    Thread 1 (Input):
        stdin → p_rawiq → thread-safe queue
        Memory: BUF_BASEBAND (128 KB)

    Thread 2 (Preprocessing):
        Queue 1 → Resampling → Queue 2
        Memory: 2 * BUF_BASEBAND (256 KB)

    Thread 3 (Demodulation):
        Queue 2 → Symbols → Queue 3
        Memory: BUF_BASEBAND + BUF_SYMBOLS (144 KB)

    Thread 4 (FEC):
        Queue 3 → Output → STDOUT
        Memory: BUF_SYMBOLS + BUF_MPEGBYTES (26 KB)

    Total: ~550 KB (vs ~200 KB single-threaded)
    Tradeoff: More memory for parallelism


MEMORY PROFILING:
━━━━━━━━━━━━━━━

Scheduler provides memory dump with --verbose:

    $ leandvb --verbose input.iq 2>&1 | grep -A 100 "buffer memory"

    .rawiq             : 4090/65536  4016 writable (  4090)
    .preprocessed      : 4090/65536  4016 writable (  0  4090)
    .resampled         : 2048/65536  63488 writable (0 2048)
    .symbols           : 0/16384     16384 writable (0 0)
    .bytes             : 512/8192    7680 writable (0 512)
    ...
    Total buffer memory: 234 KiB

    Interpretation:
    • rawiq: 4090 items written, none read yet (=writable)
    • preprocessed: 4090 in first reader, 0 in second → multiple readers
    • symbols: empty (demod not producing yet)
    • bytes: 512 items, producer filling


```

---

## Thread Communication Architecture

### Proposed Multi-Threaded Design for Parallelization

```
┌─────────────────────────────────────────────────────────────────────────┐
│              THREAD COMMUNICATION ARCHITECTURE (PROPOSED)                 │
│         Multi-threaded pipeline with lock-free or mutex-protected queues│
└─────────────────────────────────────────────────────────────────────────┘


CURRENT ARCHITECTURE: Single-Threaded Cooperative
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    ┌────────────────────────────────────────────────────────┐
    │            MAIN THREAD (Single)                         │
    │  while (!done) {                                        │
    │    for each runnable:                                  │
    │      runnable->run()  // process available data        │
    │    if (hash unchanged) break                           │
    │  }                                                      │
    └─────┬──────────────────────────────────────────────────┘
          │
    Sequential execution:
    time:  ├─ Input [2ms]
           ├─ ANF [3ms]
           ├─ Resample [8ms] ← CPU bottleneck
           ├─ Demod [5ms]
           ├─ Viterbi [10ms] ← CPU bottleneck
           ├─ RS Decoder [2ms]
           └─ Output [1ms]

    Total: 31 ms per iteration (single-core execution)


PROPOSED ARCHITECTURE: Multi-Threaded Pipeline
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Phase 2 Parallelization (Primary):

    Thread 1: Input Stage           (I/O Bound)
    Thread 2: Preprocessing         (CPU Bound - 20% load)
    Thread 3: Demodulation          (CPU Bound - 30% load)
    Thread 4: FEC Decoding          (CPU Bound - 40% load)
    Thread 5: Output/Monitoring     (I/O Bound)


Design goal: Overlap stages, reduce total latency


DETAILED ARCHITECTURE:
━━━━━━━━━━━━━━━━━━━━

┌─────────────────────────────────────────────────────────────────────────┐
│                     MULTI-THREADED PIPELINE DESIGN                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────┐         ┌──────────────┐         ┌──────────────┐    │
│  │  STDIN      │         │ Input queue  │         │ Thread 1     │    │
│  │             │────────→│              │────────→│ Input stage  │    │
│  │ IQ samples  │ (file)  │ maxsize=512K │ signal  │              │    │
│  └─────────────┘         └──────────────┘         └────────┬─────┘    │
│                                                            │            │
│    Data: cu8/s8/u16/s16/f32                      Conversion to cf32   │
│    Rate: Fs = 2.4 MHz                            Output: rawiq (cf32) │
│                                                            │            │
│                                                    ┌───────▼──────────┐│
│                                                    │ Preproc queue    ││
│                                                    │ maxsize=512K     ││
│                                                    └───────┬──────────┘│
│                                                            │            │
│  ┌─────────────┐         ┌──────────────┐         ┌──────▼──────┐    │
│  │ Scheduled   │         │ Preproc Q    │         │ Thread 2     │    │
│  │ (ANF, derot)│←────────│              │←────────│ Preproc      │    │
│  │             │         │              │ signal  │              │    │
│  └─────────────┘         └──────────────┘         └────────┬─────┘    │
│                                                            │            │
│    ANF filter (if enabled)                      Resampling (if enabled)
│    Derotation (if enabled)                      Output: preprocessed  │
│    Spectrum measurement                                   (cf32)        │
│    CNR estimation                                         │            │
│                                                    ┌───────▼──────────┐│
│                                                    │ Demod queue      ││
│                                                    │ maxsize=64K      ││
│                                                    └───────┬──────────┘│
│                                                            │            │
│  ┌─────────────┐         ┌──────────────┐         ┌──────▼──────┐    │
│  │ Demodulation│         │ Demod Q      │         │ Thread 3     │    │
│  │             │←────────│              │←────────│ Demod        │    │
│  │ (Costas PLL)│         │              │ signal  │              │    │
│  └─────────────┘         └──────────────┘         └────────┬─────┘    │
│                                                            │            │
│    cstln_receiver                                 Symbol timing & PLL  │
│    Output: softsymbols                           Output: symbols (SS) │
│    Measurements: freq, SS, MER                             │          │
│    Feedback: freq_tap                                      │          │
│                                                    ┌───────▼──────────┐│
│                                                    │ FEC queue        ││
│                                                    │ maxsize=16K      ││
│                                                    └───────┬──────────┘│
│                                                            │            │
│  ┌─────────────┐         ┌──────────────┐         ┌──────▼──────┐    │
│  │ FEC Decoding│         │ FEC Q        │         │ Thread 4     │    │
│  │             │←────────│              │←────────│ FEC          │    │
│  │ (Viterbi)   │         │              │ signal  │              │    │
│  └─────────────┘         └──────────────┘         └────────┬─────┘    │
│                                                            │            │
│    deconvol_sync / viterbi                       MPEG sync detection  │
│    mpeg_sync (0x47 pattern)                      Deinterleaving       │
│    deinterleaver (Forney I=12)                   RS error correction   │
│    rs_decoder (RS(204,188))                      Output: MPEG-TS      │
│    derandomizer (PRBS-15)                                 │            │
│                                                    ┌───────▼──────────┐│
│                                                    │ Output queue     ││
│                                                    │ maxsize=64K      ││
│                                                    └───────┬──────────┘│
│                                                            │            │
│  ┌─────────────┐         ┌──────────────┐         ┌──────▼──────┐    │
│  │  STDOUT     │←────────│ Output Q     │←────────│ Thread 5     │    │
│  │             │ (file)  │              │ signal  │ Output       │    │
│  │ MPEG-TS     │         │              │         │              │    │
│  └─────────────┘         └──────────────┘         └──────────────┘    │
│                                                                          │
│    188-byte packets                               Monitoring outputs:  │
│    Rate: Fm/8/204 packets/sec                     - Frequency offset   │
│                                                   - Signal strength     │
│                                                   - MER, CNR           │
│                                                   - Lock status        │
│                                                   - Viterbi BER        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘


INTER-THREAD QUEUE DESIGN:
━━━━━━━━━━━━━━━━━━━━━━━━

Options:
    1. Mutex-Protected Queue (simple, slower)
    2. Lock-Free Ring Buffer (faster, complex)
    3. Boost.Lockfree (production-ready)


Option 1: MUTEX-PROTECTED QUEUE
───────────────────────────────

    template<typename T>
    class ThreadSafePipeBuffer {
    private:
        std::deque<T> queue;
        std::mutex mtx;
        std::condition_variable not_empty;
        std::condition_variable not_full;
        size_t max_size;
        bool closed;

    public:
        ThreadSafePipeBuffer(size_t max) : max_size(max), closed(false) {}

        bool try_write(const T &item, int timeout_ms = 0) {
            std::unique_lock<std::mutex> lock(mtx);

            if (timeout_ms == 0) {
                // Non-blocking
                if (queue.size() >= max_size) return false;
            } else {
                // Blocking with timeout
                if (!not_full.wait_for(lock,
                    std::chrono::milliseconds(timeout_ms),
                    [this] { return queue.size() < max_size; })) {
                    return false;
                }
            }

            queue.push_back(item);
            not_empty.notify_one();
            return true;
        }

        bool try_read(T &item, int timeout_ms = 0) {
            std::unique_lock<std::mutex> lock(mtx);

            if (timeout_ms == 0) {
                // Non-blocking
                if (queue.empty()) return false;
            } else {
                // Blocking with timeout
                if (!not_empty.wait_for(lock,
                    std::chrono::milliseconds(timeout_ms),
                    [this] { return !queue.empty(); })) {
                    return false;
                }
            }

            item = queue.front();
            queue.pop_front();
            not_full.notify_one();
            return true;
        }

        size_t size() const {
            std::unique_lock<std::mutex> lock(mtx);
            return queue.size();
        }
    };

    Usage:
        ThreadSafePipeBuffer<cf32> rawiq_queue(512*1024);  // 512 K items

        // Thread 1:
        rawiq_queue.write(samples[i]);  // Blocks if full

        // Thread 2:
        cf32 sample = rawiq_queue.read();  // Blocks if empty


Option 2: LOCK-FREE RING BUFFER
────────────────────────────────

    template<typename T>
    class LockFreeRingBuffer {
    private:
        std::vector<T> buffer;
        std::atomic<size_t> write_pos;
        std::atomic<size_t> read_pos;
        size_t mask;

    public:
        LockFreeRingBuffer(size_t size) : mask(size - 1) {
            assert((size & (size - 1)) == 0);  // Power of 2
            buffer.resize(size);
            write_pos = 0;
            read_pos = 0;
        }

        bool try_write(const T &item) {
            size_t wpos = write_pos.load(std::memory_order_relaxed);
            size_t next_wpos = (wpos + 1) & mask;
            size_t rpos = read_pos.load(std::memory_order_acquire);

            if (next_wpos == rpos) return false;  // Full

            buffer[wpos] = item;
            write_pos.store(next_wpos, std::memory_order_release);
            return true;
        }

        bool try_read(T &item) {
            size_t rpos = read_pos.load(std::memory_order_relaxed);
            size_t wpos = write_pos.load(std::memory_order_acquire);

            if (rpos == wpos) return false;  // Empty

            item = buffer[rpos];
            read_pos.store((rpos + 1) & mask, std::memory_order_release);
            return true;
        }
    };

    Advantages: No locks, very fast
    Disadvantages: Can't block, producer must handle backpressure


BACKPRESSURE HANDLING:
━━━━━━━━━━━━━━━━━━━━

Scenario: Input too fast, preprocessing can't keep up

    Thread 1 (Input):            Thread 2 (Preproc):
    while (!eof) {               while (running) {
        read_from_input();         if (rawiq_queue.try_read(block)) {
        if (!rawiq_queue.try_write(block)) {  process(block);
            sleep(1ms);  ← Back off  output_queue.write(result);
        }                        }
    }


Strategies:
    1. Adaptive sleep: increase sleep duration if queue full
    2. Drop frames: skip if queue at capacity (for real-time)
    3. Producer slowdown: read() at slower rate (buffering)


SYNCHRONIZATION PATTERNS:
━━━━━━━━━━━━━━━━━━━━━━━

All threads have barrier at end:

    main() {
        launch_thread(1, input_thread);
        launch_thread(2, preproc_thread);
        launch_thread(3, demod_thread);
        launch_thread(4, fec_thread);
        launch_thread(5, output_thread);

        wait_for_all_threads();  // Implicit when joined
    }


Shutdown sequence:

    1. Input thread: EOF detected → close rawiq_queue
    2. Preproc thread: rawiq_queue empty + closed → close preproc_queue
    3. Demod thread: demod_queue empty + closed → close fec_queue
    4. FEC thread: fec_queue empty + closed → close output_queue
    5. Output thread: output_queue empty + closed → exit

    Main thread: join all (all queues closed, all threads done)


THREAD PINNING (NUMA-AWARE):
━━━━━━━━━━━━━━━━━━━━━━━━━━━

For optimal performance on multi-socket systems:

    struct ThreadConfig {
        int thread_id;
        cpu_set_t cpuset;
        int numa_node;
    };

    configs[0] = {id=1, cpu=0-1, numa=0};  // Input on socket 0
    configs[1] = {id=2, cpu=2-3, numa=0};  // Preproc on socket 0
    configs[2] = {id=3, cpu=4-5, numa=1};  // Demod on socket 1
    configs[3] = {id=4, cpu=6-7, numa=1};  // FEC on socket 1
    configs[4] = {id=5, cpu=8-9, numa=1};  // Output on socket 1

    // In thread:
    CPU_ZERO(&cpuset);
    for (int cpu = config.cpu_start; cpu < config.cpu_end; ++cpu)
        CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);


PERFORMANCE EXPECTATIONS:
━━━━━━━━━━━━━━━━━━━━━━

Single-threaded:
    Fs=2.4 MHz, input throughput = 19.2 MB/s
    Sustained on x86 at 100% CPU, Ryzen 5900X (5 GHz)

Multi-threaded (4 cores):
    ├─ Core 1: Input + Preproc (I/O + light CPU)
    ├─ Core 2: Demod (heavy CPU ~30%)
    ├─ Core 3: FEC (heavy CPU ~40%)
    └─ Core 4: Output (light CPU + I/O)

    Expected speedup: 2.5-3x (due to pipeline parallelism)
    Max throughput: 2.4 MHz → 3.6 MHz (Fs scaling)


LATENCY COMPARISON:
━━━━━━━━━━━━━━━━━

Single-threaded:
    Iteration 1-50: pipeline fill (each iteration is one stage working)
    Total: ~50 iterations * 10 ms = 500 ms until MPEG-TS output

Multi-threaded:
    Iteration 1: Thread 1 fills input queue
    Iteration 2: Thread 2 fills preproc queue (threads 1,2 parallel)
    Iteration 3: Thread 3 fills demod queue (threads 1,2,3 parallel)
    ...
    After N stages: only N time units to fill entire pipeline

    Total: ~5-10 stages * 10 ms = 50-100 ms (5-10x lower!)


QUEUE SIZE TUNING:
━━━━━━━━━━━━━━━━

    rawiq_queue:    512 K items × 8 B = 4 MB (8 ms buffer @ 2.4 MHz)
    preproc_queue:  512 K items × 8 B = 4 MB
    demod_queue:     64 K items × 4 B = 256 KB (100 μs buffer)
    fec_queue:       16 K items × 1 B = 16 KB (7 μs buffer)
    output_queue:    64 K items × 1 B = 64 KB

    Total queue memory: ~8-10 MB (reasonable for modern systems)


DEADLOCK PREVENTION:
━━━━━━━━━━━━━━━━━

Rule: Always use timeout on condition variable waits

    ✓ SAFE:
      cond.wait_for(lock, timeout, [this] { return !empty; });

    ✗ UNSAFE (can deadlock):
      cond.wait(lock, [this] { return !empty; });


Graceful shutdown:
    set_shutdown_flag();
    signal_all_threads();
    wait_for_threads();


CPU/MEMORY USAGE PROFILE:
━━━━━━━━━━━━━━━━━━━━━━

Single-threaded:
    CPU: 1 core @ 100%
    Memory: 200 KB buffers + 500 KB queues (program-relative)
    IPC: Poor (data flows through cache hierarchy)

Multi-threaded:
    CPU: 3-4 cores @ 80-100% each
    Memory: 200 KB buffers + 10 MB inter-thread queues
    IPC: Better (producer/consumer can overlap)
    Context switch overhead: ~5% (frequent)


IMPLEMENTATION ROADMAP:
━━━━━━━━━━━━━━━━━━━━━

Phase 1 (Completed): Single-threaded baseline
    - Framework in place
    - All algorithms proven
    - Scheduler works

Phase 2 (Proposed): Multi-threaded pipeline
    - ~2-3 weeks implementation
    - 5 thread pools
    - Mutex-protected queues
    - Expected 2.5x speedup

Phase 3 (Future): Lock-free optimization
    - ~2 weeks optimization
    - Replace mutexes with atomics
    - Expected 3x speedup on 4+ cores

Phase 4 (Future): NUMA awareness
    - Thread pinning
    - NUMA-local buffers
    - Expected 10-20% improvement on large systems


TESTING STRATEGY:
━━━━━━━━━━━━━━━━

Unit tests for each queue type:
    test_mutex_queue()
    test_lockfree_queue()
    test_backpressure()

Integration tests:
    test_all_threads_running()
    test_graceful_shutdown()
    test_deadlock_timeout()

Stress tests:
    run_with_high_load()
    measure_latency()
    check_memory_leaks()


```

---

## Summary and Key Insights

This comprehensive documentation covers:

### 1. **Signal Processing Chain (20 Stages)**
- Complete IQ sample-to-MPEG-TS pipeline with data types, buffer sizes
- Each stage documented with input/output formats, CPU complexity, feedback connections
- Detailed data type transformations: cu8→cf32→softsymbol→u8→tspacket

### 2. **Pipebuf Multi-Reader FIFO**
- Circular buffer with automatic compaction/packing
- Support for up to 8 readers with independent read pointers
- Writer triggers pack when approaching buffer end
- Memory-efficient: slowest reader determines reclamation point

### 3. **Scheduler Fixpoint Loop**
- Cooperative scheduling: runs all runnables until hash stabilizes
- Non-blocking design: each runnable processes available data
- Fixpoint = no progress made = all data processed/EOF reached
- Typical convergence: 100-1000 iterations depending on input size

### 4. **Feedback Loops**
- `freq_tap` pointer connects resampler and demodulator
- Enables Doppler tracking without external tuning
- Frequency estimate flows from demod → resampler → CNR estimator
- One-iteration lag acceptable due to slow frequency changes

### 5. **Buffer Memory Layout**
- Typical ~200 KB for buf_factor=4 (4096*buf_factor base size)
- Scales linearly: buf_factor=1 (50 KB), buf_factor=16 (800 KB)
- Automatic packing recovers memory from consumed regions
- Multiple readers extend buffer lifetime

### 6. **Proposed Multithreaded Architecture**
- 5-thread pipeline: Input → Preproc → Demod → FEC → Output
- Mutex-protected or lock-free inter-thread queues (8-10 MB total)
- Expected 2.5-3x speedup on 4-core systems
- Reduces latency from 500ms → 50ms through parallelism

---

**Document complete.** All diagrams include data types, buffer sizes, processing stages, and architectural details for understanding leandvb's sophisticated dataflow-based SDR architecture.
