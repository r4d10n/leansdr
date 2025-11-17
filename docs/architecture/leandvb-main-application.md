# LeanDVB Main Application Analysis (src/apps/leandvb.cc)

## Executive Summary

`leandvb.cc` is the main DVB-S/S2 receiver application implementing a complete digital television demodulation pipeline. It uses a **dataflow programming model** with a single-threaded **pull-based scheduler** that iteratively processes data through connected pipeline stages until reaching a stable state.

**Key Characteristics:**
- **Single-threaded architecture** using cooperative multitasking
- **Synchronous pipeline execution** via scheduler fixpoint iteration
- **Buffer-based inter-stage communication** with multiple readers per buffer
- **Two operating modes**: Standard mode (full preprocessing) and High-Speed mode (optimized for QPSK FEC1/2)

---

## 1. Command-Line Interface and Configuration

### Configuration Structure (Lines 43-136)

The `config` struct encapsulates all runtime parameters:

```cpp
struct config {
  // Debugging & performance
  bool verbose, debug, debug2;
  bool highspeed;
  
  // Input configuration
  enum { INPUT_U8, INPUT_S8, INPUT_U16, INPUT_S16, INPUT_F32 } input_format;
  float float_scale;
  bool loop_input;
  int input_buffer;
  int buf_factor;  // Buffer size multiplier
  
  // Signal processing
  float Fs;           // Sampling frequency
  float Fderot;       // Frequency shift
  int anf;            // Auto-notch filter count
  bool cnr;           // CNR measurement
  unsigned int decim; // Decimation factor
  float awgn;         // Noise injection
  
  // DVB parameters
  float Fm;                          // Symbol rate
  enum dvb_version standard;         // DVB-S or DVB-S2
  cstln_lut<256>::predef constellation;
  code_rate fec;
  float Ftune;                       // Frequency bias
  bool allow_drift;
  bool fastlock;
  bool viterbi;
  bool hard_metric;
  
  // Resampling
  bool resample;
  float resample_rej;
  enum sampler_type { SAMP_NEAREST, SAMP_LINEAR, SAMP_RRC };
  int rrc_steps;
  float rrc_rej;
  float rolloff;
  
  // Output & monitoring
  bool hdlc, packetized;
  bool gui;
  float duration;
  bool linger;
  int fd_info, fd_const, fd_spectrum;
  float Finfo;
  bool json;
};
```

### Command-Line Parsing (Lines 1061-1221)

Simple imperative parsing using `strcmp`:
- Single-letter flags: `-h`, `-v`, `-d`
- Long options: `--sr`, `--cr`, `--const`, etc.
- Preset configurations:
  - `--hq`: High quality (enables fastlock, viterbi, RRC)
  - `--hs`: High speed (minimal preprocessing, FEC1/2 only)

---

## 2. Pipeline Construction and Component Instantiation

### Scheduler Initialization (Lines 163-178)

```cpp
scheduler sch;
sch.verbose = cfg.verbose;
sch.debug = cfg.debug;
sch.windows = window_hints;  // GUI window positions
```

The scheduler is a **global coordinator** that:
1. Maintains lists of all pipebuf and runnable objects
2. Iteratively calls `run()` on each runnable
3. Computes hash of all pipeline states
4. Terminates when hash stabilizes (fixpoint reached)

### Pipeline Stage Construction Pattern

Each processing stage follows this pattern:

```cpp
// 1. Create output buffer
pipebuf<OutputType> *p_output = new pipebuf<OutputType>(&sch, "name", size);

// 2. Instantiate processor
ProcessorType *r_processor = new ProcessorType(&sch, *p_input, *p_output);

// 3. Configure processor
r_processor->parameter = value;

// 4. Connect to next stage
p_input = p_output;  // For linear pipelines
```

**Important:** Components self-register with the scheduler in their constructors via `sch->add_runnable(this)` and `sch->add_pipe(this)`.

---

## 3. Complete Data Flow Pipeline (Standard Mode)

### Stage 1: INPUT CONVERSION (Lines 204-262)

```
STDIN → file_reader<cu8/cs8/cu16/cs16/cf32> → cconverter → p_rawiq (cf32)
```

**Purpose:** Read raw IQ samples and convert to normalized float32 complex

**Buffer:** `BUF_BASEBAND = 4096 * buf_factor`

**Input Formats:**
- `INPUT_U8`: RTL-SDR format (offset by 128)
- `INPUT_S8`: Signed 8-bit
- `INPUT_U16`: Unsigned 16-bit (offset by 32768)
- `INPUT_S16`: Signed 16-bit (PlutoSDR)
- `INPUT_F32`: Float32 (GQRX), scaled by `float_scale`

### Stage 2: OPTIONAL GUI SCOPES (Lines 263-273)

```
p_rawiq ──┬→ cscope (IQ constellation display)
          └→ spectrumscope (FFT display)
```

### Stage 3: NOISE INJECTION (Lines 277-292)

```
p_rawiq + wgn_c<f32> → adder → p_noisy
```

**Purpose:** Testing - adds white Gaussian noise with configurable stddev

### Stage 4: AUTO-NOTCH FILTER (Lines 294-306)

```
p_preprocessed → auto_notch<f32> → p_autonotched
```

**Purpose:** Remove narrowband interferers (birdies)
**Default:** 1 notch filter
**Buffer requirement:** Minimum 4096 samples

**Critical Note:** ANF is **disabled by default** in high-speed mode as it requires clean signals

### Stage 5: FREQUENCY DEROTATION (Lines 308-318)

```
p_preprocessed → rotator<f32> → p_derot
```

**Purpose:** Shift signal in frequency domain
**Usage:** Primarily for preprocessed data output; `--tune` is preferred for demodulation

### Stage 6: CNR ESTIMATION (Lines 320-329)

```
p_preprocessed → cnr_fft<f32> → p_cnr
```

**Purpose:** Carrier-to-Noise Ratio measurement
**Requirement:** `Fs > 3 * Fm` (sample rate must be 3x symbol rate)
**Update rate:** 1 Hz

### Stage 7: SPECTRUM ANALYZER (Lines 331-343)

```
p_preprocessed → spectrum<f32> → p_spectrum (1024 bins)
```

**Purpose:** Export spectrum data for external monitoring
**Update rate:** 1 Hz
**Averaging:** `kavg = 0.5` (exponential moving average)

### Stage 8: RESAMPLING/DECIMATION (Lines 345-413)

#### Option A: Resample with Anti-Aliasing Filter

```
p_preprocessed → fir_filter<cf32,float> → p_resampled
```

**Purpose:** Low-pass filter + decimation
**Target:** ~4 samples per symbol
**Filter design:**
- Transition band: `(Fm/2) * rolloff`
- Order: `resample_rej * Fs / (22 * transition)`
- Cutoff: `(Fm/2) * (1 + rolloff/2) / Fs`

**Filter characteristics:**
- Even-order FIR
- Normalized DC gain
- Implements both filtering and decimation in one stage

#### Option B: Simple Decimation (No Filtering)

```
p_preprocessed → decimator<cf32> → p_decimated
```

**Warning:** Causes aliasing but faster

**Result:** Updates `cfg.Fs` to reflect new effective sample rate

### Stage 9: PREPROCESSED DATA OUTPUT (Lines 416-423)

```
p_preprocessed → file_writer<cf32> → fd_pp
```

**Purpose:** Export intermediate preprocessed IQ for analysis/replay

### Stage 10: CONSTELLATION RECEIVER (Lines 425-524)

```
p_preprocessed → cstln_receiver<f32> → p_symbols (softsymbol)
                                    ├→ p_freq (carrier frequency estimate)
                                    ├→ p_ss (signal strength)
                                    ├→ p_mer (modulation error ratio)
                                    └→ p_sampled (resampled constellation points)
```

**This is the core demodulator!**

**Key responsibilities:**
1. **Symbol timing recovery** - Determines optimal sampling instants
2. **Carrier phase tracking** - Corrects residual frequency/phase offset
3. **Symbol decision** - Maps received samples to constellation points
4. **Soft-decision output** - Generates likelihood values for FEC decoder

**Sampler Types:**
- `SAMP_NEAREST`: Nearest neighbor (fastest, lowest quality)
- `SAMP_LINEAR`: Linear interpolation (good balance)
- `SAMP_RRC`: Root-raised-cosine matched filter (best quality, CPU-intensive)

**Critical parameters:**
- `omega = Fs/Fm`: Samples per symbol
- `freq`: Frequency offset bias (`Ftune/Fs`)
- `min_freqw/max_freqw`: Frequency tracking limits
- `pll_adjustment`: Phase-locked loop gain (reduced for low SNR)
- `meas_decimation`: Measurement output rate

**Constellation object:**
- Lookup table with symbol coordinates and soft-decision metrics
- Created by `make_dvbs2_constellation()`
- Can be hardened for hard-decision decoding

**Buffer sizes:**
- Input: `BUF_BASEBAND = 4096 * buf_factor`
- Output symbols: `BUF_SYMBOLS = 1024 * buf_factor`
- Reads in chunks of 128+1 samples
- Writes in chunks of 128/omega symbols

### Stage 11: TRACKING FILTER FEEDBACK (Lines 504-515)

```cpp
if (r_resample) {
  r_resample->freq_tap = &demod.freq_tap;
  r_resample->tap_multiplier = 1.0 / decim;
}

if (r_cnr) {
  r_cnr->freq_tap = &demod.freq_tap;
}
```

**Purpose:** Frequency-tracking filter - adjusts resampler center frequency based on demodulator's carrier estimate

**This creates a feedback loop for Doppler compensation**

### Stage 12: DECONVOLUTION AND SYNCHRONIZATION (Lines 526-566)

#### Option A: Viterbi Decoder

```
p_symbols → viterbi_sync → p_bytes
```

**Characteristics:**
- Maximum-likelihood sequence estimation
- CPU-intensive but better performance at low SNR
- Rate 2/3 handled as 4/6 for compatibility

#### Option B: Algebraic Deconvolver (Default)

```
p_symbols → deconvol_sync_simple → p_bytes
```

**Characteristics:**
- Faster than Viterbi
- Factory function `make_deconvol_sync_simple()` selects implementation based on FEC rate
- `fastlock` mode: Resynchronize every symbol (aggressive but CPU-intensive)

**Buffer:** `BUF_BYTES = 2048 * buf_factor`
- Writes in chunks of 32 bytes
- Reads require up to 1632 bytes (for sync detection)

#### Alternative Path: HDLC Mode (Lines 546-556)

```
p_bytes → etr192_descrambler → hdlc_sync → file_writer
```

**Purpose:** Handle HDLC-framed data instead of MPEG transport stream
- ETR 192 descrambling
- HDLC frame synchronization (2-278 byte frames)
- Optional 16-bit header for packetized output
- **Early exit** - bypasses RS decoding and MPEG sync

### Stage 13: MPEG SYNCHRONIZATION (Lines 558-566)

```
p_bytes → mpeg_sync<u8,0> → p_mpegbytes
                          ├→ p_lock (lock status)
                          └→ p_locktime (time in lock)
```

**Purpose:** Find and maintain MPEG-TS packet synchronization
- Searches for 0x47 sync byte pattern
- Scans up to `204 * scan_syncs` bytes
- Outputs lock status and lock time metrics

**Buffer:** `BUF_MPEGBYTES = 2448 * buf_factor`

### Stage 14: DEINTERLEAVING (Lines 568-571)

```
p_mpegbytes → deinterleaver<u8> → p_rspackets
```

**Purpose:** Undo DVB-S Forney interleaving (I=12)
- Reads `17*11*12 + 204 = 2448` bytes
- Outputs 204-byte Reed-Solomon packets

**Buffer:** `BUF_PACKETS = buf_factor`

### Stage 15: REED-SOLOMON ERROR CORRECTION (Lines 573-579)

```
p_rspackets → rs_decoder<u8,0> → p_rtspackets
                                ├→ p_vbitcount (bits processed)
                                └→ p_verrcount (bits corrected)
```

**Purpose:** RS(204,188) error correction
- Can correct up to 8 byte errors per packet
- Outputs corrected packet + error statistics

### Stage 16: BER ESTIMATION (Lines 581-587)

```
p_verrcount, p_vbitcount → rate_estimator<float> → p_vber
```

**Purpose:** Calculate Viterbi Bit Error Rate
- Sample size: `max(Fm/2, 50000)` bits
- Provides about 2 measurements per second
- Resolution better than 2E-5

### Stage 17: DERANDOMIZATION (Lines 589-592)

```
p_rtspackets → derandomizer → p_tspackets
```

**Purpose:** Remove DVB-S energy dispersal scrambling (PRBS)

### Stage 18: OUTPUT (Lines 594-596)

```
p_tspackets → file_writer<tspacket> → STDOUT
```

**Final output:** Standard MPEG-TS packets (188 bytes each)

### Stage 19: AUXILIARY OUTPUTS (Lines 598-656)

Multiple monitoring outputs written to file descriptors:

```
p_freq       → file_printer → fd_info  (FREQ Hz)
p_ss         → file_printer → fd_info  (SS)
p_mer        → file_printer → fd_info  (MER dB)
p_lock       → file_printer → fd_info  (LOCK 0/1)
p_locktime   → file_printer → fd_info  (LOCKTIME samples)
p_cnr        → file_printer → fd_info  (CNR dB)
p_vber       → file_printer → fd_info  (VBER)
p_sampled    → file_carrayprinter → fd_const  (SYMBOLS [I,Q])
p_spectrum   → file_vectorprinter → fd_spectrum  (SPECTRUM [1024 values])
```

**Format options:**
- Text format (default)
- JSON format (`--json`)

### Stage 20: TIMELINE GUI (Lines 658-698)

```
Multi-channel oscilloscope displaying:
  - Frequency offset (cyan)
  - Signal strength (red)
  - MER (magenta)
  - CNR (yellow)
  - TS packet recovery rate (yellow)
```

**Configuration:**
- Sample frequency: `Fs / meas_decimation`
- Time span: `duration` seconds (default 60s)
- Asynchronous channels for CNR and packet count

---

## 4. Complete Data Flow Pipeline (High-Speed Mode)

High-speed mode (`--hs`, lines 727-969) is an **optimized path for QPSK FEC1/2** with minimal preprocessing:

### Simplified Pipeline

```
STDIN (u8 only)
  ↓
file_reader<cu8> → p_rawiq
  ↓
fast_qpsk_receiver<u8> → p_symbols (u8 hard symbols)
  │                    ├→ p_freq
  │                    └→ p_sampled
  ↓
dvb_deconvol_sync_hard → p_bytes
  ↓
mpeg_sync → p_mpegbytes
  ↓
deinterleaver → p_rspackets
  ↓
rs_decoder → p_rtspackets
  ↓
derandomizer → p_tspackets
  ↓
file_writer → STDOUT
```

**Key differences from standard mode:**
1. **No format conversion**: Works directly with u8 samples
2. **No preprocessing**: No ANF, derotation, resampling, or CNR
3. **Fixed-point demodulator**: `fast_qpsk_receiver` instead of `cstln_receiver`
4. **Hard-decision decoding**: `dvb_deconvol_sync_hard` instead of soft-decision
5. **Fixed parameters**: Only QPSK, only FEC1/2
6. **Reduced monitoring**: Only frequency, lock status, VBER

**Performance benefit:** Eliminates floating-point conversions and complex preprocessing

---

## 5. Buffer Sizing and Memory Management

### Buffer Size Calculation (Lines 180-203)

All buffer sizes are multiplied by `buf_factor` (default 4, configurable with `--buf-factor`):

```cpp
unsigned long BUF_BASEBAND   = 4096 * cfg.buf_factor;  // IQ samples
unsigned long BUF_SYMBOLS    = 1024 * cfg.buf_factor;  // Soft symbols
unsigned long BUF_BYTES      = 2048 * cfg.buf_factor;  // Unsynchronized bytes
unsigned long BUF_MPEGBYTES  = 2448 * cfg.buf_factor;  // Synchronized bytes
unsigned long BUF_PACKETS    = cfg.buf_factor;         // TS packets
unsigned long BUF_SLOW       = cfg.buf_factor;         // Measurements
```

**Design rationale (from comments):**

1. **BUF_BASEBAND (4096 minimum)**
   - Scopes: 1024 samples
   - SS estimator: 1024 samples
   - ANF: 4096 samples (largest requirement)
   - cstln_receiver: reads chunks of 128+1

2. **BUF_SYMBOLS (1024 minimum)**
   - cstln_receiver: writes 128/omega symbols (+ 128 margin)
   - deconv_sync: reads at least 64+32 symbols
   - **Note:** Larger buffer significantly improves performance

3. **BUF_BYTES (2048 minimum)**
   - deconv_sync: writes 32 bytes
   - mpeg_sync: reads up to 1632 bytes (204*8 for sync scanning)

4. **BUF_MPEGBYTES (2448 minimum)**
   - mpeg_sync: writes 1 RS packet (204 bytes)
   - deinterleaver: reads 17*11*12+204 = 2448 bytes (one interleaver frame)

5. **BUF_PACKETS (1 minimum)**
   - One packet is typically sufficient
   - Can increase for bursty processing

### Memory Management Strategy

**Allocation:**
- All buffers allocated with `new T[size]` in pipebuf constructor
- Self-registering: Components add themselves to scheduler

**Deallocation:**
- Automatic via destructors (when scheduler goes out of scope)
- Debug mode prints deallocation messages

**Buffer Packing (framework.h, lines 153-159):**
```cpp
void pack() {
  T *rd = wr;
  for (int i=0; i<nrd; ++i) if (rds[i] < rd) rd = rds[i];
  memmove(buf, rd, (wr-rd)*sizeof(T));
  wr -= rd - buf;
  for (int i=0; i<nrd; ++i) rds[i] -= rd - buf;
}
```

**Purpose:** Compact buffer when approaching end
- Find slowest reader
- Move unread data to beginning
- Update all pointers

**Trigger:** Called in `pipewriter::writable()` when `end - wr < min_write`

### Input Buffering

```cpp
pipebuf<cu8> *p_stdin = new pipebuf<cu8>(&sch, "stdin", 
                                         BUF_BASEBAND + cfg.input_buffer);
```

**Optional extra buffering** via `--inbuf` for handling bursty I/O

---

## 6. Performance Monitoring and Debugging

### Runtime Metrics (Continuously Updated)

1. **FREQ** (p_freq): Carrier frequency offset estimate (Hz)
   - Updated every `meas_decimation` samples
   - Source: cstln_receiver phase-locked loop

2. **SS** (p_ss): Signal strength (arbitrary units)
   - RMS amplitude of received constellation points

3. **MER** (p_mer): Modulation Error Ratio (dB)
   - Measure of constellation point dispersion
   - Similar to SNR but for modulated signal

4. **CNR** (p_cnr): Carrier-to-Noise Ratio (dB)
   - Requires `--cnr` flag
   - FFT-based estimation
   - Needs `Fs > 3*Fm`

5. **LOCK** (p_lock): Synchronization status (0/1)
   - From MPEG sync detector

6. **LOCKTIME** (p_locktime): Samples since lock acquired
   - Counter in sample units

7. **VBER** (p_vber): Viterbi Bit Error Rate
   - Ratio of corrected bits to total bits
   - Resolution ~2E-5

### Debug Output Levels

**Level 0 (default):** No debug output
- Only errors printed to stderr

**Level 1 (`-v`, verbose):**
- Startup configuration
- Component instantiation
- Parameter values
- Exit statistics

**Level 2 (`-d`, debug):**
- Runtime progress indicators:
  - `'_'`: Packet received without errors
  - `'.'`: Error-corrected packet
  - `'!'`: Packet with uncorrectable errors
  - `'^'`: HDLC framing error (HDLC mode)

**Level 3 (`-d -d`, debug2):**
- Filter coefficient dumps
- Internal state

### Scheduler Diagnostics (Lines 715-720)

```cpp
sch.shutdown();
if (cfg.debug) sch.dump();
```

**Scheduler dump output:**
- Per-pipe statistics:
  - Total samples written/read
  - Available write space
  - Unread samples per reader
- Total buffer memory usage

**Example output:**
```
.rawiq           :   15M/  15M   4096 writable  ,      0 unread ( 0 )
.PSK soft-symbols:   10M/  10M   3072 writable  ,    512 unread ( 512 )
Total buffer memory: 2048 KiB
```

### GUI Visualization (ifdef GUI)

**Real-time displays:**
1. **Raw IQ constellation** (256x256)
2. **Raw spectrum** (1024x256)
3. **Preprocessed IQ constellation** (256x256)
4. **Preprocessed spectrum** (1024x256)
5. **Demodulated symbols** (256x256) - overlaid with constellation reference
6. **Timeline** (512x256):
   - Frequency tracking
   - Signal strength
   - MER
   - CNR
   - Packet recovery rate

**Window placement:** Configurable via `window_hints` array (lines 169-178)

---

## 7. Integration of All Components

### Component Communication Model

**Producer-Consumer Pattern:**
```
[Producer runnable] → writes to → [pipebuf] → reads from → [Consumer runnable]
```

**Multiple readers:**
- Each pipebuf can have up to 8 readers (MAX_READERS)
- Separate read pointers maintained for each reader
- Buffer only packed when slowest reader advances

**Optional pipes:**
- Helper functions: `opt_writer()`, `opt_writable()`, `opt_write()`
- Allow conditional pipeline branches

### Scheduler Operation (framework.h, lines 92-104)

```cpp
void run() {
  unsigned long long prev_hash = 0;
  while (1) {
    step();  // Call run() on all runnables
    unsigned long long h = hash();  // Sum of all pipe activity
    if (h == prev_hash) break;  // Fixpoint reached
    prev_hash = h;
  }
}
```

**Fixpoint iteration algorithm:**
1. Execute all runnables once (in registration order)
2. Compute system hash (sum of all bytes written/read)
3. If hash unchanged, no progress was made → terminate
4. Otherwise, repeat

**Termination conditions:**
- Input exhausted (file_reader has no more data)
- All buffers drained
- No runnable can make progress

**Efficiency consideration:**
- Runnables should implement "do as much as possible" logic in their `run()` method
- Each runnable checks input availability and output space
- Returns immediately if can't make progress

### Data Type Flow

```
Raw IQ samples (u8/s8/u16/s16/f32)
  ↓ cconverter
Normalized IQ (cf32 = complex<float>)
  ↓ preprocessing (ANF, derotation, resampling)
Preprocessed IQ (cf32)
  ↓ cstln_receiver
Soft symbols (softsymbol = struct with value + confidence)
  ↓ deconvol_sync / viterbi_sync
Unsynchronized bytes (u8)
  ↓ mpeg_sync
Synchronized MPEG bytes (u8)
  ↓ deinterleaver
RS-encoded packets (rspacket<u8> = array of 204 bytes)
  ↓ rs_decoder
Randomized TS packets (tspacket = array of 188 bytes)
  ↓ derandomizer
Clean TS packets (tspacket)
  ↓ file_writer
STDOUT
```

### Cross-Component Dependencies

**Feedback loops:**
1. **Carrier tracking → Resampler**
   - `r_resample->freq_tap = &demod.freq_tap`
   - Resampler adjusts center frequency based on demodulator's carrier estimate

2. **Carrier tracking → CNR estimator**
   - `r_cnr->freq_tap = &demod.freq_tap`
   - CNR measurement compensates for frequency offset

**Forward references:**
- `mpeg_sync` takes pointer to `deconvol_sync` for error statistics
- Allows MPEG sync to monitor deconvolver health

---

## 8. Main Processing Loop Structure

### Program Flow

```cpp
int main(int argc, const char *argv[]) {
  // 1. Parse configuration
  config cfg;
  for (int i=1; i<argc; ++i) { /* parse argv */ }
  
  // 2. Select mode
  if (cfg.highspeed)
    return run_highspeed(cfg);
  else
    return run(cfg);
}
```

### Standard Mode Run Function (Lines 157-724)

```cpp
int run(config &cfg) {
  // 1. Create scheduler
  scheduler sch;
  sch.verbose = cfg.verbose;
  sch.debug = cfg.debug;
  
  // 2. Define buffer sizes
  unsigned long BUF_BASEBAND = 4096 * cfg.buf_factor;
  // ... (other buffer sizes)
  
  // 3. Build pipeline (in order from input to output)
  //    - INPUT
  //    - GUI scopes (if enabled)
  //    - Noise injection (if enabled)
  //    - Auto-notch filter (if enabled)
  //    - Frequency derotation (if enabled)
  //    - CNR estimation (if enabled)
  //    - Spectrum analyzer (if enabled)
  //    - Resampling/decimation (if enabled)
  //    - Constellation receiver (always)
  //    - Deconvolution (always)
  //    - MPEG sync or HDLC sync
  //    - Deinterleaving (if not HDLC)
  //    - Reed-Solomon (if not HDLC)
  //    - BER estimation (if not HDLC)
  //    - Derandomization (if not HDLC)
  //    - OUTPUT
  //    - Auxiliary outputs (if configured)
  //    - Timeline GUI (if enabled)
  
  // 4. Run scheduler
  sch.run();  // Process until fixpoint
  
  // 5. Cleanup
  sch.shutdown();
  if (cfg.debug) sch.dump();
  
  // 6. Optional GUI linger
  if (cfg.gui && cfg.linger) {
    while (1) {
      sch.run();
      usleep(10000);  // 10ms
    }
  }
  
  return 0;
}
```

### Scheduler Execution Model

**Single-threaded cooperative multitasking:**

```
┌─────────────────────────────────────┐
│ Scheduler Main Loop                 │
│                                     │
│  while (hash changing) {            │
│    for each runnable:               │
│      runnable->run()  ◄────┐       │
│  }                          │       │
└─────────────────────────────┼───────┘
                              │
                        ┌─────┴──────┐
                        │ Runnable   │
                        │            │
                        │ run() {    │
                        │   if (!input.readable()) return;   │
                        │   if (!output.writable()) return;  │
                        │   // Do work                       │
                        │   input.read(n);                   │
                        │   output.written(m);               │
                        │ }                                  │
                        └────────────────────────────────────┘
```

**Key properties:**
- **Cooperative:** Each runnable yields voluntarily
- **Non-preemptive:** No interrupts or context switches
- **Deterministic:** Same execution order each iteration
- **Synchronous:** All processing in main thread

**No explicit synchronization needed:**
- No mutexes, locks, or atomic operations
- No race conditions
- Simpler debugging

---

## 9. Thread Architecture Analysis

### Current Architecture: Single-Threaded

**Design characteristics:**
```
┌──────────────────────────────────────────────────┐
│                  Main Thread                      │
│                                                   │
│  INPUT → PREPROC → DEMOD → FEC → OUTPUT          │
│   │       │         │       │      │             │
│   └───────┴─────────┴───────┴──────┘             │
│        Sequential execution                       │
└──────────────────────────────────────────────────┘
```

**Advantages:**
1. **Simple implementation** - no threading complexity
2. **Easy debugging** - deterministic execution
3. **No synchronization overhead** - no locks/atomics
4. **Cache-friendly** - sequential memory access

**Disadvantages:**
1. **Single-core utilization** - cannot leverage multi-core CPUs
2. **Pipeline stalls** - slow stage blocks entire pipeline
3. **Bursty performance** - variable processing latency

### Parallelization Opportunities

#### Strategy 1: Pipeline Parallelism (Recommended)

**Convert linear pipeline to multi-threaded pipeline:**

```
┌─────────┐      ┌──────────┐      ┌─────────┐      ┌────────┐
│ Thread1 │      │ Thread2  │      │ Thread3 │      │Thread4 │
│         │      │          │      │         │      │        │
│ INPUT   │──Q1─→│ PREPROC  │──Q2─→│ DEMOD   │──Q3─→│  FEC   │──Q4──→ OUTPUT
│ Convert │      │ ANF/RRC  │      │ Timing  │      │ RS/    │
│         │      │ Filter   │      │ Carrier │      │ Deint  │
└─────────┘      └──────────┘      └─────────┘      └────────┘

Q1-Q4: Thread-safe lock-free queues (or mutex-protected pipebuf)
```

**Pipeline stage grouping:**

**Thread 1: Input + Conversion (I/O bound)**
- `file_reader`
- Format conversion (`cconverter`)
- Minimal CPU, mostly blocked on I/O

**Thread 2: Preprocessing (CPU bound)**
- Noise injection (if enabled)
- Auto-notch filter
- Frequency derotation
- Resampling/decimation (FIR filter - most expensive!)
- **Bottleneck:** FIR filtering with high-order filters

**Thread 3: Demodulation (CPU bound)**
- `cstln_receiver` - timing recovery, carrier tracking, symbol decisions
- **Bottleneck:** PLL updates, interpolation
- **Hot loop:** Symbol sampling at 2-4 MSps

**Thread 4: Forward Error Correction (CPU bound)**
- Deconvolution (Viterbi or algebraic)
- MPEG sync
- Deinterleaving
- Reed-Solomon decoding
- Derandomization
- **Bottleneck:** Viterbi decoder

**Thread 5: Output (I/O bound)**
- `file_writer`
- Auxiliary outputs (monitoring)

**Inter-thread communication:**
- Replace single-reader `pipebuf` with thread-safe queue
- Use lock-free ring buffers (e.g., Boost.Lockfree)
- Or add mutex protection to existing `pipebuf`

**Code changes required:**
```cpp
// Replace:
pipebuf<cf32> p_preprocessed(&sch, "preprocessed", BUF_BASEBAND);

// With:
ThreadSafePipeBuf<cf32> p_preprocessed("preprocessed", BUF_BASEBAND);
```

**Each thread runs:**
```cpp
void thread_func(Runnable *r, ThreadSafePipeBuf *in, ThreadSafePipeBuf *out) {
  while (!terminate) {
    r->run();
    if (no progress) usleep(100);  // Backoff
  }
}
```

**Expected speedup:** 2-3x on 4+ core systems
- Limited by slowest stage (typically preprocessing or Viterbi)
- Amdahl's law applies

#### Strategy 2: Data Parallelism (For High-Throughput)

**Process multiple independent streams in parallel:**

```
INPUT (multichannel)
  ├─ Thread 1: Channel 1 → Complete pipeline → Output 1
  ├─ Thread 2: Channel 2 → Complete pipeline → Output 2
  ├─ Thread 3: Channel 3 → Complete pipeline → Output 3
  └─ Thread 4: Channel 4 → Complete pipeline → Output 4
```

**Use case:** Multi-channel receiver (e.g., multiple transponders)

**Advantages:**
- Perfect scaling (embarrassingly parallel)
- No inter-thread communication
- Simple implementation

**Disadvantages:**
- Only applicable for multi-stream scenarios
- Memory overhead (N complete pipelines)

#### Strategy 3: Component-Level SIMD (Easy Wins)

**Keep single-threaded, but vectorize hot loops:**

**Target components:**
1. **FIR filter** (`fir_filter` in resampler)
   - Use NEON intrinsics for ARM
   - Process 4 complex samples per iteration
   ```cpp
   float32x4_t v_sample = vld1q_f32(samples);
   float32x4_t v_coeff = vld1q_f32(coeffs);
   float32x4_t v_result = vmulq_f32(v_sample, v_coeff);
   ```

2. **Auto-notch filter** (`auto_notch`)
   - Vectorize complex multiply-accumulate
   - 2x speedup possible

3. **Constellation receiver** (`cstln_receiver`)
   - SIMD for distance calculations
   - Process multiple soft-symbol computations in parallel

4. **Viterbi decoder**
   - SIMD for branch metric calculations
   - Use packed comparisons for path selection

**Expected speedup:** 2-4x for preprocessing stages

**Advantage:** Minimal code changes, no threading complexity

#### Strategy 4: GPU Offload (Advanced)

**Offload compute-intensive stages to GPU:**

**Candidates:**
1. **FFT operations** (spectrum, CNR estimation)
   - cuFFT library
   - Massive parallelism

2. **FIR filtering**
   - Overlap-add convolution on GPU
   - Good for large filter orders

3. **Soft-decision calculations**
   - Parallel distance computations

**Challenges:**
- Host↔Device transfer overhead
- Latency concerns for real-time operation
- Complex integration with existing dataflow

**Not recommended for leansdr** - designed for CPU efficiency

### Recommended Parallelization Approach

**Phase 1: Low-hanging fruit (1-2 weeks)**
1. Add NEON intrinsics to FIR filter
2. Vectorize auto-notch filter
3. Expected: 2x speedup for `--resample` mode

**Phase 2: Pipeline parallelism (2-4 weeks)**
1. Make `pipebuf` thread-safe (add mutex)
2. Create thread pool (4 threads)
3. Group stages: [Input], [Preproc], [Demod], [FEC+Output]
4. Test with synthetic data
5. Profile and balance stage loads
6. Expected: 2-3x overall speedup

**Phase 3: Advanced optimizations (optional)**
1. Lock-free queues for inter-thread communication
2. NUMA-aware thread pinning
3. Adaptive thread count based on CPU load
4. Expected: Additional 10-20% improvement

### Critical Challenges

**1. Buffer Management**
- Current: Single producer, multiple readers
- Need: Single producer, single consumer (per thread)
- Solution: One pipebuf per thread boundary

**2. Backpressure Handling**
- Current: Automatic via buffer fullness
- Need: Explicit flow control between threads
- Solution: Blocking writes when queue full

**3. Termination**
- Current: Natural fixpoint when input exhausted
- Need: Explicit termination signal
- Solution: Poison pill pattern or atomic flag

**4. Debugging**
- Current: Sequential, deterministic
- Multithreaded: Non-deterministic, race conditions
- Solution: Per-thread logging, thread sanitizer

**5. Performance Monitoring**
- Need: Per-thread CPU usage
- Need: Inter-thread queue depths
- Solution: Add monitoring probes

### Performance Expectations

**Current (single-threaded):**
- QPSK, no preprocessing: ~20 Msps
- QPSK with resampling: ~5 Msps
- QPSK with Viterbi: ~2 Msps

**After pipeline parallelism (4 cores):**
- QPSK, no preprocessing: ~40 Msps (2x, I/O limited)
- QPSK with resampling: ~12 Msps (2.4x)
- QPSK with Viterbi: ~5 Msps (2.5x)

**After SIMD optimization (4 cores):**
- QPSK with resampling: ~25 Msps (5x)
- QPSK with Viterbi: ~8 Msps (4x)

---

## Summary: Critical Parallelization Points

### High-Value Targets (Ranked by Impact)

1. **FIR Resampler** (Lines 349-384)
   - Convolution is embarrassingly parallel
   - NEON: 4x float operations per instruction
   - Thread-level: Process blocks in parallel
   - **Expected gain:** 3-4x

2. **Viterbi Decoder** (Lines 532-544)
   - Branch metric calculation vectorizable
   - Path storage and traceback sequential
   - **Expected gain:** 2-3x

3. **Constellation Receiver** (Lines 425-503)
   - Timing loop and PLL inherently sequential
   - But: Soft-decision calculations parallel
   - **Expected gain:** 1.5-2x

4. **Auto-Notch Filter** (Lines 294-306)
   - Adaptive filter updates parallel
   - State updates sequential
   - **Expected gain:** 2x

5. **Reed-Solomon Decoder** (Lines 573-579)
   - Syndrome calculation parallel
   - Error correction sequential
   - **Expected gain:** 1.5x

### Low-Value Targets (Not worth parallelizing)

1. **Format converters** - Memory-bound, not compute-bound
2. **MPEG sync** - Scan operation is fast
3. **Derandomizer** - Simple XOR, already fast
4. **Deinterleaver** - Memory copy, not CPU-bound

---

## Conclusion

`leandvb.cc` implements a sophisticated DVB-S receiver using a **clean dataflow architecture** with **single-threaded execution**. The design prioritizes **simplicity and maintainability** over raw performance, making it ideal for understanding SDR principles.

**Key strengths:**
- Clear separation of concerns
- Highly modular and configurable
- Easy to debug and extend
- Portable (no platform-specific code in main pipeline)

**Parallelization potential is significant** (3-5x speedup possible), primarily through:
1. **SIMD vectorization** of compute kernels (quick win)
2. **Pipeline parallelism** for multi-core scaling (moderate effort)
3. **Data parallelism** for multi-stream scenarios (application-specific)

The current architecture makes parallelization feasible without major redesign, as the component boundaries are well-defined and data dependencies are explicit.
