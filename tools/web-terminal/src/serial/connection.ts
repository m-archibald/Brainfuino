import { postSystemMessage } from '../ui/toasts';

export interface SerialCallbacks {
  onData?: (chunk: string, time: number) => void;
  onConnectionChange?: (connected: boolean) => void;
  onSettingsQuery?: () => void;
}

let port: any = null;
let reader: any = null;
let writer: any = null;
let readableStreamClosed: Promise<void> | null = null;
let keepReading = false;
let callbacks: SerialCallbacks = {};

export function setSerialCallbacks(cb: SerialCallbacks): void {
  callbacks = { ...callbacks, ...cb };
}

export function isConnected(): boolean {
  return port !== null;
}

export function getPort(): any {
  return port;
}

export function getWriter(): any {
  return writer;
}

export async function openExistingPort(selectedPort: any): Promise<void> {
  if (!selectedPort) return;
  port = selectedPort;
  try {
    await port.open({ baudRate: 115200 });
    writer = port.writable.getWriter();
    keepReading = true;

    try {
      const autoconnectChk = document.getElementById('autoconnect-chk') as HTMLInputElement;
      if (autoconnectChk && autoconnectChk.checked) {
        localStorage.setItem('bf_auto_reconnect', '1');
      }
    } catch (e) {}

    if (callbacks.onConnectionChange) callbacks.onConnectionChange(true);
    postSystemMessage('Connected to Brainfuino (115200 baud)', 'success');
    readSerialLoop();

    if (callbacks.onSettingsQuery) {
      callbacks.onSettingsQuery();
    }
  } catch (err: any) {
    console.warn('Failed to open serial port:', err);
    port = null;
    if (callbacks.onConnectionChange) callbacks.onConnectionChange(false);
    postSystemMessage(`Failed to open serial port: ${err.message || err}`, 'error', 5000);
  }
}

export async function connectPort(): Promise<void> {
  if (!('serial' in navigator)) {
    alert('Web Serial is not supported on this browser. Please use Chrome, Edge, or Brave with Web Serial enabled.');
    return;
  }

  try {
    const selectedPort = await (navigator as any).serial.requestPort({});
    await openExistingPort(selectedPort);
  } catch (err: any) {
    if (err.name !== 'NotFoundError') {
      console.error('Serial requestPort error:', err);
      alert(`Port Picker Notice: ${err.message || err.name}\n\nNote: If using Brave, please allow serial ports in brave://settings/content/serialPorts.`);
    }
  }
}

export async function disconnectPort(): Promise<void> {
  try {
    const autoconnectChk = document.getElementById('autoconnect-chk') as HTMLInputElement;
    if (!autoconnectChk || !autoconnectChk.checked) {
      localStorage.removeItem('bf_auto_reconnect');
    }
  } catch (e) {}

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

  if (callbacks.onConnectionChange) callbacks.onConnectionChange(false);
  postSystemMessage('Disconnected from Brainfuino', 'info');
}

export async function toggleConnect(): Promise<void> {
  if (port) {
    await disconnectPort();
  } else {
    await connectPort();
  }
}

export async function sendRaw(strOrBytes: string | Uint8Array): Promise<void> {
  if (!writer) return;
  try {
    let data: Uint8Array;
    if (typeof strOrBytes === 'string') {
      const encoder = new TextEncoder();
      data = encoder.encode(strOrBytes);
    } else {
      data = strOrBytes;
    }
    await writer.write(data);
  } catch (err) {
    console.error('Error writing to serial port:', err);
  }
}

export async function sendCmd(cmdStr: string): Promise<void> {
  await sendRaw(cmdStr);
}

export async function autoReconnectSerial(): Promise<void> {
  if (!('serial' in navigator)) return;
  try {
    if (localStorage.getItem('bf_auto_reconnect') !== '1') return;
    const ports = await (navigator as any).serial.getPorts();
    if (ports && ports.length > 0) {
      await openExistingPort(ports[0]);
    }
  } catch (err) {
    console.warn('Auto-reconnect notice:', err);
  }
}

export function initGlobalDisconnectListener(): void {
  if ('serial' in navigator) {
    (navigator as any).serial.addEventListener('disconnect', (event: any) => {
      if (port && event.target === port) {
        disconnectPort();
      }
    });
  }
}

async function readSerialLoop(): Promise<void> {
  if (!port || !port.readable) return;
  const textDecoder = new (window as any).TextDecoderStream();
  readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
  reader = textDecoder.readable.getReader();

  try {
    while (keepReading) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value && callbacks.onData) {
        const recvTime = performance.timeOrigin ? (performance.timeOrigin + performance.now()) : Date.now();
        callbacks.onData(value, recvTime);
      }
    }
  } catch (err: any) {
    if (keepReading) {
      postSystemMessage(`Serial connection lost: ${err.message}`, 'error', 6000);
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

