# TheoryBoard THY333 hardware reference

What is known about the board's electronics, how each part is driven, and how
confident we are. Sources: measurements on a real board, the parts' datasheets,
and static analysis of the board's original firmware (described in our own words;
no original code is reproduced). Where something is not verified it says so.
Improvements welcome: please open a "hardware finding" issue.

## Overview

| Part | Device | Connection |
| --- | --- | --- |
| Microcontroller | STM32F407VGT6 (LQFP100), Cortex-M4F, 1 MiB flash, 192 KiB RAM | |
| Clock | 8.000 MHz crystal (HSE) | PH0/PH1 |
| Pads (96) | Pressure sensors read through six 16-channel analog multiplexers | ADC1, 24 select lines |
| Touch buttons (43) | Four Cypress CapSense CY8CMBR3xxx-class controllers | I2C1 (PB8 SCL, PB9 SDA) |
| Pad lights (96) | WS2812-style RGB LEDs in one chain | SPI2 MOSI (PC3) with DMA |
| Button lights (24 channels) | TLC5947 24-channel 12-bit LED driver | PE3/PE4/PE5 |
| Display | 128x64 OLED, SSD1306-compatible | SPI3 (PB3, PB5) + PD11/PD12/PD14 |
| External memory | Winbond W25Q128JV, 16 MiB NOR flash | SPI3 (shared), select PA15, MISO PB4 |
| USB | USB full speed device (OTG_FS) | PA11/PA12 |
| TRS MIDI out (Type A) | Serial MIDI, 31,250 baud | USART1 TX, PB6 (not yet confirmed) |
| Pedal jack | Footswitch input | PD13 (not yet confirmed) |
| Debug | 6-hole header J2 (SWD) | PA13/PA14 |

## Memory map and boot

### Internal flash (1 MiB)

| Address | Sectors | Contents |
| --- | --- | --- |
| `0x08000000-0x0803FFFF` | 0-5 (4 x 16 KiB, 64 KiB, 128 KiB) | **Resident loader** (factory). Runs first after every reset. Never erase it. |
| `0x08040000-0x080DFFFF` | 6-10 (5 x 128 KiB) | Application firmware (original, or TBOSS) |
| `0x080E0000-0x080FFFFF` | 11 (128 KiB) | TBOSS: saved settings, presets, user scales (the original firmware has its image here too) |

The boards examined are **not read-protected** (RDP level 0, no write protection),
so the whole flash can be read and backed up over SWD. Never change the option bytes:
setting read protection and removing it again erases the entire chip, loader included.

### What happens at power-up

1. The resident loader starts (reset vector in sector 2).
2. It scans the pads and waits about **1.5 seconds**. If certain pads are held, it does
   something else instead (next section).
3. It disables interrupts and the SysTick timer, sets the vector table to `0x08040000`,
   loads the stack pointer and entry address from the first two words there, and jumps.
   **It does not check the image.** Clocks and peripherals are left as the loader set
   them, so an application must reset them itself (TBOSS does this in `src/tb/reset.c`).

So any firmware linked for `0x08040000` with a valid vector table runs, and a broken
one still leaves the loader (and SWD) intact.

### Loader functions selected by held pads (from static analysis; update mode verified)

From the loader's code, holding two pads during the 1.5 s window selects:

| Pads held (scanner index) | Effect |
| --- | --- |
| 0 and 3 (left half, leftmost column, top and bottom) | **Update mode**: the loader appears as a USB HID device "TheoryBoard" (vendor/product `0333:0333`, 64-byte reports) and accepts a firmware update. On the board: the display shows the TheoryBoard logo and the five navigation buttons stay lit. Verified with the TBOSS browser installer |
| 44 and 47 | An LED animation |
| 48 and 49 (right half, leftmost column, top two) | Re-initialises metadata in the external memory, then starts the application. **Avoid**: it may change the external memory's contents |

The update files the vendor utility sends start with a text header (signature `THY333`,
flags for firmware and scale data, a storage-initialise flag, offsets, three check
values, a required hardware version, and whether the payload is encrypted). The TBOSS
browser installer uses this mode directly, without such files: see
[installer-protocol.md](installer-protocol.md).

### RAM (TBOSS)

`0x20000000-0x2001FFFF`. TBOSS keeps its variables at the bottom (about 23 KiB), a
diagnostics block at `0x20018000` (readable over SWD while running), and its stack in
the top 16 KiB.

## J2: the SWD debug header

Six plated holes labelled **J2** on the main board (case opened). Counted from the
end next to the J2 label:

| Hole | Signal | Connect to the adapter's |
| --- | --- | --- |
| 1 | VDD (3.3 V from the board) | **VTref / target voltage sense only** (never let the adapter power the board) |
| 2 | PA14 = SWCLK | SWCLK |
| 3 | GND | GND |
| 4 | PA13 = SWDIO | SWDIO |
| 5 | not connected to reset (reads 0 V) | **leave unconnected** |
| 6 | PB3 = the display/memory SPI clock | **leave unconnected** (it is busy; an adapter's SWO input would load it) |

There is **no reset line** on J2. Debug tools must reset the chip in software
(SYSRESETREQ; OpenOCD: `reset_config none`). The board is powered from its own USB
connection while the adapter is attached.

Last-resort recovery if SWD ever stops answering: the STM32's built-in ROM bootloader
starts when BOOT0 (MCU pin 94) is held high during power-up. This needs soldering to
the chip's pin and has not been needed so far.

## Clocks

TBOSS runs the CPU at 48 MHz: HSE 8 MHz → PLL (M 8, N 192, P 4, Q 4) → 48 MHz system
clock and 48 MHz USB clock; APB1 (PCLK1) 24 MHz, APB2 48 MHz; flash latency 2 wait states.

## Pads: pressure sensing

Each pad is a pressure sensor read as an analog voltage. Six 16-channel analog
multiplexers ("banks") each select one of 16 pads; all six banks share the same
4-bit position, driven on separate pins per bank:

| Bank | Select bits 0..3 | Analog input (ADC1 channel) | Pads |
| --- | --- | --- | --- |
| 0 | PC1, PC2, PA1, PA2 | PC0 (ch 10) | 0-15 |
| 1 | PA7, PC5, PC4, PB1 | PA3 (ch 3) | 16-31 |
| 2 | PE7, PE8, PE9, PE10 | PA4 (ch 4) | 32-47 |
| 3 | PE11, PE12, PE13, PE14 | PA5 (ch 5) | 48-63 |
| 4 | PE15, PB10, PB11, PB12 | PA6 (ch 6) | 64-79 |
| 5 | PB13, PD8, PD9, PD10 | PB0 (ch 8) | 80-95 |

Pad number = 16 x bank + position. Physical position:
**pad = 48 x side + 4 x column + row**, where side 0 is the left (chord) half and 1 the
right (melody) half, column 0..11 runs left to right, and row 0..3 top to bottom.

Reading (TBOSS, `src/tb/scan.c`): set the position on all banks, wait 50 µs, then
convert each bank's input three times and average; one position per main-loop pass,
a full frame of 96 pads about every 15 ms. ADC1 at 12 MHz, 480-cycle sampling.
Values are 12-bit; higher means pressed harder. Thresholds that work: **pressed above
2500, released below 1500, full pressure 4000**. Pads are independent: pressing one
does not disturb the others.

## Touch buttons: four CapSense controllers

All on I2C1 (PB8 SCL, PB9 SDA, 400 kHz works). Each controller reports its buttons
as a 16-bit bitmap in register **`0xAA`** (button status; read 2 bytes, low byte
first). TBOSS only ever reads that register. **Never write the controllers'
configuration**: it lives in their own non-volatile memory and is not backed up.

| Address | Buttons | Bit for each button |
| --- | --- | --- |
| `0x39` | 12 root buttons, C..B left to right | 3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12 |
| `0x38` | 12 mode buttons, `5` `12` `M1`..`M7` `U1`..`U3` left to right | 3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12 |
| `0x36` | 5 navigation buttons (index 0..4) | 8, 7, 6, 1, 3 |
| `0x35` | 14 chord panel buttons F0..F13 | 6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2 |

Navigation index 0..4 = the five buttons below the display, left to right: ◀ ▲ ● ▼ ▶
(TBOSS uses them as back, up, enter, down, deeper).
The chord panel (checked on a board, 0.6.1): F0..F3 voicing I, II, III, guitar (top
row); F4..F6 spread I, II, III (second row); F7..F13 the octave V buttons marked −3, −2,
−1, 0, +1, +2, +3.

Practical notes:
- Read each controller once per poll and debounce for 20 ms; treat the first good
  reading as the baseline, not as presses.
- If a debugger reset interrupts an I2C transfer, a controller can hold SDA low and
  lock the bus. Before enabling the I2C peripheral, clock SCL up to 9 times with SDA
  released, then send a STOP (`tc_bus_clear` in `src/tb/i2c.c`).

## Pad lights: WS2812-style chain

One chain of 96 RGB LEDs on **PC3 (SPI2 MOSI, AF5)**. Each LED takes 24 bits in
**G, R, B** order, most significant bit first; LED *n* in the chain is pad *n*.
TBOSS generates the waveform with SPI2 at 12 MHz in 16-bit words: one word per LED bit,
`0xF000` for 0 (333 ns high) and `0xFF80` for 1 (750 ns high), followed by 320 µs low
for the reset/latch. DMA1 Stream 4 channel 0 feeds SPI2.

## Button lights: TLC5947

A TLC5947 (24 channels, 12-bit PWM) behind the centre buttons:
**PE3 = latch, PE4 = clock, PE5 = data.** Shift 288 bits, channel 23 first, 12 bits
each MSB first, then pulse latch. Channels, taken from the original firmware's light
tables (its 96-byte DMA block *n* is channel 23 − *n*):

| Button | Channel |
| --- | --- |
| Voicing I, II, III, guitar (F0..F3) | 7, 1, 13, 12 |
| Spread I, II, III (F4..F6) | 2, 0, 14 |
| Octave −3, −2, −1, 0, +1, +2, +3 (F7..F13) | 3, 4, 5, 6, 9, 10, 11 |
| Navigation buttons (order not mapped) | 8, 15, 16, 17, 18 |
| Unused | 19..23 |

The original firmware drives the panel at 60 % (off) and 100 % (on); TBOSS uses 30 % and
100 %. The LED test in the menu steps through all 24 channels one by one.

## Display: SSD1306-compatible OLED

128 x 64 pixels on **SPI3**: SCK PB3, MOSI PB5 (AF6), mode 0 (3 MHz works);
chip select **PD14** (low active), data/command **PD11** (high = data),
reset **PD12** (low active). Initialisation that works (command bytes):

```
AE  20 02  B0  C0  00 10 40  81 BB  A0  A6  A8 3F  A4  D3 00  D5 F0  D9 22  DA 12  DB 20  8D 14  AF
```

Hold reset low for 10 ms and wait 10 ms after release before sending it. Page
addressing: `B0+page, 00, 10`, then 128 data bytes.

## External memory: W25Q128JV (16 MiB)

On the same SPI3 bus as the display, chip select **PA15** (low active), MISO **PB4**
(AF6). Keep PA15 high whenever talking to the display. Standard SPI, mode 0. JEDEC ID
`EF 40 18`. It may be in power-down after reset: send `AB` (release) first.

It holds data from the original firmware, roughly (static analysis): large musical
lookup tables around `0x070000-0xA70600`, user scale and slot data from `0xB00000`, and
saved state (options, strum and arpeggiator settings, working-memory snapshots) at
`0xFE0000-0xFFD000`. **TBOSS only reads it** (commands `AB`, `9F`, `03`; enforced by
`scripts/firmware_safety.py`) and offers a verified backup from the web editor.

## Jacks: TRS MIDI out and pedal

From the board's previous firmware, by static analysis; **not yet confirmed on a board**
(Settings → About → *I/O test* in TBOSS 0.4.0 exists to confirm them):

- **TRS MIDI out:** USART1 transmit on **PB6** (alternate function 7); the previous firmware
  configures it transmit-only and releases PB6/PB7 together. TBOSS sends standard MIDI at
  31,250 baud, 8N1 (BRR 1536 at PCLK2 48 MHz). The jack follows TRS **Type A**.
- **Pedal:** **PD13**, the only external-interrupt input the previous firmware sets up
  (EXTI line 13; its handler runs a debounce/toggle sequence). TBOSS polls it with the
  internal pull-up and treats the level at power-up as "released" (auto type).
- The I/O test also shows **PA10** and **PD5**, read only, as alternatives in case the
  pedal is wired elsewhere.

## USB

USB full-speed device on PA11 (D-) / PA12 (D+), AF10. VBUS sensing is not used (the
board is USB-powered). TBOSS enumerates as a USB MIDI device "TheoryBoard MIDI"
(currently test ID `cafe:4069`).

## Pins TBOSS must never touch

- **PA13 / PA14**: SWD. Reconfiguring them could make the board unreachable for
  recovery. `scripts/firmware_safety.py` rejects any build that does.
- Option bytes, read/write protection, low-power Stop/Standby modes, the debug MCU
  configuration: also rejected by the safety check.
- Flash sectors 0-5 (the loader).
