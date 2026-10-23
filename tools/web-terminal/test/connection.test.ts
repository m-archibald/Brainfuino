import { describe, it, expect, beforeEach, vi } from 'vitest';
import { isConnected, disconnectPort, setSerialCallbacks } from '../src/serial/connection';
import { qgcMessageHistory, clearMessageHistory } from '../src/ui/toasts';

describe('Web Serial Connection Lifecycle', () => {
  beforeEach(() => {
    clearMessageHistory();
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
});

