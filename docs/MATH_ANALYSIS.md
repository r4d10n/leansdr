# LeanSDR Math.h - Comprehensive Analysis

## File Location
`/home/user/leansdr/src/leansdr/math.h` (116 lines)

---

## 1. COMPLEX NUMBER OPERATIONS

### Basic Structure (lines 26-33)
```cpp
template<typename T>
struct complex {
  T re, im;
  complex() { }
  complex(T x) : re(x), im(0) { }
  complex(T x, T y) : re(x), im(y) { }
  inline void operator +=(const complex<T> &x) { re+=x.re; im+=x.im; }
};
```

**Key Features:**
- Generic template supporting any numeric type (uint8, int16, float, etc.)
- Constructors for scalar and complex initialization
- In-place addition operator
- **NEON Optimization Opportunity**: The `+=` operator is implemented as a member function; consider vectorization for bulk operations

### Complex Addition (lines 35-38)
```cpp
template<typename T>
complex<T> operator +(const complex<T> &a, const complex<T> &b) {
  return complex<T>(a.re+b.re, a.im+b.im);
}
```
- Element-wise real and imaginary part addition
- **NEON Opportunity**: Can be vectorized with paired NEON operations

### Complex Multiplication (lines 40-53)
```cpp
template<typename T>
complex<T> operator *(const complex<T> &a, const complex<T> &b) {
  return complex<T>(a.re*b.re-a.im*b.im, a.re*b.im+a.im*b.re);
}

template<typename T>
complex<T> operator *(const complex<T> &a, const T &k) {
  return complex<T>(a.re*k, a.im*k);
}

template<typename T>
complex<T> operator *(const T &k, const complex<T> &a) {
  return complex<T>(k*a.re, k*a.im);
}
```

**Complex-Complex Multiplication** (Requires 4 multiplications + 2 additions):
- Real: `re = a.re*b.re - a.im*b.im`
- Imaginary: `im = a.re*b.im + a.im*b.re`

**Complex-Scalar Multiplication**:
- Two independent scalar multiplications
- **NEON Opportunity**: Can be vectorized with single NEON instruction

**Critical Hot Paths:**
1. FFT butterflies in `cfft_engine` (dsp.h:94-95) - **HIGH PRIORITY**
2. Constellation decoding in `cstln_receiver` (sdr.h)
3. Phase rotation in multiple places

---

## 2. TRIGONOMETRIC FUNCTIONS AND APPROXIMATIONS

### Sine/Cosine Lookup Table - `trig16` (lines 94-111)

**Structure:**
```cpp
struct trig16 {
  complex<float> lut[65536];  // 256KB lookup table
  trig16() {
    for ( int a=0; a<65536; ++a ) {
      float af = a * 2*M_PI / 65536;
      lut[a].re = cosf(af);
      lut[a].im = sinf(af);
    }
  }
  inline const complex<float> &expi(uint16_t a) const {
    return lut[a];
  }
  inline const complex<float> &expi(float a) const {
    return expi((uint16_t)(int16_t)(int32_t)a);
  }
};
```

**Key Characteristics:**
- **256 KB memory footprint** (65536 complex floats × 8 bytes)
- Pre-computed at initialization (one-time cost)
- Maps 16-bit angle representation to e^(i*angle) = cos + i*sin
- Two overloads: direct uint16_t lookup and float input with casting

**Angle Representation:**
- 16-bit angle: 0-65535 maps to [0, 2π]
- Resolution: 2π/65536 ≈ 0.0000954 radians ≈ 0.0055 degrees

**Usage Patterns (from sdr.h):**
1. Phase error calculations (sdr.h:555) using `atan2f()`
2. Constellation phase computations
3. Potential use in rotators and phase correction

**Why NOT used in current `rotator` (sdr.h:1229)**:
Interestingly, the `rotator` struct builds **separate sin/cos LUTs** instead of using `trig16`:
```cpp
float lut_cos[65536];
float lut_sin[65536];
```
This separates sin/cos for independent indexing and potential better cache locality for the specific operation pattern.

**Approximation Quality:**
- Linear interpolation between table entries could improve accuracy
- Current implementation uses exact table lookup (no interpolation)

---

## 3. FIXED-POINT VS FLOATING-POINT ARITHMETIC

### Fixed-Point Angle Representation (sdr.h:277-278)
```cpp
typedef uint16_t u_angle;  //  [0,2PI[ in 65536 steps
typedef int16_t s_angle;   // [-PI,PI[ in 65536 steps
```

**Key Points:**
- Angles stored as 16-bit integers for efficiency
- u_angle: unsigned, suitable for absolute angles
- s_angle: signed, suitable for phase errors
- Conversion: `phase_in_radians * 65536 / (2*π)`
- Conversion back: `phase_int * 2π / 65536`

### Phase Error Quantization (sdr.h:558)
```cpp
pr->phase_error = (s32)(ph_err * 65536 / (2*M_PI));  // Mod 65536
```
- Phase errors computed as fixed-point (signed 32-bit intermediate)
- Automatically wraps at ±π

### Constellation Lookup Strategy (sdr.h:485, 528-559)

**256×256 Lookup Table for Constellation Decoding:**
```cpp
result lut[R][R];  // R=256, so 65536 entries
```

**Each LUT Entry Contains:**
```cpp
struct result {
  softsymbol ss;         // {cost, symbol}
  s_angle phase_error;
};
```

**Computation (lines 539-558):**
1. Integer distance squared: `d2 = (I - symbols[s].re)² + (Q - symbols[s].im)²`
2. Metric = (distance_nearest - distance_second_nearest)
3. Phase error = arctan2(Q, I) - arctan2(symbol_imag, symbol_real)
4. Phase error quantized to 16-bit fixed-point

**Advantages:**
- Pre-computed metrics eliminate runtime arithmetic
- Integer-only distance comparisons (no sqrt needed)
- Fixed-point angle representation compact and efficient

**Hybrid Approach:**
- Input I/Q: floating-point or fixed-point (both LUT accessors exist)
- Overflow handling (lines 480-483): dynamic scaling if values exceed ±128
- Output: soft symbols and phase error as fixed-point

---

## 4. LOOKUP TABLES

### Primary LUTs in Codebase:

#### A. Trigonometric LUT (`trig16`)
- **Location**: math.h:96
- **Size**: 256 KB (65536 × complex<float>)
- **Purpose**: e^(iθ) generation
- **Access Method**: O(1) array indexing
- **Note**: Comment says "TBD static and shared" - thread-safe singleton not implemented

#### B. Hamming Weight LUT (math.h:56-71)
```cpp
static const int lut[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };
```
- **Size**: 16 entries (64 bytes)
- **Purpose**: Population count (bit population) for uint8/16/32/64
- **Algorithm**: Recursive decomposition into 4-bit chunks
- **Implementation**: Uses table lookup + recursive calls

#### C. Parity LUT (math.h:75)
```cpp
return (0x6996 >> (x&15)) & 1;  // 16-entry look-up table
```
- **Size**: 1 byte (encoded as constant)
- **Purpose**: Parity bit calculation
- **Algorithm**: Clever bit encoding of parity for all 16 4-bit patterns

#### D. Constellation LUT (`cstln_lut<R>`)
- **Location**: sdr.h:300-573
- **Size**: R² entries (typically 256×256 = 65536 entries)
- **Purpose**: Demodulation metrics and phase errors
- **Pre-computed Data**: 
  - Soft Viterbi metrics (int16_t)
  - Symbol indices (uint8_t)
  - Phase errors (s_angle, int16_t)
- **Memory per entry**: 6 bytes (with padding) × 65536 = ~390 KB
- **Template Parameter R**: Controls precision (typically 256)

#### E. Rotator SinCos LUTs (sdr.h:1258-1259)
```cpp
float lut_cos[65536];
float lut_sin[65536];
```
- **Size**: 2 × 256 KB = 512 KB per rotator instance
- **Purpose**: Phase rotation at fixed frequency
- **Lazy Loading**: Populated at constructor time per frequency
- **Separate Storage**: Unlike `trig16`, sin and cos stored separately
- **Reason**: Optimized access pattern for rotation operation

#### F. FFT Twiddle Factors (dsp.h:70-76)
```cpp
omega = new complex<T>[n];
omega_rev = new complex<T>[n];
for ( int i=0; i<n; ++i ) {
  float a = 2.0*M_PI * i / n;
  omega_rev[i].re =   (omega[i].re = cosf(a));
  omega_rev[i].im = - (omega[i].im = sinf(a));
}
```
- **Size**: 2n entries (forward and reverse)
- **Purpose**: FFT butterfly operations
- **Structure**: complex<T> pairs for Danielson-Lanczos algorithm

### Memory Footprint Summary
| LUT Type | Count | Size/Entry | Total |
|----------|-------|-----------|-------|
| trig16 | 65536 | 8B | 512 KB |
| Constellation | 65536 | 6B | 390 KB |
| Rotator (cos) | 65536 | 4B | 256 KB |
| Rotator (sin) | 65536 | 4B | 256 KB |
| FFT Twiddle (4K) | 8192 | 8B | 64 KB |
| **Total (typical)** | - | - | **~1.5 MB** |

---

## 5. CORDIC OR OTHER ALGORITHMS

### Absence of CORDIC
**No CORDIC algorithm found in codebase.**

**Why?** The project appears to prioritize:
1. Pre-computed lookup tables for speed (O(1) access)
2. Floating-point library functions (cosf, sinf, atan2f)
3. Simplicity over rotating-hardware implementation

### Alternative Algorithms Used:

#### A. Phase Calculation via atan2f (sdr.h:555-558)
```cpp
float ph_symbol = atan2f(symbols[pr->ss.symbol].im,
                         symbols[pr->ss.symbol].re);
float ph_err = atan2f(Q,I) - ph_symbol;
pr->phase_error = (s32)(ph_err * 65536 / (2*M_PI));
```
- Uses standard C math library
- **Performance**: Moderate (better than CORDIC for high precision)
- No custom approximation

#### B. Complex Magnitude via Hyperbolic Functions (dsp.h:95)
```cpp
float amp[fft.n];
for ( int i=0; i<fft.n; ++i ) 
  amp[i] = hypotf(data[i].re, data[i].im);
```
- Uses `hypotf()` for |z| = sqrt(re² + im²)
- **Not distance squared**: Actual square root computed
- **NEON Opportunity**: Could use NEON sqrt or distance squared

#### C. RMS Power Computation (sdr.h:178-180, 213-218)
```cpp
float s = 0;
for ( ; p<pend; ++p )
  s += (float)p->re*p->re + (float)p->im*p->im;
out.write(sqrtf(s/window_size));
```
- Accumulates magnitude squared
- Final sqrt for RMS value
- Could use fixed-point intermediate for speed

#### D. FFT via Danielson-Lanczos (dsp.h:78-109)
- Classic in-place radix-2 FFT
- Pre-computed twiddle factors
- Complex multiplication in butterflies (see section 1)

#### E. Distance-Based Symbol Detection (sdr.h:540-549)
```cpp
for ( int s=0; s<nsymbols; ++s ) {
  int32_t d2 = (I-symbols[s].re)*(I-symbols[s].re) + 
               (Q-symbols[s].im)*(Q-symbols[s].im);
  if ( d2 < cost ) {
    cost2 = cost;
    cost = d2;
    nearest = s;
  } else if ( d2 < cost2 ) {
    cost2 = d2;
  }
}
```
- **Optimization**: Uses distance squared, no sqrt
- **Soft metrics**: Difference between closest and second-closest
- **Viterbi-ready**: Pre-computed soft symbols

---

## 6. VECTORIZATION OPPORTUNITIES

### A. High-Priority Vectorization Targets

#### 1. Complex Multiplication (CRITICAL - Many occurrences)
**Location**: math.h:40-53, dsp.h:94-95 (FFT butterflies)

**Current Code:**
```cpp
complex<T> operator *(const complex<T> &a, const complex<T> &b) {
  return complex<T>(a.re*b.re-a.im*b.im, a.re*b.im+a.im*b.re);
}
```

**NEON Vectorization Opportunity:**
- Load two complex numbers as NEON vectors
- Use `vmul` for multiplications
- Use `vrev64` or other instructions for cross-multiplication
- Estimated Speedup: **2-4x** per pair

**Implementation Hint:**
```cpp
// Pseudo-code for NEON complex multiply
float32x4_t a_vec = {a.re, a.im, a.re, a.im};
float32x4_t b_vec = {b.re, b.im, -b.im, b.re};
float32x4_t prod = vmulq_f32(a_vec, b_vec);
// Horizontal add to get result
```

#### 2. FFT Butterflies (dsp.h:94-99)
**Hot Code:**
```cpp
complex<T> &dqk = data[q+k];
complex<T> x(w.re*dqk.re - w.im*dqk.im,
             w.re*dqk.im + w.im*dqk.re);
data[q+k].re = data[p+k].re - x.re;
data[q+k].im = data[p+k].im - x.im;
data[p+k].re = data[p+k].re + x.re;
data[p+k].im = data[p+k].im + x.im;
```

**NEON Opportunities:**
- Each butterfly has 4 complex multiplications + 8 additions
- Current implementation: scalar loop
- NEON: Process multiple butterflies in parallel
- **Comment in code (sdr.h:1305-1306)**: "gcc-4.9.2 can vectorize this form with NEON"
- Estimated Speedup: **2-4x** with proper unrolling

#### 3. Complex Scalar Multiplication (sdr.h:266-269)
```cpp
for ( ; pin<pend; ++pin,++pout ) {
  pout->re = pin->re * gain;
  pout->im = pin->im * gain;
}
```

**NEON Vectorization:**
- Very straightforward - paired scalar muls
- Process 4× entries per NEON operation (2 complex = 4 floats)
- **Compiler Auto-Vectorization**: Likely already done by modern gcc
- Estimated Speedup: **2-4x** automatic

#### 4. Phase Rotation Loop (sdr.h:1246-1251)
```cpp
for ( ; pin<pend; ++pin,++pout,++index ) {
  float c = lut_cos[index];
  float s = lut_sin[index];
  pout->re = pin->re*c - pin->im*s;
  pout->im = pin->re*s + pin->im*c;
}
```

**NEON Opportunities:**
- **SIMD-unfriendly pattern**: Sequential index dependency
- Could batch: load multiple indexes, process multiple rotations
- Alternative: Unroll and parallelize phase advance
- **Constraint**: Phase continuity required
- Estimated Speedup: **1.5-2x** with unrolling

#### 5. RMS Power Accumulation (sdr.h:176-183)
```cpp
complex<T> *p=in.rd(), *pend=p+window_size;
float s = 0;
for ( ; p<pend; ++p )
  s += (float)p->re*p->re + (float)p->im*p->im;
out.write(sqrtf(s/window_size));
```

**NEON Vectorization:**
- Use NEON to compute partial sums in parallel
- 4× floats processed per cycle
- Reduce final result with horizontal adds
- Estimated Speedup: **3-4x**

#### 6. Magnitude Squared Extraction (sdr.h:216-220)
```cpp
for ( ; p<pend; ++p ) {
  float mag2 = (float)p->re*p->re + (float)p->im*p->im;
  s2 += mag2;
  float mag = sqrtf(mag2);
  if ( mag < amin ) amin = mag;
  if ( mag > amax ) amax = mag;
}
```

**NEON Opportunities:**
- NEON min/max operations for amplitude tracking
- Could process multiple samples in parallel
- **Constraint**: sqrt() is sequential
- Estimated Speedup: **2-3x** for min/max tracking

#### 7. Distance Calculations (sdr.h:540-542)
```cpp
for ( int s=0; s<nsymbols; ++s ) {
  int32_t d2 = (I-symbols[s].re)*(I-symbols[s].re) + 
               (Q-symbols[s].im)*(Q-symbols[s].im);
```

**NEON Opportunities:**
- Pre-computed constellation symbols could be loaded as NEON vectors
- Multiple symbols compared in parallel
- **Constraint**: Data-dependent symbol selection
- Estimated Speedup: **2-4x** with SIMD min search

### B. Lower-Priority or Challenging Targets

#### 1. Hamming Weight / Parity Calculation
- Lookup table already highly optimized
- Processing bit-by-bit, limited parallelism
- **Not worth NEON investment**

#### 2. FFT Bit Reversal (dsp.h:80-83)
```cpp
for ( int i=0; i<n; ++i ) {
  int r = bitrev[i];
  if ( r < i ) { 
    complex<T> tmp=data[i]; 
    data[i]=data[r]; 
    data[r]=tmp; 
  }
}
```
- Memory-bound operation
- Random access patterns (poor cache utilization)
- **Not amenable to NEON**

#### 3. atan2 / sqrt Function Calls
- Already optimized math library functions
- Could use fast approximations (trade accuracy for speed)
- **Would require NEON-specific intrinsics**

### C. Compiler Auto-Vectorization Potential

**Code most likely auto-vectorized by GCC/Clang:**
1. Simple element-wise operations (addition, scalar multiplication)
2. Accumulation loops with no dependencies
3. Min/max tracking loops

**Code requiring manual NEON intrinsics:**
1. Complex multiplication (SIMD patterns not obvious to compiler)
2. Cross-element operations (shuffles, transposes)
3. Bit-level operations

---

## 7. COMPILATION FLAGS FOR NEON OPTIMIZATION

Current code should be compiled with:
```bash
-march=armv7-a -mfpu=neon -O3
```

For better auto-vectorization:
```bash
-march=armv7-a -mfpu=neon -O3 -ftree-vectorize -ffast-math
```

Note: `-ffast-math` trades precision for speed (may affect phase/timing-critical code)

---

## 8. NEON INTRINSICS RECOMMENDATIONS

| Operation | NEON Intrinsic | Comment |
|-----------|----------------|---------|
| Complex mul | SIMD emulation | See section 6.A.1 |
| Magnitude² | `vmulq_f32` + add | Fast vectorizable |
| Distance² | vmulq_f32, vsub | Vectorizable |
| Add | `vaddq_f32` | Auto-vectorized |
| Mul scalar | `vmulq_f32` | Auto-vectorized |
| Sqrt | `vsqrtq_f32` | ARM intrinsic, slower |
| Min/Max | `vminq_f32`, `vmaxq_f32` | Very fast |
| Reduce add | `vpadd_f32`, `vpaddl` | Horizontal ops |

---

## 9. SUMMARY & RECOMMENDATIONS

### Current State
- **Strengths**: 
  - Heavy use of lookup tables (O(1) access)
  - Avoids CORDIC (smart for floating-point)
  - Pre-computed metrics for DSP operations
  
- **Weaknesses**:
  - No NEON intrinsics in use
  - Complex multiplication not vectorized
  - Potential for 2-4x speedup on critical paths

### Recommended Priority Order

1. **Phase Rotation (rotator)**: Batch-process indexes with NEON
2. **Complex Multiplication (FFT)**: NEON intrinsics for butterflies
3. **RMS/Power Accumulation**: Auto-vectorize with `-ftree-vectorize`
4. **Constellation Decoding**: NEON min reduction for symbol search
5. **Magnitude Tracking**: NEON min/max operations

### Expected Overall Speedup
- Conservative estimate: **1.5-2x** with compiler flags alone
- With manual NEON optimization: **3-5x** on DSP-heavy code

