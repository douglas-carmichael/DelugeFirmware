# Notice for the code derived from the LXR firmware

The files listed below are derived from the LXR firmware by Erica Synths
(<https://github.com/erica-synths/lxr-desktop-app>, commit `a43cd4fcda23ec3d577dc425261c3eb28bd5abff`, 2026-10-23).
Each file says which LXR source files it comes from and how it was modified for the Deluge.

- `src/deluge/dsp/lxr/components.h`, `components.cpp`
- `src/deluge/dsp/lxr/patch.h`, `patch.cpp`
- `src/deluge/dsp/lxr/tables.h`, `tables.cpp` (including the transient samples)
- `src/deluge/dsp/lxr/voice.h`, `voice.cpp`
- `src/deluge/dsp/filter/acid.h`, `acid.cpp`

Like the LXR firmware, these files are licensed under the GNU General Public License version 3 only
(GPL-3.0-only, the licence text is in `LICENSE` at the root of this repository), with the additional terms of
the LXR firmware's notice, which follows unchanged. Firmware built from this repository with these files in it
is therefore distributed under GPL-3.0-only, which the GPL-3.0-or-later licence of the rest of the Deluge
firmware allows.

How the additional terms are met here:

- **Trademarks.** The Deluge's menus call the engine "Drum synth" and the filter mode "Acid". Documentation
  states only that the code is ported from the Erica Synths LXR-02 firmware. This project is not made, approved
  or endorsed by, or affiliated with, Erica Synths. ("LXR" also appears in source code identifiers and in the
  names under which sounds using the engine are saved.)
- **Marking of modified versions.** Every derived file says that it was modified for the Deluge and how. The
  Deluge firmware presents itself as Deluge firmware, never as Erica Synths firmware.

The additional permission for linking with STMicroelectronics software is unused here: none of that software is
part of the Deluge firmware.

## Transient samples

The 12 transient samples in `tables.cpp` (`transientData`) are copied from the LXR firmware's `transientTables.c`,
where they are short 8-bit excerpts (2205 samples each, the last one shorter). The LXR firmware distributes them
under its notice below, and names their sources only by the labels in this table. Two of the labels identify
sounds on Freesound, checked on 2026-10-26; the others' sources are not known.

| Menu name | Label in the LXR firmware | Source |
|---|---|---|
| Click | `js_expClick` | not identified |
| Click 2 | `js_click2` | not identified |
| Tick | `4102__patchen__atik-2-10-stereoatik` | "ATIK 2 - 10 stereoatik.wav" by patchen, <https://freesound.org/s/4102/>, licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); used here as a shortened 8-bit excerpt |
| Kick | `82279__kevoy__acoustic-kick_eq_8_bit_signed_pcm` | "acoustic kick.wav" by KEVOY, <https://freesound.org/s/82279/>, [CC0](https://creativecommons.org/publicdomain/zero/1.0/) |
| Rim | `altemark_rs03 rimshot` | not identified |
| Drop | `neotone_drip2` | not identified |
| Hat | `pj cohen pedal chic` | not identified |
| Clap | `Shiftr 808 clap` | not identified |
| Kick 2 | `Mattel kick` | not identified |
| Snare | `md 16 17` | not identified |
| Tom | `md 16 36` | not identified |
| Snap 2 | `md 16 81` | not identified |

---

The LXR firmware's `NOTICE.md`, unchanged:

> # LXR firmware: copyright, licence and trademark notice
>
> Copyright (C) 2012-2026 Erica Synths. All rights not expressly granted below are reserved.
>
> ## What this notice covers
>
> This notice and the licence in [LICENSE](LICENSE) apply to:
>
> - `LXR_V2_STM32F765/`: the LXR firmware
> - `LXR_V2_STM32F765_Bootloader/`: the LXR bootloader
> - `cmake/`: the build system and tools shared by both
>
> Third-party components keep their own licences, listed [below](#third-party-components).
>
> ## Licence
>
> The firmware, bootloader and build system are free software: you can redistribute and/or modify them
> under the terms of the **GNU General Public License version 3** (GPL-3.0-only) as published by the
> Free Software Foundation, with the additional permission and the additional terms below
> (GPL-3.0 section 7). The full licence text is in [LICENSE](LICENSE).
>
> This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
> the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
> License for more details.
>
> ### Additional permission under GNU GPL version 3 section 7
>
> If you modify this Program, or any covered work, by linking or combining it with software from
> STMicroelectronics that is distributed under STMicroelectronics licence terms, in particular the
> "Ultimate Liberty" licence SLA0044 (or a modified version of that software), the licensors of this
> Program grant you additional permission to convey the resulting work. This includes the STM32 USB
> Device Library, the STM32CubeMX-generated code and the STMicroelectronics example code listed below.
> Corresponding Source for a non-source form of such a combination shall include the source code for the
> parts of that software used as well as that of the covered work.
>
> ### Additional terms under GNU GPL version 3 section 7
>
> 1. **Trademarks (section 7(e)).** This licence does not grant any rights under trademark law in the
>    names "Erica Synths", "LXR" and "LXR-02" or in the associated logos. They may not be used for any
>    product, modified firmware, service or other activity, commercial or not, in a way that suggests
>    that it comes from, is approved by or is affiliated with Erica Synths, without prior written
>    permission from Erica Synths. Truthful statements of compatibility, such as "custom firmware for
>    the Erica Synths LXR-02", are allowed.
> 2. **Marking of modified versions (section 7(c)).** Modified versions must be marked in reasonable ways
>    as different from the original version. At a minimum, the start-up screen of the device must show
>    that the firmware is modified (for example a different name or version text), and a modified version
>    must not be presented as official Erica Synths firmware.
>
> ## Hardware
>
> The LXR hardware is the property of Erica Synths. That includes the electronic design (schematics,
> printed circuit board layouts, bills of materials), the front panel, enclosure and industrial design, and
> the product's appearance. It is protected by copyright, design rights and other intellectual property
> rights. The hardware is **not** part of this repository and is **not** licensed under GPL-3.0 or any
> other licence. It may not be copied, reproduced, manufactured or sold, in whole or in part, without
> prior written permission from Erica Synths.
>
> The GPL-3.0 licence of the firmware does not grant any rights to the hardware.
>
> ## Third-party components
>
> These components are included in source form and are distributed under their own licences. The
> bootloader contains its own copies of the HAL, CMSIS and FatFs files.
>
> | Component | Location | Licence |
> |---|---|---|
> | STM32F7 HAL drivers (STMicroelectronics) | `Drivers/STM32F7xx_HAL_Driver/` | BSD-3-Clause, see the `LICENSE.txt` there |
> | CMSIS core (Arm) | `Drivers/CMSIS/Include/` | Apache-2.0 |
> | CMSIS STM32F7 device files, system and startup code (STMicroelectronics) | `Drivers/CMSIS/Device/ST/`, `Core/Src/system_stm32f7xx.c`, `Core/Startup/` | Apache-2.0 / BSD-3-Clause |
> | STM32 USB Device Library, modified into a USB-MIDI class (STMicroelectronics) | `Middlewares/JS/STM32_USB_Device_Library/` | ST SLA0044 |
> | STM32CubeMX-generated code with Erica Synths additions (STMicroelectronics) | `USB_DEVICE_MIDI/`, `FATFS/`, `Core/Src/stm32f7xx_it.c`, `Core/Src/stm32f7xx_hal_msp.c`, bootloader `Core/Src/main.c` | ST SLA0044 or BSD-3-Clause as stated in each file; Erica Synths additions under GPL-3.0 |
> | newlib system call stubs (STMicroelectronics) | `Core/Src/syscalls.c`, `Core/Src/sysmem.c` | BSD-3-Clause style, as stated in the files |
> | Flash programming helpers, from an STMicroelectronics example (LwIP IAP) | `Core/Src/flash_if.c`, `Core/Inc/flash_if.h` | STMicroelectronics example code, as stated in the files |
> | FatFs R0.12c (ChaN) | `Middlewares/Third_Party/FatFs/` | FatFs licence (BSD-style, 1 clause) |
> | HD44780 LCD routines, based on the mikrocontroller.net AVR-GCC tutorial | `Core/Src/lcd.c`, `Core/Inc/lcd.h` | as published on mikrocontroller.net |
