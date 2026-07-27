# ZVRNK CAN Tool - Qt Port

This is a Qt 6 Widgets rebuild of the original WPF ZVRNK CAN Tool.

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
- RX filters for ID, frame type, and payload text
- Watch decoder with byte offset, length, endian, signed mode, and formula
- TX CSV import/export
- RX CSV export
- Event log CSV export

## Requirements

- Windows x64
- Qt 6 Widgets development kit
- CMake 3.24 or newer
- C++20 compiler compatible with the selected Qt kit
- Vendor `ControlCAN.dll`

Build details are documented in [bullding-step.md](bullding-step.md).
