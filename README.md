# ZVRNK CAN Tool - Qt Port

This is a Qt 6 Widgets rebuild of the original WPF ZVRNK CAN Tool.

It is the live-communication member of the ZVRNK tool family:

| Project | Role |
| --- | --- |
| [ZVRNKPts](https://github.com/zavaranak/ZVRNKPts) | Image cutting, transformation, ageing, keying, padding, and compositing. |
| [ZVRNK_CANTool](https://github.com/zavaranak/ZVRNK_CANTool) | USB-CAN communication, transmit presets, receive monitoring, and CSV export. |
| [ZVRNK_CANAnalyzer](https://github.com/zavaranak/ZVRNK_CANAnalyzer) | Offline analysis and signal discovery from CAN receive CSV files. |

The application keeps the same functional architecture:

- `MainWindow`
  - Hosts session tabs.
  - Adds a new independent CAN session through the `+ Add Session` tab.
- `SessionWidget`
  - Owns one complete operator session.
  - Handles device selection, connection controls, transmit presets, receive view, filters, grouping, watch decoding, CSV import/export, and event log.
- `UsbCanService`
  - Owns CAN connection state.
  - Sends frames.
  - Runs the receive loop on a background thread.
  - Emits Qt signals for received frames and log messages.
- `ControlCanNative`
  - Dynamically loads `ControlCAN.dll`.
  - Resolves the vendor `VCI_*` functions.
  - Converts discovered boards into Qt/C++ data structs.

## Current Feature Set

- Device discovery with `VCI_FindUsbDevice2`
- Connect/disconnect for `VCI_USBCAN2`
- Channel, baud, normal/listen-only, and filter mode selection
- Manual TX frame send
- Per-row cyclic TX
- RX table with a 500-frame rolling buffer
- RX grouping by channel, raw CAN ID, and frame type
- RX realtime timestamp, seen count, average receive frequency, channel, ID, type, length, and data columns
- RX filters for ID, frame type, and payload text
- Watch decoder with byte offset, length, endian, signed mode, and formula
- TX CSV import/export
- RX CSV export
- Event log CSV export

## Requirements

- Windows x64
- Qt 6 Widgets and SerialPort development kit
- CMake 3.24 or newer
- C++20 compiler compatible with the selected Qt kit
- Vendor `ControlCAN.dll` (only for ControlCAN adapters such as the Waveshare USB-CAN-B)

## Supported adapters

- **ControlCAN (VCI) adapters**, such as the Waveshare USB-CAN-B. They need the
  vendor USB driver installed on each PC and `ControlCAN.dll` beside the exe.
- **GVRET serial adapters**, the protocol SavvyCAN calls "Serial Connection
  (GVRET)", for example an ESP32 with a CAN transceiver. They appear in the
  device list as `GVRET  COMx ...`. No DLL is needed, and the board must not be
  open in SavvyCAN at the same time. GVRET has no hardware filter (the app
  always accepts all IDs) and cannot send RTR frames.

Build details are documented in [bullding-step.md](bullding-step.md).

## DBC mode

Choose the **DBC** profile and load a `.dbc` file. The last file is remembered
and reloads automatically. This mode uses the normal adapter settings, and
transmit stays enabled.

- The received table adds a **Message** column (name from the DBC) and a
  **Signals** column with decoded physical values. Hover a row to see every
  signal on its own line.
- The **Signals** table lists the latest value, unit, raw value, count and
  update time for every signal seen. You can filter it by signal or message
  name.
- The ID filter also matches DBC message names. RX CSV export adds the
  Message and Signals columns.
- **J1939 match** pairs extended IDs with DBC messages while ignoring the
  source address byte. Use it for J1939 DBCs whose IDs carry a fixed source
  address.
- Supported DBC features: Intel and Motorola byte order, signed signals,
  scaling, units, value tables (`VAL_`), simple multiplexing (`M` / `mN`) and
  comments. Float signals (`SIG_VALTYPE_`) and extended multiplexing are not
  decoded.

## UI scale

The UI is drawn at 80% of the Windows display scale. To override it, set the
`QT_SCALE_FACTOR` environment variable before starting the app, for example
`1.0` for the original size.

## Typical workflow

1. Select a discovered USB-CAN device and channel.
2. Configure baud rate, normal/listen-only mode, and filter mode.
3. Add or import transmit rows, then send manually or run a cyclic queue.
4. Monitor received frames, group or filter them, and inspect a selected ID with
   the watch decoder.
5. Export the received frames to CSV and open that file in
   `ZVRNK_CANAnalyzer` for offline signal analysis.

## Quick build on Windows

Requirements are Qt 6 Widgets, CMake 3.24+, a C++20 MSVC 64-bit kit, and the
x64 vendor `ControlCAN.dll` runtime.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="C:/Qt/QtOnlineInstall/6.8.3/msvc2022_64" `
    -DCONTROLCAN_DLL_PATH="D:/path/to/ControlCAN.dll"
cmake --build build --config Debug
.\build\Debug\ZVRNK_CANTool.exe
```

The DLL is copied beside the executable when `CONTROLCAN_DLL_PATH` points to a
real file at configure time. At runtime the application also checks
`CONTROLCAN_DLL_PATH`, the executable directory, parent directories, the
current directory, and the normal DLL search path. Vendor dependency DLLs must
be available as well. A missing device or DLL is reported in the event log.

In Qt Creator, open the root `CMakeLists.txt`, select a Qt 6 MSVC 64-bit kit,
set the CMake variable `CONTROLCAN_DLL_PATH` if needed, and build the
`ZVRNK_CANTool` target.

## CSV handoff to CAN Analyzer

The RX export uses this header:

```text
Realtime,Seen,FrequencyHz,Channel,ID,Type,Length,Data
```

`Data` contains the payload bytes as hexadecimal text. The analyzer also accepts
legacy names such as `Timestamp`, `Frame ID`, `Format`, `DLC`, and `Freq (Hz)`.
Keep the exported CSV unchanged when moving it between the two applications.

## Source map

- `src/main_window.*` — session tabs and application shell.
- `src/session_widget.*` — device controls, TX/RX tables, filters, watch view,
  CSV actions, and event log.
- `src/usb_can_service.*` — threaded CAN connection and receive loop.
- `src/control_can_native.*` — dynamic `ControlCAN.dll` loading and VCI calls.
- `src/dbc.*` — DBC parser and signal decoder (tested by `tests/dbc_test.cpp`).
- `src/dbc_signal_model.*` — latest-value table for DBC signals.
- `src/gvret_protocol.*` — GVRET serial command encoding and stream parser
  (tested by `tests/gvret_protocol_test.cpp`).
- `src/formula_evaluator.*` — watch-value formula evaluation.

## Receive Frequency Formula

The Received Frames table uses average frequency per channel/CAN ID/frame-type key:

```text
frequency_hz = (seen_count - 1) / ((last_seen_ms - first_seen_ms) / 1000.0)
```

The value is shown as `0.00` until at least two matching frames have arrived and the first-to-last observation window is at least 1000 ms. This avoids startup spikes from frames drained from the driver queue in the same receive cycle.
