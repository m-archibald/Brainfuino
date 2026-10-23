import { describe, it, expect, beforeEach } from 'vitest';
import {
  postSystemMessage,
  qgcMessageHistory,
  clearMessageHistory,
  updateQgcHistoryUI
} from '../src/ui/toasts';

describe('QGC Toast & Message History System', () => {
  beforeEach(() => {
    document.body.innerHTML = `
      <div id="qgc-toast-container"></div>
      <span id="msg-history-label"></span>
      <div id="qgc-history-panel">
        <div id="qgc-history-list"></div>
        <div id="qgc-history-empty"></div>
      </div>
    `;
    clearMessageHistory();
  });

  it('records messages with timestamp and level', () => {
    postSystemMessage('System ready', 'info');
    expect(qgcMessageHistory.length).toBe(1);
    expect(qgcMessageHistory[0].text).toBe('System ready');
    expect(qgcMessageHistory[0].level).toBe('info');

    postSystemMessage('Connection failed', 'error');
    expect(qgcMessageHistory.length).toBe(2);
    expect(qgcMessageHistory[1].level).toBe('error');
  });

  it('creates DOM toasts with proper severity classes', () => {
    postSystemMessage('Success message', 'success');
    const container = document.getElementById('qgc-toast-container')!;
    const toast = container.querySelector('.qgc-toast-success');
    expect(toast).not.toBeNull();
    expect(toast?.textContent).toContain('Success message');
  });

  it('clears message history', () => {
    postSystemMessage('Msg 1', 'info');
    postSystemMessage('Msg 2', 'warn');
    expect(qgcMessageHistory.length).toBe(2);

    clearMessageHistory();
    expect(qgcMessageHistory.length).toBe(0);
    const label = document.getElementById('msg-history-label');
    expect(label?.textContent).toBe('Messages (0)');
  });
});
