#!/usr/bin/env python3
"""
Compare MPEG-TS outputs from scalar and NEON-optimized leandvb.

This script validates that NEON optimizations produce bit-exact or
nearly-identical results compared to the scalar baseline implementation.

Metrics computed:
- Bit Error Rate (BER)
- Packet Error Rate (PER)
- Sync byte alignment
- Byte-by-byte comparison
- Structural validation

Copyright (C) 2025 LeanSDR Project
Licensed under GPL-3.0
"""

import argparse
import sys
import numpy as np
from pathlib import Path


class MPEGTSAnalyzer:
    """Analyze and compare MPEG-TS streams."""

    SYNC_BYTE = 0x47
    PACKET_SIZE = 188

    def __init__(self, filename):
        """Load MPEG-TS file."""
        self.filename = filename
        self.data = self._load_file()
        self.packets = self._parse_packets()

    def _load_file(self):
        """Load file as byte array."""
        try:
            with open(self.filename, 'rb') as f:
                data = f.read()
            return bytearray(data)
        except FileNotFoundError:
            print(f"ERROR: File not found: {self.filename}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"ERROR: Failed to read {self.filename}: {e}", file=sys.stderr)
            sys.exit(1)

    def _parse_packets(self):
        """Parse MPEG-TS packets."""
        packets = []
        offset = 0

        # Find first sync byte
        while offset < len(self.data) and self.data[offset] != self.SYNC_BYTE:
            offset += 1

        if offset >= len(self.data):
            print(f"WARNING: No sync byte found in {self.filename}", file=sys.stderr)
            return []

        # Extract packets
        while offset + self.PACKET_SIZE <= len(self.data):
            packet = self.data[offset:offset + self.PACKET_SIZE]

            # Verify sync byte
            if packet[0] == self.SYNC_BYTE:
                packets.append(bytes(packet))
                offset += self.PACKET_SIZE
            else:
                # Try to resync
                offset += 1
                while offset < len(self.data) and self.data[offset] != self.SYNC_BYTE:
                    offset += 1

        return packets

    def get_stats(self):
        """Get statistics about the TS stream."""
        return {
            'file_size': len(self.data),
            'num_packets': len(self.packets),
            'sync_ratio': len(self.packets) * self.PACKET_SIZE / max(len(self.data), 1),
            'valid_packets': sum(1 for p in self.packets if p[0] == self.SYNC_BYTE),
        }


def compute_ber(data1, data2):
    """
    Compute Bit Error Rate between two byte sequences.

    Args:
        data1, data2: Byte sequences to compare

    Returns:
        BER (0.0 to 1.0)
    """
    if len(data1) == 0 or len(data2) == 0:
        return 1.0

    # Compare common length
    min_len = min(len(data1), len(data2))
    bytes1 = np.frombuffer(data1[:min_len], dtype=np.uint8)
    bytes2 = np.frombuffer(data2[:min_len], dtype=np.uint8)

    # XOR to find differing bits
    xor = np.bitwise_xor(bytes1, bytes2)

    # Count bits
    bit_errors = np.unpackbits(xor).sum()
    total_bits = min_len * 8

    return bit_errors / total_bits


def compute_per(packets1, packets2):
    """
    Compute Packet Error Rate.

    Args:
        packets1, packets2: Lists of packet bytes

    Returns:
        PER (0.0 to 1.0)
    """
    if len(packets1) == 0 or len(packets2) == 0:
        return 1.0

    min_packets = min(len(packets1), len(packets2))
    packet_errors = 0

    for i in range(min_packets):
        if packets1[i] != packets2[i]:
            packet_errors += 1

    return packet_errors / min_packets


def compare_streams(file1, file2, verbose=False):
    """
    Compare two MPEG-TS streams.

    Args:
        file1: Reference stream (scalar)
        file2: Test stream (NEON)
        verbose: Print detailed comparison

    Returns:
        Dictionary with comparison results
    """
    print(f"\nComparing MPEG-TS outputs:")
    print(f"  Reference (scalar): {file1}")
    print(f"  Test (NEON):        {file2}")

    # Load and parse streams
    stream1 = MPEGTSAnalyzer(file1)
    stream2 = MPEGTSAnalyzer(file2)

    stats1 = stream1.get_stats()
    stats2 = stream2.get_stats()

    if verbose:
        print(f"\nReference stream:")
        print(f"  File size:     {stats1['file_size']:,} bytes")
        print(f"  Packets:       {stats1['num_packets']}")
        print(f"  Valid packets: {stats1['valid_packets']}")
        print(f"  Sync ratio:    {stats1['sync_ratio']*100:.2f}%")

        print(f"\nTest stream:")
        print(f"  File size:     {stats2['file_size']:,} bytes")
        print(f"  Packets:       {stats2['num_packets']}")
        print(f"  Valid packets: {stats2['valid_packets']}")
        print(f"  Sync ratio:    {stats2['sync_ratio']*100:.2f}%")

    # Compute metrics
    results = {
        'file1_size': stats1['file_size'],
        'file2_size': stats2['file_size'],
        'file1_packets': stats1['num_packets'],
        'file2_packets': stats2['num_packets'],
        'size_match': stats1['file_size'] == stats2['file_size'],
        'packet_count_match': stats1['num_packets'] == stats2['num_packets'],
    }

    # Bit error rate (on raw bytes)
    ber = compute_ber(stream1.data, stream2.data)
    results['ber'] = ber

    # Packet error rate
    per = compute_per(stream1.packets, stream2.packets)
    results['per'] = per

    # Exact match check
    min_len = min(len(stream1.data), len(stream2.data))
    results['exact_match'] = (stream1.data[:min_len] == stream2.data[:min_len])

    # Byte-level similarity
    if min_len > 0:
        matching_bytes = sum(1 for i in range(min_len)
                           if stream1.data[i] == stream2.data[i])
        results['byte_match_ratio'] = matching_bytes / min_len
    else:
        results['byte_match_ratio'] = 0.0

    return results


def print_comparison_results(results, tolerance=1e-6):
    """
    Print comparison results and determine pass/fail.

    Args:
        results: Dictionary from compare_streams()
        tolerance: Maximum acceptable BER for PASS

    Returns:
        True if test passed, False otherwise
    """
    print(f"\n{'='*60}")
    print(f"COMPARISON RESULTS")
    print(f"{'='*60}")

    print(f"\nFile sizes:")
    print(f"  Reference: {results['file1_size']:,} bytes")
    print(f"  Test:      {results['file2_size']:,} bytes")
    print(f"  Match:     {'✓ YES' if results['size_match'] else '✗ NO'}")

    print(f"\nPacket counts:")
    print(f"  Reference: {results['file1_packets']} packets")
    print(f"  Test:      {results['file2_packets']} packets")
    print(f"  Match:     {'✓ YES' if results['packet_count_match'] else '✗ NO'}")

    print(f"\nError rates:")
    print(f"  Bit Error Rate (BER):      {results['ber']:.2e}")
    print(f"  Packet Error Rate (PER):   {results['per']:.2e}")
    print(f"  Byte match ratio:          {results['byte_match_ratio']*100:.4f}%")

    print(f"\nExact match: {'✓ YES' if results['exact_match'] else '✗ NO'}")

    # Determine pass/fail
    passed = True
    print(f"\n{'='*60}")
    print(f"VALIDATION")
    print(f"{'='*60}")

    checks = []

    # Check 1: Files should have similar sizes (within 1%)
    size_diff_ratio = abs(results['file1_size'] - results['file2_size']) / max(results['file1_size'], 1)
    size_ok = size_diff_ratio < 0.01
    checks.append(('File sizes similar', size_ok))
    passed &= size_ok

    # Check 2: Packet counts should match (or be within 1)
    packet_diff = abs(results['file1_packets'] - results['file2_packets'])
    packets_ok = packet_diff <= 1
    checks.append(('Packet counts match', packets_ok))
    passed &= packets_ok

    # Check 3: BER should be below tolerance (ideally 0 for bit-exact)
    ber_ok = results['ber'] <= tolerance
    checks.append((f'BER ≤ {tolerance:.0e}', ber_ok))
    passed &= ber_ok

    # Check 4: PER should be very low (< 1% for acceptable)
    per_ok = results['per'] < 0.01
    checks.append(('PER < 1%', per_ok))
    passed &= per_ok

    # Check 5: Byte match ratio should be very high (> 99%)
    byte_match_ok = results['byte_match_ratio'] > 0.99
    checks.append(('Byte match > 99%', byte_match_ok))
    passed &= byte_match_ok

    for check_name, check_result in checks:
        status = '✓ PASS' if check_result else '✗ FAIL'
        print(f"  {status}: {check_name}")

    print(f"\n{'='*60}")
    if passed:
        print(f"✓ OVERALL: PASS - NEON output matches scalar baseline")
    else:
        print(f"✗ OVERALL: FAIL - NEON output differs from scalar baseline")
    print(f"{'='*60}\n")

    return passed


def main():
    parser = argparse.ArgumentParser(
        description='Compare MPEG-TS outputs from scalar and NEON leandvb',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument('reference', help='Reference output file (scalar)')
    parser.add_argument('test', help='Test output file (NEON)')
    parser.add_argument('--tolerance', type=float, default=1e-6,
                       help='Maximum acceptable BER')
    parser.add_argument('--verbose', action='store_true',
                       help='Print detailed comparison')
    parser.add_argument('--json', action='store_true',
                       help='Output results as JSON')

    args = parser.parse_args()

    # Perform comparison
    results = compare_streams(args.reference, args.test, args.verbose)

    # Output results
    if args.json:
        import json
        print(json.dumps(results, indent=2))
        return 0

    # Print formatted results
    passed = print_comparison_results(results, args.tolerance)

    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
