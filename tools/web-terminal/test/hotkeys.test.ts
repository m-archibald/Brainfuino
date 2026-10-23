import { describe, it, expect } from 'vitest';
import { mapDirectKey } from '../src/terminal/hotkeys';

describe('Terminal Keyboard Router (Direct Mode)', () => {
  it('maps standard control keys correctly', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Enter' }))).toBe('\r');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Backspace' }))).toBe('\b');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Delete' }))).toBe('\x1b[3~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Tab' }))).toBe('\t');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Escape' }))).toBe('\x1b');
  });

  it('maps arrow keys to ANSI escape sequences', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowUp' }))).toBe('\x1b[A');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowDown' }))).toBe('\x1b[B');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowRight' }))).toBe('\x1b[C');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowLeft' }))).toBe('\x1b[D');
  });

  it('maps PageUp and PageDown to VT100 codes and suppresses repeats', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'PageUp', repeat: false }))).toBe('\x1b[5~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'PageUp', repeat: true }))).toBeNull();

    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'PageDown', repeat: false }))).toBe('\x1b[6~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'PageDown', repeat: true }))).toBeNull();
  });

  it('forwards single character keystrokes directly', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'a' }))).toBe('a');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '+' }))).toBe('+');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '-' }))).toBe('-');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: ']' }))).toBe(']');
  });

  it('ignores unhandled non-character keys like Shift, Meta, Alt', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Shift' }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Alt' }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Control' }))).toBeNull();
  });
});
