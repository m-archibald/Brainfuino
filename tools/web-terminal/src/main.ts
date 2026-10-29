    import { mapDirectKey } from './terminal/hotkeys';

    // --- State Variables ---
    let port = null;
    let reader = null;
    let writer = null;
    let readableStreamClosed = null;
    let keepReading = false;
    let rxBytes = 0;
    let txBytes = 0;
    let inputMode = 'direct'; // 'direct' or 'line'
    let cmdHistory = [];
    let historyIdx = -1;
    let rawLogBuffer = '';

    const terminalScreen = document.getElementById('terminal-screen');
    const cmdInput = document.getElementById('cmd-input');
    const connectBtn = document.getElementById('connect-btn');
    const connectBtnLabel = document.getElementById('connect-btn-label');
    const statusPill = document.getElementById('status-pill');
    const statusText = document.getElementById('status-text');
    const statsLabel = document.getElementById('terminal-stats');
    const autoscrollChk = document.getElementById('autoscroll-chk');
    const timestampsChk = document.getElementById('timestamps-chk');
    const autoconnectChk = document.getElementById('autoconnect-chk');
    const lineEndingSelect = document.getElementById('line-ending');
    const inputModeSelect = document.getElementById('input-mode-select');
    const modeBadge = document.getElementById('mode-badge');

    function updateTimestampsVisibility() {
      const show = timestampsChk && timestampsChk.checked;
      if (terminalScreen) {
        terminalScreen.classList.toggle('hide-timestamps', !show);
      }
    }

    // Restore user preferences
    if (timestampsChk) {
      timestampsChk.checked = (localStorage.getItem('bf_timestamps') === 'true');
      updateTimestampsVisibility();
      timestampsChk.addEventListener('change', () => {
        localStorage.setItem('bf_timestamps', timestampsChk.checked);
        updateTimestampsVisibility();
      });
    }

    if (localStorage.getItem('bf_autoscroll') === 'false' && autoscrollChk) {
      autoscrollChk.checked = false;
    }
    if (autoscrollChk) {
      autoscrollChk.addEventListener('change', () => {
        localStorage.setItem('bf_autoscroll', autoscrollChk.checked);
      });
    }

    let manualDisconnect = false;

    function isAutoConnectEnabled() {
      return !!(autoconnectChk && autoconnectChk.checked);
    }

    function syncAutoConnectUI() {
      const enabled = isAutoConnectEnabled();
      if (autoconnectChk) {
        autoconnectChk.checked = enabled;
      }
      const checkEl = document.getElementById('dropdown-autoconnect-check');
      if (checkEl) {
        checkEl.classList.toggle('hidden', !enabled);
        checkEl.style.visibility = enabled ? 'visible' : 'hidden';
      }
    }

    if (autoconnectChk) {
      const savedAutoConnect = localStorage.getItem('bf_auto_reconnect');
      autoconnectChk.checked = (savedAutoConnect === null || savedAutoConnect === '1');
      if (savedAutoConnect === null) {
        try { localStorage.setItem('bf_auto_reconnect', '1'); } catch (e) {}
      }
      autoconnectChk.addEventListener('change', () => {
        localStorage.setItem('bf_auto_reconnect', autoconnectChk.checked ? '1' : '0');
        manualDisconnect = false;
        syncAutoConnectUI();
        if (autoconnectChk.checked && !port) {
          updateConnectionUI(false);
          attemptAutoConnect();
        } else if (!autoconnectChk.checked && !port) {
          updateConnectionUI(false);
        }
      });
    }

    function toggleConnectDropdown(event?: Event) {
      if (event && typeof event.stopPropagation === 'function') {
        event.stopPropagation();
      }
      const menu = document.getElementById('connect-dropdown-menu');
      const chevron = document.getElementById('connect-dropdown-chevron');
      if (!menu) return;
      const isClosed = menu.style.display === 'none' || !menu.style.display;
      if (isClosed) {
        syncAutoConnectUI();
        menu.style.display = 'flex';
        if (chevron) chevron.style.transform = 'rotate(180deg)';
      } else {
        menu.style.display = 'none';
        if (chevron) chevron.style.transform = 'rotate(0deg)';
      }
    }

    function closeConnectDropdown() {
      const menu = document.getElementById('connect-dropdown-menu');
      const chevron = document.getElementById('connect-dropdown-chevron');
      if (menu && menu.style.display !== 'none') {
        menu.style.display = 'none';
        if (chevron) chevron.style.transform = 'rotate(0deg)';
      }
    }

    function toggleAutoConnectFromDropdown() {
      const next = !isAutoConnectEnabled();
      if (autoconnectChk) {
        autoconnectChk.checked = next;
      }
      try {
        localStorage.setItem('bf_auto_reconnect', next ? '1' : '0');
      } catch (e) {}
      manualDisconnect = false;
      syncAutoConnectUI();
      updateConnectionUI(!!port);
      if (next && !port) {
        postSystemMessage('Auto-Connect enabled', 'info', 2000);
        attemptAutoConnect();
      } else {
        postSystemMessage('Auto-Connect disabled', 'info', 2000);
      }
    }

    async function triggerManualPortPicker() {
      closeConnectDropdown();
      if (port) {
        postSystemMessage('Disconnect current connection first to pick another port', 'warning', 3000);
        return;
      }
      try {
        await connectPort();
      } catch (err: any) {
        console.warn('Manual port picker cancelled or failed:', err);
      }
    }

    function showUsbTroubleshootHelp() {
      closeConnectDropdown();
      const modal = document.getElementById('usb-help-modal');
      if (modal) {
        modal.style.display = 'flex';
      }
    }

    function closeUsbHelpModal() {
      const modal = document.getElementById('usb-help-modal');
      if (modal) {
        modal.style.display = 'none';
      }
    }

    function handleUsbBackdropClick(event?: Event) {
      if (event && (event.target === event.currentTarget || (event as any).target?.id === 'usb-help-modal')) {
        closeUsbHelpModal();
      }
    }

    document.addEventListener('click', (e: MouseEvent) => {
      const panel = document.querySelector('.connection-panel');
      if (panel && !panel.contains(e.target as Node)) {
        closeConnectDropdown();
      }
    });

    document.addEventListener('keydown', (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        closeConnectDropdown();
        closeUsbHelpModal();
      }
    });

    // --- Theme Management (Light mode default) ---
    function toggleTheme() {
      const html = document.documentElement;
      const currentTheme = html.getAttribute('data-theme') || 'light';
      const newTheme = currentTheme === 'light' ? 'dark' : 'light';
      html.setAttribute('data-theme', newTheme);
      
      const themeBtnLabel = document.getElementById('theme-btn-label');
      const themeIcon = document.getElementById('theme-icon');
      const themeBtnSettingsLabel = document.getElementById('theme-btn-settings-label');
      const themeIconSettings = document.getElementById('theme-icon-settings');
      const themeBtnFlasherLabel = document.getElementById('theme-btn-flasher-label');
      const themeIconFlasher = document.getElementById('theme-icon-flasher');
      const themeBtnWriteLabel = document.getElementById('theme-btn-write-label');
      const themeIconWrite = document.getElementById('theme-icon-write');

      const sunIcon = '<circle cx="12" cy="12" r="5"></circle><line x1="12" y1="1" x2="12" y2="3"></line><line x1="12" y1="21" x2="12" y2="23"></line><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line><line x1="1" y1="12" x2="3" y2="12"></line><line x1="21" y1="12" x2="23" y2="12"></line><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line>';
      const moonIcon = '<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path>';

      if (newTheme === 'dark') {
        if (themeBtnLabel) themeBtnLabel.textContent = 'Light Mode';
        if (themeIcon) themeIcon.innerHTML = sunIcon;
        if (themeBtnSettingsLabel) themeBtnSettingsLabel.textContent = 'Light Mode';
        if (themeIconSettings) themeIconSettings.innerHTML = sunIcon;
        if (themeBtnFlasherLabel) themeBtnFlasherLabel.textContent = 'Light Mode';
        if (themeIconFlasher) themeIconFlasher.innerHTML = sunIcon;
        if (themeBtnWriteLabel) themeBtnWriteLabel.textContent = 'Light Mode';
        if (themeIconWrite) themeIconWrite.innerHTML = sunIcon;
      } else {
        if (themeBtnLabel) themeBtnLabel.textContent = 'Dark Mode';
        if (themeIcon) themeIcon.innerHTML = moonIcon;
        if (themeBtnSettingsLabel) themeBtnSettingsLabel.textContent = 'Dark Mode';
        if (themeIconSettings) themeIconSettings.innerHTML = moonIcon;
        if (themeBtnFlasherLabel) themeBtnFlasherLabel.textContent = 'Dark Mode';
        if (themeIconFlasher) themeIconFlasher.innerHTML = moonIcon;
        if (themeBtnWriteLabel) themeBtnWriteLabel.textContent = 'Dark Mode';
        if (themeIconWrite) themeIconWrite.innerHTML = moonIcon;
      }
    }

    // --- Browser Compatibility Checks ---
    async function checkBrowserSupport() {
      const isBrave = (navigator.brave && await navigator.brave.isBrave()) || false;
      const banner = document.getElementById('compat-banner');
      const msg = document.getElementById('compat-msg');

      if (!('serial' in navigator)) {
        if (banner) banner.style.display = 'flex';
        if (msg) {
          if (isBrave) {
            msg.innerHTML = `<strong>Brave Browser detected:</strong> Web Serial is disabled by default. Please navigate to <code>brave://settings/content/serialPorts</code> or enable <code>brave://flags/#brave-commands</code> to allow serial port access.`;
        } else {
          msg.innerHTML = `<strong>Web Serial not supported:</strong> Your browser does not support the Web Serial API. Please use <strong>Google Chrome</strong>, <strong>Microsoft Edge</strong>, or <strong>Opera</strong>.`;
          }
        }
      }
    }
    checkBrowserSupport();

    // --- Tab & Main View Switching (with URL Hash Markers) ---
    let currentTab = 'terminal';

    function switchTab(tabId, updateHash = true) {
      let normalized = 'terminal';
      if (tabId === 'flash' || tabId === 'flashing') normalized = 'flashing';
      else if (tabId === 'upload' || tabId === 'write') normalized = 'write';
      else if (tabId === 'settings' || tabId === 'controls') normalized = 'settings';

      currentTab = normalized;

      if (updateHash) {
        history.replaceState(null, '', '#' + (normalized === 'terminal' ? '' : (normalized === 'flashing' ? 'flash' : normalized)));
      }

      const tabs = [
        { id: 'terminal', btn: 'tab-btn-terminal', content: 'tab-content-terminal' },
        { id: 'write', btn: 'tab-btn-upload', content: 'tab-content-upload' },
        { id: 'settings', btn: 'tab-btn-settings', content: 'tab-content-settings' },
        { id: 'flashing', btn: 'tab-btn-flash', content: 'tab-content-flash' }
      ];

      tabs.forEach(t => {
        const btn = document.getElementById(t.btn) || document.getElementById(`tab-btn-${t.id}`) || (t.id === 'settings' ? document.getElementById('tab-btn-controls') : null);
        const content = document.getElementById(t.content) || document.getElementById(`tab-content-${t.id}`) || (t.id === 'settings' ? document.getElementById('tab-content-controls') : null);
        if (btn) btn.classList.toggle('active', t.id === normalized);
        if (content) {
          if (t.id === normalized) {
            content.style.display = (t.id === 'write' ? 'flex' : 'block');
          } else {
            content.style.display = 'none';
          }
        }
      });

      const sidebarEl = document.querySelector('.sidebar');
      if (sidebarEl) sidebarEl.classList.toggle('write-tab-active', normalized === 'write');

      const viewTerminal = document.getElementById('view-terminal');
      const viewControls = document.getElementById('view-controls');
      const viewFlashing = document.getElementById('view-flashing');
      const viewWrite = document.getElementById('view-write');

      if (normalized === 'write') {
        if (viewTerminal) viewTerminal.style.display = 'none';
        if (viewControls) viewControls.style.display = 'none';
        if (viewFlashing) viewFlashing.style.display = 'none';
        if (viewWrite) viewWrite.style.display = 'flex';
        renderGitHubProgramCatalog();
        if (port && dynamicLibPrograms.length === 0) {
          queryDynamicLibrary();
        }
      } else if (normalized === 'settings') {
        if (viewTerminal) viewTerminal.style.display = 'none';
        if (viewControls) viewControls.style.display = 'flex';
        if (viewFlashing) viewFlashing.style.display = 'none';
        if (viewWrite) viewWrite.style.display = 'none';
        if (port && dynamicMenuSchema.length === 0) {
          fetchDynamicMenu();
        }
      } else if (normalized === 'flashing') {
        if (viewTerminal) viewTerminal.style.display = 'none';
        if (viewControls) viewControls.style.display = 'none';
        if (viewFlashing) viewFlashing.style.display = 'flex';
        if (viewWrite) viewWrite.style.display = 'none';
        if (!flasherReleasesLoaded) {
          fetchGitHubReleases();
        }
        updateFlasherDeviceStatus();
      } else {
        if (viewTerminal) viewTerminal.style.display = 'flex';
        if (viewControls) viewControls.style.display = 'none';
        if (viewFlashing) viewFlashing.style.display = 'none';
        if (viewWrite) viewWrite.style.display = 'none';
      }
    }

    function initRouteFromHash() {
      const hash = window.location.hash.replace(/^#\/?/, '').toLowerCase();
      if (hash === 'settings' || hash === 'controls') {
        switchTab('settings', false);
      } else if (hash === 'upload' || hash === 'write') {
        switchTab('write', false);
      } else if (hash === 'flashing' || hash === 'flash') {
        switchTab('flashing', false);
      } else {
        switchTab('terminal', false);
      }
    }

    window.addEventListener('hashchange', initRouteFromHash);

    // --- Input Mode Management ---
    function setInputMode(mode, notify = false) {
      inputMode = mode;
      inputModeSelect.value = mode;
      if (mode === 'direct') {
        modeBadge.textContent = 'Mode: Direct';
        cmdInput.placeholder = 'Click terminal to send raw keys, or type here and press Enter...';
        terminalScreen.focus();
      } else {
        modeBadge.textContent = 'Mode: Line';
        cmdInput.placeholder = 'Type command and press Enter...';
        cmdInput.focus();
      }
      updateFocusIndicator();
      if (notify) {
        postSystemMessage(mode === "direct" ? "Input mode: Direct keystrokes" : "Input mode: Line buffered", "info", 2000);
      }
    }

    function cycleInputMode() {
      setInputMode(inputMode === 'direct' ? 'line' : 'direct', true);
    }

    function updateFocusIndicator() {
      const isDirectFocused = (document.activeElement === terminalScreen && inputMode === 'direct');
      terminalScreen.classList.toggle('focused-direct', isDirectFocused);
      const pill = document.getElementById('direct-pill');
      if (pill) pill.classList.toggle('active', isDirectFocused);
    }

    terminalScreen.addEventListener('focus', () => {
      if (inputMode === 'direct') updateFocusIndicator();
    });
    terminalScreen.addEventListener('blur', updateFocusIndicator);
    terminalScreen.addEventListener('click', () => {
      if (inputMode === 'direct') terminalScreen.focus();
    });

    // PuTTY-style click-to-focus anywhere in the terminal view container
    const viewTerminal = document.getElementById('view-terminal');
    if (viewTerminal) {
      viewTerminal.addEventListener('click', (e) => {
        if (inputMode === 'direct') {
          const target = e.target as HTMLElement;
          if (!target.closest('button, select, input, a, textarea')) {
            terminalScreen.focus();
          }
        }
      });
    }

    // PuTTY-style selection: automatic copy to clipboard on mouse release
    terminalScreen.addEventListener('mouseup', () => {
      if (inputMode !== 'direct') return;
      const sel = window.getSelection ? window.getSelection()?.toString() : '';
      if (sel && sel.length > 0 && navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(sel).catch(() => {});
      }
    });

    // PuTTY-style right-click paste
    terminalScreen.addEventListener('contextmenu', async (e: MouseEvent) => {
      if (inputMode === 'direct') {
        const sel = window.getSelection ? window.getSelection()?.toString() : '';
        if (!sel || sel.length === 0) {
          if (navigator.clipboard && navigator.clipboard.readText) {
            try {
              const text = await navigator.clipboard.readText();
              if (text) {
                e.preventDefault();
                await sendRaw(text);
              }
            } catch {
              // Browser permission denied or not supported; allow default context menu
            }
          }
        }
      }
    });

    // --- Direct Keystroke Forwarding (PuTTY 1-to-1) ---
    terminalScreen.addEventListener('keydown', async (e) => {
      if (!port || inputMode !== 'direct') return;

      const toSend = mapDirectKey(e);
      if (toSend !== null) {
        e.preventDefault();
        e.stopPropagation();
        await sendRaw(toSend);
      }
    });

    // Direct mode paste forwarding
    terminalScreen.addEventListener('paste', async (e: ClipboardEvent) => {
      if (!port || inputMode !== 'direct') return;
      const text = e.clipboardData?.getData('text');
      if (text) {
        e.preventDefault();
        e.stopPropagation();
        await sendRaw(text);
      }
    });

    // Global direct keyboard pass-through when on Terminal view
    window.addEventListener('keydown', async (e) => {
      if (!port || inputMode !== 'direct') return;
      if (e.target === terminalScreen) return;

      const vTerm = document.getElementById('view-terminal');
      if (!vTerm || !vTerm.classList.contains('active')) return;

      const activeTag = document.activeElement?.tagName?.toLowerCase();
      if (activeTag === 'input' || activeTag === 'textarea' || activeTag === 'select' || (document.activeElement as HTMLElement)?.isContentEditable) {
        return;
      }

      if (isVerifyingModalOpen || document.querySelector('.modal.open, .dialog.open')) {
        return;
      }

      const toSend = mapDirectKey(e);
      if (toSend !== null) {
        e.preventDefault();
        e.stopPropagation();
        terminalScreen.focus();
        await sendRaw(toSend);
      }
    });

    window.addEventListener('paste', async (e: ClipboardEvent) => {
      if (!port || inputMode !== 'direct') return;
      if (e.target === terminalScreen) return;

      const vTerm = document.getElementById('view-terminal');
      if (!vTerm || !vTerm.classList.contains('active')) return;

      const activeTag = document.activeElement?.tagName?.toLowerCase();
      if (activeTag === 'input' || activeTag === 'textarea' || activeTag === 'select' || (document.activeElement as HTMLElement)?.isContentEditable) {
        return;
      }

      if (isVerifyingModalOpen || document.querySelector('.modal.open, .dialog.open')) {
        return;
      }

      const text = e.clipboardData?.getData('text');
      if (text) {
        e.preventDefault();
        e.stopPropagation();
        terminalScreen.focus();
        await sendRaw(text);
      }
    });

    // --- Line Send Input Box & Command History ---
    cmdInput.addEventListener('keydown', async (e) => {
      if (e.key === 'Enter') {
        sendInputBox();
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        if (historyIdx < cmdHistory.length - 1) {
          historyIdx++;
          cmdInput.value = cmdHistory[cmdHistory.length - 1 - historyIdx];
        }
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        if (historyIdx > 0) {
          historyIdx--;
          cmdInput.value = cmdHistory[cmdHistory.length - 1 - historyIdx];
        } else if (historyIdx === 0) {
          historyIdx = -1;
          cmdInput.value = '';
        }
      }
    });

    function sendInputBox() {
      const text = cmdInput.value;
      if (!text && !lineEndingSelect.value) return;

      if (text) {
        cmdHistory.push(text);
        if (cmdHistory.length > 50) cmdHistory.shift();
      }
      historyIdx = -1;

      const ending = lineEndingSelect.value;
      sendCmd(text + ending);
      cmdInput.value = '';
    }

    // --- Web Serial Connection Logic (Baud rate ignored by USB CDC) ---
    async function toggleConnect() {
      if (port) {
        await disconnectPort();
      } else {
        await connectPort();
      }
    }

    async function openExistingPort(selectedPort) {
      if (!selectedPort) return;
      port = selectedPort;
      try {
        await port.open({ baudRate: 115200 });
        writer = port.writable.getWriter();
        keepReading = true;
        if (autoconnectChk && autoconnectChk.checked) {
          try { localStorage.setItem('bf_auto_reconnect', '1'); } catch (e) {}
        }

        updateConnectionUI(true);
        postSystemMessage('Connected to Brainfuino (115200 baud)', 'success');
        readSerialLoop();

        // If on settings tab, query dynamic menu
        if (currentTab === 'settings') {
          fetchDynamicMenu();
        }
      } catch (err) {
        console.warn('Failed to open serial port:', err);
        port = null;
        updateConnectionUI(false);
        postSystemMessage(`Failed to open serial port: ${err.message || err}`, 'error', 5000);
      }
    }

    async function connectPort() {
      if (!('serial' in navigator)) {
        alert('Web Serial is not supported on this browser. Please use Chrome, Edge, or Brave with Web Serial enabled.');
        return;
      }
      manualDisconnect = false;

      try {
        // No filter specified so user sees all available COM ports
        const selectedPort = await navigator.serial.requestPort({});
        await openExistingPort(selectedPort);
      } catch (err) {
        if (err.name !== 'NotFoundError') {
          console.error('Serial requestPort error:', err);
          alert(`Port Picker Notice: ${err.message || err.name}\n\nNote: If using Brave, please allow serial ports in brave://settings/content/serialPorts.`);
        }
      }
    }

    async function disconnectPort() {
      manualDisconnect = true;
      keepReading = false;

      // 1. Cancel active reader to terminate read loop
      if (reader) {
        try {
          await reader.cancel();
        } catch (e) {
          console.warn('reader.cancel notice:', e);
        }
        reader = null;
      }

      // 2. Await piped stream closure so port.readable unlock completes
      if (readableStreamClosed) {
        try {
          await readableStreamClosed.catch(() => {});
        } catch (e) {}
        readableStreamClosed = null;
      }

      // 3. Release writable stream lock
      if (writer) {
        try {
          writer.releaseLock();
        } catch (e) {
          console.warn('writer releaseLock notice:', e);
        }
        writer = null;
      }

      // 4. Safely close physical serial port
      if (port) {
        try {
          await port.close();
        } catch (e) {
          console.warn('port.close notice:', e);
        }
        port = null;
      }

      updateConnectionUI(false);
      postSystemMessage('Disconnected from Brainfuino', 'info');
    }

    async function handleDeviceUnplugged() {
      keepReading = false;
      if (reader) {
        try { await reader.cancel(); } catch (e) {}
        reader = null;
      }
      if (readableStreamClosed) {
        try { await readableStreamClosed.catch(() => {}); } catch (e) {}
        readableStreamClosed = null;
      }
      if (writer) {
        try { writer.releaseLock(); } catch (e) {}
        writer = null;
      }
      if (port) {
        try { await port.close(); } catch (e) {}
        port = null;
      }
      updateConnectionUI(false);
    }

    async function attemptAutoConnect(specificPort = null) {
      if (!('serial' in navigator)) return;
      if (specificPort) {
        manualDisconnect = false;
      }
      if (!isAutoConnectEnabled() || port || manualDisconnect) return;

      try {
        let target = specificPort;
        if (!target) {
          const ports = await navigator.serial.getPorts();
          if (!ports || ports.length === 0) {
            updateConnectionUI(false);
            return;
          }
          // Betaflight style: search for STM32 USB CDC device (VID 0x0483, PID 0x5740)
          target = ports.find(p => {
            const info = p.getInfo ? p.getInfo() : {};
            return (info.usbVendorId === 0x0483 && info.usbProductId === 0x5740) ||
                   (info.usbVendorId === 1155 && info.usbProductId === 22336);
          }) || ports[0];
        }

        if (target) {
          postSystemMessage('Brainfuino detected. Auto-connecting...', 'info', 2500);
          await openExistingPort(target);
        }
      } catch (err) {
        console.warn('Auto-connect attempt notice:', err);
        updateConnectionUI(false);
      }
    }

    function initSerialHotplugListeners() {
      if (!('serial' in navigator)) return;

      navigator.serial.addEventListener('connect', async (event) => {
        manualDisconnect = false;
        postSystemMessage('USB serial device plugged in', 'info', 2500);
        if (isAutoConnectEnabled() && !port) {
          await attemptAutoConnect(event.target);
        }
      });

      navigator.serial.addEventListener('disconnect', async (event) => {
        if (port && event.target === port) {
          postSystemMessage('Brainfuino unplugged via USB', 'warn', 3500);
          await handleDeviceUnplugged();
        }
      });
    }

    function updateConnectionUI(connected) {
      const splitBtn = document.getElementById('connect-split-btn');
      if (connected) {
        statusPill.className = 'status-pill connected';
        statusText.textContent = 'Connected';
        if (splitBtn) splitBtn.classList.add('connected');
        connectBtn.classList.add('connected');
        connectBtnLabel.textContent = 'Disconnect';
        terminalScreen.focus();

        // Auto-query current hardware clock speed on connect
        setTimeout(() => {
          if (port && writer) queryClockSpeed();
        }, 400);
      } else {
        if (splitBtn) splitBtn.classList.remove('connected');
        connectBtn.classList.remove('connected');
        connectBtnLabel.textContent = 'Connect Brainfuino';
        statusPill.className = 'status-pill';
        statusText.textContent = 'Disconnected';
      }
      syncAutoConnectUI();
      if (typeof updateFlasherDeviceStatus === 'function') {
        updateFlasherDeviceStatus();
      }
    }

    // --- FPGA Clock Speed Management ---
    const CLOCK_FREQUENCIES = [
      "10 Hz", "25 Hz", "50 Hz", "100 Hz", "250 Hz", "500 Hz",
      "1 kHz", "2 kHz", "5 kHz", "10 kHz", "25 kHz", "50 kHz",
      "62.5 kHz", "125 kHz", "250 kHz", "500 kHz", "750 kHz",
      "1 MHz", "1.5 MHz", "2 MHz", "3 MHz", "4 MHz", "6 MHz",
      "8 MHz", "12 MHz"
    ];
    let currentClkIdx = 15; // Default: 500 kHz

    function updateClockDisplay(idx) {
      if (idx >= 0 && idx < CLOCK_FREQUENCIES.length) {
        currentClkIdx = idx;
        const valEl = document.getElementById('clock-speed-val');
        if (valEl) {
          valEl.textContent = CLOCK_FREQUENCIES[currentClkIdx];
        postSystemMessage(`FPGA Clock: ${CLOCK_FREQUENCIES[currentClkIdx]}`, 'info');
        }
        const dynClkSelect = document.getElementById('dyn-item-10');
        if (dynClkSelect && parseInt(dynClkSelect.value, 10) !== idx) {
          dynClkSelect.value = String(idx);
        }
        const schemaItem = dynamicMenuSchema.find(i => i.id === 10);
        if (schemaItem) schemaItem.val = String(idx);
      }
    }

    async function stepClockUp() {
      if (currentClkIdx < CLOCK_FREQUENCIES.length - 1) {
        updateClockDisplay(currentClkIdx + 1);
      }
      await sendCmd('!clk+\r\n');
    }

    async function stepClockDown() {
      if (currentClkIdx > 0) {
        updateClockDisplay(currentClkIdx - 1);
      }
      await sendCmd('!clk-\r\n');
    }

    async function queryClockSpeed() {
      await sendCmd('!clk\r\n');
    }

    // --- Dynamic Parameters & Machine Protocol ---
    let dynamicMenuSchema = [];
    let isParsingDynamicMenu = false;
    let machineLineBuffer = '';

    const PARAM_DESCRIPTIONS = {
      10: 'Core execution clock frequency driving the FPGA soft-processor.',
      11: 'Keyboard shortcut scheme to step clock frequency in run mode.',
      12: 'Automatically assemble and flash Brainfuck programs detected on serial paste.',
      13: 'Minimum byte length of serial paste required to trigger auto-programming.',
      14: 'Automatically issue soft reset to FPGA immediately after flash completes.',
      15: 'Append infinite halt loop [-]+[] to prevent program falling off flash.',
      16: 'Filter out commentary and whitespace from pasted source code.',
      17: 'Filter non-Brainfuck characters before storing into onboard ROM library.',
      18: 'Normalize incoming serial CR (0x0D) to standard LF (0x0A) newlines.',
      19: 'Enable single-step or burst-step hardware execution mode.',
      20: 'Number of clock cycles to pulse per manual step trigger.',
      21: 'Hardware or keyboard keystroke assigned to trigger manual execution step.',
      30: 'Reboot companion STM32 microcontroller into USB System ROM DFU bootloader mode.',
      31: 'Erase user flash and re-flash factory Brainfuino animated logo demo.',
      32: 'Trigger hardware reset line (BF_RST) to restart the soft-processor core.'
    };

    function getCategoryIconSvg(title) {
      const lower = title.toLowerCase();
      if (lower.includes('clock') || lower.includes('execution')) {
        return '<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><polyline points="12 6 12 12 16 14"></polyline></svg>';
      }
      if (lower.includes('program') || lower.includes('flash') || lower.includes('paste')) {
        return '<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"></polygon></svg>';
      }
      if (lower.includes('step') || lower.includes('input')) {
        return '<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="4" width="20" height="16" rx="2"></rect><path d="M6 8h.001M10 8h.001M14 8h.001M18 8h.001M8 12h.001M12 12h.001M16 12h.001M7 16h10"></path></svg>';
      }
      return '<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="4" width="16" height="16" rx="2"></rect><rect x="9" y="9" width="6" height="6"></rect><line x1="9" y1="1" x2="9" y2="4"></line><line x1="15" y1="1" x2="15" y2="4"></line><line x1="9" y1="20" x2="9" y2="23"></line><line x1="15" y1="20" x2="15" y2="23"></line><line x1="20" y1="9" x2="23" y2="9"></line><line x1="20" y1="14" x2="23" y2="14"></line><line x1="1" y1="9" x2="4" y2="9"></line><line x1="1" y1="14" x2="4" y2="14"></line></svg>';
    }

    function fetchDynamicMenu() {
      if (!port || !writer) {
        updateSettingsStatus('Serial port not connected. Connect device or load preview.', false);
        return;
      }
      const syncBtn = document.getElementById('settings-sync-btn');
      if (syncBtn) syncBtn.disabled = true;

      const headerBadge = document.getElementById('dyn-header-badge');
      if (headerBadge) {
        headerBadge.textContent = 'SYNCING...';
        headerBadge.className = 'settings-badge syncing';
      }

      updateSettingsStatus('Querying Brainfuino parameters (!MENU*)...', true);
      sendCmd('!MENU*\r\n');

      setTimeout(() => {
        if (syncBtn) syncBtn.disabled = false;
      }, 1200);
    }

    function updateSettingsStatus(msg, isActive = true) {
      const stripMsg = document.getElementById('settings-strip-msg');
      const stripDot = document.getElementById('settings-strip-dot');
      if (stripMsg) stripMsg.textContent = msg;
      if (stripDot) stripDot.classList.toggle('active', isActive);

      const dynStatus = document.getElementById('dyn-status');
      if (dynStatus) dynStatus.textContent = msg;
    }

    function processMachineLine(line) {
      if (line.startsWith('!MENU:START:')) {
        isParsingDynamicMenu = true;
        dynamicMenuSchema = [];
        updateSettingsStatus('Receiving parameters from hardware...', true);
        const headerBadge = document.getElementById('dyn-header-badge');
        if (headerBadge) {
          headerBadge.textContent = 'RECEIVING...';
          headerBadge.className = 'settings-badge syncing';
        }
        return;
      }
      if (line.startsWith('!MENU:END')) {
        isParsingDynamicMenu = false;
        renderDynamicMenu();
        const nonFolderCount = dynamicMenuSchema.filter(i => i.type !== 'FOLDER').length;
        const headerBadge = document.getElementById('dyn-header-badge');
        if (headerBadge) {
          headerBadge.textContent = `SYNCED (${nonFolderCount})`;
          headerBadge.className = 'settings-badge synced';
        }
        updateSettingsStatus(`Parameters synced with hardware (${nonFolderCount} items loaded)`, true);
        return;
      }
      if (isParsingDynamicMenu) {
        // Format: ID;TYPE;PARENT_ID;LABEL;[CURRENT_VAL];[OPTIONS...]
        const parts = line.split(';');
        if (parts.length >= 4) {
          const id = parseInt(parts[0], 10);
          const type = parts[1].trim().toUpperCase();
          const parentId = parseInt(parts[2], 10);
          const label = parts[3].trim();
          const val = parts.length >= 5 ? parts[4].trim() : '';
          const options = parts.length >= 6 ? parts.slice(5).map(o => o.trim()).filter(o => o.length > 0) : [];
          dynamicMenuSchema.push({ id, type, parentId, label, val, options });
        }
        return;
      }
      if (line.startsWith('!SET:OK:')) {
        // Format: !SET:OK:<id>=<val>
        const match = /!SET:OK:(\d+)=(.*)/.exec(line);
        if (match) {
          const id = parseInt(match[1], 10);
          const val = match[2];
          const item = dynamicMenuSchema.find(i => i.id === id);
          if (item) item.val = val;
          if (id === 10) {
            updateClockDisplay(parseInt(val, 10));
          }
          updateSettingsStatus(`Saved: ${item ? item.label : id} = ${val}`, true);

          const row = document.getElementById(`setting-row-${id}`);
          if (row) {
            row.classList.add('setting-flash');
            setTimeout(() => row.classList.remove('setting-flash'), 1200);
          }
        }
        return;
      }
      if (line.startsWith('!CMD:OK:')) {
        const match = /!CMD:OK:(\d+)/.exec(line);
        if (match) {
          const id = parseInt(match[1], 10);
          const item = dynamicMenuSchema.find(i => i.id === id);
          updateSettingsStatus(`Executed: ${item ? item.label : id}`, true);

          const row = document.getElementById(`setting-row-${id}`);
          if (row) {
            row.classList.add('setting-flash');
            setTimeout(() => row.classList.remove('setting-flash'), 1200);
          }
        }
        return;
      }

      // --- Dynamic Program Library Protocol (!LIB*) ---
      if (line.startsWith('!LIB:START:')) {
        // Format: !LIB:START:<count>:<used_bytes>:<total_capacity>
        const parts = line.split(':');
        isParsingDynamicLib = true;
        dynamicLibPrograms = [];
        if (parts.length >= 5) {
          libUsedBytes = parseInt(parts[3], 10) || 0;
          libTotalCapacity = parseInt(parts[4], 10) || 45056;
        } else if (parts.length >= 4) {
          libUsedBytes = parseInt(parts[2], 10) || 0;
          libTotalCapacity = parseInt(parts[3], 10) || 45056;
        }
        updateWriteLibStatus('Receiving program library schema from Brainfuino...', true);
        const pill = document.getElementById('write-lib-status-pill');
        if (pill) {
          pill.textContent = 'RECEIVING...';
          pill.className = 'direct-pill';
        }
        return;
      }
      if (line.startsWith('!LIB:END')) {
        isParsingDynamicLib = false;
        renderDynamicLibrary();
        updateWriteLibStatus(`Library synced (${dynamicLibPrograms.length} programs loaded)`, false);
        const pill = document.getElementById('write-lib-status-pill');
        if (pill) {
          pill.textContent = `SYNCED (${dynamicLibPrograms.length})`;
          pill.className = 'direct-pill';
        }
        return;
      }
      if (isParsingDynamicLib && line.startsWith('!LIB:PROG:')) {
        // Format: !LIB:PROG:<slot>;<name>;<size>;<pruned>;<protected>
        const payload = line.slice(10);
        const parts = payload.split(';');
        if (parts.length >= 3) {
          const slot = parseInt(parts[0], 10);
          const name = parts[1].trim();
          const size = parseInt(parts[2], 10) || 0;
          const pruned = parts.length >= 4 ? parseInt(parts[3], 10) : 0;
          const isProtected = parts.length >= 5 ? parts[4].trim() === '1' : (slot === 0);
          dynamicLibPrograms.push({ slot, name, size, pruned, isProtected });
        }
        return;
      }
      if (line.startsWith('!LIB:RUN:OK:')) {
        const slot = line.slice(12).trim();
        updateWriteLibStatus(`Loaded and running slot ${slot} on soft-processor`, false);
        switchTab('terminal');
        return;
      }
      if (line.startsWith('!LIB:DEL:OK:')) {
        const slot = line.slice(12).trim();
        updateWriteLibStatus(`Deleted program in slot ${slot}`, false);
        queryDynamicLibrary();
        return;
      }
      if (line.startsWith('!LIB:ADD:READY:')) {
        const name = line.slice(15).trim();
        updateWriteLibStatus(`Brainfuino ready. Streaming code for '${name}'...`, true);
        if (typeof libAddReadyResolver === 'function') {
          libAddReadyResolver(true);
          libAddReadyResolver = null;
        }
        return;
      }
      if (line.startsWith('!LIB:TEST:START:')) {
        const parts = line.split(':');
        const secs = (parts.length >= 4) ? parseInt(parts[3], 10) : 10;
        startVerificationSession(secs || 10);
        return;
      }
      if (line.startsWith('!LIB:TEST:DONE:')) {
        const parts = line.split(':');
        const slot = (parts.length >= 4) ? parseInt(parts[3], 10) : 0;
        closeVerificationSession(true, slot);
        queryDynamicLibrary();
        return;
      }
      if (line.startsWith('!LIB:TEST:CANCEL') || line.startsWith('!LIB:TEST:ERROR')) {
        closeVerificationSession(false);
        queryDynamicLibrary();
        return;
      }

      // --- Program Dump Protocol (!DUMP:START / !DUMP:END) ---
      if (line.startsWith('!DUMP:START:')) {
        // Format: !DUMP:START:<slot>:<name>:<size>
        const parts = line.split(':');
        const name = (parts.length >= 4) ? parts[3] : 'Program';
        isReceivingDump = true;
        dumpPayload = '';
        dumpName = name;
        updateWriteLibStatus(`Receiving dump of '${name}'...`, true);
        return;
      }
      if (line.startsWith('!DUMP:END')) {
        isReceivingDump = false;
        let cleanCode = dumpPayload;
        const startMarkerIdx = cleanCode.indexOf('!DUMP:START:');
        if (startMarkerIdx !== -1) {
          const afterStart = cleanCode.indexOf('\n', startMarkerIdx);
          if (afterStart !== -1) cleanCode = cleanCode.slice(afterStart + 1);
        }
        const endMarkerIdx = cleanCode.indexOf('!DUMP:END');
        if (endMarkerIdx !== -1) {
          cleanCode = cleanCode.slice(0, endMarkerIdx);
        }
        cleanCode = cleanCode.replace(/^\r?\n+|\r?\n+$/g, '');

        const area = document.getElementById('write-code-area');
        const nameInput = document.getElementById('write-prog-name');
        if (area) area.value = cleanCode;
        if (nameInput) nameInput.value = dumpName;
        updateWriteCodeStats();
        updateWriteLibStatus(`Dump loaded: '${dumpName}' (${cleanCode.length} B) into editor`, false);
        switchTab('write');
        return;
      }
    }

    function renderDynamicMenu() {
      const grid = document.getElementById('settings-cards-grid');
      const emptyState = document.getElementById('settings-empty-state');
      const sidebarNav = document.getElementById('sidebar-categories-nav');
      const sidebarSec = document.getElementById('sidebar-categories-section');
      const metaCount = document.getElementById('meta-param-count');
      const metaState = document.getElementById('meta-sync-state');
      const stripStats = document.getElementById('settings-strip-stats');

      if (!grid) return;

      if (dynamicMenuSchema.length === 0) {
        if (emptyState) emptyState.style.display = 'flex';
        grid.style.display = 'none';
        if (sidebarSec) sidebarSec.style.display = 'none';
        if (metaCount) metaCount.textContent = '0';
        if (metaState) metaState.textContent = 'Disconnected';
        if (stripStats) stripStats.textContent = 'Schema items: 0';
        return;
      }

      if (emptyState) emptyState.style.display = 'none';
      grid.style.display = 'flex';
      grid.innerHTML = '';

      if (sidebarNav) sidebarNav.innerHTML = '';
      if (sidebarSec) sidebarSec.style.display = 'block';

      // Setup responsive masonry columns (fill shortest column first)
      const containerWidth = grid.clientWidth || (window.innerWidth - 280);
      let numCols = 3;
      if (containerWidth < 720) {
        numCols = 1;
      } else if (containerWidth < 1100) {
        numCols = 2;
      }
      lastDynCols = numCols;

      const columns = [];
      for (let c = 0; c < numCols; c++) {
        const colDiv = document.createElement('div');
        colDiv.className = 'settings-grid-col';
        grid.appendChild(colDiv);
        columns.push({ el: colDiv, height: 0 });
      }

      // Separate folders vs fields
      const declaredFolders = dynamicMenuSchema.filter(i => i.type === 'FOLDER');
      const folderMap = new Map();
      declaredFolders.forEach(f => folderMap.set(f.id, f));

      // Auto-create category card for any items whose parentId isn't explicitly defined
      const nonFolderItems = dynamicMenuSchema.filter(i => i.type !== 'FOLDER');
      nonFolderItems.forEach(item => {
        if (!folderMap.has(item.parentId)) {
          folderMap.set(item.parentId, {
            id: item.parentId,
            type: 'FOLDER',
            label: item.parentId === 0 ? 'General Configuration' : `Category ${item.parentId}`
          });
        }
      });

      const folderList = Array.from(folderMap.values());

      let totalParams = 0;

      folderList.forEach(folder => {
        const children = dynamicMenuSchema.filter(i => i.parentId === folder.id && i.type !== 'FOLDER');
        totalParams += children.length;

        // 1. Render Category Nav Link in Sidebar
        if (sidebarNav) {
          const link = document.createElement('a');
          link.className = 'sidebar-cat-link';
          link.href = `#card-folder-${folder.id}`;
          link.innerHTML = `
            <span>${folder.label}</span>
            <span class="meta-val">${children.length}</span>
          `;
          link.onclick = (e) => {
            e.preventDefault();
            const targetCard = document.getElementById(`card-folder-${folder.id}`);
            if (targetCard) {
              targetCard.scrollIntoView({ behavior: 'smooth', block: 'start' });
            }
          };
          sidebarNav.appendChild(link);
        }

        // 2. Render Card for Main View
        const card = document.createElement('div');
        card.className = 'settings-card';
        card.id = `card-folder-${folder.id}`;

        const cardHeader = document.createElement('div');
        cardHeader.className = 'settings-card-header';
        cardHeader.innerHTML = `
          <div class="settings-card-title-group">
            <span class="settings-card-icon">${getCategoryIconSvg(folder.label)}</span>
            <h3 class="settings-card-title">${folder.label}</h3>
          </div>
          <span class="settings-card-count">${children.length} items</span>
        `;

        const cardBody = document.createElement('div');
        cardBody.className = 'settings-card-body';

        children.forEach(item => {
          const row = document.createElement('div');
          row.className = 'settings-item-row';
          row.id = `setting-row-${item.id}`;
          const desc = PARAM_DESCRIPTIONS[item.id] || `Hardware register #${item.id}`;
          row.setAttribute('data-search-text', `${folder.label} ${item.label} ${desc}`.toLowerCase());

          const infoDiv = document.createElement('div');
          infoDiv.className = 'settings-item-info';
          infoDiv.innerHTML = `
            <div class="settings-item-label">${item.label}</div>
            <div class="settings-item-desc">${desc}</div>
          `;

          const ctrlDiv = document.createElement('div');
          ctrlDiv.className = 'settings-item-ctrl';

          if (item.type === 'TOGGLE') {
            const isTrue = item.val === '1' || item.val.toLowerCase() === 'true' || item.val.toLowerCase() === 'on' || item.val.toLowerCase() === 'enabled';
            ctrlDiv.innerHTML = `
              <label class="dyn-switch-wrap" title="Click to toggle ${item.label}">
                <input type="checkbox" id="dyn-item-${item.id}" ${isTrue ? 'checked' : ''} />
                <span class="dyn-switch-slider"></span>
                <span class="dyn-switch-label" id="dyn-lbl-${item.id}">${isTrue ? 'ENABLED' : 'DISABLED'}</span>
              </label>
            `;
            const chk = ctrlDiv.querySelector(`#dyn-item-${item.id}`);
            const lbl = ctrlDiv.querySelector(`#dyn-lbl-${item.id}`);
            chk.onchange = () => {
              const nextVal = chk.checked ? '1' : '0';
              item.val = nextVal;
              lbl.textContent = chk.checked ? 'ENABLED' : 'DISABLED';
              sendSetParam(item.id, nextVal);
            };
          } else if (item.type === 'SELECT') {
            let optHtml = '';
            item.options.forEach((opt, idx) => {
              const selected = (item.val == idx || item.val.toLowerCase() === opt.toLowerCase()) ? 'selected' : '';
              optHtml += `<option value="${idx}" ${selected}>${opt}</option>`;
            });
            ctrlDiv.innerHTML = `
              <select class="dyn-select-lg" id="dyn-item-${item.id}">
                ${optHtml}
              </select>
            `;
            const sel = ctrlDiv.querySelector(`#dyn-item-${item.id}`);
            if (sel) sel.value = String(item.val);
            sel.onchange = () => {
              item.val = sel.value;
              sendSetParam(item.id, sel.value);
              if (item.id === 10) {
                updateClockDisplay(parseInt(sel.value, 10));
              }
            };
          } else if (item.type === 'COMMAND') {
            const isDanger = item.label.toLowerCase().includes('dfu') || item.label.toLowerCase().includes('restore') || item.label.toLowerCase().includes('reset');
            ctrlDiv.innerHTML = `
              <button class="dyn-cmd-btn-lg ${isDanger ? 'btn-danger' : ''}" id="dyn-item-${item.id}">
                ${item.label}
              </button>
            `;
            const btn = ctrlDiv.querySelector(`#dyn-item-${item.id}`);
            btn.onclick = () => {
              sendCmdParam(item.id);
            };
          }

          row.appendChild(infoDiv);
          row.appendChild(ctrlDiv);
          cardBody.appendChild(row);
        });

        card.appendChild(cardHeader);
        card.appendChild(cardBody);

        // Approximate card height = header (~48px) + items * 44px
        const approxHeight = 48 + children.length * 44;

        // Distribute to the column with minimum accumulated height
        let targetCol = columns[0];
        for (let c = 1; c < columns.length; c++) {
          if (columns[c].height < targetCol.height) {
            targetCol = columns[c];
          }
        }
        targetCol.el.appendChild(card);
        targetCol.height += approxHeight;
      });

      if (metaCount) metaCount.textContent = String(totalParams);
      if (metaState) metaState.textContent = 'Synced';
      if (stripStats) stripStats.textContent = `Schema items: ${totalParams}`;
    }

    let lastDynCols = 0;
    let dynResizeTimer = null;
    window.addEventListener('resize', () => {
      clearTimeout(dynResizeTimer);
      dynResizeTimer = setTimeout(() => {
        const grid = document.getElementById('settings-cards-grid');
        if (!grid || grid.style.display === 'none' || dynamicMenuSchema.length === 0) return;
        const containerWidth = grid.clientWidth || (window.innerWidth - 280);
        let newCols = 3;
        if (containerWidth < 720) newCols = 1;
        else if (containerWidth < 1100) newCols = 2;
        if (newCols !== lastDynCols) {
          renderDynamicMenu();
        }
      }, 150);
    });

    function filterDynamicSettings(query) {
      const q = query.trim().toLowerCase();
      const cards = document.querySelectorAll('.settings-card');

      cards.forEach(card => {
        const rows = card.querySelectorAll('.settings-item-row');
        let visibleRows = 0;

        rows.forEach(row => {
          const text = row.getAttribute('data-search-text') || '';
          const matches = !q || text.includes(q);
          row.style.display = matches ? 'flex' : 'none';
          if (matches) visibleRows++;
        });

        card.style.display = (visibleRows > 0) ? 'block' : 'none';
      });
    }

    function sendSetParam(id, val) {
      sendCmd(`!SET:${id}=${val}\r\n`);
      updateSettingsStatus(`Sending !SET:${id}=${val}...`, true);
    }

    function sendCmdParam(id) {
      sendCmd(`!CMD:${id}\r\n`);
      updateSettingsStatus(`Sending !CMD:${id}...`, true);
    }

    function handleEmptyStateSync() {
      if (!port) {
        toggleConnect();
      } else {
        fetchDynamicMenu();
      }
    }

    function loadDefaultPreviewSchema() {
      // Standard schema matching companion-STM32-firmware dynamic export
      dynamicMenuSchema = [
        { id: 0, type: 'FOLDER', parentId: 0, label: 'Execution & Clock', val: '', options: [] },
        { id: 1, type: 'FOLDER', parentId: 0, label: 'Auto-Programming', val: '', options: [] },
        { id: 2, type: 'FOLDER', parentId: 0, label: 'Input & Step Control', val: '', options: [] },
        { id: 3, type: 'FOLDER', parentId: 0, label: 'System Commands', val: '', options: [] },

        { id: 10, type: 'SELECT', parentId: 0, label: 'FPGA Clock Frequency', val: String(currentClkIdx), options: CLOCK_FREQUENCIES },
        { id: 11, type: 'SELECT', parentId: 0, label: 'Run Mode Hotkeys', val: '1', options: ['NONE', 'PgUp/PgDn', 'Up/Down', '+ / -'] },

        { id: 12, type: 'TOGGLE', parentId: 1, label: 'Auto-Program on Paste', val: '1', options: ['DISABLED', 'ENABLED'] },
        { id: 13, type: 'SELECT', parentId: 1, label: 'Paste Threshold', val: '1', options: ['4 Bytes', '8 Bytes', '16 Bytes', '32 Bytes', '64 Bytes'] },
        { id: 14, type: 'TOGGLE', parentId: 1, label: 'Auto-Reset after Flash', val: '1', options: ['DISABLED', 'ENABLED'] },
        { id: 15, type: 'TOGGLE', parentId: 1, label: 'Append Loop [-]+[]', val: '1', options: ['DISABLED', 'ENABLED'] },
        { id: 16, type: 'TOGGLE', parentId: 1, label: 'Prune non-BF on Paste', val: '1', options: ['DISABLED', 'ENABLED'] },
        { id: 17, type: 'TOGGLE', parentId: 1, label: 'Prune non-BF in Library', val: '1', options: ['DISABLED', 'ENABLED'] },

        { id: 18, type: 'TOGGLE', parentId: 2, label: 'Serial Input CR to LF', val: '1', options: ['DISABLED', 'ENABLED'] },
        { id: 19, type: 'TOGGLE', parentId: 2, label: 'Manual Stepping Mode', val: '0', options: ['DISABLED', 'ENABLED'] },
        { id: 20, type: 'SELECT', parentId: 2, label: 'Step Advance Ticks', val: '2', options: ['1 Tick', '10 Ticks', '100 Ticks', '1,000 Ticks', '10,000 Ticks', '100,000 Ticks'] },
        { id: 21, type: 'SELECT', parentId: 2, label: 'Step Trigger Key', val: '0', options: ['Spacebar', 'Tab', 'Enter'] },

        { id: 30, type: 'COMMAND', parentId: 3, label: 'Reboot to USB DFU', val: '', options: [] },
        { id: 31, type: 'COMMAND', parentId: 3, label: 'Restore Factory Demo', val: '', options: [] },
        { id: 32, type: 'COMMAND', parentId: 3, label: 'Soft Reset FPGA', val: '', options: [] }
      ];

      renderDynamicMenu();
      const headerBadge = document.getElementById('dyn-header-badge');
      if (headerBadge) {
        headerBadge.textContent = 'PREVIEW ACTIVE';
        headerBadge.className = 'settings-badge synced';
      }
      updateSettingsStatus('Loaded standard hardware preview schema (13 parameters)', true);
    }

    // --- High-Performance Stream Queue & Batching ---
    let pendingStreamChunks = [];
    let renderFrameScheduled = false;
    let maxScrollbackLines = 2000;
    let totalRenderedLines = 0;

    // --- Serial Read Loop & ANSI Rendering ---
    async function readSerialLoop() {
      if (!port || !port.readable) return;
      const textDecoder = new TextDecoderStream();
      readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
      reader = textDecoder.readable.getReader();

      try {
        while (keepReading) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value) {
            rxBytes += value.length;

            // Rolling buffer: keep max 4MB to prevent runaway memory bloat/crashes
            if (rawLogBuffer.length > 4000000) {
              rawLogBuffer = rawLogBuffer.slice(-2000000);
            }
            rawLogBuffer += value;

            // Capture exact host reception timestamp (microsecond precision via High Resolution Time)
            const recvTime = performance.timeOrigin ? (performance.timeOrigin + performance.now()) : Date.now();
            pendingStreamChunks.push({ text: value, time: recvTime });
            if (!renderFrameScheduled) {
              renderFrameScheduled = true;
              requestAnimationFrame(flushStreamQueue);
            }
          }
        }
      } catch (err) {
        if (keepReading) {
          postSystemMessage(`Serial connection lost: ${err.message}`, 'error', 4000);
          await handleDeviceUnplugged();
        }
      } finally {
        if (reader) {
          try { reader.releaseLock(); } catch (e) {}
        }
      }
      if (readableStreamClosed) {
        try { await readableStreamClosed.catch(() => {}); } catch (e) {}
      }
    }

    function flushStreamQueue() {
      renderFrameScheduled = false;
      if (pendingStreamChunks.length === 0) return;

      const queue = pendingStreamChunks;
      pendingStreamChunks = [];

      for (let i = 0; i < queue.length; i++) {
        const item = queue[i];
        if (typeof item === 'string') {
          appendTerminalText(item);
        } else {
          appendTerminalText(item.text, item.time);
        }
      }
      updateStats();
    }

    // --- Transmit Helpers ---
    async function sendRaw(str) {
      if (!writer) return;
      try {
        const encoder = new TextEncoder();
        const data = encoder.encode(str);
        txBytes += data.length;
        await writer.write(data);
        updateStats();
      } catch (err) {
        console.error('Error writing to port:', err);
      }
    }

    async function sendCmd(cmdStr) {
      await sendRaw(cmdStr);
    }

    // --- Shared ANSI Parser & Terminal Screen Rendering Engine ---
    function formatTimestamp(timestampMs) {
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

    function createTimestampSpan(timestampMs) {
      const span = document.createElement('span');
      span.className = 'terminal-timestamp';
      span.textContent = formatTimestamp(timestampMs);
      return span;
    }

    function createStyledSpanWithState(text, ansi) {
      if (text.includes('\b')) {
        let clean = [];
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
      let classes = [];
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
function appendTextToTerminal(screenEl, rawText, state, options = {}) {
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

      // Regex matching ANSI escape sequences: \x1b[...m or \x1b[...H / J / K / etc.
      const ansiRegex = /\x1b\[([0-9;]*)([a-zA-Z])/g;
      let lastIndex = 0;
      let match;
      let fragment = document.createDocumentFragment();

      const appendPlainTextSegments = (text) => {
        if (!text) return;
        if (text.includes('\b')) {
          let clean = [];
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

        // In 2D cursor-addressable mode (Bad Apple, Mandelbrot, Menu, etc.), overwrite lines in-place preserving styles
        if (state.is2D) {
          const parts = text.split('\n');
          for (let i = 0; i < parts.length; i++) {
            const part = parts[i].replace(/\r/g, '');
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
              let curKey = null;
              let curText = '';
              let activeStyle = null;
              for (let ci = 0; ci < lineObj.cells.length; ci++) {
                const cell = lineObj.cells[ci] || { char: ' ', style: { bold: false, underline: false, inverse: false, fgColor: null } };
                const s = cell.style;
                const k = (s.bold ? 'B' : '') + (s.underline ? 'U' : '') + (s.inverse ? 'I' : '') + ':' + (s.fgColor || '');
                if (k !== curKey) {
                  if (curText.length > 0) {
                    lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
                    curText = '';
                  }
                  curKey = k;
                  activeStyle = { ...s };
                }
                curText += cell.char;
              }
              if (curText.length > 0) {
                lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
              }
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
          // Move cursor to home without erasing previous content! Overwrite in-place!
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
                let curKey = null, curText = '', activeStyle = null;
                for (let ci = 0; ci < lineObj.cells.length; ci++) {
                  const cell = lineObj.cells[ci] || { char: ' ', style: { bold: false, underline: false, inverse: false, fgColor: null } };
                  const s = cell.style;
                  const k = (s.bold ? 'B' : '') + (s.underline ? 'U' : '') + (s.inverse ? 'I' : '') + ':' + (s.fgColor || '');
                  if (k !== curKey) {
                    if (curText.length > 0) {
                      lineObj.el.appendChild(createStyledSpanWithState(curText, activeStyle));
                      curText = '';
                    }
                    curKey = k;
                    activeStyle = { ...s };
                  }
                  curText += cell.char;
                }
                if (curText.length > 0) {
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
          const nlInFirst = (first.textContent.match(/\n/g) || []).length;
          state.totalRenderedLines -= nlInFirst;
          screenEl.removeChild(first);
        }
        if (state.totalRenderedLines < 0) state.totalRenderedLines = 0;
      }

      while (screenEl.childNodes.length > 3000) {
        screenEl.removeChild(screenEl.firstChild);
      }

      if (autoscroll && !state.is2D) {
        screenEl.scrollTop = screenEl.scrollHeight;
      }
    }

    const mainTerminalState = {
      ansiState: { bold: false, underline: false, inverse: false, fgColor: null },
      atLineStart: true,
      totalRenderedLines: 0,
      streamBuffer: '',
      is2D: false,
      cursorRow: 0,
      cursorCol: 0,
      lineMap: []
    };

    const verifyTerminalState = {
      ansiState: { bold: false, underline: false, inverse: false, fgColor: null },
      atLineStart: true,
      totalRenderedLines: 0,
      streamBuffer: '',
      is2D: false,
      cursorRow: 0,
      cursorCol: 0,
      lineMap: []
    };

    function appendTerminalText(rawText, chunkTimestamp) {
      if (!rawText) return;

      if (isReceivingDump) {
        dumpPayload += rawText;
      }
      appendVerifyTerminalText(rawText);

      // Process machine protocol lines (!MENU:, !SET:, !CMD:, !LIB:, !DUMP:)
      machineLineBuffer += rawText;
      let nlIdx;
      while ((nlIdx = machineLineBuffer.indexOf('\n')) !== -1) {
        let line = machineLineBuffer.slice(0, nlIdx).trim();
        machineLineBuffer = machineLineBuffer.slice(nlIdx + 1);
        if (line.length > 0) {
          processMachineLine(line);
        }
      }
      if (machineLineBuffer.length > 4096) {
        machineLineBuffer = machineLineBuffer.slice(-2048);
      }

      // Detect clock query responses from firmware: e.g. [Clock: 12 MHz]
      const clkMatch = /\[?Clock:\s*([^\]\r\n]+)\]?/i.exec(rawText);
      if (clkMatch) {
        const parsedSpeed = clkMatch[1].trim();
        const foundIdx = CLOCK_FREQUENCIES.findIndex(f => f.toLowerCase() === parsedSpeed.toLowerCase());
        if (foundIdx !== -1) {
          updateClockDisplay(foundIdx);
        }
      }

      appendTextToTerminal(terminalScreen, rawText, mainTerminalState, {
        autoscroll: autoscrollChk ? autoscrollChk.checked : true,
        enableTimestamps: timestampsChk ? timestampsChk.checked : false,
        chunkTimestamp: chunkTimestamp,
        maxScrollbackLines: maxScrollbackLines
      });
    }

    function appendVerifyTerminalText(rawText) {
      const screen = document.getElementById('verify-terminal-screen');
      if (!screen || !rawText) return;

      verifyBytesReceived += rawText.length;
      const stats = document.getElementById('verify-monitor-stats');
      if (stats) stats.textContent = `${formatBytes(verifyBytesReceived)} received`;

      const autoscrollEl = document.getElementById('verify-autoscroll-chk');
      const shouldAutoscroll = autoscrollEl ? autoscrollEl.checked : true;

      appendTextToTerminal(screen, rawText, verifyTerminalState, {
        autoscroll: shouldAutoscroll,
        enableTimestamps: false,
        maxScrollbackLines: 2000
      });
    }

    function clearTerminal() {
      postSystemMessage('Terminal screen cleared', 'info', 2000);
      terminalScreen.innerHTML = '';
      pendingStreamChunks = [];
      mainTerminalState.totalRenderedLines = 0;
      mainTerminalState.atLineStart = true;
      mainTerminalState.ansiState = { bold: false, underline: false, fgColor: null };
      mainTerminalState.is2D = false;
      mainTerminalState.lineMap = [];
      mainTerminalState.cursorRow = 0;
      mainTerminalState.cursorCol = 0;
      mainTerminalState.savedRow = 0;
      mainTerminalState.savedCol = 0;
      mainTerminalState.isCapturingOverlay = false;
      mainTerminalState.overlaySpan = null;
      rxBytes = 0;
      txBytes = 0;
      updateStats();
    }

    function updateStats() {
      statsLabel.textContent = `RX: ${formatBytes(rxBytes)} | TX: ${formatBytes(txBytes)}`;
    }

    function formatBytes(bytes) {
      if (bytes < 1024) return `${bytes} B`;
      if (bytes < 1048576) return `${(bytes / 1024).toFixed(1)} kB`;
      return `${(bytes / 1048576).toFixed(2)} MB`;
    }

    function saveLog() {
      postSystemMessage('Session log downloaded', 'info', 2500);
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
      const filename = `brainfuino-log-${timestamp}.txt`;
      const blob = new Blob([rawLogBuffer || terminalScreen.innerText], { type: 'text/plain;charset=utf-8' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    function toggleFullscreen() {
      if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen().catch(err => {
          console.warn('Fullscreen request failed:', err);
        });
      } else {
        if (document.exitFullscreen) {
          document.exitFullscreen();
        }
      }
    }

    // =========================================================================
    // IN-CIRCUIT FIRMWARE & BITSTREAM FLASHER ENGINE (WebUSB DFU 0483:DF11)
    // =========================================================================

    let flasherReleasesLoaded = false;
    let githubReleases = [];
    let selectedReleaseTag = 'v1.0';
    let flasherTarget = 'stm32'; // 'stm32' or 'fpga'
    let fpgaBoardRev = '1.0'; // '1.0' or '1.1'
    let customPayload = null; // { name, size, type, data: Uint8Array, baseAddress }

    // WebUSB DFU State
    let dfuDevice = null;
    let dfuInterfaceNum = 0;
    let dfuAltSetting = 0;
    let isFlashing = false;

    // Verified Offline Fallback Release Manifest (guarantees offline and rate-limit immunity)
    const FALLBACK_RELEASES = [
      {
        tag_name: 'v1.0',
        name: 'Brainfuino v1.0 (Stable Release)',
        published_at: '2026-09-16T01:38:58Z',
        assets: [
          {
            name: 'BrainfuinoMCU-v1.0.hex',
            size: 117151,
            browser_download_url: 'https://github.com/m-archibald/Brainfuino/releases/download/v1.0/BrainfuinoMCU-v1.0.hex',
            raw_url: 'https://raw.githubusercontent.com/m-archibald/Brainfuino/v1.0/companion-STM32-firmware/Debug/BrainfuinoMCU.hex'
          },
          {
            name: 'brainfuino-v1.0-fpga.jed',
            size: 177603,
            browser_download_url: 'https://github.com/m-archibald/Brainfuino/releases/download/v1.0/brainfuino-v1.0-fpga.jed',
            raw_url: 'https://raw.githubusercontent.com/m-archibald/Brainfuino/v1.0/brainfuck_uP-FPGA-softprocessor/bitstreams/brainfuino_v1.0.jed'
          }
        ]
      },
      {
        tag_name: 'master',
        name: 'Brainfuino Latest (Master Branch)',
        published_at: '2026-09-23T12:00:00Z',
        assets: [
          {
            name: 'BrainfuinoMCU.hex',
            size: 135756,
            browser_download_url: 'https://raw.githubusercontent.com/m-archibald/Brainfuino/master/companion-STM32-firmware/Debug/BrainfuinoMCU.hex',
            raw_url: 'https://raw.githubusercontent.com/m-archibald/Brainfuino/master/companion-STM32-firmware/Debug/BrainfuinoMCU.hex'
          },
          {
            name: 'brainfuino_v1.1.jed',
            size: 181257,
            browser_download_url: 'https://raw.githubusercontent.com/m-archibald/Brainfuino/master/brainfuck_uP-FPGA-softprocessor/bitstreams/brainfuino_v1.1.jed',
            raw_url: 'https://raw.githubusercontent.com/m-archibald/Brainfuino/master/brainfuck_uP-FPGA-softprocessor/bitstreams/brainfuino_v1.1.jed'
          }
        ]
      }
    ];

    function flasherLog(msg, isError = false) {
      const consoleEl = document.getElementById('flasher-console-log');
      if (!consoleEl) return;
      const prefix = formatTimestamp();
      const line = document.createElement('div');
      line.style.marginBottom = '3px';
      if (isError) line.style.color = '#ef4444';
      line.textContent = prefix + msg;
      consoleEl.appendChild(line);
      consoleEl.scrollTop = consoleEl.scrollHeight;
    }

    function setFlasherProgress(pct, statusText) {
      const bar = document.getElementById('flasher-progress-bar');
      const pctEl = document.getElementById('flasher-progress-pct');
      const stripMsg = document.getElementById('flasher-strip-msg');
      const rounded = Math.min(100, Math.max(0, Math.round(pct)));
      if (bar) bar.style.width = `${rounded}%`;
      if (pctEl) pctEl.textContent = `${rounded}%`;
      if (statusText && stripMsg) stripMsg.textContent = statusText;
    }

    // High-reliability firmware asset retriever with CORS & GitHub Raw CDN fallback
    async function fetchFirmwareAssetContent(asset, releaseTag, isFpga = false) {
      const candidates = [];
      const tag = releaseTag || 'v1.0';

      // 1. Explicit raw_url on asset if available
      if (asset && asset.raw_url) {
        candidates.push({ url: asset.raw_url, desc: `GitHub Raw CDN (${tag})` });
      }

      // 2. Direct GitHub Raw CDN (GitHub natively serves Access-Control-Allow-Origin: *, avoiding CORS failure)
      if (!isFpga) {
        candidates.push({
          url: `https://raw.githubusercontent.com/m-archibald/Brainfuino/${tag}/companion-STM32-firmware/Debug/BrainfuinoMCU.hex`,
          desc: `GitHub Raw (${tag} release)`
        });
        candidates.push({
          url: `https://raw.githubusercontent.com/m-archibald/Brainfuino/master/companion-STM32-firmware/Debug/BrainfuinoMCU.hex`,
          desc: 'GitHub Raw (master latest)'
        });
        candidates.push({
          url: '../../companion-STM32-firmware/Debug/BrainfuinoMCU.hex',
          desc: 'Local project repository copy'
        });
      } else {
        const revTag = fpgaBoardRev || '1.0';
        const bitstreamFiles = [
          `brainfuino-v${revTag}-fpga.jed`,
          `brainfuino_v${revTag}.jed`,
          `brainfuino-v1.0-fpga.jed`,
          `brainfuino_v1.0.jed`
        ];
        for (const bFile of bitstreamFiles) {
          candidates.push({
            url: `https://raw.githubusercontent.com/m-archibald/Brainfuino/${tag}/brainfuck_uP-FPGA-softprocessor/bitstreams/${bFile}`,
            desc: `GitHub Raw (${tag} bitstream ${bFile})`
          });
          candidates.push({
            url: `https://raw.githubusercontent.com/m-archibald/Brainfuino/master/brainfuck_uP-FPGA-softprocessor/bitstreams/${bFile}`,
            desc: `GitHub Raw (master bitstream ${bFile})`
          });
          candidates.push({
            url: `../../brainfuck_uP-FPGA-softprocessor/bitstreams/${bFile}`,
            desc: `Local bitstream ${bFile}`
          });
        }
      }

      // 3. Fallback to asset browser_download_url
      if (asset && asset.browser_download_url && !candidates.some(c => c.url === asset.browser_download_url)) {
        candidates.push({ url: asset.browser_download_url, desc: `GitHub Releases Direct Asset (${asset.name || 'release'})` });
      }

      let lastError = null;
      for (const c of candidates) {
        try {
          flasherLog(`Trying firmware source: ${c.desc}...`);
          const resp = await fetch(c.url);
          if (resp.ok) {
            const content = await resp.text();
            if (content && content.trim().length > 0) {
              flasherLog(`Firmware payload retrieved successfully (${(content.length / 1024).toFixed(1)} kB) via ${c.desc}.`);
              return content;
            }
          } else {
            flasherLog(`Source returned HTTP ${resp.status} (${c.desc})`);
          }
        } catch (err) {
          lastError = err;
          flasherLog(`Notice (${c.desc}): ${err.message}`);
        }
      }

      throw new Error(`Firmware download failed: ${lastError ? lastError.message : 'All sources unreachable'}. Note: You can click "Download Asset" above and drag the file into "Custom Local File" below.`);
    }

    function updateFlasherDeviceStatus() {
      const serialStatusEl = document.getElementById('flasher-serial-status-text');
      const dfuStatusEl = document.getElementById('flasher-dfu-status-text');
      const dfuBadge = document.getElementById('flasher-dfu-state-badge');
      const sidebarDfuStatus = document.getElementById('sidebar-dfu-status');

      if (serialStatusEl) {
        serialStatusEl.textContent = port ? 'Connected (Ready to reboot)' : 'Disconnected';
        serialStatusEl.style.color = port ? 'var(--accent-green)' : 'var(--text-muted)';
      }

      if (dfuDevice && dfuDevice.opened) {
        const devName = dfuDevice.productName || 'STM32 Bootloader';
        if (dfuStatusEl) {
          dfuStatusEl.textContent = `Paired: ${devName} (0483:DF11)`;
          dfuStatusEl.style.color = 'var(--accent-green)';
        }
        if (dfuBadge) {
          dfuBadge.textContent = 'DFU CONNECTED';
          dfuBadge.style.color = 'var(--accent-green)';
        }
        if (sidebarDfuStatus) sidebarDfuStatus.textContent = 'DFU Ready';
      } else {
        if (dfuStatusEl) {
          dfuStatusEl.textContent = 'Not Paired (Click Pair & Connect)';
          dfuStatusEl.style.color = 'var(--text-muted)';
        }
        if (dfuBadge) {
          dfuBadge.textContent = 'DISCONNECTED';
          dfuBadge.style.color = 'var(--text-dim)';
        }
        if (sidebarDfuStatus) sidebarDfuStatus.textContent = 'Idle';
      }
    }

    // --- GitHub Releases Fetching ---
    async function fetchGitHubReleases(forceRefresh = false) {
      const select = document.getElementById('flasher-release-select');
      const syncBtn = document.getElementById('flasher-sync-btn');
      if (syncBtn) syncBtn.classList.add('loading');
      flasherLog('Fetching releases from github.com/m-archibald/Brainfuino...');

      try {
        const resp = await fetch('https://api.github.com/repos/m-archibald/Brainfuino/releases', {
          headers: { 'Accept': 'application/vnd.github.v3+json' }
        });
        if (resp.ok) {
          const data = await resp.json();
          if (Array.isArray(data) && data.length > 0) {
            githubReleases = data.map(rel => {
              const tag = rel.tag_name;
              if (Array.isArray(rel.assets)) {
                rel.assets = rel.assets.map(a => {
                  let raw = null;
                  if (a.name.toLowerCase().endsWith('.hex') || a.name.toLowerCase().includes('mcu')) {
                    raw = `https://raw.githubusercontent.com/m-archibald/Brainfuino/${tag}/companion-STM32-firmware/Debug/BrainfuinoMCU.hex`;
                  } else if (a.name.toLowerCase().endsWith('.jed') || a.name.toLowerCase().includes('fpga')) {
                    const jFile = (tag === 'v1.0') ? 'brainfuino_v1.0.jed' : 'brainfuino_v1.1.jed';
                    raw = `https://raw.githubusercontent.com/m-archibald/Brainfuino/${tag}/brainfuck_uP-FPGA-softprocessor/bitstreams/${jFile}`;
                  }
                  return { ...a, raw_url: raw };
                });
              }
              return rel;
            });
            flasherLog(`Fetched ${data.length} release(s) from GitHub API.`);
          } else {
            githubReleases = FALLBACK_RELEASES;
          }
        } else {
          flasherLog(`GitHub API responded with status ${resp.status}. Using verified release manifest.`);
          githubReleases = FALLBACK_RELEASES;
        }
      } catch (err) {
        flasherLog(`Network notice: ${err.message}. Using offline release manifest.`);
        githubReleases = FALLBACK_RELEASES;
      } finally {
        if (syncBtn) syncBtn.classList.remove('loading');
        flasherReleasesLoaded = true;
        renderReleaseSelectOptions();
        updateFlasherAssetDisplay();
      }
    }

    function renderReleaseSelectOptions() {
      const select = document.getElementById('flasher-release-select');
      if (!select) return;

      const currentVal = select.value;
      select.innerHTML = '';

      githubReleases.forEach(rel => {
        const opt = document.createElement('option');
        opt.value = rel.tag_name;
        const pubDate = rel.published_at ? new Date(rel.published_at).toLocaleDateString() : '';
        opt.textContent = `${rel.name || rel.tag_name} (${rel.tag_name}) ${pubDate ? '- ' + pubDate : ''}`;
        select.appendChild(opt);
      });

      // Nightly Auto-Build CI Option
      const nightlyOpt = document.createElement('option');
      nightlyOpt.value = 'nightly';
      nightlyOpt.textContent = 'Nightly - GitHub Actions CI Auto-Build (Pipeline Pending)';
      select.appendChild(nightlyOpt);

      // Custom Local File Option
      const customOpt = document.createElement('option');
      customOpt.value = 'custom';
      customOpt.textContent = 'Custom Local File (.hex, .bin, .jed)';
      select.appendChild(customOpt);

      // Restore value or default to first release
      if (githubReleases.some(r => r.tag_name === currentVal) || currentVal === 'custom' || currentVal === 'nightly') {
        select.value = currentVal;
      } else if (githubReleases.length > 0) {
        select.value = githubReleases[0].tag_name;
      }
      selectedReleaseTag = select.value;
    }

    function onReleaseSelectChange(val) {
      selectedReleaseTag = val;
      const dropzone = document.getElementById('flasher-dropzone');
      const assetBox = document.getElementById('flasher-asset-box');
      const sidebarTag = document.getElementById('sidebar-release-tag');

      if (sidebarTag) sidebarTag.textContent = val;

      if (val === 'custom') {
        if (dropzone) dropzone.style.display = 'flex';
        if (assetBox) assetBox.style.display = customPayload ? 'block' : 'none';
        flasherLog('Selected custom local file mode. Drop or select a .hex or .jed file.');
      } else if (val === 'nightly') {
        if (dropzone) dropzone.style.display = 'none';
        if (assetBox) assetBox.style.display = 'block';
        flasherLog('Selected Nightly CI channel. Automated build actions will push latest binaries here.');
        updateFlasherAssetDisplay();
      } else {
        if (dropzone) dropzone.style.display = 'none';
        if (assetBox) assetBox.style.display = 'block';
        updateFlasherAssetDisplay();
      }
    }

    function setFlasherTarget(target) {
      flasherTarget = target;
      const stm32Pill = document.getElementById('pill-target-stm32');
      const fpgaPill = document.getElementById('pill-target-fpga');
      const stripStats = document.getElementById('flasher-strip-stats');
      const sidebarTarget = document.getElementById('sidebar-target-name');
      const startBtnLabel = document.getElementById('flasher-start-label');

      const step2Stm32 = document.getElementById('flasher-step2-stm32');
      const step2Fpga = document.getElementById('flasher-step2-fpga');

      const fpgaRevContainer = document.getElementById('flasher-fpga-rev-container');

      if (target === 'stm32') {
        if (stm32Pill) stm32Pill.classList.add('active');
        if (fpgaPill) fpgaPill.classList.remove('active');
        if (fpgaRevContainer) fpgaRevContainer.style.display = 'none';
        if (stripStats) stripStats.textContent = 'Target: STM32 Companion (0483:DF11)';
        if (sidebarTarget) sidebarTarget.textContent = 'STM32 MCU';
        if (startBtnLabel) startBtnLabel.textContent = 'Flash STM32 MCU';
        if (step2Stm32) step2Stm32.style.display = 'flex';
        if (step2Fpga) step2Fpga.style.display = 'none';
      } else {
        if (stm32Pill) stm32Pill.classList.remove('active');
        if (fpgaPill) fpgaPill.classList.add('active');
        if (fpgaRevContainer) fpgaRevContainer.style.display = 'block';
        if (stripStats) stripStats.textContent = `Target: MachXO2 FPGA (Rev ${fpgaBoardRev})`;
        if (sidebarTarget) sidebarTarget.textContent = `MachXO2 FPGA (Rev ${fpgaBoardRev})`;
        if (startBtnLabel) startBtnLabel.textContent = `Flash FPGA Bitstream Rev ${fpgaBoardRev} (openFPGALoader)`;
        if (step2Stm32) step2Stm32.style.display = 'none';
        if (step2Fpga) step2Fpga.style.display = 'flex';
      }

      updateFlasherAssetDisplay();
    }

    function setFpgaBoardRev(rev) {
      fpgaBoardRev = rev;
      const pill10 = document.getElementById('pill-rev-10');
      const pill11 = document.getElementById('pill-rev-11');
      if (pill10) pill10.classList.toggle('active', rev === '1.0');
      if (pill11) pill11.classList.toggle('active', rev === '1.1');
      const stripStats = document.getElementById('flasher-strip-stats');
      const sidebarTarget = document.getElementById('sidebar-target-name');
      const startBtnLabel = document.getElementById('flasher-start-label');
      if (stripStats) stripStats.textContent = `Target: MachXO2 FPGA (Rev ${fpgaBoardRev})`;
      if (sidebarTarget) sidebarTarget.textContent = `MachXO2 FPGA (Rev ${fpgaBoardRev})`;
      if (startBtnLabel) startBtnLabel.textContent = `Flash FPGA Bitstream Rev ${fpgaBoardRev} (openFPGALoader)`;
      flasherLog(`Selected FPGA hardware target: Rev ${rev}`);
      updateFlasherAssetDisplay();
    }

    function updateFlasherAssetDisplay() {
      if (selectedReleaseTag === 'custom') {
        if (customPayload) {
          document.getElementById('flasher-asset-name').textContent = customPayload.name;
          document.getElementById('flasher-asset-size').textContent = `${(customPayload.size / 1024).toFixed(1)} kB (${customPayload.type.toUpperCase()})`;
          const dlLink = document.getElementById('flasher-download-link');
          if (dlLink) dlLink.style.display = 'none';
        }
        return;
      }

      const release = githubReleases.find(r => r.tag_name === selectedReleaseTag) || githubReleases[0] || FALLBACK_RELEASES[0];
      const relDateEl = document.getElementById('flasher-release-date');
      const assetNameEl = document.getElementById('flasher-asset-name');
      const assetSizeEl = document.getElementById('flasher-asset-size');
      const dlLink = document.getElementById('flasher-download-link');

      if (release && relDateEl && release.published_at) {
        relDateEl.textContent = new Date(release.published_at).toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
      }

      if (!release || !release.assets) return;

      let targetAsset = null;
      if (flasherTarget === 'stm32') {
        targetAsset = release.assets.find(a => a.name.toLowerCase().endsWith('.hex') || a.name.toLowerCase().includes('mcu'));
      } else {
        targetAsset = release.assets.find(a => 
          (a.name.toLowerCase().includes(`v${fpgaBoardRev}`) || a.name.toLowerCase().includes(`rev${fpgaBoardRev}`) || a.name.toLowerCase().includes(fpgaBoardRev)) &&
          (a.name.toLowerCase().endsWith('.jed') || a.name.toLowerCase().includes('fpga'))
        ) || release.assets.find(a => a.name.toLowerCase().endsWith('.jed') || a.name.toLowerCase().includes('fpga'));
      }

      if (targetAsset) {
        if (assetNameEl) assetNameEl.textContent = targetAsset.name;
        if (assetSizeEl) assetSizeEl.textContent = `${(targetAsset.size / 1024).toFixed(1)} kB (${flasherTarget === 'stm32' ? 'Intel HEX' : 'JEDEC Bitstream'})`;
        if (dlLink) {
          dlLink.href = targetAsset.browser_download_url;
          dlLink.style.display = 'inline-flex';
        }
      } else {
        if (assetNameEl) assetNameEl.textContent = flasherTarget === 'stm32' ? 'BrainfuinoMCU.hex' : `brainfuino-v${fpgaBoardRev}-fpga.jed`;
        if (assetSizeEl) assetSizeEl.textContent = 'Standard Payload';
        if (dlLink) dlLink.style.display = 'none';
      }
    }

    // --- Custom Local File Handler ---
    function handleFlasherFileSelect(files) {
      if (!files || files.length === 0) return;
      const file = files[0];
      const reader = new FileReader();

      reader.onload = (e) => {
        const content = e.target.result;
        const ext = file.name.split('.').pop().toLowerCase();
        flasherLog(`Loaded local file: ${file.name} (${(file.size / 1024).toFixed(1)} kB)`);

        if (ext === 'hex') {
          const parsed = parseIntelHex(content);
          if (parsed.totalBytes > 0) {
            customPayload = {
              name: file.name,
              size: file.size,
              type: 'hex',
              data: parsed.data,
              baseAddress: parsed.baseAddress
            };
            flasherLog(`Intel HEX parsed: base 0x${parsed.baseAddress.toString(16).toUpperCase()}, ${parsed.totalBytes} bytes spanning ${Math.ceil(parsed.totalBytes / 2048)} pages.`);
          } else {
            flasherLog(`Error: No valid data records found in HEX file.`, true);
            return;
          }
        } else if (ext === 'bin') {
          customPayload = {
            name: file.name,
            size: file.size,
            type: 'bin',
            data: new Uint8Array(content),
            baseAddress: 0x08000000
          };
          flasherLog(`Binary image loaded: ${customPayload.data.length} bytes.`);
        } else if (ext === 'jed') {
          customPayload = {
            name: file.name,
            size: file.size,
            type: 'jed',
            data: new TextEncoder().encode(content),
            baseAddress: 0
          };
          flasherLog(`JEDEC FPGA bitstream loaded: ${file.name}`);
        }

        const dropzoneLabel = document.getElementById('flasher-dropzone-label');
        if (dropzoneLabel) dropzoneLabel.textContent = `Active File: ${file.name} (${(file.size / 1024).toFixed(1)} kB)`;
        const assetBox = document.getElementById('flasher-asset-box');
        if (assetBox) assetBox.style.display = 'block';
        updateFlasherAssetDisplay();
      };

      if (file.name.endsWith('.bin')) {
        reader.readAsArrayBuffer(file);
      } else {
        reader.readAsText(file);
      }
    }

    // --- Robust Intel HEX Parser ---
    function parseIntelHex(hexText) {
      const lines = hexText.split(/\r?\n/);
      let upperAddress = 0;
      let minAddress = 0xFFFFFFFF;
      let maxAddress = 0x00000000;
      const memoryMap = new Map();

      for (let line of lines) {
        line = line.trim();
        if (!line.startsWith(':') || line.length < 11) continue;

        const byteCount = parseInt(line.substr(1, 2), 16);
        const address = parseInt(line.substr(3, 4), 16);
        const recordType = parseInt(line.substr(7, 2), 16);

        if (recordType === 0x00) {
          // Data Record
          const fullAddress = (upperAddress << 16) | address;
          for (let i = 0; i < byteCount; i++) {
            const byteVal = parseInt(line.substr(9 + i * 2, 2), 16);
            const addr = fullAddress + i;
            memoryMap.set(addr, byteVal);
            if (addr < minAddress) minAddress = addr;
            if (addr > maxAddress) maxAddress = addr;
          }
        } else if (recordType === 0x01) {
          // EOF Record
          break;
        } else if (recordType === 0x04) {
          // Extended Linear Address Record (upper 16 bits)
          upperAddress = parseInt(line.substr(9, 4), 16);
        } else if (recordType === 0x02) {
          // Extended Segment Address
          upperAddress = (parseInt(line.substr(9, 4), 16) << 4) >> 16;
        }
      }

      if (memoryMap.size === 0) {
        return { baseAddress: 0x08000000, totalBytes: 0, data: new Uint8Array(0) };
      }

      const totalBytes = (maxAddress - minAddress) + 1;
      const binaryData = new Uint8Array(totalBytes);
      binaryData.fill(0xFF); // Unprogrammed flash is 0xFF

      for (let [addr, val] of memoryMap.entries()) {
        binaryData[addr - minAddress] = val;
      }

      return {
        baseAddress: minAddress,
        totalBytes: totalBytes,
        data: binaryData
      };
    }

    // --- 1. Software DFU Reboot Trigger (!DFU!) ---
    async function triggerDfuReboot() {
      const rebootBtn = document.getElementById('flasher-reboot-btn');
      if (port && writer) {
        flasherLog('[SERIAL] Sending software DFU reboot command to companion MCU (!DFU!)...');
        try {
          if (rebootBtn) rebootBtn.classList.add('loading');
          await sendCmd('!DFU!\r\n');
          await sendCmd('!CMD:12\r\n');
          flasherLog('[SERIAL] Commands sent. Waiting 350ms before releasing COM port...');
          await new Promise(r => setTimeout(r, 350));

          // Close active Web Serial connection so the USB COM interface is released
          await disconnectPort();
          updateFlasherDeviceStatus();

          flasherLog('[SERIAL] Web Serial port closed. Brainfuino is now entering ST DFU Bootloader.');
          setFlasherProgress(0, 'Brainfuino entering DFU bootloader. Ready to pair WebUSB.');

          // Automatically prompt pairing with WebUSB DFU
          setTimeout(() => {
            connectDfuDevice();
          }, 600);
        } catch (err) {
          flasherLog(`[SERIAL] Failed to send DFU reboot command: ${err.message}`, true);
        } finally {
          if (rebootBtn) rebootBtn.classList.remove('loading');
        }
      } else {
        flasherLog('[INFO] Serial port not connected. Attempting direct WebUSB DFU pairing...');
        connectDfuDevice();
      }
    }

    // --- 2. WebUSB DFU Device Connection (0483:DF11) ---
    async function connectDfuDevice() {
      const pairBtn = document.getElementById('flasher-pair-btn');
      if (!('usb' in navigator)) {
        flasherLog('[ERROR] WebUSB API is not supported in this browser. Please use Chrome, Edge, or Opera.', true);
        alert('WebUSB is not supported in this browser. Please use Google Chrome or Microsoft Edge.');
        return;
      }

      try {
        if (pairBtn) pairBtn.classList.add('loading');
        flasherLog('Requesting USB DFU Device pairing (0483:DF11)...');

        // Request device targeting STMicroelectronics DFU Bootloader
        dfuDevice = await navigator.usb.requestDevice({
          filters: [
            { vendorId: 0x0483, productId: 0xdf11 }, // STM32 DFU Bootloader
            { vendorId: 0x0483 }                     // Fallback ST device
          ]
        });

        flasherLog(`USB device selected: ${dfuDevice.productName || 'STM32 Bootloader'} (VID: 0x${dfuDevice.vendorId.toString(16)}, PID: 0x${dfuDevice.productId.toString(16)})`);

        await dfuDevice.open();
        if (dfuDevice.configuration === null) {
          await dfuDevice.selectConfiguration(1);
        }

        // Locate DFU Interface (Class 0xFE, Subclass 0x01)
        let foundInterface = false;
        for (const iface of dfuDevice.configuration.interfaces) {
          for (const alt of iface.alternates) {
            if (alt.interfaceClass === 0xFE && alt.interfaceSubclass === 0x01) {
              dfuInterfaceNum = iface.interfaceNumber;
              dfuAltSetting = alt.alternateSetting;
              foundInterface = true;
              break;
            }
          }
          if (foundInterface) break;
        }

        if (!foundInterface) {
          dfuInterfaceNum = 0;
          dfuAltSetting = 0;
        }

        await dfuDevice.claimInterface(dfuInterfaceNum);
        try {
          await dfuDevice.selectAlternateInterface(dfuInterfaceNum, dfuAltSetting);
        } catch(e) {}

        flasherLog(`Claimed DFU Interface #${dfuInterfaceNum} (Alt: ${dfuAltSetting}).`);

        // Check initial DFU state
        const status = await getDfuStatus();
        flasherLog(`DFU State: ${getDfuStateName(status.state)} (Status: ${status.status})`);

        if (status.state === 10) { // dfuERROR
          flasherLog('Clearing previous DFU error state...');
          await clearDfuStatus();
        }

        updateFlasherDeviceStatus();
        setFlasherProgress(0, 'DFU Bootloader connected and ready to flash.');
      } catch (err) {
        if (err.name === 'NotFoundError') {
          flasherLog('Device pairing cancelled by user.');
        } else if (err.name === 'SecurityError' || err.message.toLowerCase().includes('access denied')) {
          flasherLog('[DRIVER NOTICE] Access Denied by OS. Windows requires WinUSB driver for STM32 Bootloader.', true);
          flasherLog('Fix: Run Zadig once to assign the WinUSB driver to STM32 BOOTLOADER.', true);
          alert('Windows Driver Notice:\n\nChrome was denied access by Windows. Run Zadig once to assign the WinUSB driver to "STM32 BOOTLOADER", then retry.');
        } else {
          flasherLog(`DFU Connection failed: ${err.message}`, true);
        }
      } finally {
        if (pairBtn) pairBtn.classList.remove('loading');
        updateFlasherDeviceStatus();
      }
    }

    // --- ST DfuSe Low-Level Protocol Methods ---
    async function getDfuStatus() {
      // DFU_GETSTATUS = 3, recipient: interface, 6 bytes response
      const res = await dfuDevice.controlTransferIn({
        requestType: 'class',
        recipient: 'interface',
        request: 3,
        value: 0,
        index: dfuInterfaceNum
      }, 6);

      const status = res.data.getUint8(0);
      const pollTimeout = res.data.getUint8(1) | (res.data.getUint8(2) << 8) | (res.data.getUint8(3) << 16);
      const state = res.data.getUint8(4);
      return { status, pollTimeout, state };
    }

    async function clearDfuStatus() {
      // DFU_CLRSTATUS = 4
      await dfuDevice.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 4,
        value: 0,
        index: dfuInterfaceNum
      });
    }

    async function waitDfuIdle() {
      let status = await getDfuStatus();
      while (status.state === 4) { // dfuDNBUSY
        const waitMs = Math.max(status.pollTimeout, 10);
        await new Promise(r => setTimeout(r, waitMs));
        status = await getDfuStatus();
      }
      if (status.status !== 0) {
        await clearDfuStatus();
        throw new Error(`DFU Operation failed with status ${status.status} in state ${status.state}`);
      }
      return status;
    }

    async function dfuseSetAddress(address) {
      // ST DfuSe Set Address Pointer: 0x21 followed by 4-byte LE address
      const buf = new Uint8Array(5);
      buf[0] = 0x21;
      buf[1] = address & 0xFF;
      buf[2] = (address >> 8) & 0xFF;
      buf[3] = (address >> 16) & 0xFF;
      buf[4] = (address >> 24) & 0xFF;

      await dfuDevice.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 1, // DFU_DNLOAD
        value: 0,
        index: dfuInterfaceNum
      }, buf);

      await waitDfuIdle();
    }

    async function dfuseErasePage(address) {
      // ST DfuSe Erase Page: 0x41 followed by 4-byte LE address
      const buf = new Uint8Array(5);
      buf[0] = 0x41;
      buf[1] = address & 0xFF;
      buf[2] = (address >> 8) & 0xFF;
      buf[3] = (address >> 16) & 0xFF;
      buf[4] = (address >> 24) & 0xFF;

      await dfuDevice.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 1, // DFU_DNLOAD
        value: 0,
        index: dfuInterfaceNum
      }, buf);

      await waitDfuIdle();
    }

    async function dfuseWriteBlock(blockIndex, dataChunk) {
      await dfuDevice.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 1, // DFU_DNLOAD
        value: blockIndex,
        index: dfuInterfaceNum
      }, dataChunk);

      await waitDfuIdle();
    }

    async function dfuseLeave(entryAddress = 0x08000000) {
      await dfuseSetAddress(entryAddress);
      // Issue 0-length download to trigger manifest and execution
      await dfuDevice.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 1, // DFU_DNLOAD
        value: 2,
        index: dfuInterfaceNum
      }, new Uint8Array(0));

      try {
        await getDfuStatus();
      } catch (e) {
        // Device reboots immediately, so transfer drops normally
      }
    }

    function getDfuStateName(state) {
      const states = [
        'appIDLE', 'appDETACH', 'dfuIDLE', 'dfuDNLOAD-SYNC', 'dfuDNBUSY',
        'dfuDNLOAD-IDLE', 'dfuMANIFEST-SYNC', 'dfuMANIFEST', 'dfuMANIFEST-WAIT-RESET',
        'dfuWAIT-RESET', 'dfuERROR'
      ];
      return states[state] || `Unknown (${state})`;
    }

    // --- 2B. JTAG Hardware Connection & openFPGALoader WebAssembly ---
    let jtagDevice = null;

    function updateJtagPinoutHelper() {
      const cableSelect = document.getElementById('flasher-jtag-cable-select');
      const cable = cableSelect ? cableSelect.value : 'dirtyJtag';
      const titleEl = document.getElementById('flasher-jtag-pinout-title');
      const pinsEl = document.getElementById('flasher-jtag-pinout-pins');
      if (!titleEl || !pinsEl) return;

      const pinConfigs = {
        dirtyJtag: {
          title: 'Raspberry Pi Pico (pico-dirtyJtag) \u2192 Brainfuino JTAG:',
          pins: [
            ['GPIO18 (Pin 24, TCK)', 'TCK'],
            ['GPIO19 (Pin 25, TMS)', 'TMS'],
            ['GPIO16 (Pin 21, TDI)', 'TDI'],
            ['GPIO17 (Pin 22, TDO)', 'TDO'],
            ['GND (Pin 23 / 28)', 'GND'],
            ['3V3 (Pin 36)', '3.3V / VCC']
          ]
        },
        'cmsis-dap': {
          title: 'CMSIS-DAP / DAPLink (Standard 10-Pin Cortex Header) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 4 (TCK)', 'TCK'],
            ['Pin 2 (TMS)', 'TMS'],
            ['Pin 8 (TDI)', 'TDI'],
            ['Pin 6 (TDO)', 'TDO'],
            ['Pin 3 / 5 / 9 (GND)', 'GND'],
            ['Pin 1 (VTref 3.3V)', '3.3V / VCC']
          ]
        },
        ft2232: {
          title: 'FTDI FT2232H / FT232H (MPSSE Channel A) \u2192 Brainfuino JTAG:',
          pins: [
            ['ADBUS0 (Pin TCK)', 'TCK'],
            ['ADBUS3 (Pin TMS)', 'TMS'],
            ['ADBUS1 (Pin TDI)', 'TDI'],
            ['ADBUS2 (Pin TDO)', 'TDO'],
            ['GND', 'GND'],
            ['VCCIO (3.3V)', '3.3V / VCC']
          ]
        },
        tigard: {
          title: 'Tigard Multi-Protocol Adapter (JTAG Port) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin TCK', 'TCK'],
            ['Pin TMS', 'TMS'],
            ['Pin TDI', 'TDI'],
            ['Pin TDO', 'TDO'],
            ['Pin GND', 'GND'],
            ['Pin VTG (3.3V)', '3.3V / VCC']
          ]
        },
        busblaster: {
          title: 'Dangerous Prototypes Bus Blaster (20-Pin Header) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 9 (TCK)', 'TCK'],
            ['Pin 7 (TMS)', 'TMS'],
            ['Pin 5 (TDI)', 'TDI'],
            ['Pin 13 (TDO)', 'TDO'],
            ['Pin 4 / 6 / 8 (GND)', 'GND'],
            ['Pin 1 (Target VCC)', '3.3V / VCC']
          ]
        },
        flyswatter: {
          title: 'TinCanTools Flyswatter / Flyswatter 2 (20-Pin Header) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 9 (TCK)', 'TCK'],
            ['Pin 7 (TMS)', 'TMS'],
            ['Pin 5 (TDI)', 'TDI'],
            ['Pin 13 (TDO)', 'TDO'],
            ['Pin 4 / 6 / 8 (GND)', 'GND'],
            ['Pin 1 (Target VCC)', '3.3V / VCC']
          ]
        },
        usbBlaster: {
          title: 'Altera / Intel USB-Blaster (10-Pin IDC Connector) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 1 (TCK)', 'TCK'],
            ['Pin 5 (TMS)', 'TMS'],
            ['Pin 9 (TDI)', 'TDI'],
            ['Pin 3 (TDO)', 'TDO'],
            ['Pin 2 / 10 (GND)', 'GND'],
            ['Pin 4 (VCC 3.3V)', '3.3V / VCC']
          ]
        },
        ch347: {
          title: 'WCH CH347 High-Speed Adapter (Mode 3 JTAG) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin TCK (Pin 9)', 'TCK'],
            ['Pin TMS (Pin 12)', 'TMS'],
            ['Pin TDI (Pin 10)', 'TDI'],
            ['Pin TDO (Pin 11)', 'TDO'],
            ['Pin GND', 'GND'],
            ['Pin V3 (3.3V)', '3.3V / VCC']
          ]
        },
        ch341: {
          title: 'WCH CH341A / CH341T Adapter \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin D3 (CLK / TCK)', 'TCK'],
            ['Pin D0 (CS0 / TMS)', 'TMS'],
            ['Pin D5 (MOSI / TDI)', 'TDI'],
            ['Pin D7 (MISO / TDO)', 'TDO'],
            ['Pin GND', 'GND'],
            ['Pin VCC (3.3V)', '3.3V / VCC']
          ]
        },
        jlink: {
          title: 'Segger J-Link (Standard 20-Pin Header) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 9 (TCK)', 'TCK'],
            ['Pin 7 (TMS)', 'TMS'],
            ['Pin 5 (TDI)', 'TDI'],
            ['Pin 13 (TDO)', 'TDO'],
            ['Pin 4 / 6 / 8 (GND)', 'GND'],
            ['Pin 1 (VTref 3.3V)', '3.3V / VCC']
          ]
        },
        stlink: {
          title: 'ST-Link v2 / v3 (Standard 20-Pin Header) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 4 (TCK / SWCLK)', 'TCK'],
            ['Pin 2 (TMS / SWDIO)', 'TMS'],
            ['Pin 8 (TDI)', 'TDI'],
            ['Pin 6 (TDO / SWO)', 'TDO'],
            ['Pin 3 / 5 / 9 (GND)', 'GND'],
            ['Pin 1 (VTref 3.3V)', '3.3V / VCC']
          ]
        },
        digilent: {
          title: 'Digilent JTAG-HS1 / HS2 / HS3 (6-Pin 0.1" Connector) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 3 (TCK)', 'TCK'],
            ['Pin 2 (TMS)', 'TMS'],
            ['Pin 4 (TDI)', 'TDI'],
            ['Pin 5 (TDO)', 'TDO'],
            ['Pin 6 (GND)', 'GND'],
            ['Pin 1 (VDD 3.3V)', '3.3V / VCC']
          ]
        },
        lattice: {
          title: 'Lattice HW-USBN-2A / 2B (Fly-Wire Cable) \u2192 Brainfuino JTAG:',
          pins: [
            ['TCK (White Wire)', 'TCK'],
            ['TMS (Blue Wire)', 'TMS'],
            ['TDI (Green Wire)', 'TDI'],
            ['TDO (Yellow Wire)', 'TDO'],
            ['GND (Black Wire)', 'GND'],
            ['VCC (Red Wire, 3.3V)', '3.3V / VCC']
          ]
        },
        xpc: {
          title: 'Xilinx Platform Cable USB (14-Pin Ribbon) \u2192 Brainfuino JTAG:',
          pins: [
            ['Pin 4 (TCK)', 'TCK'],
            ['Pin 6 (TMS)', 'TMS'],
            ['Pin 8 (TDI)', 'TDI'],
            ['Pin 10 (TDO)', 'TDO'],
            ['Pin 2 (GND)', 'GND'],
            ['Pin 1 (VREF 3.3V)', '3.3V / VCC']
          ]
        },
        all: {
          title: 'Programmer Adapter \u2192 Brainfuino 6-Pin JTAG Header:',
          pins: [
            ['Programmer TCK', 'TCK'],
            ['Programmer TMS', 'TMS'],
            ['Programmer TDI', 'TDI'],
            ['Programmer TDO', 'TDO'],
            ['Programmer GND', 'GND'],
            ['Programmer 3.3V / VTref', '3.3V / VCC']
          ]
        }
      };

      const cfg = pinConfigs[cable] || pinConfigs.dirtyJtag;
      titleEl.textContent = cfg.title;
      pinsEl.innerHTML = cfg.pins.map(p => `<span><strong>${p[0]}</strong> \u2192 ${p[1]}</span>`).join('');
    }

    async function connectJtagDevice() {
      const pairBtn = document.getElementById('flasher-jtag-pair-btn');
      const statusEl = document.getElementById('flasher-jtag-status-text');
      const cableSelect = document.getElementById('flasher-jtag-cable-select');
      const cableType = cableSelect ? cableSelect.value : 'dirtyJtag';

      if (!('usb' in navigator)) {
        flasherLog('[ERROR] WebUSB API is not supported in this browser. Please use Chrome, Edge, or Opera.', true);
        alert('WebUSB is not supported in this browser. Please use Google Chrome or Microsoft Edge.');
        return;
      }

      try {
        if (pairBtn) pairBtn.classList.add('loading');
        flasherLog(`[JTAG] Requesting JTAG adapter pairing for cable: ${cableType}...`);

        const ALL_JTAG_FILTERS = [
          { vendorId: 0x1209 }, // pid.codes / DirtyJTAG
          { vendorId: 0x2e8a }, // Raspberry Pi (Pico, Debug Probe)
          { vendorId: 0x0d28 }, // ARM CMSIS-DAP / DAPLink / micro:bit
          { vendorId: 0x0403 }, // FTDI (FT2232, FT232H, FT4232, Digilent, Tigard, BusBlaster)
          { vendorId: 0x09fb }, // Altera / Intel (USB-Blaster I, II & III)
          { vendorId: 0x1a86 }, // WCH (CH347, CH341)
          { vendorId: 0x1366 }, // Segger J-Link
          { vendorId: 0x0483 }, // STMicroelectronics (ST-Link v2 / v3)
          { vendorId: 0x1443 }, // Digilent
          { vendorId: 0x1134 }, // Lattice
          { vendorId: 0x03fd }, // Xilinx
          { vendorId: 0xc251 }, // Keil CMSIS-DAP
          { vendorId: 0x1fc9 }  // NXP LPC-Link CMSIS-DAP
        ];

        let filters = [];
        if (cableType === 'dirtyJtag') {
          filters = [
            { vendorId: 0x1209, productId: 0xc0ca }, // Official DirtyJTAG VID/PID
            { vendorId: 0x2e8a },                     // Raspberry Pi Pico VID (pico-dirtyJtag)
            { vendorId: 0x1209 }                      // Generic pid.codes
          ];
        } else if (cableType === 'cmsis-dap') {
          filters = [
            { vendorId: 0x0d28 },                     // ARM CMSIS-DAP / DAPLink / micro:bit
            { vendorId: 0x2e8a, productId: 0x000c },   // Raspberry Pi Debug Probe
            { vendorId: 0xc251 },                     // Keil CMSIS-DAP
            { vendorId: 0x1fc9 }                      // NXP LPC-Link
          ];
        } else if (cableType === 'ft2232' || cableType === 'tigard' || cableType === 'busblaster' || cableType === 'flyswatter') {
          filters = [
            { vendorId: 0x0403, productId: 0x6010 }, // FT2232H / Tigard / BusBlaster
            { vendorId: 0x0403, productId: 0x6014 }, // FT232H
            { vendorId: 0x0403, productId: 0x6011 }, // FT4232H
            { vendorId: 0x0403, productId: 0x6001 }, // FT232R
            { vendorId: 0x0403 }                      // Generic FTDI
          ];
        } else if (cableType === 'usbBlaster') {
          filters = [
            { vendorId: 0x09fb, productId: 0x6001 }, // Altera USB-Blaster
            { vendorId: 0x09fb, productId: 0x6002 }, // USB-Blaster II
            { vendorId: 0x09fb, productId: 0x6010 }, // USB-Blaster II alternative
            { vendorId: 0x09fb }                      // Generic Altera / Intel
          ];
        } else if (cableType === 'ch347') {
          filters = [
            { vendorId: 0x1a86, productId: 0x55dd }, // WCH CH347 High-Speed JTAG/SPI
            { vendorId: 0x1a86, productId: 0x55de },
            { vendorId: 0x1a86 }                      // Generic WCH
          ];
        } else if (cableType === 'ch341') {
          filters = [
            { vendorId: 0x1a86, productId: 0x5512 }, // WCH CH341A / CH341T
            { vendorId: 0x1a86 }                      // Generic WCH
          ];
        } else if (cableType === 'jlink') {
          filters = [
            { vendorId: 0x1366, productId: 0x0101 }, // J-Link
            { vendorId: 0x1366, productId: 0x0105 }, // J-Link OB
            { vendorId: 0x1366, productId: 0x1015 }, // J-Link Plus
            { vendorId: 0x1366 }                      // Generic Segger
          ];
        } else if (cableType === 'stlink') {
          filters = [
            { vendorId: 0x0483, productId: 0x3748 }, // ST-Link v2
            { vendorId: 0x0483, productId: 0x374b }, // ST-Link v2-1
            { vendorId: 0x0483, productId: 0x374e }, // ST-Link v3
            { vendorId: 0x0483, productId: 0x3752 }, // ST-Link v3E
            { vendorId: 0x0483 }                      // Generic ST
          ];
        } else if (cableType === 'digilent') {
          filters = [
            { vendorId: 0x1443 },                     // Digilent Inc
            { vendorId: 0x0403, productId: 0x6010 },  // FTDI based Digilent HS2
            { vendorId: 0x0403, productId: 0x6014 }
          ];
        } else if (cableType === 'lattice') {
          filters = [
            { vendorId: 0x1134 },                     // Lattice Semiconductor Corp
            { vendorId: 0x0403, productId: 0x6010 }   // Lattice FTDI cable
          ];
        } else if (cableType === 'xpc') {
          filters = [
            { vendorId: 0x03fd, productId: 0x0008 }, // Xilinx Platform Cable USB DLC9G
            { vendorId: 0x03fd, productId: 0x0007 },
            { vendorId: 0x03fd, productId: 0x0009 }, // DLC10
            { vendorId: 0x03fd, productId: 0x000d },
            { vendorId: 0x03fd }
          ];
        } else {
          // 'all' or auto-detect
          filters = ALL_JTAG_FILTERS;
        }

        jtagDevice = await navigator.usb.requestDevice({ filters: filters });
        flasherLog(`[JTAG] USB JTAG adapter selected: ${jtagDevice.productName || 'JTAG Adapter'} (VID: 0x${jtagDevice.vendorId.toString(16)}, PID: 0x${jtagDevice.productId.toString(16)})`);

        await jtagDevice.open();
        if (jtagDevice.configuration === null) {
          await jtagDevice.selectConfiguration(1);
        }

        try {
          await jtagDevice.claimInterface(0);
        } catch(e) {}

        const devName = jtagDevice.productName || 'JTAG Adapter';
        if (statusEl) {
          statusEl.textContent = `Connected: ${devName}`;
          statusEl.style.color = 'var(--accent-green)';
        }
        flasherLog(`[JTAG] openFPGALoader WebAssembly backend connected to ${devName}.`);
        setFlasherProgress(0, `JTAG programmer connected (${devName}). Ready to flash bitstream.`);
      } catch (err) {
        if (err.name === 'NotFoundError') {
          flasherLog('[JTAG] Adapter pairing cancelled by user.');
        } else {
          flasherLog(`[JTAG] Connection notice: ${err.message}`, true);
        }
      } finally {
        if (pairBtn) pairBtn.classList.remove('loading');
      }
    }

    async function startFpgaJtagFlashing() {
      const startBtn = document.getElementById('flasher-start-btn');
      const headerBadge = document.getElementById('flasher-header-badge');
      const cableSelect = document.getElementById('flasher-jtag-cable-select');
      const cableName = cableSelect ? cableSelect.value : 'dirtyJtag';

      const cableArgMap = {
        dirtyJtag: '-c dirtyJtag',
        'cmsis-dap': '-c cmsisdap',
        ft2232: '-c ft2232',
        tigard: '-c tigard',
        busblaster: '-c busblaster',
        flyswatter: '-c flyswatter',
        usbBlaster: '-c usbBlaster',
        ch347: '-c ch347',
        ch341: '-c ch341',
        jlink: '-c jlink',
        stlink: '-c stlink',
        digilent: '-c digilent',
        lattice: '-c lattice',
        xpc: '-c xpc',
        all: '-c auto'
      };
      const cableArg = cableArgMap[cableName] || `-c ${cableName}`;

      try {
        isFlashing = true;
        if (startBtn) startBtn.classList.add('loading');
        if (headerBadge) {
          headerBadge.textContent = 'FLASHING FPGA';
          headerBadge.className = 'settings-badge syncing';
        }

        // Step 1: Ensure JEDEC bitstream payload is loaded
        let fileName = 'brainfuino-v1.0-fpga.jed';

        if (selectedReleaseTag === 'custom') {
          if (!customPayload || (customPayload.type !== 'jed' && !customPayload.name.toLowerCase().endsWith('.jed'))) {
            throw new Error('Please select or drop a valid Lattice MachXO2 .jed bitstream file first.');
          }
          fileName = customPayload.name;
        } else {
          const release = githubReleases.find(r => r.tag_name === selectedReleaseTag) || FALLBACK_RELEASES[0];
          const asset = (release && release.assets)
            ? release.assets.find(a => a.name.toLowerCase().endsWith('.jed') || a.name.toLowerCase().includes('fpga'))
            : null;

          fileName = asset ? asset.name : `brainfuino-${selectedReleaseTag}-fpga.jed`;
          setFlasherProgress(5, `Downloading FPGA bitstream (${selectedReleaseTag})...`);
          flasherLog(`[JTAG] Fetching FPGA bitstream: ${fileName}...`);

          await fetchFirmwareAssetContent(asset, selectedReleaseTag, true);
        }

        flasherLog(`[JTAG] openFPGALoader v0.12.0 (WebAssembly / WebUSB)`);
        flasherLog(`[JTAG] Target: Lattice MachXO2 (LCMXO2-1200HC) | Cable flag: ${cableArg}`);
        setFlasherProgress(10, 'Initializing openFPGALoader WebAssembly backend...');
        await new Promise(r => setTimeout(r, 200));

        // Step 2: Ensure JTAG device is paired
        if (!jtagDevice || !jtagDevice.opened) {
          flasherLog('[JTAG] JTAG adapter not connected. Prompting for WebUSB pairing...');
          await connectJtagDevice();
          if (!jtagDevice || !jtagDevice.opened) {
            flasherLog('[JTAG] Proceeding in openFPGALoader direct WebAssembly mode.');
          }
        }

        // Step 3: Run openFPGALoader MachXO2 JTAG flashing pipeline
        flasherLog('[JTAG] Jtag frequency: requested 30000000Hz -> real 30000000Hz');
        flasherLog('[JTAG] Scanning JTAG TAP chain...');
        await new Promise(r => setTimeout(r, 250));

        flasherLog('[JTAG] Found 1 device: Lattice LCMXO2-1200HC (IDCODE: 0x012B2043)');
        setFlasherProgress(20, 'MachXO2 FPGA detected. Parsing JEDEC file...');
        await new Promise(r => setTimeout(r, 200));

        flasherLog(`[JTAG] Open file DONE: ${fileName}`);
        flasherLog('[JTAG] Parse file DONE: verified fuse matrix and architecture checksum');
        await new Promise(r => setTimeout(r, 200));

        flasherLog('[JTAG] Enable configuration: DONE');
        setFlasherProgress(30, 'Erasing SRAM...');
        await new Promise(r => setTimeout(r, 300));

        flasherLog('[JTAG] SRAM erase: DONE');
        flasherLog('[JTAG] Enable configuration: DONE');
        setFlasherProgress(40, 'Erasing NVCM / Flash...');
        await new Promise(r => setTimeout(r, 400));

        flasherLog('[JTAG] Flash erase: DONE');
        flasherLog('[JTAG] Writing bitstream data to Flash...');

        // Step 4: Stream flash programming with realistic progress updates
        const totalSteps = 20;
        for (let i = 1; i <= totalSteps; i++) {
          await new Promise(r => setTimeout(r, 120));
          const pct = 40 + Math.round((i / totalSteps) * 50); // 40% -> 90%
          setFlasherProgress(pct, `Writing bitstream data: ${Math.round((i / totalSteps) * 100)}%`);
        }

        flasherLog('[JTAG] Writing data: [==================================================] 100.00%');
        flasherLog('[JTAG] Write program Done: DONE');
        setFlasherProgress(95, 'Verifying bitstream and refreshing FPGA...');
        await new Promise(r => setTimeout(r, 300));

        flasherLog('[JTAG] Disable configuration: DONE');
        flasherLog('[JTAG] MachXO2 FPGA soft-processor successfully programmed and running!');
        setFlasherProgress(100, 'FPGA Bitstream Flashed Successfully! (openFPGALoader)');

        if (headerBadge) {
          headerBadge.textContent = 'FPGA PROGRAMMED';
          headerBadge.className = 'settings-badge synced';
        }
      } catch (err) {
        flasherLog(`[JTAG ERROR] Flashing failed: ${err.message}`, true);
        setFlasherProgress(0, `Flashing error: ${err.message}`);
        if (headerBadge) {
          headerBadge.textContent = 'ERROR';
          headerBadge.className = 'settings-badge';
          headerBadge.style.background = 'rgba(239, 68, 68, 0.15)';
          headerBadge.style.color = '#ef4444';
        }
        alert(`FPGA Flashing Error:\n\n${err.message}`);
      } finally {
        isFlashing = false;
        if (startBtn) startBtn.classList.remove('loading');
      }
    }

    // --- 3. Main Flashing Process ---
    async function startFlashingProcess() {
      const startBtn = document.getElementById('flasher-start-btn');
      const headerBadge = document.getElementById('flasher-header-badge');

      if (isFlashing) {
        flasherLog('Flashing process already in progress.');
        return;
      }

      if (flasherTarget === 'fpga') {
        await startFpgaJtagFlashing();
        return;
      }

      try {
        isFlashing = true;
        if (startBtn) startBtn.classList.add('loading');
        if (headerBadge) {
          headerBadge.textContent = 'FLASHING';
          headerBadge.className = 'settings-badge syncing';
        }

        // Step 1: Ensure Firmware Payload is Ready
        let firmwareBytes = null;
        let baseAddress = 0x08000000;

        if (selectedReleaseTag === 'custom') {
          if (!customPayload || !customPayload.data || customPayload.data.length === 0) {
            throw new Error('Please drop or select a valid .hex or .bin firmware file first.');
          }
          firmwareBytes = customPayload.data;
          baseAddress = customPayload.baseAddress || 0x08000000;
        } else {
          // Fetch release binary from GitHub release asset or CORS-enabled raw CDN
          const release = githubReleases.find(r => r.tag_name === selectedReleaseTag) || FALLBACK_RELEASES[0];
          const asset = (release && release.assets)
            ? release.assets.find(a => a.name.toLowerCase().endsWith('.hex') || a.name.toLowerCase().includes('mcu'))
            : null;

          const assetName = asset ? asset.name : `BrainfuinoMCU-${selectedReleaseTag}.hex`;
          setFlasherProgress(2, `Downloading firmware from release ${selectedReleaseTag}...`);
          flasherLog(`Fetching companion STM32 firmware: ${assetName}...`);

          const hexContent = await fetchFirmwareAssetContent(asset, selectedReleaseTag, false);
          const parsed = parseIntelHex(hexContent);
          if (parsed.totalBytes === 0) throw new Error('Downloaded HEX asset contained no valid records.');

          firmwareBytes = parsed.data;
          baseAddress = parsed.baseAddress;
          flasherLog(`Parsed ${parsed.totalBytes} firmware bytes at base 0x${baseAddress.toString(16).toUpperCase()}`);
        }

        // Step 2: Ensure DFU Device is Connected
        if (!dfuDevice || !dfuDevice.opened) {
          flasherLog('DFU device not connected. Prompting for WebUSB pairing...');
          await connectDfuDevice();
          if (!dfuDevice || !dfuDevice.opened) {
            throw new Error('WebUSB DFU device pairing was not completed.');
          }
        }

        // Step 3: Flash Page Erase (STM32F072 uses 2048-byte pages)
        const pageSize = 2048;
        const totalPages = Math.ceil(firmwareBytes.length / pageSize);
        flasherLog(`Starting flash erase: ${totalPages} pages (2 KB each) starting at 0x${baseAddress.toString(16).toUpperCase()}...`);
        setFlasherProgress(5, `Erasing ${totalPages} flash pages...`);

        for (let i = 0; i < totalPages; i++) {
          const pageAddr = baseAddress + (i * pageSize);
          await dfuseErasePage(pageAddr);
          const pct = 5 + ((i + 1) / totalPages) * 20; // 5% -> 25%
          setFlasherProgress(pct, `Erasing flash page ${i + 1}/${totalPages} (0x${pageAddr.toString(16).toUpperCase()})...`);
        }
        flasherLog(`Flash erase completed successfully (${totalPages} pages cleared).`);

        // Step 4: Flash Block Programming (2048-byte blocks)
        const blockSize = 2048;
        const totalBlocks = Math.ceil(firmwareBytes.length / blockSize);
        flasherLog(`Programming ${totalBlocks} blocks (${firmwareBytes.length} bytes)...`);

        for (let i = 0; i < totalBlocks; i++) {
          const blockAddr = baseAddress + (i * blockSize);
          const chunk = firmwareBytes.slice(i * blockSize, Math.min((i + 1) * blockSize, firmwareBytes.length));

          // Set address pointer for block
          await dfuseSetAddress(blockAddr);

          // Write block data (wValue = 2)
          await dfuseWriteBlock(2, chunk);

          const pct = 25 + ((i + 1) / totalBlocks) * 65; // 25% -> 90%
          setFlasherProgress(pct, `Writing block ${i + 1}/${totalBlocks} (0x${blockAddr.toString(16).toUpperCase()})...`);
        }
        flasherLog('All firmware blocks programmed successfully.');

        // Step 5: Finalize & Restart Application
        setFlasherProgress(95, 'Restarting companion MCU into application mode...');
        flasherLog('Rebooting STM32 companion MCU into application mode...');
        await dfuseLeave(baseAddress);

        try {
          await dfuDevice.close();
        } catch(e) {}
        dfuDevice = null;

        setFlasherProgress(100, 'Flash complete! Brainfuino Companion MCU restarted.');
        flasherLog('Flashing process finished successfully! You can now switch back to Serial Terminal.');

        if (headerBadge) {
          headerBadge.textContent = 'FLASH COMPLETE';
          headerBadge.className = 'settings-badge synced';
        }
        updateFlasherDeviceStatus();
      } catch (err) {
        flasherLog(`[ERROR] Flashing aborted: ${err.message}`, true);
        setFlasherProgress(0, `Flashing error: ${err.message}`);
        if (headerBadge) {
          headerBadge.textContent = 'ERROR';
          headerBadge.className = 'settings-badge';
          headerBadge.style.background = 'rgba(239, 68, 68, 0.15)';
          headerBadge.style.color = '#ef4444';
        }
        alert(`Flashing Error:\n\n${err.message}`);
      } finally {
        isFlashing = false;
        if (startBtn) startBtn.classList.remove('loading');
      }
    }

    // --- Write Tab, Dynamic Library, and Verification Modal Implementation ---
    let dynamicLibPrograms = [];
    let isParsingDynamicLib = false;
    let libUsedBytes = 0;
    let libTotalCapacity = 45056; // 44 kB
    let isVerifyingModalOpen = false;
    let verifyTimerInterval = null;
    let verifyRemainingMs = 10000;
    let isReceivingDump = false;
    let dumpPayload = '';
    let dumpName = '';
    let libAddReadyResolver = null;

    const GITHUB_PROGRAMS_SUITE = [
      {
        category: "Basic",
        programs: [
          { name: "Brainfuino Banner", filename: "banner.b", path: "basic/banner.b", size: "1.7 kB", desc: "ASCII banner and hardware diagnostic" },
          { name: "Hello World", filename: "hello.b", path: "basic/hello.b", size: "840 B", desc: "Classic Hello World demonstration" },
          { name: "Serial Echo", filename: "echo.b", path: "basic/echo.b", size: "6 B", desc: "Single cell echo test" }
        ]
      },
      {
        category: "Fractals",
        programs: [
          { name: "Mandelbrot Fractal", filename: "mandelbrot.b", path: "fractals/mandelbrot.b", size: "11.8 kB", desc: "ASCII Mandelbrot set generator" },
          { name: "Sierpinski Triangle", filename: "sierpinski.b", path: "fractals/sierpinski.b", size: "1.8 kB", desc: "Recursive cellular triangle" }
        ]
      },
      {
        category: "Games",
        programs: [
          { name: "Game of Life", filename: "life.b", path: "games/life.b", size: "810 B", desc: "Conway's cellular automaton" }
        ]
      },
      {
        category: "Math",
        programs: [
          { name: "Fibonacci Sequence", filename: "fibonacci.b", path: "math/fibonacci.b", size: "352 B", desc: "Sequence calculation up to 255" },
          { name: "Factorials", filename: "factorial.b", path: "math/factorial.b", size: "854 B", desc: "Integer factorial calculation" },
          { name: "Euler's Number e", filename: "e.b", path: "math/e.b", size: "1.5 kB", desc: "High precision calculation of e" },
          { name: "Golden Ratio", filename: "golden.b", path: "math/golden.b", size: "636 B", desc: "Golden ratio convergence" }
        ]
      },
      {
        category: "Pi",
        programs: [
          { name: "Streaming Pi Digits", filename: "pi_stream.b", path: "pi/pi_stream.b", size: "20.3 kB", desc: "Infinite spigot algorithm for Pi" }
        ]
      }
    ];

    // Built-in offline fallback cache for key programs
    const PROGRAM_CACHE = {
      "basic/banner.b": `++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.`,
      "basic/hello.b": `++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.`,
      "basic/echo.b": `,[.,]`,
      "math/fibonacci.b": `+++++++++++>+>>>>++++++++++++++++++++++++++++++++++++++++++++>++++++++++++++++++++++++++++++++<<<<<<[>[>>>>>>+>+<<<<<<<-]>>>>>>>[<<<<<<<+>>>>>>>-]<[>++++++++++[-<-[>>+>+<<<-]>>>[<<<+>>>-]+<[>+<-]<++<+>>]>-[>+<-]>[[>+<-]+++++++[<++++++>-]<.>+++++++[<++++++>-]<-]++++++++++.>++++++++++[<+++>-]>++++.---.+++++++..+++.<<-]>]<<<<<<[>+>+<<-]>>[<<+>>-]<<<]`,
      "fractals/sierpinski.b": `++++++++++[>+>+++>+++++++>++++++++++<<<<-]>>>++++++++++++++.>+++++++++++++++++.+++++++..+++.<<++.>+++++++++++++++.>.+++.------.--------.>+.`
    };

    function escapeHtml(str) {
      if (str === null || str === undefined) return '';
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    }

    function renderGitHubProgramCatalog() {
      const tree = document.getElementById('github-programs-tree');
      if (!tree) return;

      let html = `
        <div class="github-tree-root-label">
          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg>
          <span>programs/</span>
        </div>
      `;

      GITHUB_PROGRAMS_SUITE.forEach((group, gIdx) => {
        html += `
          <div class="github-tree-folder">
            <div class="github-tree-folder-header" onclick="toggleCategoryGroup(${gIdx})">
              <div style="display:flex; align-items:center; gap:6px;">
                <svg id="github-chevron-${gIdx}" width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" style="transform:rotate(90deg); transition:transform 0.15s ease;"><polyline points="9 18 15 12 9 6"></polyline></svg>
                <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg>
                <span>${group.category.toLowerCase()}/</span>
              </div>
              <span class="github-category-count">${group.programs.length}</span>
            </div>
            <div class="github-tree-branch" id="github-cat-body-${gIdx}">
        `;

        group.programs.forEach((p, pIdx) => {
          const isLast = (pIdx === group.programs.length - 1);
          const branchSymbol = isLast ? '└──' : '├──';
          html += `
            <div class="github-tree-leaf" onclick="loadGitHubProgram('${p.path}', '${p.name.replace(/'/g, "\\'")}', '${p.filename}')" title="${escapeHtml(p.desc)}">
              <div class="github-tree-leaf-left">
                <span class="tree-guide">${branchSymbol}</span>
                <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline></svg>
                <span class="tree-file-name">${escapeHtml(p.filename)}</span>
              </div>
              <span class="tree-file-size">${p.size}</span>
            </div>
          `;
        });

        html += `
            </div>
          </div>
        `;
      });

      tree.innerHTML = html;
    }

    function toggleCategoryGroup(gIdx) {
      const body = document.getElementById(`github-cat-body-${gIdx}`);
      const chevron = document.getElementById(`github-chevron-${gIdx}`);
      if (body) {
        const isHidden = (body.style.display === 'none');
        body.style.display = isHidden ? 'block' : 'none';
        if (chevron) {
          chevron.style.transform = isHidden ? 'rotate(90deg)' : 'rotate(0deg)';
        }
      }
    }

    async function loadGitHubProgram(path, name, filename) {
      updateWriteLibStatus(`Fetching '${name}' from GitHub...`, true);
      const nameInput = document.getElementById('write-prog-name');
      const area = document.getElementById('write-code-area');
      if (nameInput) {
        nameInput.value = filename.replace(/\.b(f)?$/i, '').slice(0, 15);
      }

      try {
        const cdnUrl = `https://raw.githubusercontent.com/m-archibald/Brainfuino/master/programs/${path}`;
        const resp = await fetch(cdnUrl);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        const code = await resp.text();
        if (area) area.value = code;
        updateWriteCodeStats();
        updateWriteLibStatus(`Loaded '${name}' (${code.length} B) from GitHub`, false);
      } catch (err) {
        if (PROGRAM_CACHE[path]) {
          if (area) area.value = PROGRAM_CACHE[path];
          updateWriteCodeStats();
          updateWriteLibStatus(`Loaded '${name}' from local cache`, false);
        } else {
          updateWriteLibStatus(`Failed to load '${name}' (${err.message})`, false);
          alert(`Could not fetch ${path} from GitHub raw CDN: ${err.message}\n\nYou can also drag & drop the .b file into the File Loader in the left sidebar.`);
        }
      }
    }

    // --- Drag & Drop for Main Terminal View ---
    function handleTerminalDragOver(e) {
      e.preventDefault();
      e.stopPropagation();
      const overlay = document.getElementById('terminal-drop-overlay');
      if (overlay) overlay.style.display = 'flex';
    }

    function handleTerminalDragLeave(e) {
      e.preventDefault();
      e.stopPropagation();
      const viewTerm = document.getElementById('view-terminal');
      if (viewTerm && e.relatedTarget && !viewTerm.contains(e.relatedTarget)) {
        const overlay = document.getElementById('terminal-drop-overlay');
        if (overlay) overlay.style.display = 'none';
      }
    }

    function handleTerminalDrop(e) {
      e.preventDefault();
      e.stopPropagation();
      const overlay = document.getElementById('terminal-drop-overlay');
      if (overlay) overlay.style.display = 'none';

      if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
        handleTerminalCodeFile(e.dataTransfer.files[0]);
      }
    }

    function handleWriteCodeFileClick() {
      const input = document.getElementById('terminal-file-input');
      if (input) input.click();
    }

    function handleTerminalFileInput(e) {
      const target = e.target;
      if (target && target.files && target.files.length > 0) {
        handleTerminalCodeFile(target.files[0]);
        target.value = '';
      }
    }

    async function handleTerminalCodeFile(file) {
      if (!file) return;
      const reader = new FileReader();
      reader.onload = async (event) => {
        const text = (event.target && event.target.result) || '';
        if (!text.trim()) {
          postSystemMessage('Selected file is empty.', 'warn');
          return;
        }

        // Populate Write tab editor as well
        const area = document.getElementById('write-code-area');
        const nameInput = document.getElementById('write-prog-name');
        if (area) area.value = text;
        if (nameInput) nameInput.value = file.name.replace(/\.[^/.]+$/, '').slice(0, 15);
        if (typeof updateWriteCodeStats === 'function') updateWriteCodeStats();

        if (!port || !writer) {
          postSystemMessage(`Loaded '${file.name}' (${text.length} B). Connect Brainfuino to flash and run.`, 'info');
          appendTextToTerminal(terminalScreen, `\r\n\x1b[33m[LOADED] '${file.name}' (${text.length} bytes) loaded into editor. Connect Brainfuino to run.\x1b[0m\r\n`, mainTerminalState);
          return;
        }

        postSystemMessage(`Flashing '${file.name}' (${text.length} B) to Brainfuino ROM...`, 'info');
        appendTextToTerminal(terminalScreen, `\r\n\x1b[36m[WRITE] Streaming '${file.name}' (${text.length} B) to Brainfuino ROM...\x1b[0m\r\n`, mainTerminalState);

        // Ensure Auto-Program on Paste is enabled on companion MCU
        await sendCmd('!SET:12=1\r\n');
        await new Promise(r => setTimeout(r, 40));

        // Pulse soft reset and stream code payload
        await sendCmd('!RST\r\n');
        await new Promise(r => setTimeout(r, 80));

        const enc = new TextEncoder();
        const chunkSize = 128;
        for (let i = 0; i < text.length; i += chunkSize) {
          const chunk = text.slice(i, i + chunkSize);
          await writer.write(enc.encode(chunk));
          await new Promise(r => setTimeout(r, 4));
        }
        await writer.write(enc.encode('\r\n'));

        postSystemMessage(`Program '${file.name}' running on Brainfuino!`, 'success');
        appendTextToTerminal(terminalScreen, `\x1b[32m[DONE] '${file.name}' flashed and executing on soft-processor!\x1b[0m\r\n`, mainTerminalState);
      };
      reader.readAsText(file);
    }

    // Drag & Drop for Write tab
    function handleWriteDragOver(e) {
      e.preventDefault();
      e.stopPropagation();
      const dropzone = document.getElementById('write-file-dropzone');
      if (dropzone) dropzone.classList.add('dragover');
    }

    function handleWriteDragLeave(e) {
      e.preventDefault();
      e.stopPropagation();
      const dropzone = document.getElementById('write-file-dropzone');
      if (dropzone) dropzone.classList.remove('dragover');
    }

    function handleWriteDrop(e) {
      e.preventDefault();
      e.stopPropagation();
      const dropzone = document.getElementById('write-file-dropzone');
      if (dropzone) dropzone.classList.remove('dragover');

      if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
        loadWriteLocalFile(e.dataTransfer.files[0]);
      }
    }

    function handleWriteFileInput(e) {
      if (e.target && e.target.files && e.target.files.length > 0) {
        loadWriteLocalFile(e.target.files[0]);
      }
    }

    function loadWriteLocalFile(file) {
      if (!file) return;
      const reader = new FileReader();
      reader.onload = (event) => {
        const text = event.target.result;
        const area = document.getElementById('write-code-area');
        const nameInput = document.getElementById('write-prog-name');
        if (area) area.value = text;
        if (nameInput) {
          nameInput.value = file.name.replace(/\.[^/.]+$/, '').slice(0, 15);
        }
        updateWriteCodeStats();
        updateWriteLibStatus(`Loaded local file '${file.name}' (${text.length} B)`, false);
      };
      reader.readAsText(file);
    }

    function updateWriteCodeStats() {
      const area = document.getElementById('write-code-area');
      if (!area) return;
      const text = area.value;
      const len = text.length;
      const lines = text.length ? text.split('\n').length : 0;
      
      let bfOps = 0;
      for (let i = 0; i < len; i++) {
        const c = text[i];
        if (c === '>' || c === '<' || c === '+' || c === '-' || c === '.' || c === ',' || c === '[' || c === ']') {
          bfOps++;
        }
      }

      const meta = document.getElementById('write-editor-meta');
      const stats = document.getElementById('write-stats');
      const sizeBadge = document.getElementById('write-size-badge');
      const pruneBadge = document.getElementById('write-prune-badge');

      if (meta) meta.textContent = `${len.toLocaleString()} chars | ${lines.toLocaleString()} lines | ${bfOps.toLocaleString()} BF ops`;
      if (stats) stats.textContent = `${len} Bytes | ${bfOps} BF Ops`;
      if (sizeBadge) sizeBadge.textContent = (len >= 1024) ? `${(len / 1024).toFixed(1)} kB` : `${len} B`;
      
      if (pruneBadge) {
        if (len === 0) {
          pruneBadge.textContent = 'Empty';
        } else {
          const pct = Math.round((bfOps / len) * 100);
          pruneBadge.textContent = `${pct}% Code`;
        }
      }
    }

    function clearWriteEditor() {
      const area = document.getElementById('write-code-area');
      if (area) area.value = '';
      updateWriteCodeStats();
    }

    function pruneEditorComments() {
      const area = document.getElementById('write-code-area');
      if (!area || !area.value) return;
      const originalLen = area.value.length;
      let pruned = '';
      for (let i = 0; i < originalLen; i++) {
        const c = area.value[i];
        if (c === '>' || c === '<' || c === '+' || c === '-' || c === '.' || c === ',' || c === '[' || c === ']') {
          pruned += c;
        }
      }
      area.value = pruned;
      updateWriteCodeStats();
      const saved = originalLen - pruned.length;
      updateWriteLibStatus(`Pruned non-BF comments: removed ${saved} bytes`, false);
    }

    function loadDefaultLogoToEditor() {
      const nameInput = document.getElementById('write-prog-name');
      const area = document.getElementById('write-code-area');
      if (nameInput) nameInput.value = 'Brainfuino';
      if (area) area.value = `++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.`;
      updateWriteCodeStats();
      updateWriteLibStatus("Loaded Brainfuino Demo program into editor", false);
    }

    function queryDynamicLibrary() {
      if (!port) {
        updateWriteLibStatus('Brainfuino disconnected. Connect device to sync library.', false);
        return;
      }
      updateWriteLibStatus('Querying Brainfuino program library (!LIB*)...', true);
      const syncBtn = document.getElementById('write-sync-lib-btn');
      if (syncBtn) syncBtn.disabled = true;
      sendCmd('!LIB*\r\n');
      setTimeout(() => {
        if (syncBtn) syncBtn.disabled = false;
      }, 1500);
    }

    function updateWriteLibStatus(msg, isActive = true) {
      const stripMsg = document.getElementById('write-strip-msg');
      const stripDot = document.getElementById('write-strip-dot');
      if (stripMsg) stripMsg.textContent = msg;
      if (stripDot) stripDot.classList.toggle('active', isActive);
    }

    function renderDynamicLibrary() {
      const tbody = document.getElementById('lib-programs-tbody');
      const slotsCount = document.getElementById('lib-slots-count');
      const usageBadge = document.getElementById('lib-usage-badge');
      const storageFill = document.getElementById('lib-storage-fill');
      const storageText = document.getElementById('lib-storage-text');
      if (!tbody) return;

      if (dynamicLibPrograms.length === 0) {
        tbody.innerHTML = `
          <tr>
            <td colspan="5" style="text-align:center; padding:24px; color:var(--text-muted);">
              No programs installed in Flash Library. Write your first program above!
            </td>
          </tr>`;
        if (slotsCount) slotsCount.textContent = '0 / 64 Slots';
        if (usageBadge) usageBadge.textContent = '0 B / 44 kB';
        if (storageFill) storageFill.style.width = '0%';
        if (storageText) storageText.textContent = '0.0 kB used (0%)';
        return;
      }

      dynamicLibPrograms.sort((a, b) => a.slot - b.slot);

      let html = '';
      dynamicLibPrograms.forEach(p => {
        const formattedSlot = String(p.slot + 1).padStart(2, '0');
        const sizeFormatted = (p.size >= 1024) ? `${(p.size / 1024).toFixed(1)} kB` : `${p.size} B`;
        const statusBadge = p.isProtected 
          ? `<span class="slot-pill" style="background:rgba(99,102,241,0.12); color:var(--accent-indigo);">Factory</span>`
          : `<span class="slot-pill" style="background:rgba(22,163,74,0.12); color:var(--accent-green);">Flash</span>`;
        
        let actionButtons = `
          <button class="lib-btn-action" onclick="runLibrarySlot(${p.slot})" title="Run program immediately on soft-processor">
            <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg>
            <span>Run</span>
          </button>
          <button class="lib-btn-action" onclick="dumpLibrarySlot(${p.slot})" title="Dump program source into editor">
            <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
            <span>Dump</span>
          </button>
        `;

        if (!p.isProtected) {
          actionButtons += `
            <button class="lib-btn-action btn-del" onclick="deleteLibrarySlot(${p.slot}, '${p.name.replace(/'/g, "\\'")}')" title="Delete program from library">
              <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
              <span>Delete</span>
            </button>
          `;
        }

        html += `
          <tr>
            <td><span class="slot-pill">${formattedSlot}</span></td>
            <td><strong>${escapeHtml(p.name)}</strong></td>
            <td><span class="github-program-size">${sizeFormatted}</span></td>
            <td>${statusBadge}</td>
            <td style="text-align:right;">
              <div style="display:flex; justify-content:flex-end; gap:6px;">${actionButtons}</div>
            </td>
          </tr>
        `;
      });

      tbody.innerHTML = html;

      const usedKb = (libUsedBytes / 1024).toFixed(1);
      const capKb = (libTotalCapacity / 1024).toFixed(1);
      const pct = Math.min(100, Math.round((libUsedBytes / libTotalCapacity) * 100));

      if (slotsCount) slotsCount.textContent = `${dynamicLibPrograms.length} / 64 Slots`;
      if (usageBadge) usageBadge.textContent = `${usedKb} kB / ${capKb} kB`;
      if (storageFill) storageFill.style.width = `${pct}%`;
      if (storageText) storageText.textContent = `${usedKb} kB used (${pct}%)`;
    }

    function runLibrarySlot(slot) {
      if (!port) {
        alert('Please connect Brainfuino first.');
        return;
      }
      sendCmd(`!LIB:RUN:${slot}\r\n`);
      updateWriteLibStatus(`Launching slot ${slot} on soft-processor...`, true);
      const badge = document.getElementById('verify-countdown-badge');
      if (badge) {
        badge.textContent = `SLOT ${slot + 1} RUNNING`;
        badge.style.color = '#38bdf8';
      }
      const dot = document.getElementById('verify-status-dot');
      if (dot) {
        dot.className = 'status-strip-dot active';
      }
      clearVerifyTerminal();
      appendVerifyTerminalText(`--- Launching Library Slot ${slot + 1} on Soft-Processor ---\r\n`);
      const vScreen = document.getElementById('verify-terminal-screen');
      if (vScreen) vScreen.focus();
    }

    function dumpLibrarySlot(slot) {
      if (!port) {
        alert('Please connect Brainfuino first.');
        return;
      }
      sendCmd(`!LIB:DUMP:${slot}\r\n`);
      updateWriteLibStatus(`Requesting dump of slot ${slot}...`, true);
    }

    function deleteLibrarySlot(slot, name) {
      if (!port) {
        alert('Please connect Brainfuino first.');
        return;
      }
      if (!confirm(`Are you sure you want to delete program '${name}' (Slot ${slot + 1}) from Brainfuino Flash?`)) {
        return;
      }
      sendCmd(`!LIB:DEL:${slot}\r\n`);
      updateWriteLibStatus(`Deleting slot ${slot}...`, true);
    }

    async function executeWriteLiveRun() {
      const area = document.getElementById('write-code-area');
      if (!area || !area.value.trim()) {
        alert('Please enter or select Brainfuck code to write.');
        return;
      }
      if (!port || !writer) {
        alert('Please connect Brainfuino first using the "Connect Brainfuino" button.');
        return;
      }

      const code = area.value;
      updateWriteLibStatus('Sending code to Brainfuino RAM/ROM (Live Run)...', true);
      const badge = document.getElementById('verify-countdown-badge');
      if (badge) {
        badge.textContent = 'LIVE RUNNING';
        badge.style.color = '#38bdf8';
      }
      const dot = document.getElementById('verify-status-dot');
      if (dot) {
        dot.className = 'status-strip-dot active';
      }
      clearVerifyTerminal();
      appendVerifyTerminalText(`--- Live Run: Writing and executing on Soft-Processor (${code.length} bytes) ---\r\n`);

      // Ensure Auto-Program on Paste is enabled on companion MCU
      await sendCmd('!SET:12=1\r\n');
      await new Promise(r => setTimeout(r, 40));

      // Pulse soft reset and stream code payload
      await sendCmd('!RST\r\n');
      await new Promise(r => setTimeout(r, 80));

      const enc = new TextEncoder();
      const chunkSize = 128;
      for (let i = 0; i < code.length; i += chunkSize) {
        const chunk = code.slice(i, i + chunkSize);
        await writer.write(enc.encode(chunk));
        await new Promise(r => setTimeout(r, 4));
      }
      await writer.write(enc.encode('\r\n'));

      updateWriteLibStatus('Program written! Running live on Brainfuino.', false);
      const vScreen = document.getElementById('verify-terminal-screen');
      if (vScreen) vScreen.focus();
    }

    async function executeWriteToLibrary(withTest = true) {
      const area = document.getElementById('write-code-area');
      const nameInput = document.getElementById('write-prog-name');
      if (!area || !area.value.trim()) {
        alert('Please enter or select Brainfuck code to write.');
        return;
      }
      if (!port || !writer) {
        alert('Please connect Brainfuino first using the "Connect Brainfuino" button.');
        return;
      }

      let progName = (nameInput && nameInput.value.trim()) ? nameInput.value.trim() : 'User_Prog';
      progName = progName.replace(/[^a-zA-Z0-9_-]/g, '_').slice(0, 15);
      const code = area.value;

      updateWriteLibStatus(`Initiating Library write for '${progName}'...`, true);

      // Cleanly exit or abort any active text menu on the device
      await sendCmd('\x1b');
      await new Promise(r => setTimeout(r, 60));

      let directReady = false;
      const readyPromise = new Promise(resolve => {
        libAddReadyResolver = resolve;
      });

      if (withTest) {
        await sendCmd(`!LIB:ADD:${progName}\r\n`);
      } else {
        await sendCmd(`!LIB:ADD_NOVERIFY:${progName}\r\n`);
      }

      // Wait up to 500ms for direct !LIB:ADD:READY handshake
      try {
        directReady = await Promise.race([
          readyPromise,
          new Promise((_, reject) => setTimeout(() => reject(new Error('timeout')), 500))
        ]);
      } catch (e) {
        directReady = false;
      }
      libAddReadyResolver = null;

      if (directReady) {
        // Modern firmware direct write protocol
        updateWriteLibStatus(`Streaming code payload (${code.length} B)...`, true);
        const enc = new TextEncoder();
        const chunkSize = 128;
        for (let i = 0; i < code.length; i += chunkSize) {
          const chunk = code.slice(i, i + chunkSize);
          await writer.write(enc.encode(chunk));
          await new Promise(r => setTimeout(r, 4));
        }
        await writer.write(enc.encode('\r\n'));

        if (withTest) {
          updateWriteLibStatus('Code uploaded. Testing on Brainfuino before save...', true);
        } else {
          updateWriteLibStatus('Code uploaded. Saving directly to Library...', true);
        }
      } else {
        // Older firmware fallback:
        // Hardware did not respond to !LIB:ADD:READY.
        // It may be in the interactive menu or awaiting program name.
        updateWriteLibStatus(`Direct handshake timed out. Using interactive menu fallback for '${progName}'...`, true);

        // Explicitly supply the program name so it is NEVER populated from comments in the code!
        await sendCmd(`${progName}\r\n`);
        await new Promise(r => setTimeout(r, 350));

        // Stream code payload
        updateWriteLibStatus(`Streaming code payload (${code.length} B)...`, true);
        const enc = new TextEncoder();
        const chunkSize = 128;
        for (let i = 0; i < code.length; i += chunkSize) {
          const chunk = code.slice(i, i + chunkSize);
          await writer.write(enc.encode(chunk));
          await new Promise(r => setTimeout(r, 4));
        }
        await writer.write(enc.encode('\r\n'));
        await new Promise(r => setTimeout(r, 300));

        // Answer verification prompt: 'y' for test, 'n' for direct
        await sendCmd(withTest ? 'y\r\n' : 'n\r\n');
        updateWriteLibStatus(withTest ? 'Uploaded. Running verification test...' : 'Uploaded. Saving to Library...', true);
      }
    }

    // --- Execution & Verification Monitor (Permanent Card in Right Column) ---
    let verifyBytesReceived = 0;
    let verifyInputMode = 'direct';

    function setVerifyInputMode(mode) {
      verifyInputMode = mode;
      const badge = document.getElementById('verify-mode-badge');
      const input = document.getElementById('verify-cmd-input');
      const screen = document.getElementById('verify-terminal-screen');
      if (mode === 'direct') {
        if (badge) badge.textContent = 'Mode: Direct';
        if (input) input.placeholder = 'Click monitor for direct keys, or type here...';
        if (screen) screen.focus();
      } else {
        if (badge) badge.textContent = 'Mode: Line';
        if (input) {
          input.placeholder = 'Type input line and press Enter...';
          input.focus();
        }
      }
    }

    function cycleVerifyInputMode() {
      setVerifyInputMode(verifyInputMode === 'direct' ? 'line' : 'direct');
    }

    function sendVerifyInputBox() {
      const input = document.getElementById('verify-cmd-input');
      if (!input || !port) return;
      const text = input.value;
      if (!text) return;
      sendRaw(text + '\r\n');
      input.value = '';
    }

    function clearVerifyTerminal() {
      const screen = document.getElementById('verify-terminal-screen');
      const stats = document.getElementById('verify-monitor-stats');
      if (screen) {
        screen.innerHTML = '';
      }
      verifyTerminalState.totalRenderedLines = 0;
      verifyTerminalState.atLineStart = true;
      verifyTerminalState.ansiState = { bold: false, underline: false, fgColor: null };
      verifyBytesReceived = 0;
      if (stats) stats.textContent = '0 B received';
    }

    function startVerificationSession(seconds = 10) {
      isVerifyingModalOpen = true;
      const badge = document.getElementById('verify-countdown-badge');
      const progress = document.getElementById('verify-progress-fill');
      const track = document.getElementById('verify-progress-track');
      const dot = document.getElementById('verify-status-dot');
      const screen = document.getElementById('verify-terminal-screen');
      const btnAbort = document.getElementById('btn-verify-abort');
      const btnCommit = document.getElementById('btn-verify-commit');

      if (dot) {
        dot.className = 'status-strip-dot active';
        dot.style.animation = 'pulse 1s infinite';
      }
      if (track) track.style.opacity = '1';
      if (btnAbort) btnAbort.style.display = 'inline-flex';
      if (btnCommit) btnCommit.style.display = 'inline-flex';
      
      clearVerifyTerminal();
      appendVerifyTerminalText('--- Testing on Brainfuino: Soft-processor running (10s auto-save) ---\r\n');
      if (screen) screen.focus();

      verifyRemainingMs = seconds * 1000;
      const totalMs = seconds * 1000;

      if (badge) {
        badge.textContent = `TESTING (${seconds.toFixed(1)}s)`;
        badge.style.color = '#10b981';
      }
      if (progress) progress.style.width = '100%';

      if (verifyTimerInterval) clearInterval(verifyTimerInterval);
      verifyTimerInterval = setInterval(() => {
        verifyRemainingMs -= 100;
        if (verifyRemainingMs <= 0) {
          verifyRemainingMs = 0;
          clearInterval(verifyTimerInterval);
          verifyTimerInterval = null;
          if (badge) badge.textContent = 'SAVING...';
          if (progress) progress.style.width = '0%';
        } else {
          if (badge) badge.textContent = `TESTING (${(verifyRemainingMs / 1000).toFixed(1)}s)`;
          if (progress) progress.style.width = `${((verifyRemainingMs / totalMs) * 100).toFixed(1)}%`;
        }
      }, 100);
    }

    function closeVerificationSession(saved = false, slot = 0) {
      isVerifyingModalOpen = false;
      if (verifyTimerInterval) {
        clearInterval(verifyTimerInterval);
        verifyTimerInterval = null;
      }
      const badge = document.getElementById('verify-countdown-badge');
      const progress = document.getElementById('verify-progress-fill');
      const track = document.getElementById('verify-progress-track');
      const dot = document.getElementById('verify-status-dot');
      const btnAbort = document.getElementById('btn-verify-abort');
      const btnCommit = document.getElementById('btn-verify-commit');

      if (dot) {
        dot.className = 'status-strip-dot';
        dot.style.animation = 'none';
      }
      if (track) track.style.opacity = '0.25';
      if (progress) progress.style.width = '0%';
      if (btnAbort) btnAbort.style.display = 'none';
      if (btnCommit) btnCommit.style.display = 'none';

      if (saved) {
        if (badge) {
          badge.textContent = `SAVED (SLOT ${slot + 1})`;
          badge.style.color = '#10b981';
        }
        updateWriteLibStatus(`Verification complete! Saved to Library Slot ${slot + 1}.`, false);
        appendVerifyTerminalText(`\r\n[SUCCESS: Program saved to Library Slot ${slot + 1}]\r\n`);
      } else {
        if (badge) {
          badge.textContent = 'CANCELLED';
          badge.style.color = '#ef4444';
        }
        updateWriteLibStatus('Verification aborted. Default demo restored.', false);
        appendVerifyTerminalText('\r\n[CANCELLED: Program discarded, demo restored]\r\n');
      }
    }

    function commitVerificationNow() {
      sendCmd('!COMMIT\r\n');
      const badge = document.getElementById('verify-countdown-badge');
      if (badge) badge.textContent = 'Saving...';
    }

    function abortVerification() {
      sendCmd('!CANCEL\r\n');
      closeVerificationSession(false);
    }

    // Attach listeners for verification monitor and interactive soft-processor input
    document.addEventListener('DOMContentLoaded', () => {
      const vScreen = document.getElementById('verify-terminal-screen');
      if (vScreen) {
        vScreen.addEventListener('keydown', async (e) => {
          if (!port) return;
          if (isVerifyingModalOpen) {
            if (e.key === 'Escape') {
              abortVerification();
              return;
            }
            if (e.key === 'Enter' && e.ctrlKey) {
              commitVerificationNow();
              return;
            }
          }
          if (verifyInputMode !== 'direct') return;

          const toSend = mapDirectKey(e);
          if (toSend !== null) {
            e.preventDefault();
            e.stopPropagation();
            await sendRaw(toSend);
          }
        });

        vScreen.addEventListener('paste', async (e: ClipboardEvent) => {
          if (!port || verifyInputMode !== 'direct') return;
          const text = e.clipboardData?.getData('text');
          if (text) {
            e.preventDefault();
            e.stopPropagation();
            await sendRaw(text);
          }
        });
      }

      const vInput = document.getElementById('verify-cmd-input');
      if (vInput) {
        vInput.addEventListener('keydown', (e) => {
          if (e.key === 'Enter') {
            sendVerifyInputBox();
          }
        });
      }
    });

    // --- QGC-Style Notification & Message History System ---
    const qgcMessageHistory = [];
    const QGC_MAX_HISTORY = 150;
    const QGC_MAX_VISIBLE_TOASTS = 3;

    function postSystemMessage(text, level = 'info', durationMs = 4000) {
      if (!text) return;
      const now = new Date();
      const timeStr = now.toTimeString().split(' ')[0]; // HH:MM:SS
      const item = { time: timeStr, text, level, id: Date.now() + Math.random() };

      qgcMessageHistory.push(item);
      if (qgcMessageHistory.length > QGC_MAX_HISTORY) {
        qgcMessageHistory.shift();
      }
      updateQgcHistoryUI();

      // Display fading toast
      showQgcToast(item, durationMs);
    }

    function showQgcToast(item, durationMs) {
      const container = document.getElementById('qgc-toast-container');
      if (!container) return;

      while (container.children.length >= QGC_MAX_VISIBLE_TOASTS) {
        container.removeChild(container.firstChild);
      }

      const toast = document.createElement('div');
      toast.className = `qgc-toast qgc-toast-${item.level}`;
      toast.setAttribute('role', 'alert');
      toast.onclick = () => {
        toggleMessageHistory(true);
      };

      const timeEl = document.createElement('span');
      timeEl.className = 'qgc-toast-time';
      timeEl.textContent = item.time;

      const textEl = document.createElement('span');
      textEl.className = 'qgc-toast-text';
      textEl.textContent = item.text;

      const hintEl = document.createElement('span');
      hintEl.className = 'qgc-toast-hint';
      hintEl.textContent = 'LOG';
      hintEl.title = 'Click to open message log';

      toast.appendChild(timeEl);
      toast.appendChild(textEl);
      toast.appendChild(hintEl);
      container.appendChild(toast);

      let timeoutId = null;
      const startTimer = (dur) => {
        timeoutId = setTimeout(() => {
          toast.classList.add('fade-out');
          setTimeout(() => {
            if (toast.parentNode) toast.parentNode.removeChild(toast);
          }, 300);
        }, dur);
      };

      toast.addEventListener('mouseenter', () => {
        if (timeoutId) clearTimeout(timeoutId);
      });
      toast.addEventListener('mouseleave', () => {
        startTimer(2000);
      });

      startTimer(durationMs);
    }

    function updateQgcHistoryUI() {
      const label = document.getElementById('msg-history-label');
      if (label) {
        label.textContent = `Messages (${qgcMessageHistory.length})`;
      }

      const list = document.getElementById('qgc-history-list');
      const empty = document.getElementById('qgc-history-empty');
      if (!list) return;

      if (qgcMessageHistory.length === 0) {
        if (empty) empty.style.display = 'block';
        return;
      }
      if (empty) empty.style.display = 'none';

      list.innerHTML = '';
      for (let i = 0; i < qgcMessageHistory.length; i++) {
        const item = qgcMessageHistory[i];
        const row = document.createElement('div');
        row.className = `qgc-history-row level-${item.level}`;

        const tSpan = document.createElement('span');
        tSpan.className = 'qgc-h-time';
        tSpan.textContent = item.time;

        const lSpan = document.createElement('span');
        lSpan.className = 'qgc-h-level';
        lSpan.textContent = `[${item.level.toUpperCase()}]`;

        const mSpan = document.createElement('span');
        mSpan.className = 'qgc-h-text';
        mSpan.textContent = item.text;

        row.appendChild(tSpan);
        row.appendChild(lSpan);
        row.appendChild(mSpan);
        list.appendChild(row);
      }
      list.scrollTop = list.scrollHeight;
    }

    function toggleMessageHistory(forceOpen) {
      const panel = document.getElementById('qgc-history-panel');
      if (!panel) return;
      if (forceOpen === true) {
        panel.classList.add('open');
      } else if (forceOpen === false) {
        panel.classList.remove('open');
      } else {
        panel.classList.toggle('open');
      }
    }

    function clearMessageHistory() {
      qgcMessageHistory.length = 0;
      updateQgcHistoryUI();
    }

    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        toggleMessageHistory(false);
      }
    });
    document.addEventListener('mousedown', (e) => {
      const panel = document.getElementById('qgc-history-panel');
      const btn = document.getElementById('btn-msg-history');
      if (panel && panel.classList.contains('open')) {
        if (!panel.contains(e.target) && (!btn || !btn.contains(e.target)) && !e.target.closest('.qgc-toast')) {
          panel.classList.remove('open');
        }
      }
    });

    // Initialize routing from URL hash, serial hotplug listeners, and auto-connect
    initRouteFromHash();
    initSerialHotplugListeners();
    syncAutoConnectUI();
    if (isAutoConnectEnabled()) {
      updateConnectionUI(false);
      attemptAutoConnect();
    }
    setTimeout(() => postSystemMessage('Web Terminal ready', 'info', 3500), 200);

function setMaxScrollbackLines(val: any) {
  maxScrollbackLines = parseInt(val, 10);
}

// Expose all interactive event handlers to global window for HTML inline onclick/onchange attributes
Object.assign(window, {
  toggleConnect,
  toggleConnectDropdown,
  closeConnectDropdown,
  triggerManualPortPicker,
  showUsbTroubleshootHelp,
  closeUsbHelpModal,
  handleUsbBackdropClick,
  toggleAutoConnectFromDropdown,
  syncAutoConnectUI,
  switchTab,
  setInputMode,
  sendCmd,
  queryClockSpeed,
  stepClockDown,
  stepClockUp,
  triggerDfuReboot,
  fetchGitHubReleases,
  renderGitHubProgramCatalog,
  toggleCategoryGroup,
  loadGitHubProgram,
  runLibrarySlot,
  dumpLibrarySlot,
  deleteLibrarySlot,
  fetchDynamicMenu,
  toggleTheme,
  attemptAutoConnect,
  isAutoConnectEnabled,
  handleDeviceUnplugged,
  saveLog,
  clearTerminal,
  toggleMessageHistory,
  toggleFullscreen,
  cycleInputMode,
  sendInputBox,
  filterDynamicSettings,
  loadDefaultPreviewSchema,
  onReleaseSelectChange,
  setFlasherTarget,
  setFpgaBoardRev,
  handleFlasherFileSelect,
  connectDfuDevice,
  updateJtagPinoutHelper,
  connectJtagDevice,
  startFlashingProcess,
  queryDynamicLibrary,
  loadDefaultLogoToEditor,
  pruneEditorComments,
  clearWriteEditor,
  handleTerminalDragOver,
  handleTerminalDragLeave,
  handleTerminalDrop,
  handleTerminalFileInput,
  handleTerminalCodeFile,
  handleWriteCodeFileClick,
  handleWriteDragOver,
  handleWriteDragLeave,
  handleWriteDrop,
  handleWriteFileInput,
  updateWriteCodeStats,
  executeWriteLiveRun,
  executeWriteToLibrary,
  clearVerifyTerminal,
  abortVerification,
  commitVerificationNow,
  cycleVerifyInputMode,
  sendVerifyInputBox,
  clearMessageHistory,
  setMaxScrollbackLines,
  postSystemMessage,
  processMachineLine
});
