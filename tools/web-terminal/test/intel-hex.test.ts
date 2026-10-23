import { describe, it, expect } from 'vitest';
import { parseIntelHex } from '../src/flasher/intel-hex';

describe('Intel HEX Firmware Parser', () => {
  it('returns empty result for empty or invalid hex string', () => {
    const empty = parseIntelHex('');
    expect(empty.totalBytes).toBe(0);
    expect(empty.data.length).toBe(0);
    expect(empty.baseAddress).toBe(0x08000000);

    const invalid = parseIntelHex('not a hex file\nsome random text\n');
    expect(invalid.totalBytes).toBe(0);
    expect(invalid.data.length).toBe(0);
  });

  it('parses standard 16-bit address data records (record type 00)', () => {
    // :10 0100 00 214601360121470136007EFE09D21901 40
    // 16 bytes at address 0x0100
    const hex = [
      ':10010000214601360121470136007EFE09D2190140',
      ':00000001FF' // EOF
    ].join('\n');

    const result = parseIntelHex(hex);
    expect(result.baseAddress).toBe(0x0100);
    expect(result.totalBytes).toBe(16);
    expect(result.data.length).toBe(16);
    expect(result.data[0]).toBe(0x21);
    expect(result.data[1]).toBe(0x46);
    expect(result.data[15]).toBe(0x01);
  });

  it('parses 32-bit addresses with Extended Linear Address records (record type 04)', () => {
    // Extended Linear Address 0x0800 -> 32-bit base 0x08000000 (STM32 Flash)
    const hex = [
      ':020000040800F2', // Set upper 16 bits to 0x0800
      ':04000000AABBCCDD8E', // 4 bytes at 0x08000000
      ':00000001FF'
    ].join('\n');

    const result = parseIntelHex(hex);
    expect(result.baseAddress).toBe(0x08000000);
    expect(result.totalBytes).toBe(4);
    expect(result.data[0]).toBe(0xAA);
    expect(result.data[1]).toBe(0xBB);
    expect(result.data[2]).toBe(0xCC);
    expect(result.data[3]).toBe(0xDD);
  });

  it('fills unprogrammed address gaps with 0xFF', () => {
    // 2 bytes at 0x0000, gap of 2 bytes, 2 bytes at 0x0004
    const hex = [
      ':020000001122CB',
      ':0200040055663F',
      ':00000001FF'
    ].join('\n');

    const result = parseIntelHex(hex);
    expect(result.baseAddress).toBe(0x0000);
    expect(result.totalBytes).toBe(6); // 0, 1, 2, 3, 4, 5
    expect(result.data[0]).toBe(0x11);
    expect(result.data[1]).toBe(0x22);
    expect(result.data[2]).toBe(0xFF); // Unprogrammed gap
    expect(result.data[3]).toBe(0xFF); // Unprogrammed gap
    expect(result.data[4]).toBe(0x55);
    expect(result.data[5]).toBe(0x66);
  });

  it('parses multiple extended linear address pages', () => {
    const hex = [
      ':020000040800F2',
      ':020000001234B8', // 0x08000000: 0x12, 0x34
      ':020000040801F1',
      ':02000000567830', // 0x08010000: 0x56, 0x78
      ':00000001FF'
    ].join('\n');

    const result = parseIntelHex(hex);
    expect(result.baseAddress).toBe(0x08000000);
    // Span from 0x08000000 to 0x08010001 = 0x10002 (65538 bytes)
    expect(result.totalBytes).toBe(65538);
    expect(result.data[0]).toBe(0x12);
    expect(result.data[1]).toBe(0x34);
    expect(result.data[65536]).toBe(0x56);
    expect(result.data[65537]).toBe(0x78);
  });

  it('ignores lines after EOF record (record type 01)', () => {
    const hex = [
      ':02000000AABBA5',
      ':00000001FF', // EOF
      ':02000200CCDDA1' // Should be ignored
    ].join('\n');

    const result = parseIntelHex(hex);
    expect(result.totalBytes).toBe(2);
    expect(result.data[0]).toBe(0xAA);
    expect(result.data[1]).toBe(0xBB);
  });
});

