import { describe, it, expect, beforeEach } from 'vitest';
import { createTerminalState, appendTextToTerminal } from '../src/terminal/ansi-parser';

describe('ANSI Terminal Parser & Cursor Engine', () => {
  let container: HTMLElement;
  let state: ReturnType<typeof createTerminalState>;

  beforeEach(() => {
    container = document.createElement('div');
    state = createTerminalState();
  });

  it('parses plain text and SGR styles correctly', () => {
    appendTextToTerminal(container, '\x1b[1;32mHello \x1b[0mWorld\n', state);
    const spans = container.querySelectorAll('span');
    expect(spans.length).toBe(2);
    expect(spans[0].textContent).toBe('Hello ');
    expect(spans[0].className).toContain('ansi-bold');
    expect(spans[0].className).toContain('ansi-green');
    expect(spans[1].textContent).toBe('World');
  });

  it('overwrites speed overlay in place using \\x1b[s and \\x1b[u (no horizontal duplicates)', () => {
    // Simulate initial text on terminal
    appendTextToTerminal(container, 'Brainfuino ready: ', state);
    expect(container.textContent).toBe('Brainfuino ready: ');

    // First speed badge arrives: [Clock: 4 MHz]
    appendTextToTerminal(container, '\x1b[s\x1b[32m[Clock: 4 MHz]    \x1b[0m\x1b[u', state);
    expect(container.textContent).toBe('Brainfuino ready: [Clock: 4 MHz]    ');

    // Second speed badge arrives: [Clock: 6 MHz]
    appendTextToTerminal(container, '\x1b[s\x1b[32m[Clock: 6 MHz]    \x1b[0m\x1b[u', state);
    // MUST OVERWRITE IN PLACE: Must NOT append side-by-side!
    expect(container.textContent).toBe('Brainfuino ready: [Clock: 6 MHz]    ');

    // Third speed badge arrives: [Clock: 8 MHz]
    appendTextToTerminal(container, '\x1b[s\x1b[32m[Clock: 8 MHz]    \x1b[0m\x1b[u', state);
    expect(container.textContent).toBe('Brainfuino ready: [Clock: 8 MHz]    ');

    // Verify exactly one overlay badge element exists
    const badges = container.querySelectorAll('.ansi-overlay-badge');
    expect(badges.length).toBe(1);
  });

  it('retires the temporary speed badge when real FPGA output arrives', () => {
    appendTextToTerminal(container, 'Prompt: ', state);
    appendTextToTerminal(container, '\x1b[s[Clock: 12 MHz]   \x1b[u', state);
    expect(container.textContent).toBe('Prompt: [Clock: 12 MHz]   ');

    // Real output arrives from FPGA
    appendTextToTerminal(container, 'User Program Running!\n', state);
    expect(container.textContent).toBe('Prompt: User Program Running!\n');
    expect(container.querySelectorAll('.ansi-overlay-badge').length).toBe(0);
  });

  it('handles 2D grid mode and in-place overwriting', () => {
    appendTextToTerminal(container, '\x1b[HRow 0 Col 0\nRow 1 Col 0', state);
    expect(state.is2D).toBe(true);
    expect(state.lineMap.length).toBe(2);

    // Reposition cursor to home and overwrite characters
    appendTextToTerminal(container, '\x1b[HABC', state);
    expect(state.lineMap[0].cells.slice(0, 3).map(c => c.char).join('')).toBe('ABC');
  });
});
