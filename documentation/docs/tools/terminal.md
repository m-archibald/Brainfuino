# Web Serial Terminal

The **Brainfuino Web Serial Terminal** allows you to connect directly to your Brainfuino from modern web browsers (Chrome, Edge, Opera, or Brave) using the **Web Serial API**—with zero software installation required.

<div class="grid cards" markdown="1">

-   [:octicons-link-external-16: **Launch Web Terminal**](../web-terminal.html){ .md-button .md-button--primary target="_blank" }

    ---

    Launches the full-screen terminal interface with sidebar controls, speed stepping, Brainfuck code writing, firmware flashing, and hardware configuration.

-   [:octicons-terminal-16: **CLI Runner**](../user-guide/quick-start.md)

    ---

    Prefer a command-line terminal? Use `python run_program.py` from your local terminal.

</div>

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
- **Direct Keystroke Streaming:** Click inside the terminal to stream keystrokes directly to the FPGA (supports arrow keys, Backspace, Enter, PageUp/PageDown, and hotkeys).
- **Drag & Drop / Write Code File:** Drop a `.b` / `.bf` file directly onto the terminal or click **Write Code File** to stream code to Brainfuino ROM and execute immediately.
- **Line Mode:** Type commands into the bottom bar and press `Enter` to send with configurable line endings (`LF`, `CR`, `CRLF`).
- **Hardware Quick Actions:**
    - `!RST`: Reset the FPGA soft-processor.
    - `!MENU`: Open the interactive VT100 configuration menu.
    - `!DUMP`: Stream current program ROM to the console.
    - `PageUp` / `PageDown` or `!clk+` / `!clk-`: Step clock speed up or down dynamically.
- **Save Log:** Download the entire session output as a timestamped `.txt` file with one click.
