export interface AnsiStyle {
  bold: boolean;
  underline: boolean;
  inverse: boolean;
  fgColor: string | null;
}

export interface TerminalCell {
  char: string;
  style: AnsiStyle;
}

export interface TerminalLine {
  el: HTMLDivElement;
  cells: TerminalCell[];
}

export interface TerminalState {
  ansiState: AnsiStyle;
  atLineStart: boolean;
  totalRenderedLines: number;
  streamBuffer: string;
  is2D: boolean;
  cursorRow: number;
  cursorCol: number;
  savedRow?: number;
  savedCol?: number;
  isCapturingOverlay?: boolean;
  overlaySpan?: HTMLSpanElement | null;
  lineMap: TerminalLine[];
}

export interface TerminalRenderOptions {
  autoscroll?: boolean;
  enableTimestamps?: boolean;
  chunkTimestamp?: number;
  maxScrollbackLines?: number;
}

export function createTerminalState(): TerminalState {
  return {
    ansiState: { bold: false, underline: false, inverse: false, fgColor: null },
    atLineStart: true,
    totalRenderedLines: 0,
    streamBuffer: '',
    is2D: false,
    cursorRow: 0,
    cursorCol: 0,
    savedRow: 0,
    savedCol: 0,
    isCapturingOverlay: false,
    overlaySpan: null,
    lineMap: []
  };
}

export function formatTimestamp(timestampMs?: number): string {
  const now = timestampMs !== undefined 
    ? timestampMs 
    : (performance.timeOrigin ? (performance.timeOrigin + performance.now()) : Date.now());
  const d = new Date(now);
  const h = String(d.getHours()).padStart(2, '0');
  const m = String(d.getMinutes()).padStart(2, '0');
  const s = String(d.getSeconds()).padStart(2, '0');
  
  const ms = String(d.getMilliseconds()).padStart(3, '0');
  const tenth = Math.min(9, Math.max(0, Math.floor(((now % 1) + 1e-5) * 10)));
  return `[${h}:${m}:${s}.${ms}${tenth}] `;
}

export function createTimestampSpan(timestampMs?: number): HTMLSpanElement {
  const span = document.createElement('span');
  span.className = 'terminal-timestamp';
  span.textContent = formatTimestamp(timestampMs);
  return span;
}

export function createStyledSpanWithState(text: string, ansi: AnsiStyle): HTMLSpanElement {
  if (text.includes('\b')) {
    let clean: string[] = [];
    for (let char of text) {
      if (char === '\b') {
        if (clean.length > 0) clean.pop();
      } else {
        clean.push(char);
      }
    }
    text = clean.join('');
  }

  const span = document.createElement('span');
  let classes: string[] = [];
  if (ansi.bold) classes.push('ansi-bold');
  if (ansi.underline) classes.push('ansi-underline');
  if (ansi.inverse) classes.push('ansi-inverse');
  if (ansi.fgColor) {
    if (ansi.fgColor.startsWith('rgb(') || ansi.fgColor.startsWith('#')) {
      span.style.color = ansi.fgColor;
    } else {
      classes.push(`ansi-${ansi.fgColor}`);
    }
  }

  if (classes.length > 0) span.className = classes.join(' ');
  span.textContent = text;
  return span;
}

export function appendTextToTerminal(
  screenEl: HTMLElement,
  rawText: string,
  state: TerminalState,
  options: TerminalRenderOptions = {}
): void {
  if (!screenEl || !rawText) return;

  const autoscroll = options.autoscroll !== undefined ? options.autoscroll : true;
  const enableTimestamps = options.enableTimestamps || false;
  const chunkTimestamp = options.chunkTimestamp;
  const maxLines = options.maxScrollbackLines || 2000;

  // Handle stream defragmentation: prepend leftover partial ANSI sequence from prior chunk
  if (state.streamBuffer) {
    rawText = state.streamBuffer + rawText;
    state.streamBuffer = '';
  }

  // Buffer any incomplete trailing ANSI sequence (e.g. '\x1b', '\x1b[', '\x1b[1;2')
  const partialAnsi = /\x1b(\[[0-9;]*)?$/.exec(rawText);
  if (partialAnsi) {
    state.streamBuffer = partialAnsi[0];
    rawText = rawText.slice(0, -state.streamBuffer.length);
  }
  if (!rawText) return;

  // Regex matching ANSI escape sequences: \x1b[...m or \x1b[...H / J / K / s / u / etc.
  const ansiRegex = /\x1b\[([0-9;]*)([a-zA-Z])/g;
  let lastIndex = 0;
  let match: RegExpExecArray | null;
  let fragment = document.createDocumentFragment();

  const appendPlainTextSegments = (text: string) => {
    if (!text) return;
    if (text.includes('\b')) {
      let clean: string[] = [];
      for (let char of text) {
        if (char === '\b') {
          if (clean.length > 0) clean.pop();
        } else {
          clean.push(char);
        }
      }
      text = clean.join('');
    }
    if (!text) return;

    // In 2D cursor-addressable mode, overwrite lines in-place preserving styles
    if (state.is2D) {
      const parts = text.split('\n');
      for (let i = 0; i < parts.length; i++) {
        let part = parts[i];
        const hasCR = part.includes('\r');
        if (hasCR) {
          part = part.replace(/\r/g, '');
        }
        if (part.length > 0) {
          while (state.lineMap.length <= state.cursorRow) {
            const lineDiv = document.createElement('div');
            lineDiv.className = 'term-2d-line';
            screenEl.appendChild(lineDiv);
            state.lineMap.push({ el: lineDiv, cells: [] });
          }
          const lineObj = state.lineMap[state.cursorRow];
          if (!lineObj.cells) lineObj.cells = [];
          const col = state.cursorCol || 0;
          while (lineObj.cells.length < col) {
            lineObj.cells.push({ char: ' ', style: { bold: false, underline: false, inverse: false, fgColor: null } });
          }
          for (let ci = 0; ci < part.length; ci++) {
            lineObj.cells[col + ci] = { char: part[ci], style: { ...state.ansiState } };
          }
          state.cursorCol = col + part.length;

          // Render styled spans for this line
          lineObj.el.innerHTML = '';
          let curKey: string | null = null;
          let curText = '';
          let activeStyle: AnsiStyle | null = null;
          for (let ci = 0; ci < lineObj.cells.length; ci++) {
            const cell = lineObj.cells[ci] || { char: ' ', style: { bold: false, underline: false, inverse: false, fgColor: null } };
            const s = cell.style;
            const k = (s.bold ? 'B' : '') + (s.underline ? 'U' : '') + (s.inverse ? 'I' : '') + ':' + (s.fgColor || '');
            if (k !== curKey) {
              if (curText.length > 0 && activeStyle) {
                lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
                curText = '';
              }
              curKey = k;
              activeStyle = { ...s };
            }
            curText += cell.char;
          }
          if (curText.length > 0 && activeStyle) {
            lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
          }
        }
        if (hasCR) {
          state.cursorCol = 0;
        }
        if (i < parts.length - 1) {
          state.cursorRow++;
          state.cursorCol = 0;
        }
      }
      return;
    }

    // Split text by newlines so we can place timestamp spans exactly at line starts
    const parts = text.split('\n');
    for (let i = 0; i < parts.length; i++) {
      const part = parts[i].replace(/\r/g, '');
      if (part.length > 0) {
        if (state.isCapturingOverlay && state.overlaySpan) {
          state.overlaySpan.appendChild(createStyledSpanWithState(part, state.ansiState));
        } else {
          if (state.overlaySpan && state.overlaySpan.parentNode) {
            state.overlaySpan.parentNode.removeChild(state.overlaySpan);
            state.overlaySpan = null;
          }
          if (enableTimestamps && state.atLineStart) {
            fragment.appendChild(createTimestampSpan(chunkTimestamp));
            state.atLineStart = false;
          }
          fragment.appendChild(createStyledSpanWithState(part, state.ansiState));
          state.atLineStart = false;
        }
      }

      if (i < parts.length - 1) {
        if (state.overlaySpan && state.overlaySpan.parentNode) {
          state.overlaySpan.parentNode.removeChild(state.overlaySpan);
          state.overlaySpan = null;
        }
        fragment.appendChild(document.createTextNode('\n'));
        state.atLineStart = true;
      }
    }
  };

  while ((match = ansiRegex.exec(rawText)) !== null) {
    if (match.index > lastIndex) {
      const plainText = rawText.substring(lastIndex, match.index);
      appendPlainTextSegments(plainText);
    }

    const params = match[1] ? match[1].split(';').map(n => parseInt(n, 10)) : [0];
    const command = match[2];

    if (command === 'm') {
      for (let pi = 0; pi < params.length; pi++) {
        let code = params[pi];
        if (code === 0) {
          state.ansiState.bold = false;
          state.ansiState.underline = false;
          state.ansiState.inverse = false;
          state.ansiState.fgColor = null;
        } else if (code === 1) {
          state.ansiState.bold = true;
        } else if (code === 4) {
          state.ansiState.underline = true;
        } else if (code === 7) {
          state.ansiState.inverse = true;
        } else if (code === 22) {
          state.ansiState.bold = false;
        } else if (code === 24) {
          state.ansiState.underline = false;
        } else if (code === 27) {
          state.ansiState.inverse = false;
        } else if (code === 38) {
          if (params[pi + 1] === 5 && pi + 2 < params.length) {
            const colorIdx = params[pi + 2];
            pi += 2;
            if (colorIdx >= 16 && colorIdx <= 231) {
              const cube = [0, 95, 135, 175, 215, 255];
              const r = cube[Math.floor((colorIdx - 16) / 36)];
              const g = cube[Math.floor(((colorIdx - 16) % 36) / 6)];
              const b = cube[(colorIdx - 16) % 6];
              state.ansiState.fgColor = `rgb(${r},${g},${b})`;
            } else if (colorIdx >= 232 && colorIdx <= 255) {
              const gray = 8 + (colorIdx - 232) * 10;
              state.ansiState.fgColor = `rgb(${gray},${gray},${gray})`;
            } else if (colorIdx >= 0 && colorIdx < 16) {
              const colors = ['black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan', 'white', 'bright-black', 'bright-red', 'bright-green', 'bright-yellow', 'bright-blue', 'bright-magenta', 'bright-cyan', 'bright-white'];
              state.ansiState.fgColor = colors[colorIdx];
            }
          }
        } else if (code >= 30 && code <= 37) {
          const colors = ['black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan', 'white'];
          state.ansiState.fgColor = colors[code - 30];
        } else if (code === 39) {
          state.ansiState.fgColor = null;
        } else if (code >= 90 && code <= 97) {
          const brightColors = ['bright-black', 'bright-red', 'bright-green', 'bright-yellow', 'bright-blue', 'bright-magenta', 'bright-cyan', 'bright-white'];
          state.ansiState.fgColor = brightColors[code - 90];
        }
      }
    } else if (command === 'J') {
      // Clear screen: \x1b[2J or \x1b[J
      screenEl.innerHTML = '';
      fragment = document.createDocumentFragment();
      state.totalRenderedLines = 0;
      state.atLineStart = true;
      state.cursorRow = 0;
      state.cursorCol = 0;
      state.lineMap = [];
      state.is2D = false;
    } else if (command === 'H' || command === 'f') {
      // Cursor Home: \x1b[H or \x1b[row;colH
      const r = (params && params[0] > 0) ? params[0] - 1 : 0;
      const c = (params && params.length > 1 && params[1] > 0) ? params[1] - 1 : 0;
      state.cursorRow = r;
      state.cursorCol = c;
      state.is2D = true;
      state.lineMap = state.lineMap || [];
    } else if (command === 's') {
      // Save Cursor Position (ANSI SCP: \x1b[s)
      if (state.is2D) {
        state.savedRow = state.cursorRow;
        state.savedCol = state.cursorCol;
      } else {
        if (fragment.hasChildNodes()) {
          screenEl.appendChild(fragment);
          fragment = document.createDocumentFragment();
        }
        if (state.overlaySpan && state.overlaySpan.parentNode) {
          state.overlaySpan.innerHTML = '';
        } else {
          state.overlaySpan = document.createElement('span');
          state.overlaySpan.className = 'ansi-overlay-badge';
          screenEl.appendChild(state.overlaySpan);
        }
        state.isCapturingOverlay = true;
      }
    } else if (command === 'u') {
      // Restore Cursor Position (ANSI RCP: \x1b[u)
      if (state.is2D) {
        state.cursorRow = (state.savedRow !== undefined) ? state.savedRow : 0;
        state.cursorCol = (state.savedCol !== undefined) ? state.savedCol : 0;
      } else {
        state.isCapturingOverlay = false;
      }
    } else if (command === 'K') {
      // Erase in Line (ANSI EL: \x1b[K)
      if (state.is2D) {
        if (state.lineMap && state.lineMap[state.cursorRow]) {
          const lineObj = state.lineMap[state.cursorRow];
          if (lineObj.cells) {
            lineObj.cells.length = state.cursorCol;
            lineObj.el.innerHTML = '';
            let curKey: string | null = null, curText = '', activeStyle: AnsiStyle | null = null;
            for (let ci = 0; ci < lineObj.cells.length; ci++) {
              const cell = lineObj.cells[ci] || { char: ' ', style: { bold: false, underline: false, inverse: false, fgColor: null } };
              const s = cell.style;
              const k = (s.bold ? 'B' : '') + (s.underline ? 'U' : '') + (s.inverse ? 'I' : '') + ':' + (s.fgColor || '');
              if (k !== curKey) {
                if (curText.length > 0 && activeStyle) {
                  lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
                  curText = '';
                }
                curKey = k;
                activeStyle = { ...s };
              }
              curText += cell.char;
            }
            if (curText.length > 0 && activeStyle) {
              lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
            }
          }
        }
      } else {
        if (state.overlaySpan && state.overlaySpan.parentNode) {
          state.overlaySpan.parentNode.removeChild(state.overlaySpan);
          state.overlaySpan = null;
        }
        state.isCapturingOverlay = false;
      }
    } else if (command === 'A') {
      const n = (params && params[0] > 0) ? params[0] : 1;
      state.cursorRow = Math.max(0, state.cursorRow - n);
      state.is2D = true;
    } else if (command === 'B') {
      const n = (params && params[0] > 0) ? params[0] : 1;
      state.cursorRow += n;
      state.is2D = true;
    } else if (command === 'C') {
      const n = (params && params[0] > 0) ? params[0] : 1;
      state.cursorCol += n;
      state.is2D = true;
    } else if (command === 'D') {
      const n = (params && params[0] > 0) ? params[0] : 1;
      state.cursorCol = Math.max(0, state.cursorCol - n);
      state.is2D = true;
    }

    lastIndex = ansiRegex.lastIndex;
  }

  if (lastIndex < rawText.length) {
    const remainingText = rawText.substring(lastIndex);
    appendPlainTextSegments(remainingText);
  }

  const newLinesInBatch = (rawText.match(/\n/g) || []).length;
  state.totalRenderedLines += newLinesInBatch;

  screenEl.appendChild(fragment);

  if (state.totalRenderedLines > maxLines) {
    while (state.totalRenderedLines > maxLines && screenEl.firstChild) {
      const first = screenEl.firstChild;
      const nlInFirst = (first.textContent ? (first.textContent.match(/\n/g) || []).length : 0);
      state.totalRenderedLines -= nlInFirst;
      screenEl.removeChild(first);
    }
    if (state.totalRenderedLines < 0) state.totalRenderedLines = 0;
  }

  while (screenEl.childNodes.length > 3000) {
    if (screenEl.firstChild) screenEl.removeChild(screenEl.firstChild);
  }

  if (autoscroll && !state.is2D) {
    screenEl.scrollTop = screenEl.scrollHeight;
  }
}
