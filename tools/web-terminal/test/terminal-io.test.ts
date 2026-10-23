import { describe, it, expect, beforeAll, vi } from 'vitest';
import fs from 'fs';
import path from 'path';

describe('Terminal I/O, Command History & Code Stats Suite', () => {
  let win: any;

  beforeAll(async () => {
    const htmlPath = path.resolve(__dirname, '../src/index.html');
    const html = fs.readFileSync(htmlPath, 'utf8');

    const bodyMatch = html.match(/<body[^>]*>([\s\S]*)<\/body>/i);
    const cleanBody = (bodyMatch ? bodyMatch[1] : html).replace(/<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi, '');
    document.body.innerHTML = cleanBody;

    window.alert = vi.fn();
    window.confirm = vi.fn(() => true);
    (window as any).navigator.serial = {
      addEventListener: vi.fn(),
      getPorts: vi.fn().mockResolvedValue([]),
      requestPort: vi.fn()
    };

    await import('../src/main');
    win = window as any;
  });

  describe('Command Input & History Navigation', () => {
    it('sends input box text with configured line ending and resets input', () => {
      const cmdInput = document.getElementById('cmd-input') as HTMLInputElement;
      const lineEnding = document.getElementById('line-ending') as HTMLSelectElement;

      cmdInput.value = 'help';
      lineEnding.value = '\r\n';

      win.sendInputBox();

      expect(cmdInput.value).toBe('');
    });

    it('navigates command history via ArrowUp and ArrowDown keys', () => {
      const cmdInput = document.getElementById('cmd-input') as HTMLInputElement;
      const lineEnding = document.getElementById('line-ending') as HTMLSelectElement;
      lineEnding.value = '\r\n';

      // Send three commands in order
      const commands = ['cmd1', 'cmd2', 'cmd3'];
      commands.forEach(cmd => {
        cmdInput.value = cmd;
        win.sendInputBox();
      });

      // Press ArrowUp: should retrieve most recent command ('cmd3')
      cmdInput.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
      expect(cmdInput.value).toBe('cmd3');

      // Press ArrowUp again: should retrieve 'cmd2'
      cmdInput.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
      expect(cmdInput.value).toBe('cmd2');

      // Press ArrowUp again: should retrieve 'cmd1'
      cmdInput.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
      expect(cmdInput.value).toBe('cmd1');

      // Press ArrowDown: should go back to 'cmd2'
      cmdInput.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true }));
      expect(cmdInput.value).toBe('cmd2');

      // Press ArrowDown again: should go back to 'cmd3'
      cmdInput.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true }));
      expect(cmdInput.value).toBe('cmd3');

      // Press ArrowDown at end: should clear input to blank
      cmdInput.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown', bubbles: true }));
      expect(cmdInput.value).toBe('');
    });
  });

  describe('Brainfuck Editor Code Statistics', () => {
    it('computes character count, line count, BF op count, and % code accurately', () => {
      const area = document.getElementById('write-code-area') as HTMLTextAreaElement;
      const meta = document.getElementById('write-editor-meta')!;
      const stats = document.getElementById('write-stats')!;
      const sizeBadge = document.getElementById('write-size-badge')!;
      const pruneBadge = document.getElementById('write-prune-badge')!;

      // 10 chars: 5 BF ops (><+-.) and 5 comment chars ('hello') across 2 lines
      area.value = '><+-.\nhello';
      win.updateWriteCodeStats();

      expect(meta.textContent).toContain('11 chars');
      expect(meta.textContent).toContain('2 lines');
      expect(meta.textContent).toContain('5 BF ops');
      expect(stats.textContent).toBe('11 Bytes | 5 BF Ops');
      expect(sizeBadge.textContent).toBe('11 B');
      expect(pruneBadge.textContent).toBe('45% Code'); // 5 / 11 = 45%
    });

    it('formats size in kB when code exceeds 1024 bytes', () => {
      const area = document.getElementById('write-code-area') as HTMLTextAreaElement;
      const sizeBadge = document.getElementById('write-size-badge')!;

      area.value = '+'.repeat(2048);
      win.updateWriteCodeStats();

      expect(sizeBadge.textContent).toBe('2.0 kB');
    });

    it('displays Empty badge when editor is blank', () => {
      const area = document.getElementById('write-code-area') as HTMLTextAreaElement;
      const pruneBadge = document.getElementById('write-prune-badge')!;

      area.value = '';
      win.updateWriteCodeStats();

      expect(pruneBadge.textContent).toBe('Empty');
    });
  });

  describe('Brainfuck Comment Pruning', () => {
    it('strips all non-BF characters while strictly preserving BF instructions', () => {
      const area = document.getElementById('write-code-area') as HTMLTextAreaElement;
      area.value = `
        +++ [ > +++++++ < - ] > .
        This is a multiline comment with numbers 12345!
        , . - + < > [ ]
      `;

      win.pruneEditorComments();

      // Only valid Brainfuck instructions: > < + - . , [ ]
      expect(area.value).toBe('+++[>+++++++<-]>.,.-+<>[]');
    });
  });
});
