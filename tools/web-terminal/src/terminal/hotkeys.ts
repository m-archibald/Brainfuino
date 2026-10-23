export function mapDirectKey(e: KeyboardEvent): string | null {
  if (e.isComposing) return null;

  // Allow Ctrl+C / Cmd+C copy if text is selected
  const selection = typeof window !== 'undefined' && window.getSelection ? window.getSelection()?.toString() : '';
  const hasSelection = !!(selection && selection.length > 0);

  if ((e.ctrlKey || e.metaKey) && hasSelection && (e.key === 'c' || e.key === 'C')) {
    return null;
  }

  // Ctrl+V paste: return null so browser paste event triggers and can be intercepted
  if ((e.ctrlKey || e.metaKey) && (e.key === 'v' || e.key === 'V')) {
    return null;
  }

  const key = e.key;

  // --- Ctrl / Cmd Combinations (PuTTY 1-to-1 ASCII control codes 0x01 - 0x1A) ---
  if (e.ctrlKey || e.metaKey) {
    if (key.length === 1) {
      const upper = key.toUpperCase();
      const code = upper.charCodeAt(0);
      // Ctrl+A through Ctrl+Z -> ASCII 0x01 through 0x1A
      if (code >= 65 && code <= 90) {
        return String.fromCharCode(code - 64);
      }
      if (key === '[' || key === '{') return '\x1b'; // ESC (Ctrl+[)
      if (key === '\\' || key === '|') return '\x1c'; // FS  (Ctrl+\)
      if (key === ']' || key === '}') return '\x1d'; // GS  (Ctrl+])
      if (key === '^' || key === '6') return '\x1e'; // RS  (Ctrl+^)
      if (key === '_' || key === '-') return '\x1f'; // US  (Ctrl+_)
      if (key === '@' || key === ' ') return '\x00'; // NUL (Ctrl+@ / Ctrl+Space)
    }

    // Ctrl + Arrow / Navigation keys
    if (key === 'ArrowUp') return '\x1b[1;5A';
    if (key === 'ArrowDown') return '\x1b[1;5B';
    if (key === 'ArrowRight') return '\x1b[1;5C';
    if (key === 'ArrowLeft') return '\x1b[1;5D';
    if (key === 'Home') return '\x1b[1;5H';
    if (key === 'End') return '\x1b[1;5F';
    if (key === 'Delete') return '\x1b[3;5~';
    if (key === 'Backspace') return '\x08';
    return null;
  }

  // --- Alt Combinations (PuTTY / VT100: Alt sends ESC prefix) ---
  if (e.altKey && !e.ctrlKey && !e.metaKey) {
    if (key.length === 1) {
      return `\x1b${key}`;
    }
    if (key === 'ArrowUp') return '\x1b[1;3A';
    if (key === 'ArrowDown') return '\x1b[1;3B';
    if (key === 'ArrowRight') return '\x1b[1;3C';
    if (key === 'ArrowLeft') return '\x1b[1;3D';
    return null;
  }

  // --- Shift Combinations ---
  if (e.shiftKey) {
    if (key === 'Tab') return '\x1b[Z'; // ANSI Backtab
    if (key === 'ArrowUp') return '\x1b[1;2A';
    if (key === 'ArrowDown') return '\x1b[1;2B';
    if (key === 'ArrowRight') return '\x1b[1;2C';
    if (key === 'ArrowLeft') return '\x1b[1;2D';
    if (key === 'Home') return '\x1b[1;2H';
    if (key === 'End') return '\x1b[1;2F';
    if (key === 'PageUp') return '\x1b[5;2~';
    if (key === 'PageDown') return '\x1b[6;2~';
    // Shift + single character produces key with length === 1 (e.g. 'A', '!', etc.), handled below
  }

  // --- Standard VT100 / Xterm Function Keys F1-F12 ---
  if (key === 'F1') return '\x1bOP';
  if (key === 'F2') return '\x1bOQ';
  if (key === 'F3') return '\x1bOR';
  if (key === 'F4') return '\x1bOS';
  if (key === 'F5') return '\x1b[15~';
  if (key === 'F6') return '\x1b[17~';
  if (key === 'F7') return '\x1b[18~';
  if (key === 'F8') return '\x1b[19~';
  if (key === 'F9') return '\x1b[20~';
  if (key === 'F10') return '\x1b[21~';
  if (key === 'F11') return '\x1b[23~';
  if (key === 'F12') return '\x1b[24~';

  // --- Standard Terminal Control Keys ---
  if (key === 'Enter') return '\r';
  if (key === 'Backspace') return '\b';
  if (key === 'Delete') return '\x1b[3~';
  if (key === 'Tab') return '\t';
  if (key === 'Escape') return '\x1b';

  // --- Cursor & Navigation Keys ---
  if (key === 'ArrowUp') return '\x1b[A';
  if (key === 'ArrowDown') return '\x1b[B';
  if (key === 'ArrowRight') return '\x1b[C';
  if (key === 'ArrowLeft') return '\x1b[D';
  if (key === 'PageUp') {
    if (e.repeat) return null;
    return '\x1b[5~';
  }
  if (key === 'PageDown') {
    if (e.repeat) return null;
    return '\x1b[6~';
  }
  if (key === 'Home') return '\x1b[H';
  if (key === 'End') return '\x1b[F';
  if (key === 'Insert') return '\x1b[2~';

  // --- Single Character Pass-Through ---
  if (key.length === 1) return key;

  return null;
}
