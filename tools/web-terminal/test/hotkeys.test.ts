import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { mapDirectKey } from '../src/terminal/hotkeys';

describe('Terminal Keyboard Router (PuTTY 1-to-1 Direct Mode)', () => {
  it('maps standard control keys correctly', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Enter' }))).toBe('\r');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Backspace' }))).toBe('\b');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Delete' }))).toBe('\x1b[3~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Tab' }))).toBe('\t');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Escape' }))).toBe('\x1b');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Home' }))).toBe('\x1b[H');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'End' }))).toBe('\x1b[F');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Insert' }))).toBe('\x1b[2~');
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
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Z' }))).toBe('Z');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '+' }))).toBe('+');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '-' }))).toBe('-');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: ']' }))).toBe(']');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '5' }))).toBe('5');
  });

  it('maps Ctrl+A through Ctrl+Z to standard ASCII control codes (0x01 - 0x1A)', () => {
    const letters = 'abcdefghijklmnopqrstuvwxyz';
    for (let i = 0; i < letters.length; i++) {
      const char = letters[i];
      if (char === 'v') continue; // Ctrl+V is deferred to native paste handler
      const expectedCode = String.fromCharCode(i + 1);
      expect(mapDirectKey(new KeyboardEvent('keydown', { key: char, ctrlKey: true }))).toBe(expectedCode);
    }

    // Verify key control codes specifically:
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'c', ctrlKey: true }))).toBe('\x03'); // ETX / SIGINT
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'd', ctrlKey: true }))).toBe('\x04'); // EOT / EOF
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'l', ctrlKey: true }))).toBe('\x0c'); // FF / Clear screen
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'z', ctrlKey: true }))).toBe('\x1a'); // SUB / Suspend
  });

  it('preserves native browser copy on Ctrl+C / Cmd+C when text is selected', () => {
    const origGetSelection = window.getSelection;
    window.getSelection = () => ({
      toString: () => 'selected text to copy'
    } as Selection);

    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'c', ctrlKey: true }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'c', metaKey: true }))).toBeNull();

    // When selection is empty, Ctrl+C sends \x03
    window.getSelection = () => ({
      toString: () => ''
    } as Selection);
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'c', ctrlKey: true }))).toBe('\x03');

    window.getSelection = origGetSelection;
  });

  it('allows native paste handler to intercept Ctrl+V / Cmd+V', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'v', ctrlKey: true }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'v', metaKey: true }))).toBeNull();
  });

  it('maps special Ctrl symbols correctly', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '[', ctrlKey: true }))).toBe('\x1b'); // ESC
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '\\', ctrlKey: true }))).toBe('\x1c'); // FS
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: ']', ctrlKey: true }))).toBe('\x1d'); // GS
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '^', ctrlKey: true }))).toBe('\x1e'); // RS
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: '_', ctrlKey: true }))).toBe('\x1f'); // US
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: ' ', ctrlKey: true }))).toBe('\x00'); // NUL
  });

  it('maps Ctrl navigation keys correctly', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowUp', ctrlKey: true }))).toBe('\x1b[1;5A');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowDown', ctrlKey: true }))).toBe('\x1b[1;5B');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowRight', ctrlKey: true }))).toBe('\x1b[1;5C');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowLeft', ctrlKey: true }))).toBe('\x1b[1;5D');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Home', ctrlKey: true }))).toBe('\x1b[1;5H');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'End', ctrlKey: true }))).toBe('\x1b[1;5F');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Delete', ctrlKey: true }))).toBe('\x1b[3;5~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Backspace', ctrlKey: true }))).toBe('\x08');
  });

  it('maps Shift navigation and Tab correctly', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Tab', shiftKey: true }))).toBe('\x1b[Z'); // Backtab
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowUp', shiftKey: true }))).toBe('\x1b[1;2A');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowDown', shiftKey: true }))).toBe('\x1b[1;2B');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowRight', shiftKey: true }))).toBe('\x1b[1;2C');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowLeft', shiftKey: true }))).toBe('\x1b[1;2D');
  });

  it('maps Alt keys to ESC prefixed sequences', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'b', altKey: true }))).toBe('\x1bb');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'f', altKey: true }))).toBe('\x1bf');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowLeft', altKey: true }))).toBe('\x1b[1;3D');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'ArrowRight', altKey: true }))).toBe('\x1b[1;3C');
  });

  it('maps standard VT100/Xterm function keys F1 through F12', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F1' }))).toBe('\x1bOP');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F2' }))).toBe('\x1bOQ');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F3' }))).toBe('\x1bOR');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F4' }))).toBe('\x1bOS');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F5' }))).toBe('\x1b[15~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F6' }))).toBe('\x1b[17~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F7' }))).toBe('\x1b[18~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F8' }))).toBe('\x1b[19~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F9' }))).toBe('\x1b[20~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F10' }))).toBe('\x1b[21~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F11' }))).toBe('\x1b[23~');
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'F12' }))).toBe('\x1b[24~');
  });

  it('ignores modifier keys alone (Shift, Control, Alt, Meta, CapsLock)', () => {
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Shift' }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Control' }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Alt' }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'Meta' }))).toBeNull();
    expect(mapDirectKey(new KeyboardEvent('keydown', { key: 'CapsLock' }))).toBeNull();
  });
});
