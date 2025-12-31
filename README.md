# HD44780 LCD Kernel Driver for Raspberry Pi

Char driver implementation for the [lcdcontrol project](https://github.com/cu-ecen-aeld/final-project-stradiot/wiki).

A Linux kernel module (`lcdcontrol.ko`) for driving generic HD44780 16x2 and 20x4 LCD character displays via GPIO in 4-bit mode. Designed for the Raspberry Pi 4 (Aarch64) and integrated with the Yocto Project.

## 🔌 Hardware Wiring

This driver operates in **4-bit mode** to save GPIO pins.

> **⚠️ CRITICAL HARDWARE NOTE:**
> Ensure the **RW (Read/Write)** pin is connected directly to **GND**. This driver is write-only and does not poll the busy flag. If this pin is floating or high, the display will show garbage characters or vertical lines.

| LCD Pin | Function | Connection | Note |
| :--- | :--- | :--- | :--- |
| 1 | VSS | GND | Logic Ground |
| 2 | VDD | +5V | Logic Power |
| 3 | V0 | Potentiometer | Contrast adjustment (Wiper pin) |
| 4 | **RS** | GPIO *X* | Register Select (Configurable in `src/lcdcontrol.h`) |
| 5 | **RW** | **GND** | **MUST be grounded** (Write mode only) |
| 6 | **E** | GPIO *Y* | Enable Signal |
| 11 | D4 | GPIO *A* | Data 4 |
| 12 | D5 | GPIO *B* | Data 5 |
| 13 | D6 | GPIO *C* | Data 6 |
| 14 | D7 | GPIO *D* | Data 7 |
| 15 | A | +5V / Resistor | Backlight Anode |
| 16 | K | GND | Backlight Cathode |

**Note:** If you see "scrolling" garbage characters or the "III III" pattern:
1. Check the RW pin connection.
2. Shorten the **Enable (E)** wire to prevent crosstalk.
3. Reload the driver to re-trigger the sync sequence: `rmmod lcdcontrol && modprobe lcdcontrol`.

## 🛠 Building & Installation

### Option 1: Yocto Project (Recommended)
This driver is designed to be built as a Yocto recipe.

1.  **Add the Recipe:**
    Ensure `lcdcontrol_git.bb` (or similar) is present in your meta-layer (e.g., `meta-lcdcontrol`).

2.  **Configure `local.conf`:**
    Add the driver to your image installation list.
    ```bitbake
    IMAGE_INSTALL:append = " lcdcontrol"
    ```
    *Note: The `lcdcontrol` package handles module installation and autoloading configuration automatically.*

3.  **Build:**
    ```bash
    bitbake lcdcontrol-image-dev
    ```

### Option 2: Manual Cross-Compilation
To build manually against a specific kernel source tree (e.g., if you are not using Yocto):

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export KERNEL_SRC=/path/to/linux-headers-rpi

make
```

## 🚀 Usage

### Loading the Module

To load the driver manually:
```bash
modprobe lcdcontrol
```

### Writing to the Display
The driver exposes a character device (typically `/dev/lcdcontrol`) for writing. Writing is designed to provide terminal-like behavior with automatic vertical scrolling.

* The driver waits for a full line (or newline) before updating the display.
* Lines longer than 16 characters are **truncated** (only the first 16 are printed).
* Multiple lines can be written in a single chunk; the driver respects newline delimiters.
* Supports streaming input (e.g., logs, sockets, `dmesg`).
* Only ASCII characters are supported.

**Via Command Line:**
```bash
echo "Hello World" > /dev/lcdcontrol
```

**Via User Application:**
See the companion project [lcdcontrol-user](https://github.com/stradiot/lcdcontrol-user) for the userspace CLI tool (`lcdtool`).

### Reading from the Display
Direct reading from the LCD hardware is not supported, as the **RW pin is grounded** to prevent logic voltage mismatches (3.3V Pi vs 5V LCD) and remove the need for bi-directional logic shifters.

However, the driver maintains an internal memory buffer of the LCD screen state (32 bytes), which can be read via the device file to verify what should be on the screen.

**Via Command Line:**
```bash
cat /dev/lcdcontrol
```

**Via User Application:**
See the companion project [lcdcontrol-user](https://github.com/stradiot/lcdcontrol-user) for the userspace CLI tool (`lcdtool`).

### IOCTL Commands
The driver supports IOCTL commands for advanced LCD control. Definitions can be found in `include/lcdcontrol.h`.

**Supported commands:**
* **Clear Display:** Clears screen and resets cursor to home.
* **Display On/Off:** Toggles the entire display visibility.
* **Cursor On/Off:** Toggles the underline cursor.
* **Blink On/Off:** Toggles the blinking block cursor.

**Via User Application:**
See the companion project [lcdcontrol-user](https://github.com/stradiot/lcdcontrol-user) for usage examples.

## 📜 License
GPL-2.0-only
