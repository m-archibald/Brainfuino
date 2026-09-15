# Manufacturing & Assembly Guide

Everything needed to manufacture and assemble the **Brainfuino Rev 1.1** board is pre-packaged in the repository under [`brainfuino-PCB/Rev 1.1 - KiCad/jlcpcb/production_files/`](https://github.com/m-archibald/Brainfuino/tree/main/brainfuino-PCB/Rev%201.1%20-%20KiCad/jlcpcb/production_files).


---

## Production Files

The production directory contains three key files tailored for JLCPCB SMT assembly:

| File | Purpose |
| :--- | :--- |
| `GERBER-BrainFuino.zip` | Complete 2-layer Gerber and drill archive (copper, solder mask, silkscreen, edge cuts, NPTH/PTH). |
| `BOM-BrainFuino.csv` | Bill of Materials specifying designators, footprints, and matching **LCSC part numbers**. |
| `CPL-BrainFuino.csv` | Component Placement List (Centroid / Pick-and-Place data: X, Y coordinates, rotation, and layer). |

---

## Ordering via JLCPCB SMT Service

1. Go to [JLCPCB](https://jlcpcb.com/) and click **Order Now**.
2. Upload `GERBER-BrainFuino.zip`.
3. Select standard PCB options:
   - **Layers:** 2
   - **Dimensions:** Detected automatically (~68.6 mm × 53.4 mm Arduino Uno footprint)
   - **PCB Color:** Green, Blue, Black, or Purple (your preference!)
   - **Surface Finish:** HASL (with lead) or LeadFree HASL / ENIG
4. Under **SMT Assembly**, toggle **Enable**:
   - **Assembly Side:** Top Side
   - **Quantity:** Select desired assembled boards
5. Proceed to the SMT file upload step:
   - Upload `BOM-BrainFuino.csv` as the **BOM file**.
   - Upload `CPL-BrainFuino.csv` as the **CPL / Placement file**.
6. Review component placement and confirm any part substitutions if an LCSC part is out of stock.
7. Finalize order!

---

## Post-Assembly Bring-up Checklist

Once your manufactured boards arrive:

1. **Visual Inspection:**
    * Inspect solder joints on the fine-pitch QFP-100 packages (IC1 MachXO2 and IC4 STM32) and TSOP-32 memory chips for any solder bridges or misalignments.

2. **Power Rail Test:**
    * Without programming anything yet, plug in a USB-C cable and verify the voltages with a multimeter:
        * **5.0V** on the VBUS side of Schottky diode `D1/D2`.
        * **3.3V** on the output pin of regulator `IC6` (`SPX3819`).

3. **Flash STM32 Coprocessor:**
    * Connect an ST-Link programmer to the 4-pin STM32 SWD header:
        * Pins: `SWDIO`, `SWCLK`, `GND`, `3.3V`
    * Flash the binary: [`companion-STM32-firmware/Debug/BrainfuinoMCU.hex`](https://github.com/m-archibald/Brainfuino/blob/main/companion-STM32-firmware/Debug/BrainfuinoMCU.hex).

4. **Program FPGA Bitstream:**
    * Connect a programmer to the 6-pin JTAG header:
        * Pins: `TCK`, `TMS`, `TDI`, `TDO`, `GND`, `3.3V`
    * Write `brainfuck_uP_brainfuck_uP.jed` into the MachXO2 internal flash.

5. **Serial Test:**
    * Unplug the programmers, reconnect via USB-C, and verify that the virtual COM port opens and responds in PuTTY!

