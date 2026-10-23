export function mapDirectKey(e: KeyboardEvent): string | null {
  // Allow Ctrl+C / Ctrl+V clipboard operations if text is selected
  if ((e.ctrlKey || e.metaKey) && window.getSelection && window.getSelection()!.toString().length > 0) {
    return null;
  }

  const key = e.key;

  if (key === 'Enter') return '\r';
  if (key === 'Backspace') return '\b';
  if (key === 'Delete') return '\x1b[3~';
  if (key === 'Tab') return '\t';
  if (key === 'Escape') return '\x1b';
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
  if (key.length === 1) return key;

  return null;
}
