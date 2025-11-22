#!/usr/bin/env python3
"""
Generate synthetic DVB-S IQ test signals for NEON optimization validation.

This script creates DVB-S modulated IQ data with configurable parameters:
- Symbol rate
- Sample rate (oversampling ratio)
- Signal-to-noise ratio (SNR)
- Modulation format (QPSK, 8PSK, etc.)
- Number of test packets

The output is compatible with leandvb for demodulation testing.

Copyright (C) 2025 LeanSDR Project
Licensed under GPL-3.0
"""

import numpy as np
import argparse
import struct
import sys


class DVBSModulator:
    """DVB-S QPSK modulator for test signal generation."""

    def __init__(self, symbol_rate, sample_rate, rolloff=0.35):
        """
        Initialize DVB-S modulator.

        Args:
            symbol_rate: Symbol rate in Hz
            sample_rate: Sample rate in Hz
            rolloff: RRC filter roll-off factor (default 0.35)
        """
        self.symbol_rate = symbol_rate
        self.sample_rate = sample_rate
        self.rolloff = rolloff
        self.samples_per_symbol = sample_rate / symbol_rate

        # Generate RRC filter coefficients
        self.rrc_taps = self._generate_rrc_filter()

    def _generate_rrc_filter(self, ntaps=64):
        """Generate root-raised cosine filter coefficients."""
        T = 1.0 / self.symbol_rate
        Ts = 1.0 / self.sample_rate
        alpha = self.rolloff

        taps = []
        for i in range(ntaps):
            t = (i - ntaps/2) * Ts

            # Handle special cases to avoid division by zero
            if t == 0:
                h = (1.0 + alpha * (4.0/np.pi - 1.0))
            elif abs(t) == T / (4.0 * alpha):
                h = (alpha / np.sqrt(2.0)) * (
                    (1.0 + 2.0/np.pi) * np.sin(np.pi/(4.0*alpha)) +
                    (1.0 - 2.0/np.pi) * np.cos(np.pi/(4.0*alpha))
                )
            else:
                h = (np.sin(np.pi * t/T * (1.0 - alpha)) +
                     4.0 * alpha * t/T * np.cos(np.pi * t/T * (1.0 + alpha))) / \
                    (np.pi * t/T * (1.0 - (4.0 * alpha * t/T)**2))

            taps.append(h)

        # Normalize
        taps = np.array(taps)
        taps /= np.sqrt(np.sum(taps**2))
        return taps

    def modulate(self, bits):
        """
        Modulate bits to QPSK symbols.

        Args:
            bits: Array of bits (0 or 1)

        Returns:
            Complex IQ samples
        """
        # Ensure even number of bits for QPSK
        if len(bits) % 2 != 0:
            bits = np.append(bits, 0)

        # Map bits to QPSK symbols
        # Gray coding: 00->1+1j, 01->-1+1j, 11->-1-1j, 10->1-1j
        i_bits = bits[0::2]
        q_bits = bits[1::2]

        i_symbols = 2*i_bits - 1  # Map 0->-1, 1->1
        q_symbols = 2*q_bits - 1

        symbols = (i_symbols + 1j*q_symbols) / np.sqrt(2.0)  # Normalize power

        # Upsample symbols with proper fractional handling
        # Calculate total number of output samples
        num_samples = int(np.ceil(len(symbols) * self.samples_per_symbol))
        upsampled = np.zeros(num_samples, dtype=complex)

        # Place symbols at appropriate sample positions
        sps_int = int(self.samples_per_symbol)
        for i, sym in enumerate(symbols):
            idx = int(i * self.samples_per_symbol)
            if idx < len(upsampled):
                upsampled[idx] = sym

        # Apply RRC filter
        filtered = np.convolve(upsampled, self.rrc_taps, mode='same')

        return filtered


def generate_random_bits(num_bits, seed=None):
    """Generate random bit sequence."""
    if seed is not None:
        np.random.seed(seed)
    return np.random.randint(0, 2, num_bits)


def generate_mpeg_ts_packets(num_packets=100):
    """
    Generate synthetic MPEG-TS packets.

    Each packet is 188 bytes starting with sync byte 0x47.

    Args:
        num_packets: Number of TS packets to generate

    Returns:
        Byte array of MPEG-TS packets
    """
    packets = []
    for i in range(num_packets):
        # Create packet with 0x47 sync byte
        packet = bytearray(188)
        packet[0] = 0x47  # MPEG-TS sync byte

        # Add continuity counter (cycling 0-15)
        packet[3] = (packet[3] & 0xF0) | (i % 16)

        # Fill with pseudo-random data (deterministic for testing)
        np.random.seed(i)
        random_data = np.random.randint(0, 256, 184, dtype=np.uint8)
        packet[4:] = bytes(random_data)

        packets.append(packet)

    return b''.join(packets)


def add_awgn(signal, snr_db):
    """
    Add Additive White Gaussian Noise to signal.

    Args:
        signal: Complex IQ signal
        snr_db: Desired SNR in dB

    Returns:
        Noisy signal
    """
    # Calculate signal power
    signal_power = np.mean(np.abs(signal)**2)

    # Calculate noise power for desired SNR
    snr_linear = 10**(snr_db / 10.0)
    noise_power = signal_power / snr_linear

    # Generate complex Gaussian noise
    noise_std = np.sqrt(noise_power / 2.0)  # Divide by 2 for I and Q
    noise = noise_std * (np.random.randn(len(signal)) +
                        1j * np.random.randn(len(signal)))

    return signal + noise


def write_iq_file(filename, iq_samples, format='f32'):
    """
    Write IQ samples to file.

    Args:
        filename: Output filename
        iq_samples: Complex IQ samples
        format: Output format ('f32', 'u8', 's16')
    """
    with open(filename, 'wb') as f:
        if format == 'f32':
            # Write as 32-bit float I/Q pairs
            for sample in iq_samples:
                f.write(struct.pack('ff', sample.real, sample.imag))

        elif format == 'u8':
            # Scale to 0-255 range
            scale = 127.0
            offset = 128.0
            for sample in iq_samples:
                i_u8 = int(np.clip(sample.real * scale + offset, 0, 255))
                q_u8 = int(np.clip(sample.imag * scale + offset, 0, 255))
                f.write(struct.pack('BB', i_u8, q_u8))

        elif format == 's16':
            # Scale to -32768 to 32767 range
            scale = 32767.0
            for sample in iq_samples:
                i_s16 = int(np.clip(sample.real * scale, -32768, 32767))
                q_s16 = int(np.clip(sample.imag * scale, -32768, 32767))
                f.write(struct.pack('hh', i_s16, q_s16))


def main():
    parser = argparse.ArgumentParser(
        description='Generate DVB-S test signals for NEON optimization validation',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument('-o', '--output', required=True,
                       help='Output IQ file')
    parser.add_argument('-f', '--format', choices=['f32', 'u8', 's16'],
                       default='f32', help='Output format')
    parser.add_argument('--symbol-rate', type=float, default=1000000,
                       help='Symbol rate in Hz')
    parser.add_argument('--sample-rate', type=float, default=2400000,
                       help='Sample rate in Hz')
    parser.add_argument('--snr', type=float, default=10.0,
                       help='Signal-to-noise ratio in dB')
    parser.add_argument('--num-packets', type=int, default=100,
                       help='Number of MPEG-TS packets to generate')
    parser.add_argument('--rolloff', type=float, default=0.35,
                       help='RRC filter roll-off factor')
    parser.add_argument('--seed', type=int, default=42,
                       help='Random seed for reproducibility')
    parser.add_argument('--verbose', action='store_true',
                       help='Print verbose output')

    args = parser.parse_args()

    # Set random seed for reproducibility
    np.random.seed(args.seed)

    if args.verbose:
        print(f"Generating DVB-S test signal...")
        print(f"  Symbol rate: {args.symbol_rate/1e6:.2f} Msps")
        print(f"  Sample rate: {args.sample_rate/1e6:.2f} Msps")
        print(f"  Oversampling: {args.sample_rate/args.symbol_rate:.2f}x")
        print(f"  SNR: {args.snr} dB")
        print(f"  Packets: {args.num_packets}")
        print(f"  Format: {args.format}")

    # Generate MPEG-TS packets
    ts_packets = generate_mpeg_ts_packets(args.num_packets)

    # Convert bytes to bits
    bits = np.unpackbits(np.frombuffer(ts_packets, dtype=np.uint8))

    if args.verbose:
        print(f"  Total bits: {len(bits)}")
        print(f"  Total bytes: {len(ts_packets)}")

    # Create modulator
    modulator = DVBSModulator(args.symbol_rate, args.sample_rate, args.rolloff)

    # Modulate to IQ
    iq_samples = modulator.modulate(bits)

    # Add noise
    iq_noisy = add_awgn(iq_samples, args.snr)

    if args.verbose:
        print(f"  IQ samples: {len(iq_noisy)}")
        print(f"  Duration: {len(iq_noisy)/args.sample_rate:.3f} seconds")
        print(f"  Signal power: {np.mean(np.abs(iq_samples)**2):.6f}")
        print(f"  Noise power: {np.mean(np.abs(iq_noisy - iq_samples)**2):.6f}")

    # Write to file
    write_iq_file(args.output, iq_noisy, args.format)

    if args.verbose:
        print(f"  Output written to: {args.output}")
        print(f"  File size: {len(iq_noisy) * (8 if args.format == 'f32' else 2 if args.format == 'u8' else 4)} bytes")

    print(f"SUCCESS: Generated {len(iq_noisy)} IQ samples")


if __name__ == '__main__':
    main()
