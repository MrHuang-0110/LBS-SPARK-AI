# CLAUDE.md

## Agent skills

### Issue tracker

GitHub Issues on `MrHuang-0110/LBS-SPARK-AI`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five canonical labels (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context — one `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.


## What this is

STM32F103 (Alientek MiniSTM32 V4) firmware for a "Spark AI" educational robotics hub. Bare-metal C
(no RTOS) with an embedded **PikaPython** interpreter so user Python scripts, pushed over USB/Bluetooth,
drive motors and sensors on 4 hot-pluggable ports. UI is a small LED matrix.

## Build & toolchain

- **IDE-only build**: open `Projects/MDK-ARM/atk_f103.uvprojx` in Keil µVision (MDK-ARM). There is no
  Makefile/CMake for the firmware itself — Keil project files are the source of truth for includes,
  defines, and source groups.
- Build target: `atk_f103`. Linker output `Output/atk_f103.axf`, converted to `Output/atk_f103.bin`.
- **`postbuild.bat`** runs after build and copies `Output/atk_f103.bin` →
  `E:\LBS-FramWare\products\SPARK-AI\fwlib\app\app.bin`. Both paths are hardcoded absolute paths;
  the current source checkout is `E:\LBS-SPARK-AI`. Keep them synchronized with the repository and
  firmware-tool locations, otherwise the download tool may flash a stale binary.
- **`keilkill.bat`** deletes Keil intermediate files (`.o`, `.axf`, `.map`, `.dep`, etc.) across the tree.
- Keil project files (`.uvprojx`/`.uvoptx`/`.uvguix.*`) are binary-ish XML; edit source groups through the
  IDE, not by hand.

## PikaPython integration (the non-obvious part)

PikaPython (v1.13.4, pinned in `python/requestment.txt`) is compiled *into* the firmware — it is not a
host-side dependency.

- **C↔Python bindings are generated**, not hand-written. The `.pyi` stubs in `python/` (`_motor.pyi`,
  `_key.pyi`, `_color.pyi`, `_touch.pyi`, `_ultrasion.pyi`, `_matrix.pyi`, `_beep.pyi`, `_os.pyi`,
  `_math.pyi`, `_random.pyi`) describe the C modules exposed to Python. `python/pikaPackage.exe`
  consumes these to generate `python/pikascript-api/__pikaBinding.c` and the `PikaMain.h`/`*.h` glue.
  **If you change a `.pyi` or add a C module, regenerate the binding with `pikaPackage.exe`** (run from
  `python/`) and commit the generated `__pikaBinding.c`; Keil compiles it in.
- `python/pikascript-core` and `python/pikascript-lib/{PikaStdLib,math,random,ExternLib}` are the
  interpreter + stdlib sources, also compiled into the firmware.
- `python/main.py` is the import manifest the binding generator reads — it lists every module to bind.

### How a Python script runs at runtime

`run_python(name)` in `Middlewares/lbs_file_manager/lbsfilemanager.c` is the entry point, called from
`Main_Loop_Process()` (Users/main.c) when the user short-presses a UI item:

1. Special name `"Pauto"` → runs the built-in C demo loop `pauto_play()` (Users/main.c) instead of Python.
2. Otherwise the name is treated as a FATFS filename; the `.py` is read into a `mymalloc(SRAMIN,...)`
   buffer and compiled + executed by the PikaVM. After it returns, `cloase_all_motor()` stops actuators.

So the workflow is: C binding module exposes `_motor_run_power` etc. → a `.py` script on the device's
flash filesystem calls those → firmware executes it on demand.

## Architecture

### Boot layout (bootloader + app, IAP)

- `main()` calls `sys_nvic_set_vector_table(FLASH_BASE, 0x10000)` — the **application's vector table
  lives at 0x08010000** (64 KB offset), i.e. a bootloader occupies the first 64 KB. `APP_START_ADDR`
  (0x08008000) in main.h is a legacy/secondary marker — the active offset is 0x10000. Don't "fix" this
  mismatch without understanding the bootloader.
- JTAG is disabled at boot (`disable_jtag_enable_swd()`), SWD retained — flashing/debugging must use SWD.
- Firmware can be updated in-field via **OTA over USB CDC or Bluetooth** (see below).

### Co-operative event loop (no RTOS)

`main()` runs a single `while(1)` loop calling, per iteration: `ui_manager_update()` →
`Main_Loop_Process()` → OTA frame handling → `check_battery_with_debounce()` → USB/Bluetooth monitor
JSON output → `delay_ms(5)`.

Time-driven work is done by the **event manager**: `event_t[]` in Users/main.c registers named events
(`iwdg_feedevent` 10 ms, `scan_adc` 100 ms, `matrix_event`, `key_middle_event`, `monitor_event`, etc.)
created via `create_event_manger()`. Enable/disable at runtime with `set_event_enable()`/
`set_event_disable()` (e.g. monitor is disabled during OTA). Add a periodic task by adding an entry
here + an `EVENT_MANAGER` struct + a callback — do not spawn threads.

The IWDG watchdog is fed from the `iwdg_feedevent` event; `is_iwdg` flag distinguishes watchdog reset
from power-on. Long-press key → `Time_SaveToFlash()` + power-off animation + `while(1)` (intentional halt).

### Sensor hub (4 hot-pluggable ports)

`Middlewares/deviceIdentify/` auto-detects what is plugged into each of 4 ports and binds it to a
`SensorBase` in `hub_port[9]`. Device type IDs (used in `pauto_play()` and protocol frames):
`0xA2`=color, `0xA3`=ultrasonic, `0xA4`=touch. Motors are paired per group (ports 0/1 = group 0 →
motor pair 4/5; ports 2/3 = group 1 → motor pair 6/7). Per-group, an ultrasonic sensor takes priority
and suppresses touch/color on the other port of that group. Respect this priority logic when adding
sensor behavior.

Per-sensor drivers live in `application/{beep,blue,color,matrix,motor,touch,ultrasion}/`. The C Python
bindings (`_*.pyi` → `_*.c`) are the API surface scripts use; the raw `application/*` drivers are the
implementation. `application/blue/` is the Bluetooth module (BLE), not a sensor.

### UI: LED matrix + ui_manager

`application/matrix/` drives an LED matrix (TM1640-backed, see `tm1640_config.h`) through `led_matrix.c`
(framebuffer) + `animation.c` (power-on/off, vertical scroll transitions) + `ui_manager.c`
(items with patterns + click/release callbacks). Short-press cycles items and, on release, runs the
script named after the item; `is_refresh_matrix` triggers a redraw. `display.c`/`display.h` is the
high-level display API (`display_init`, `display_clear`, `display_play_power_off_animation`).

### OTA / file transfer protocol

`busDataparsing()` (Users/main.c) dispatches frames (`_AGREEMENT` struct with `.index` command byte).
USB frames arrive via `usbOTAhandle` (processed in main loop); Bluetooth OTA frames are buffered in ISR
(`ble_ota_frame`/`ble_ota_pending`) and processed in the main loop to avoid doing flash work in interrupt
context. `touchOtherFile()` (lbs_file_manager) writes received payload to flash/FATFS. Index bytes:
`0xDA` = OTA start (disables monitor), `0xBB`/`0xBC` = OTA complete/error (re-enables monitor),
`0x6F` = delete `updata.txt` + force reset, `0xB6`/`0xB9` = enter/exit python mode, `0xBE`/`0xBA` =
disable/enable monitor. **When adding a frame type, follow the monitor-disable-during-OTA convention.**

### Filesystems & storage

- **FATFS** (`Middlewares/fatfs/`, glue in `exfuns.c`) on internal flash — holds `.py` scripts and
  `.cfg` config files (e.g. `blue_cfg.cfg`, written via `fatfs_create_file`).
- **W25Q80x** SPI flash (`Middlewares/w25q80x/`) for additional storage.
- `Middlewares/file_manager/` + `Middlewares/lbs_file_manager/` wrap file ops; the latter also owns
  `run_python` and OTA file handling.
- `Middlewares/malloc/` provides `mymalloc(SRAMIN,...)` / `myfree` — the firmware's allocator. Use it
  (not raw `malloc`) for heap that crosses driver boundaries.

### Other middlewares

`protocol/` (frame parsing `_AGREEMENT`), `monitor/` (builds JSON status via `json-maker.c`, sent over
USB CDC `usb_printf` and BLE `blue_printf`), `event_manager/`, `RTC/`, `bat_manager/` (battery +
`check_battery_with_debounce`), `usb/` (STM32 USB Device Library CDC). `Drivers/BSP/` holds peripheral
drivers (ADC/IIC/KEY/LED/SPI/STMFLASH/TIMER/WDG); `Drivers/STM32F1xx_HAL_Driver` + `CMSIS` are vendor HAL.

## Conventions

- Comments and identifiers are largely Chinese; matching the surrounding comment style is expected.
- Each `application/*` and many `Middlewares/*` modules carry a `SYSTEM/` symlink/folder shadow of
  `Drivers/SYSTEM/{delay,sys,usart}` for relative includes — preserve this when adding a module.
- Memory: prefer `mymalloc(SRAMIN, ...)`/`myfree` over `malloc` for buffers that outlive a single call
  (e.g. `run_python` loads the whole script this way).
- All long-running user logic (Python scripts, `pauto_play`) must remain **non-blocking** or yield back
  to the main loop — there is no preemption. Blocking loops starve the IWDG and event manager.
- Time-sensitive constants (event periods, debounce, key timings) are set in `systemInit()` and
  `Key_Config_Params()`; change there, not scattered in drivers.

## Repository context

This firmware is one component of a larger workspace; `postbuild.bat` hands the binary to the sibling
`LBS-FramWare` firmware tool, and `../CanMV/` holds a separate K230 camera project. This repo is
self-contained for firmware builds — it does not depend on those siblings at compile time.
