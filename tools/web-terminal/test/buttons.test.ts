import { describe, it, expect, beforeAll, vi } from 'vitest';
import fs from 'fs';
import path from 'path';

describe('Web Terminal Buttons & Interactive Handlers Test Suite', () => {
  let html: string;
  let win: any;

  beforeAll(async () => {
    const htmlPath = path.resolve(__dirname, '../src/index.html');
    html = fs.readFileSync(htmlPath, 'utf8');

    // Extract body HTML and strip script tags to avoid happy-dom module fetch warnings
    const bodyMatch = html.match(/<body[^>]*>([\s\S]*)<\/body>/i);
    const cleanBody = (bodyMatch ? bodyMatch[1] : html).replace(/<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi, '');
    document.body.innerHTML = cleanBody;
    document.documentElement.setAttribute('data-theme', 'light');

    // Mock window APIs for unit test environment
    window.alert = vi.fn();
    window.confirm = vi.fn(() => true);
    window.scrollTo = vi.fn();
    (window as any).navigator.serial = {
      addEventListener: vi.fn(),
      getPorts: vi.fn().mockResolvedValue([]),
      requestPort: vi.fn()
    };

    // Import main.ts which binds event handlers and populates window
    await import('../src/main');
    win = window as any;
  });

  describe('Global Window Handlers Verification', () => {
    it('exposes all required interactive event handlers on window', () => {
      const requiredHandlers = [
        'toggleConnect',
        'switchTab',
        'setInputMode',
        'sendCmd',
        'queryClockSpeed',
        'stepClockDown',
        'stepClockUp',
        'triggerDfuReboot',
        'fetchGitHubReleases',
        'renderGitHubProgramCatalog',
        'toggleCategoryGroup',
        'loadGitHubProgram',
        'runLibrarySlot',
        'dumpLibrarySlot',
        'deleteLibrarySlot',
        'fetchDynamicMenu',
        'toggleTheme',
        'attemptAutoConnect',
        'isAutoConnectEnabled',
        'handleDeviceUnplugged',
        'saveLog',
        'clearTerminal',
        'toggleMessageHistory',
        'toggleFullscreen',
        'cycleInputMode',
        'sendInputBox',
        'filterDynamicSettings',
        'loadDefaultPreviewSchema',
        'onReleaseSelectChange',
        'setFlasherTarget',
        'setFpgaBoardRev',
        'handleFlasherFileSelect',
        'connectDfuDevice',
        'updateJtagPinoutHelper',
        'connectJtagDevice',
        'startFlashingProcess',
        'queryDynamicLibrary',
        'loadDefaultLogoToEditor',
        'pruneEditorComments',
        'clearWriteEditor',
        'handleTerminalDragOver',
        'handleTerminalDragLeave',
        'handleTerminalDrop',
        'handleTerminalFileInput',
        'handleTerminalCodeFile',
        'handleWriteCodeFileClick',
        'handleWriteDragOver',
        'handleWriteDragLeave',
        'handleWriteDrop',
        'handleWriteFileInput',
        'updateWriteCodeStats',
        'executeWriteLiveRun',
        'executeWriteToLibrary',
        'clearVerifyTerminal',
        'abortVerification',
        'commitVerificationNow',
        'cycleVerifyInputMode',
        'sendVerifyInputBox',
        'clearMessageHistory',
        'setMaxScrollbackLines',
        'postSystemMessage'
      ];

      for (const fnName of requiredHandlers) {
        expect(typeof win[fnName], `Expected window.${fnName} to be a function`).toBe('function');
      }
    });

    it('validates every inline event handler in src/index.html points to a valid function', () => {
      const inlineHandlerRegex = /\s(on[a-z]+)=["']([^"']+)["']/gi;
      let match;
      const verifiedFns = new Set<string>();

      while ((match = inlineHandlerRegex.exec(html)) !== null) {
        const handlerCode = match[2];
        const fnCallRegex = /(?:window\.)?([a-zA-Z0-9_$]+)\s*\(/g;
        let fnMatch;
        while ((fnMatch = fnCallRegex.exec(handlerCode)) !== null) {
          const fnName = fnMatch[1];
          if (['parseInt', 'alert', 'confirm', 'console', 'preventDefault', 'stopPropagation'].includes(fnName)) {
            continue;
          }
          if (fnName === 'getElementById') {
            continue;
          }
          expect(typeof win[fnName], `Inline handler '${handlerCode}' calls missing function '${fnName}'`).toBe('function');
          verifiedFns.add(fnName);
        }
      }

      expect(verifiedFns.size).toBeGreaterThan(20);
    });

    it('verifies all <button> elements in HTML have an interactive trigger', () => {
      const buttons = document.querySelectorAll('button');
      expect(buttons.length).toBeGreaterThan(15);

      buttons.forEach((btn, idx) => {
        const hasOnClick = btn.hasAttribute('onclick');
        const hasId = Boolean(btn.id);
        const hasTypeSubmit = btn.getAttribute('type') === 'submit';
        const isInteractive = hasOnClick || hasId || hasTypeSubmit;
        expect(
          isInteractive,
          `Button at index ${idx} with text "${btn.textContent?.trim()}" has neither onclick nor id`
        ).toBe(true);
      });
    });
  });

  describe('Tab Navigation Buttons', () => {
    it('switches between Terminal, Write/Program Suite, Settings, and Flasher views', () => {
      const viewTerminal = document.getElementById('view-terminal')!;
      const viewWrite = document.getElementById('view-write')!;
      const viewControls = document.getElementById('view-controls')!;
      const viewFlashing = document.getElementById('view-flashing')!;

      // 1. Switch to Write / Program Suite
      win.switchTab('write');
      expect(viewWrite.style.display).toBe('flex');
      expect(viewTerminal.style.display).toBe('none');
      expect(viewControls.style.display).toBe('none');
      expect(viewFlashing.style.display).toBe('none');

      // 2. Switch to Settings
      win.switchTab('settings');
      expect(viewControls.style.display).toBe('flex');
      expect(viewWrite.style.display).toBe('none');
      expect(viewTerminal.style.display).toBe('none');

      // 3. Switch to Flasher
      win.switchTab('flash');
      expect(viewFlashing.style.display).toBe('flex');
      expect(viewControls.style.display).toBe('none');

      // 4. Switch back to Terminal
      win.switchTab('terminal');
      expect(viewTerminal.style.display).toBe('flex');
      expect(viewFlashing.style.display).toBe('none');
    });
  });

  describe('Program Suite Interactive Tree & Loading', () => {
    it('renders GitHub Program Catalog tree with categories and programs', () => {
      win.renderGitHubProgramCatalog();
      const tree = document.getElementById('github-programs-tree')!;
      expect(tree.children.length).toBeGreaterThan(0);

      const folders = tree.querySelectorAll('.github-tree-folder');
      expect(folders.length).toBeGreaterThan(0);

      const leaves = tree.querySelectorAll('.github-tree-leaf');
      expect(leaves.length).toBeGreaterThan(10);
    });

    it('toggles program category groups open and closed', () => {
      win.renderGitHubProgramCatalog();
      const catBody = document.getElementById('github-cat-body-0')!;
      const chevron = document.getElementById('github-chevron-0')!;

      // First toggle: collapse
      win.toggleCategoryGroup(0);
      expect(catBody.style.display).toBe('none');
      expect(chevron.style.transform).toBe('rotate(0deg)');

      // Second toggle: expand
      win.toggleCategoryGroup(0);
      expect(catBody.style.display).toBe('block');
      expect(chevron.style.transform).toBe('rotate(90deg)');
    });

    it('loads a program from the Program Suite into Write editor', async () => {
      const codeArea = document.getElementById('write-code-area') as HTMLTextAreaElement;
      const nameInput = document.getElementById('write-prog-name') as HTMLInputElement;

      expect(codeArea).not.toBeNull();
      expect(nameInput).not.toBeNull();

      // Load banner.b which is in the built-in offline PROGRAM_CACHE
      await win.loadGitHubProgram('basic/banner.b', 'Banner', 'banner.b');

      expect(nameInput.value).toBe('banner');
      expect(codeArea.value.length).toBeGreaterThan(20);
      expect(codeArea.value).toContain('+++++++');
    });

    it('loads another program from the Program Suite (fibonacci)', async () => {
      const codeArea = document.getElementById('write-code-area') as HTMLTextAreaElement;
      const nameInput = document.getElementById('write-prog-name') as HTMLInputElement;

      await win.loadGitHubProgram('math/fibonacci.b', 'Fibonacci Sequence', 'fibonacci.b');

      expect(nameInput.value).toBe('fibonacci');
      expect(codeArea.value.length).toBeGreaterThan(100);
    });
  });

  describe('Editor & Utility Action Buttons', () => {
    it('loads default logo to editor via loadDefaultLogoToEditor', () => {
      const codeArea = document.getElementById('write-code-area') as HTMLTextAreaElement;
      const nameInput = document.getElementById('write-prog-name') as HTMLInputElement;
      win.loadDefaultLogoToEditor();
      expect(nameInput.value).toBe('Brainfuino');
      expect(codeArea.value.length).toBeGreaterThan(50);
      expect(codeArea.value).toContain('+++++');
    });

    it('prunes editor comments via pruneEditorComments', () => {
      const codeArea = document.getElementById('write-code-area') as HTMLTextAreaElement;
      codeArea.value = '++ This is a comment -- [>] . ,';
      win.pruneEditorComments();
      expect(codeArea.value).toBe('++--[>].,');
    });

    it('clears editor via clearWriteEditor', () => {
      const codeArea = document.getElementById('write-code-area') as HTMLTextAreaElement;
      codeArea.value = '++[>++]';
      win.clearWriteEditor();
      expect(codeArea.value).toBe('');
    });

    it('toggles theme between light and dark', () => {
      const htmlEl = document.documentElement;
      htmlEl.setAttribute('data-theme', 'light');

      win.toggleTheme();
      expect(htmlEl.getAttribute('data-theme')).toBe('dark');

      win.toggleTheme();
      expect(htmlEl.getAttribute('data-theme')).toBe('light');
    });

    it('cycles input mode between direct and line', () => {
      win.setInputMode('direct', false);
      win.cycleInputMode();
      const select = document.getElementById('input-mode-select') as HTMLSelectElement;
      expect(select?.value).toBe('line');

      win.cycleInputMode();
      expect(select?.value).toBe('direct');
    });

    it('clears terminal buffer via clearTerminal', () => {
      const terminalScreen = document.getElementById('terminal-screen')!;
      terminalScreen.innerHTML = '<div>Line 1</div><div>Line 2</div>';

      win.clearTerminal();
      expect(terminalScreen.innerHTML).toBe('');
    });
  });

  describe('Dynamic Library Slot Buttons', () => {
    it('exposes runLibrarySlot, dumpLibrarySlot, and deleteLibrarySlot functions', () => {
      expect(typeof win.runLibrarySlot).toBe('function');
      expect(typeof win.dumpLibrarySlot).toBe('function');
      expect(typeof win.deleteLibrarySlot).toBe('function');

      // Calling when disconnected triggers alert without uncaught exception
      win.runLibrarySlot(0);
      expect(window.alert).toHaveBeenCalledWith('Please connect Brainfuino first.');

      win.dumpLibrarySlot(0);
      expect(window.alert).toHaveBeenCalledWith('Please connect Brainfuino first.');

      win.deleteLibrarySlot(0, 'test');
      expect(window.alert).toHaveBeenCalledWith('Please connect Brainfuino first.');
    });
  });

  describe('Dynamic Settings Schema & Clock Adjustment Buttons', () => {
    it('loads default preview schema into settings view', () => {
      win.switchTab('settings');
      win.loadDefaultPreviewSchema();

      const grid = document.getElementById('settings-cards-grid')!;
      expect(grid).not.toBeNull();
      expect(grid.children.length).toBeGreaterThan(0);
    });

    it('filters settings cards via filterDynamicSettings', () => {
      const searchInput = document.getElementById('settings-search-input') as HTMLInputElement;
      expect(searchInput).not.toBeNull();
      searchInput.value = 'baud';
      win.filterDynamicSettings('baud');

      const cards = document.querySelectorAll('.settings-card');
      expect(cards.length).toBeGreaterThan(0);
    });

    it('handles clock speed adjustments safely when disconnected', () => {
      // stepClockUp, stepClockDown, queryClockSpeed should run without throwing
      expect(() => win.queryClockSpeed()).not.toThrow();
      expect(() => win.stepClockUp()).not.toThrow();
      expect(() => win.stepClockDown()).not.toThrow();
    });

    it('handles terminal drag-and-drop overlay and write code file button', () => {
      const overlay = document.getElementById('terminal-drop-overlay');
      expect(overlay).not.toBeNull();

      // Drag over shows overlay
      const dragOverEvent = new Event('dragover', { bubbles: true });
      win.handleTerminalDragOver(dragOverEvent);
      expect(overlay!.style.display).toBe('flex');

      // Drag leave hides overlay
      const dragLeaveEvent = new Event('dragleave', { bubbles: true });
      win.handleTerminalDragLeave(dragLeaveEvent);
      // Drop also hides overlay
      const dropEvent = new Event('drop', { bubbles: true });
      win.handleTerminalDrop(dropEvent);
      expect(overlay!.style.display).toBe('none');

      // Write code file button exists
      const writeBtn = document.getElementById('btn-write-code-file');
      expect(writeBtn).not.toBeNull();
      const fileInput = document.getElementById('terminal-file-input') as HTMLInputElement;
      expect(fileInput).not.toBeNull();

      const clickSpy = vi.spyOn(fileInput, 'click');
      win.handleWriteCodeFileClick();
      expect(clickSpy).toHaveBeenCalled();
    });
  });
});
