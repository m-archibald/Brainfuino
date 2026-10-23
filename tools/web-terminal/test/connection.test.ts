import { describe, it, expect, beforeEach, vi } from 'vitest';
import {
  isConnected,
  disconnectPort,
  setSerialCallbacks,
  attemptAutoConnect,
  isAutoConnectEnabled,
  handleDeviceUnplugged,
  setManualDisconnect,
  initSerialHotplugListeners
} from '../src/serial/connection';
import { qgcMessageHistory, clearMessageHistory } from '../src/ui/toasts';

describe('Web Serial Connection Lifecycle & Betaflight-style Auto-Connect', () => {
  beforeEach(() => {
    clearMessageHistory();
    localStorage.clear();
    setManualDisconnect(false);
  });

  it('starts disconnected', () => {
    expect(isConnected()).toBe(false);
  });

  it('safely handles disconnect when not connected', async () => {
    let connectionReported: boolean | null = null;
    setSerialCallbacks({
      onConnectionChange: (connected) => {
        connectionReported = connected;
      }
    });

    await disconnectPort();
    expect(isConnected()).toBe(false);
    expect(connectionReported).toBe(false);
    expect(qgcMessageHistory.length).toBe(1);
    expect(qgcMessageHistory[0].text).toContain('Disconnected');
  });

  it('evaluates isAutoConnectEnabled based on storage preference', () => {
    // Default is true when never set
    expect(isAutoConnectEnabled()).toBe(true);

    localStorage.setItem('bf_auto_reconnect', '0');
    expect(isAutoConnectEnabled()).toBe(false);

    localStorage.setItem('bf_auto_reconnect', '1');
    expect(isAutoConnectEnabled()).toBe(true);
  });

  it('automatically finds and connects to STM32 VID/PID 0x0483/0x5740', async () => {
    const mockOpened = vi.fn().mockResolvedValue(undefined);
    const mockSTM32Port = {
      getInfo: () => ({ usbVendorId: 0x0483, usbProductId: 0x5740 }),
      open: mockOpened,
      close: vi.fn().mockResolvedValue(undefined),
      readable: { pipeTo: vi.fn() },
      writable: { getWriter: () => ({ releaseLock: vi.fn(), write: vi.fn() }) }
    };

    const mockGenericPort = {
      getInfo: () => ({ usbVendorId: 0x1234, usbProductId: 0x5678 }),
      open: vi.fn(),
    };

    (navigator as any).serial = {
      getPorts: vi.fn().mockResolvedValue([mockGenericPort, mockSTM32Port]),
      addEventListener: vi.fn()
    };

    let connectionState = false;
    setSerialCallbacks({
      onConnectionChange: (connected) => {
        connectionState = connected;
      }
    });

    await attemptAutoConnect();

    expect(mockOpened).toHaveBeenCalled();
    expect(isConnected()).toBe(true);
    expect(connectionState).toBe(true);
    expect(qgcMessageHistory.some(m => m.text.includes('Connected to Brainfuino'))).toBe(true);

    // Clean up
    await disconnectPort();
  });

  it('cleans up connection cleanly on device unplug', async () => {
    let connectionState = true;
    setSerialCallbacks({
      onConnectionChange: (connected) => {
        connectionState = connected;
      }
    });

    await handleDeviceUnplugged();
    expect(isConnected()).toBe(false);
    expect(connectionState).toBe(false);
  });

  it('registers serial hotplug listeners without throwing', () => {
    const addListenerMock = vi.fn();
    (navigator as any).serial = {
      addEventListener: addListenerMock
    };

    expect(() => initSerialHotplugListeners()).not.toThrow();
    expect(addListenerMock).toHaveBeenCalledWith('connect', expect.any(Function));
    expect(addListenerMock).toHaveBeenCalledWith('disconnect', expect.any(Function));
  });
});
