import { describe, it, expect, beforeAll, vi } from 'vitest';
import fs from 'fs';
import path from 'path';

describe('Machine Protocol & Firmware State Synchronizer Suite', () => {
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

  describe('Dynamic Settings Menu Protocol (!MENU:*)', () => {
    it('parses dynamic menu stream into interactive settings cards and sync badge', () => {
      win.switchTab('settings');

      // Feed machine lines simulating STM32 Companion firmware dynamic menu dump
      win.processMachineLine('!MENU:START:4');
      win.processMachineLine('0;FOLDER;0;Execution & Clock;;');
      win.processMachineLine('10;SELECT;0;FPGA Clock Frequency;2;10 Hz;500 Hz;1 MHz;12 MHz');
      win.processMachineLine('11;TOGGLE;0;Direct Execution Mode;1;');
      win.processMachineLine('12;COMMAND;0;Reboot to DFU;;');
      win.processMachineLine('!MENU:END');

      const headerBadge = document.getElementById('dyn-header-badge');
      expect(headerBadge?.textContent).toBe('SYNCED (3)');
      expect(headerBadge?.className).toContain('synced');

      const grid = document.getElementById('settings-cards-grid');
      expect(grid?.children.length).toBeGreaterThan(0);

      const selectItem = document.getElementById('dyn-item-10') as HTMLSelectElement;
      expect(selectItem).not.toBeNull();
      expect(selectItem.value).toBe('2');

      const toggleItem = document.getElementById('dyn-item-11') as HTMLInputElement;
      expect(toggleItem).not.toBeNull();
      expect(toggleItem.checked).toBe(true);

      const cmdBtn = document.getElementById('dyn-item-12') as HTMLButtonElement;
      expect(cmdBtn).not.toBeNull();
      expect(cmdBtn.textContent?.trim()).toBe('Reboot to DFU');
    });

    it('handles !SET:OK confirmation and flashes affected setting row', () => {
      win.processMachineLine('!SET:OK:10=3');

      const row = document.getElementById('setting-row-10');
      expect(row?.classList.contains('setting-flash')).toBe(true);

      const clkDisplay = document.getElementById('clock-speed-val');
      expect(clkDisplay?.textContent).toBe('100 Hz'); // Index 3 is 100 Hz
    });

    it('handles !CMD:OK execution confirmation', () => {
      win.processMachineLine('!CMD:OK:12');

      const statusEl = document.getElementById('settings-strip-msg');
      expect(statusEl?.textContent).toContain('Executed: Reboot to DFU');
    });
  });

  describe('Dynamic Program Library Protocol (!LIB:*)', () => {
    it('parses program library schema and updates storage meter and slot count', () => {
      win.switchTab('write');

      // Simulate firmware library response: 2 programs, 1024 used bytes, 45056 total capacity
      win.processMachineLine('!LIB:START:2:1024:45056');
      win.processMachineLine('!LIB:PROG:0;Hello_World;128;1;0');
      win.processMachineLine('!LIB:PROG:1;Fibonacci;896;0;0');
      win.processMachineLine('!LIB:END');

      const slotsCount = document.getElementById('lib-slots-count');
      expect(slotsCount?.textContent).toBe('2 / 64 Slots');

      const storageText = document.getElementById('lib-storage-text');
      expect(storageText?.textContent).toContain('1.0 kB used');

      const tbody = document.getElementById('lib-programs-tbody');
      expect(tbody?.children.length).toBe(2);
      expect(tbody?.innerHTML).toContain('Hello_World');
    });
  });

  describe('Hardware Clock Query Detection', () => {
    it('detects clock frequency responses in terminal stream', () => {
      const clockRegex = /\[?Clock:\s*([^\]\r\n]+)\]?/i;

      const m1 = clockRegex.exec('[Clock: 12 MHz]');
      expect(m1).not.toBeNull();
      expect(m1![1].trim()).toBe('12 MHz');

      const m2 = clockRegex.exec('Clock: 500 kHz\r\n');
      expect(m2).not.toBeNull();
      expect(m2![1].trim()).toBe('500 kHz');

      const m3 = clockRegex.exec('[Clock: 4 MHz]');
      expect(m3).not.toBeNull();
      expect(m3![1].trim()).toBe('4 MHz');
    });
  });
});
