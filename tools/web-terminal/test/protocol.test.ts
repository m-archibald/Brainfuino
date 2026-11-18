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
