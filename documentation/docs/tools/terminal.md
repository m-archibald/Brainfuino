# Web Serial Terminal

The **Brainfuino Web Serial Terminal** allows you to connect directly to your Brainfuino from modern web browsers (Chrome, Edge, Opera, or Brave) using the **Web Serial API**—with zero software installation required.

<div class="grid cards" markdown="1">

-   [:octicons-link-external-16: **Open Dedicated Fullscreen Terminal**](web-terminal.html){ .md-button .md-button--primary target="_blank" }

    ---

    Launches the full-screen terminal interface with sidebar controls, quick speed toggles, and log downloading.

-   [:octicons-terminal-16: **CLI Runner**](../user-guide/quick-start.md)

    ---

    Prefer a command-line terminal? Use `python run_program.py` from your local terminal.

</div>

---

## Live Embedded Terminal

You can interact with your board right here inside the documentation:

<iframe src="../web-terminal.html" style="width: 100%; height: 750px; border: 1px solid var(--md-default-fg-color--lightest); border-radius: 12px; background: #ffffff;" allow="serial"></iframe>

---

## Supported Browsers & Setup

| Browser | Web Serial Support | Setup Required |
| :--- | :---: | :--- |
| **Google Chrome** | :material-check-circle: **Native** | None. Supported out of the box on Windows, Mac, Linux, and ChromeOS. |
| **Microsoft Edge** | :material-check-circle: **Native** | None. Works out of the box. |
| **Brave Browser** | :material-alert: **Permission Flag** | Brave blocks Web Serial by default. Navigate to `brave://settings/content/serialPorts` and allow sites to ask for permission, or enable the `#brave-commands` flag in `brave://flags`. |
| **Opera** | :material-check-circle: **Native** | None. Works out of the box. |
| **Firefox / Safari** | :material-close-circle: **Unsupported** | Mozilla and Apple have not yet implemented the Web Serial API. Please use a Chromium browser. |

---

## Features & Shortcuts

- **Universal Port Picker:** Click **Connect Brainfuino** to pick any available COM port.
- **Direct Keystroke Streaming:** Click inside the terminal to stream keystrokes directly to the FPGA (supports arrow keys, Backspace, Enter, and hotkeys).
- **Line Mode:** Type commands into the bottom bar and press `Enter` to send with configurable line endings (`LF`, `CR`, `CRLF`).
- **Hardware Quick Actions:**
    - `!RST`: Reset the FPGA soft-processor.
    - `!MENU`: Open the interactive VT100 configuration menu.
    - `1`–`7`: Dynamically switch clock speeds on the fly (500 kHz to 12 MHz).
- **Save Log:** Download the entire session output as a timestamped `.txt` file with one click.
