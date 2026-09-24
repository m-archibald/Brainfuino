| Idx | Frequency | Generator / Prescaler | Period | Application / Timing Margin |
| :---: | :--- | :--- | :--- | :--- |
| `1` | **10 Hz** | TIM1 PWM (PSC: 47999, ARR: 99) | 100.0 ms | Human-speed instruction stepping, visually tracking register LEDs |
| `2` | **25 Hz** | TIM1 PWM (PSC: 47999, ARR: 39) | 40.0 ms | Ultra-slow execution, oscilloscope & visual debugging |
| `3` | **50 Hz** | TIM1 PWM (PSC: 47999, ARR: 19) | 20.0 ms | Very slow pacing |
| `4` | **100 Hz** | TIM1 PWM (PSC: 47999, ARR: 9) | 10.0 ms | Slow step-by-step program inspection |
| `5` | **250 Hz** | TIM1 PWM (PSC: 47999, ARR: 3) | 4.0 ms | Smooth slow-motion execution |
| `6` | **500 Hz** | TIM1 PWM (PSC: 47999, ARR: 1) | 2.0 ms | Low frequency inspection |
| `7` | **1 kHz** | TIM1 PWM (PSC: 479, ARR: 99) | 1.0 ms | 1 kHz clock baseline |
| `8` | **2 kHz** | TIM1 PWM (PSC: 479, ARR: 49) | 500.0 µs | Audio-frequency experimentation |
| `9` | **5 kHz** | TIM1 PWM (PSC: 479, ARR: 19) | 200.0 µs | Moderate low-speed execution |
| `10` | **10 kHz** | TIM1 PWM (PSC: 479, ARR: 9) | 100.0 µs | Logic analyzer trace verification |
| `11` | **25 kHz** | TIM1 PWM (PSC: 479, ARR: 3) | 40.0 µs | High PWM speed |
| `12` | **50 kHz** | TIM1 PWM (PSC: 479, ARR: 1) | 20.0 µs | **Maximum hardware PWM speed** (+19.9 µs ROM margin) |
| `13` | **62.5 kHz** | MCO HSI (8 MHz) / 128 | 16.0 µs | Ultra-slow MCO stepping |
| `14` | **125 kHz** | MCO HSI (8 MHz) / 64 | 8.0 µs | Slow visual stepping, watching I/O operations |
| `15` | **250 kHz** | MCO HSI (8 MHz) / 32 | 4.0 µs | Stepping and interactive algorithm inspection |
| `16` | **500 kHz** | MCO HSI (8 MHz) / 16 | 2.0 µs | **Default safe clock**, balanced speed for general programs |
| `17` | **750 kHz** | MCO HSI48 (48 MHz) / 64 | 1.33 µs | Intermediate speed |
| `18` | **1 MHz** | MCO HSI (8 MHz) / 8 | 1.0 µs | 1 MHz baseline |
| `19` | **1.5 MHz** | MCO HSI48 (48 MHz) / 32 | 666.7 ns | Intermediate speed |
| `20` | **2 MHz** | MCO HSI (8 MHz) / 4 | 500.0 ns | Smooth intermediate speed |
| `21` | **3 MHz** | MCO HSI48 (48 MHz) / 16 | 333.3 ns | Moderate speed |
| `22` | **4 MHz** | MCO HSI (8 MHz) / 2 | 250.0 ns | Fast execution |
| `23` | **6 MHz** | MCO HSI48 (48 MHz) / 8 | 166.7 ns | High speed |
| `24` | **8 MHz** | MCO HSI (8 MHz) / 1 | 125.0 ns | High speed execution |
| `25` | **12 MHz** | MCO HSI48 (48 MHz) / 4 | 83.3 ns | **Maximum safe speed** (+28 ns margin above 55 ns ROM limit) |
