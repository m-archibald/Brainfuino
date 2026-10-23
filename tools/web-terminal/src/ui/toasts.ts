export type ToastLevel = 'info' | 'success' | 'warn' | 'error';

export interface SystemMessage {
  id: number;
  time: string;
  text: string;
  level: ToastLevel;
}

export const qgcMessageHistory: SystemMessage[] = [];
export const QGC_MAX_HISTORY = 150;
export const QGC_MAX_VISIBLE_TOASTS = 3;

export function postSystemMessage(text: string, level: ToastLevel = 'info', durationMs: number = 4000): void {
  if (!text) return;
  const now = new Date();
  const timeStr = now.toTimeString().split(' ')[0]; // HH:MM:SS
  const item: SystemMessage = { time: timeStr, text, level, id: Date.now() + Math.random() };

  qgcMessageHistory.push(item);
  if (qgcMessageHistory.length > QGC_MAX_HISTORY) {
    qgcMessageHistory.shift();
  }
  updateQgcHistoryUI();

  // Display fading toast
  showQgcToast(item, durationMs);
}

export function showQgcToast(item: SystemMessage, durationMs: number): void {
  const container = document.getElementById('qgc-toast-container');
  if (!container) return;

  while (container.children.length >= QGC_MAX_VISIBLE_TOASTS) {
    if (container.firstChild) container.removeChild(container.firstChild);
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

  let timeoutId: any = null;
  const startTimer = (dur: number) => {
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

export function updateQgcHistoryUI(): void {
  const label = document.getElementById('msg-history-label');
  if (label) {
    label.textContent = `Messages (${qgcMessageHistory.length})`;
  }

  const list = document.getElementById('qgc-history-list');
  const empty = document.getElementById('qgc-history-empty');
  if (!list) return;

  if (qgcMessageHistory.length === 0) {
    if (empty) empty.style.display = 'block';
    list.innerHTML = '';
    if (empty) list.appendChild(empty);
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
    tSpan.textContent = `[${item.time}]`;

    const lSpan = document.createElement('span');
    lSpan.className = 'qgc-h-level';
    lSpan.textContent = item.level.toUpperCase();

    const mSpan = document.createElement('span');
    mSpan.className = 'qgc-h-text';
    mSpan.textContent = item.text;

    row.appendChild(tSpan);
    row.appendChild(lSpan);
    row.appendChild(mSpan);
    list.appendChild(row);
  }

  // Autoscroll history to newest entry
  list.scrollTop = list.scrollHeight;
}

export function toggleMessageHistory(forceOpen?: boolean): void {
  const panel = document.getElementById('qgc-history-panel');
  if (!panel) return;

  const shouldOpen = (forceOpen !== undefined) ? forceOpen : !panel.classList.contains('open');
  if (shouldOpen) {
    updateQgcHistoryUI();
    panel.classList.add('open');
  } else {
    panel.classList.remove('open');
  }
}

export function clearMessageHistory(): void {
  qgcMessageHistory.length = 0;
  updateQgcHistoryUI();
}
