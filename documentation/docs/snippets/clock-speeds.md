| Idx | Frequency | Prescaler & Source | Period | Application / Timing Margin |
| :---: | :--- | :--- | :--- | :--- |
| `1` | **62.5 kHz** | HSI (8 MHz) / 128 | $16.0\ \mu\text{s}$ | Ultra-slow stepping, LED visualization, logic analyzer capture |
| `2` | **125 kHz** | HSI (8 MHz) / 64 | $8.0\ \mu\text{s}$ | Slow visual stepping, watching I/O operations |
| `3` | **250 kHz** | HSI (8 MHz) / 32 | $4.0\ \mu\text{s}$ | Stepping and interactive algorithm inspection |
| `4` | **500 kHz** | HSI (8 MHz) / 16 | $2.0\ \mu\text{s}$ | **Default safe clock**, balanced speed for general programs |
| `5` | **750 kHz** | HSI48 (48 MHz) / 64 | $1.33\ \mu\text{s}$ | Intermediate speed |
| `6` | **1 MHz** | HSI (8 MHz) / 8 | $1.0\ \mu\text{s}$ | 1 MHz baseline |
| `7` | **1.5 MHz** | HSI48 (48 MHz) / 32 | $666.7\text{ ns}$ | Intermediate speed |
| `8` | **2 MHz** | HSI (8 MHz) / 4 | $500.0\text{ ns}$ | Smooth intermediate speed |
| `9` | **3 MHz** | HSI48 (48 MHz) / 16 | $333.3\text{ ns}$ | Moderate speed |
| `10` | **4 MHz** | HSI (8 MHz) / 2 | $250.0\text{ ns}$ | Fast execution |
| `11` | **6 MHz** | HSI48 (48 MHz) / 8 | $166.7\text{ ns}$ | High speed |
| `12` | **8 MHz** | HSI (8 MHz) / 1 | $125.0\text{ ns}$ | High speed execution |
| `13` | **12 MHz** | HSI48 (48 MHz) / 4 | $83.3\text{ ns}$ | **Maximum safe speed** ($+28\text{ ns}$ margin above 55 ns ROM limit) |
