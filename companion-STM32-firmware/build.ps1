param(
    [switch]$Flash,
    [string]$ComPort = ""
)

$ErrorActionPreference = "Stop"

$toolchain = "C:\ST\STM32CubeIDE_2.0.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.100.202509120712\tools\bin"
$gcc = "$toolchain\arm-none-eabi-gcc.exe"
$objcopy = "$toolchain\arm-none-eabi-objcopy.exe"
$size = "$toolchain\arm-none-eabi-size.exe"
$programmer = "C:\ST\STM32CubeIDE_2.0.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.300.202508131133\tools\bin\STM32_Programmer_CLI.exe"

$cFiles = @(
    "Core/Src/main.c",
    "Core/Src/stm32f0xx_hal_msp.c",
    "Core/Src/stm32f0xx_it.c",
    "Core/Src/syscalls.c",
    "Core/Src/sysmem.c",
    "Core/Src/system_stm32f0xx.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_adc_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_adc.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_cortex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_dma.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_exti.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_gpio.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pcd_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pcd.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_tim_ex.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_tim.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal.c",
    "Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_ll_usb.c",
    "Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c",
    "Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c",
    "Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c",
    "Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c",
    "USB_DEVICE/App/usb_device.c",
    "USB_DEVICE/App/usbd_cdc_if.c",
    "USB_DEVICE/App/usbd_desc.c",
    "USB_DEVICE/Target/usbd_conf.c"
)

$incArgs = @(
    "-ICore/Inc",
    "-IDrivers/STM32F0xx_HAL_Driver/Inc",
    "-IDrivers/STM32F0xx_HAL_Driver/Inc/Legacy",
    "-IDrivers/CMSIS/Device/ST/STM32F0xx/Include",
    "-IDrivers/CMSIS/Include",
    "-IUSB_DEVICE/App",
    "-IUSB_DEVICE/Target",
    "-IMiddlewares/ST/STM32_USB_Device_Library/Core/Inc",
    "-IMiddlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc"
)

$defArgs = @("-DUSE_HAL_DRIVER", "-DSTM32F072xB", "-DDEBUG")
$cFlags = @("-mcpu=cortex-m0", "-mthumb", "-O2", "-g3", "-Wall", "-ffunction-sections", "-fdata-sections")

if (!(Test-Path "Debug/build")) {
    New-Item -ItemType Directory -Path "Debug/build" | Out-Null
}

$objs = @()

Write-Host "Assembling startup code..."
$startupObj = "Debug/build/startup_stm32f072v8tx.o"
& $gcc -mcpu=cortex-m0 -mthumb -c -x assembler-with-cpp -o $startupObj "Core/Startup/startup_stm32f072v8tx.s"
$objs += $startupObj

Write-Host "Compiling C files..."
foreach ($file in $cFiles) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($file)
    $obj = "Debug/build/$baseName.o"
    & $gcc @cFlags @defArgs @incArgs -c -o $obj $file
    $objs += $obj
}

Write-Host "Linking ELF..."
$linkArgs = @(
    "-mcpu=cortex-m0",
    "-mthumb",
    "-TSTM32F072V8TX_FLASH.ld",
    "--specs=nosys.specs",
    "-Wl,-Map=Debug/BrainfuinoMCU.map",
    "-Wl,--gc-sections",
    "-static",
    "--specs=nano.specs",
    "-o", "Debug/BrainfuinoMCU.elf"
)
& $gcc @linkArgs @objs -lm

Write-Host "Generating HEX..."
& $objcopy -O ihex "Debug/BrainfuinoMCU.elf" "Debug/BrainfuinoMCU.hex"

Write-Host "Firmware size:"
& $size "Debug/BrainfuinoMCU.elf"
Write-Host "Build complete: Debug/BrainfuinoMCU.hex"

if ($Flash) {
    Write-Host ""
    Write-Host "=== Flashing Brainfuino via USB DFU ===" -ForegroundColor Cyan

    if (!$ComPort) {
        try {
            $st = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "STMicroelectronics.*(COM\d+)" }
            if ($st -and ($st.Name -match "(COM\d+)")) {
                $ComPort = $Matches[1]
                Write-Host "Auto-detected Brainfuino on $ComPort"
            }
        } catch {}

        if (!$ComPort) {
            $ports = [System.IO.Ports.SerialPort]::GetPortNames()
            if ($ports.Count -eq 1) {
                $ComPort = $ports[0]
            } elseif ($ports.Count -gt 1) {
                Write-Host "Detected ports: $($ports -join ', '). Using $($ports[0])."
                $ComPort = $ports[0]
            }
        }
    }

    if ($ComPort) {
        Write-Host "Triggering DFU reboot via $ComPort..."
        $sp = $null
        try {
            $sp = New-Object System.IO.Ports.SerialPort $ComPort, 115200
            $sp.ReadTimeout = 300
            $sp.WriteTimeout = 300
            $sp.Open()
            $sp.Write("!DFU!`r`n")
            $sp.BaseStream.Flush()
            $sp.Close()
        } catch {
            Write-Host "SerialPort.Open failed, attempting raw Win32 DFU trigger via $ComPort..."
            python -c "import ctypes; from ctypes import wintypes; k=ctypes.WinDLL('kernel32', use_last_error=True); h=k.CreateFileW('\\\\.\\$ComPort', 0xC0000000, 0, None, 3, 0, None); w=wintypes.DWORD(); (k.WriteFile(h, b'!DFU!\r\n', 7, ctypes.byref(w), None), k.CloseHandle(h)) if h!=-1 else None"
        } finally {
            if ($sp) { $sp.Dispose() }
        }
        Write-Host "Waiting 2 seconds for ST DFU USB device enumeration..."
        Start-Sleep -Seconds 2
    } else {
        Write-Host "No active COM port detected. Checking if board is already in DFU mode..."
    }

    Write-Host "Connecting and programming via STM32_Programmer_CLI..."
    & $programmer -c port=usb1 -w "Debug/BrainfuinoMCU.hex" -v -g 0x08000000
}
