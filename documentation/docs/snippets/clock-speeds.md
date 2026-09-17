| Idx | Frequency | Generator / Prescaler | Period | Application / Timing Margin |
| :---: | :--- | :--- | :--- | :--- |
| `1` | **10 Hz** | TIM1 PWM (PSC: 47999, ARR: 99) | $100.0\text{ ms}$ | Human-speed instruction stepping, visually tracking register LEDs |
| `2` | **25 Hz** | TIM1 PWM (PSC: 47999, ARR: 39) | $40.0\text{ ms}$ | Ultra-slow execution, oscilloscope & visual debugging |
| `3` | **50 Hz** | TIM1 PWM (PSC: 47999, ARR: 19) | $20.0\text{ ms}$ | Very slow pacing |
| `4` | **100 Hz** | TIM1 PWM (PSC: 47999, ARR: 9) | $10.0\text{ ms}$ | Slow step-by-step program inspection |
| `5` | **250 Hz** | TIM1 PWM (PSC: 47999, ARR: 3) | $4.0\text{ ms}$ | Smooth slow-motion execution |
| `6` | **500 Hz** | TIM1 PWM (PSC: 47999, ARR: 1) | $2.0\text{ ms}$ | Low frequency inspection |
| `7` | **1 kHz** | TIM1 PWM (PSC: 479, ARR: 99) | $1.0\text{ ms}$ | 1 kHz clock baseline |
| `8` | **2 kHz** | TIM1 PWM (PSC: 479, ARR: 49) | $500.0\ \mu\text{s}$ | Audio-frequency experimentation |
| `9` | **5 kHz** | TIM1 PWM (PSC: 479, ARR: 19) | $200.0\ \mu\text{s}$ | Moderate low-speed execution |
| `10` | **10 kHz** | TIM1 PWM (PSC: 479, ARR: 9) | $100.0\ \mu\text{s}$ | Logic analyzer trace verification |
| `11` | **25 kHz** | TIM1 PWM (PSC: 479, ARR: 3) | $40.0\ \mu\text{s}$ | High PWM speed |
| `12` | **50 kHz** | TIM1 PWM (PSC: 479, ARR: 1) | $20.0\ \mu\text{s}$ | **Maximum hardware PWM speed** ($+19.9\ \mu\text{s}$ ROM margin) |
| `13` | **62.5 kHz** | MCO HSI (8 MHz) / 128 | $16.0\ \mu\text{s}$ | Ultra-slow MCO stepping |
| `14` | **125 kHz** | MCO HSI (8 MHz) / 64 | $8.0\ \mu\text{s}$ | Slow visual stepping, watching I/O operations |
| `15` | **250 kHz** | MCO HSI (8 MHz) / 32 | $4.0\ \mu\text{s}$ | Stepping and interactive algorithm inspection |
| `16` | **500 kHz** | MCO HSI (8 MHz) / 16 | $2.0\ \mu\text{s}$ | **Default safe clock**, balanced speed for general programs |
| `17` | **750 kHz** | MCO HSI48 (48 MHz) / 64 | $1.33\ \mu\text{s}$ | Intermediate speed |
| `18` | **1 MHz** | MCO HSI (8 MHz) / 8 | $1.0\ \mu\text{s}$ | 1 MHz baseline |
| `19` | **1.5 MHz** | MCO HSI48 (48 MHz) / 32 | $666.7\text{ ns}$ | Intermediate speed |
| `20` | **2 MHz** | MCO HSI (8 MHz) / 4 | $500.0\text{ ns}$ | Smooth intermediate speed |
| `21` | **3 MHz** | MCO HSI48 (48 MHz) / 16 | $333.3\text{ ns}$ | Moderate speed |
| `22` | **4 MHz** | MCO HSI (8 MHz) / 2 | $250.0\text{ ns}$ | Fast execution |
| `23` | **6 MHz** | MCO HSI48 (48 MHz) / 8 | $166.7\text{ ns}$ | High speed |
| `24` | **8 MHz** | MCO HSI (8 MHz) / 1 | $125.0\text{ ns}$ | High speed execution |
| `25` | **12 MHz** | MCO HSI48 (48 MHz) / 4 | $83.3\text{ ns}$ | **Maximum safe speed** ($+28\text{ ns}$ margin above 55 ns ROM limit) |
