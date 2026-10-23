import { describe, it, expect, beforeAll, vi } from 'vitest';
import fs from 'fs';
import path from 'path';

describe('Flasher & Hardware Target Automation Suite', () => {
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

  describe('Hardware Target Switching', () => {
    it('configures UI for STM32 Companion MCU target', () => {
      win.setFlasherTarget('stm32');

      const stm32Pill = document.getElementById('pill-target-stm32')!;
      const fpgaPill = document.getElementById('pill-target-fpga')!;
      const revContainer = document.getElementById('flasher-fpga-rev-container')!;
      const stripStats = document.getElementById('flasher-strip-stats')!;
      const startBtnLabel = document.getElementById('flasher-start-label')!;
      const step2Stm32 = document.getElementById('flasher-step2-stm32')!;
      const step2Fpga = document.getElementById('flasher-step2-fpga')!;

      expect(stm32Pill.classList.contains('active')).toBe(true);
      expect(fpgaPill.classList.contains('active')).toBe(false);
      expect(revContainer.style.display).toBe('none');
      expect(stripStats.textContent).toBe('Target: STM32 Companion (0483:DF11)');
      expect(startBtnLabel.textContent).toBe('Flash STM32 MCU');
      expect(step2Stm32.style.display).toBe('flex');
      expect(step2Fpga.style.display).toBe('none');
    });

    it('configures UI for MachXO2 FPGA openFPGALoader target', () => {
      win.setFlasherTarget('fpga');

      const stm32Pill = document.getElementById('pill-target-stm32')!;
      const fpgaPill = document.getElementById('pill-target-fpga')!;
      const revContainer = document.getElementById('flasher-fpga-rev-container')!;
      const stripStats = document.getElementById('flasher-strip-stats')!;
      const startBtnLabel = document.getElementById('flasher-start-label')!;
      const step2Stm32 = document.getElementById('flasher-step2-stm32')!;
      const step2Fpga = document.getElementById('flasher-step2-fpga')!;

      expect(fpgaPill.classList.contains('active')).toBe(true);
      expect(stm32Pill.classList.contains('active')).toBe(false);
      expect(revContainer.style.display).toBe('block');
      expect(stripStats.textContent).toContain('MachXO2 FPGA');
      expect(startBtnLabel.textContent).toContain('openFPGALoader');
      expect(step2Stm32.style.display).toBe('none');
      expect(step2Fpga.style.display).toBe('flex');
    });

    it('switches FPGA hardware board revisions between 1.0 and 1.1', () => {
      const pill10 = document.getElementById('pill-rev-10')!;
      const pill11 = document.getElementById('pill-rev-11')!;
      const stripStats = document.getElementById('flasher-strip-stats')!;
      const startBtnLabel = document.getElementById('flasher-start-label')!;

      // Switch to Rev 1.0
      win.setFpgaBoardRev('1.0');
      expect(pill10.classList.contains('active')).toBe(true);
      expect(pill11.classList.contains('active')).toBe(false);
      expect(stripStats.textContent).toBe('Target: MachXO2 FPGA (Rev 1.0)');
      expect(startBtnLabel.textContent).toContain('Rev 1.0');

      // Switch to Rev 1.1
      win.setFpgaBoardRev('1.1');
      expect(pill11.classList.contains('active')).toBe(true);
      expect(pill10.classList.contains('active')).toBe(false);
      expect(stripStats.textContent).toBe('Target: MachXO2 FPGA (Rev 1.1)');
      expect(startBtnLabel.textContent).toContain('Rev 1.1');
    });
  });

  describe('JTAG Pinout Wiring Helper', () => {
    it('updates pinout reference for Raspberry Pi Pico (dirtyJtag)', () => {
      const cableSelect = document.getElementById('flasher-jtag-cable-select') as HTMLSelectElement;
      cableSelect.value = 'dirtyJtag';

      win.updateJtagPinoutHelper();

      const titleEl = document.getElementById('flasher-jtag-pinout-title')!;
      const pinsEl = document.getElementById('flasher-jtag-pinout-pins')!;

      expect(titleEl.textContent).toContain('Raspberry Pi Pico');
      expect(pinsEl.innerHTML).toContain('GPIO18');
      expect(pinsEl.innerHTML).toContain('TCK');
    });

    it('updates pinout reference for FTDI FT2232H / FT232H adapter', () => {
      const cableSelect = document.getElementById('flasher-jtag-cable-select') as HTMLSelectElement;
      cableSelect.value = 'ft2232';

      win.updateJtagPinoutHelper();

      const titleEl = document.getElementById('flasher-jtag-pinout-title')!;
      const pinsEl = document.getElementById('flasher-jtag-pinout-pins')!;

      expect(titleEl.textContent).toContain('FT2232H');
      expect(pinsEl.children.length).toBeGreaterThan(3);
    });
  });
});
