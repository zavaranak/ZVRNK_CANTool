# Project Handoff

This directory contains the Qt/C++ rebuild of the original WPF ZVRNK CAN Tool.

## ZVRNK project family

- `ZVRNKPts` — image processing and compositing.
- `ZVRNK_CANTool` — live USB-CAN communication and CSV export.
- `ZVRNK_CANAnalyzer` — offline CAN payload and signal analysis.

The CAN tool is the producer in the CAN workflow. Its RX CSV is consumed by the
separate analyzer repository.

## What Was Built Here

- Initialized Git repository.
- Added `.gitignore` for build output, Qt generated files, binaries, IDE files, and OS noise.
- Created a Qt 6 Widgets / CMake / C++20 application.
- Added verified build documentation in `bullding-step.md`.
- Added architecture and feature overview in `README.md`.

## Current Application Architecture

- `MainWindow`
  - Hosts session tabs.
  - Supports a `+ Add Session` tab.
  - Each session is independent.

- `SessionWidget`
  - Device selection.
  - Connect/disconnect controls.
  - TX frame table.
  - Manual frame send.
  - Cyclic TX per frame row.
  - RX frame table.
  - RX filtering by ID, frame type, and payload text.
  - RX grouping by channel, raw CAN ID, and frame type.
  - Watch decoder for selected CAN ID / byte range.
  - CSV import/export.
  - Event log.
  - Draggable splitters for:
    - connection bar / middle panel / event log
    - transmit panel / received frames panel

- `UsbCanService`
  - Owns CAN connection state.
  - Sends CAN frames.
  - Runs receive loop on a background thread.
  - Emits Qt signals for received frames and log messages.

- `ControlCanNative`
  - Dynamically loads `ControlCAN.dll`.
  - Resolves vendor `VCI_*` functions.
  - Wraps USB-CAN device discovery and CAN channel calls.

## Verified Build

Qt path used:

```powershell
C:\Qt\QtOnlineInstall\6.8.3\msvc2022_64
```

Configure:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\QtOnlineInstall\6.8.3\msvc2022_64"
```

Build:

```powershell
cmake --build build --config Debug
```

The build succeeds and produces:

```text
build\Debug\ZVRNK_CANTool.exe
```

Runtime hardware testing still requires the USB-CAN adapter, driver, and `ControlCAN.dll`.

## Notes For New CAN Analyzer Application

The next application should be separate from this directory.

Purpose:

- Import RX CSV exported by this CAN tool.
- Analyze CAN payload bytes over time.
- Help decode unknown CAN messages in a system.

CSV produced by the current Qt tool:

```text
Realtime,Seen,FrequencyHz,Channel,ID,Type,Length,Data
```

The analyzer accepts this schema directly and also recognizes legacy aliases
such as `Timestamp`, `Frame ID`, `Format`, `DLC`, and `Freq (Hz)`.

Recommended analyzer architecture:

- CSV importer.
- Frame parser.
- CAN ID grouping.
- Signal candidate generator.
- Time-series model.
- Plot view.
- Custom decoder definitions.

Default analysis:

- Group frames by CAN ID.
- For each CAN ID, analyze every single byte by default.
- Convert each byte to unsigned decimal.
- Track value over timestamp.
- Show min, max, latest, unique count, number of changes, and change rate.

Important analyzer features:

- Plot selected byte value over time.
- Allow user-selected multi-byte decode.
- Multi-byte options:
  - start byte
  - byte length
  - LSB/little-endian
  - MSB/big-endian
  - signed/unsigned
  - scale
  - offset
  - name
  - unit
- Plot custom decoded values over time.

Better decoding helpers:

- Bit-level analysis for all 64 payload bits.
- Hide constant bytes/bits.
- Highlight changing bytes.
- Detect counters.
- Detect flags/state bits.
- Rank candidate signals by activity/change behavior.
- Allow overlaying multiple signals on one plot.

Suggested build order for the analyzer:

1. CSV import parser.
2. CAN frame data model.
3. CAN ID grouping.
4. Single-byte candidate analysis.
5. Candidate summary table.
6. Plot selected byte over time.
7. Custom multi-byte decoder.
8. Save/load decoder definitions.
9. Bit-level analysis.
10. Discovery/ranking helpers.

The analyzer should be a decoding assistant, not only a graph viewer. Its core job is to help identify which byte, bit, or byte group changes when the physical system state changes.
