# mitzi-midi
Towards a capturing MIDI signals via USB using Flipper Zero. The goal is to allow real-time monitoring of MIDI events including Note On/Off, Control Changes, Program Changes, Pitch Bend, and more.

<img alt="Main Screen"  src="screenshots/MainScreen.png" width="40%" />

## Intended Functionality
- **MIDI Message Parsing:** Full support for MIDI 1.0 specification with proper status byte parsing
- **USB MIDI Packet Handling:** Parses 4-byte USB MIDI packets with CIN (Code Index Number)
- **Message Display:** Human-readable formatting including musical note names (C4, A#5, etc.)
- **Ring Buffer History:** Stores last 8 messages with scrolling

## Usage
- **Arrow Keys**: Scrolls through message history
- **OK Button**: Clear message history
- **Back Button**: Exits

## Technical details
The app formats MIDI as follows:

```
NoteOn  Ch01 C4 Vel127     // Note On, channel 1, note C4, velocity 127
NoteOff Ch01 C4            // Note Off, channel 1, note C4
CC      Ch01 #007=064      // Control Change, channel 1, CC#7, value 64
ProgChg Ch01 Prg042        // Program Change, channel 1, program 42
PitchBd Ch01 +2048         // Pitch Bend, channel 1, +2048 semitones
ChPress Ch01 Val080        // Channel Pressure, channel 1, value 80
PolyAT  Ch01 C4 P080       // Poly Aftertouch, channel 1, note C4, pressure 80
System  0xF8               // System message (e.g., Clock)
```
A pitfall is that MIDI messages have variable lengths:
- Program Change: 2 bytes
- Note On/Off: 3 bytes
- SysEx: any length (can be hundreds of bytes)

USB MIDI uses fixed 4-byte packets. How do we know how many bytes are valid?
**Answer:** The *Code Index Number* (CIN) is the answer to that issue. CIN is metadata about the packet, not part of the MIDI message itself.

| CIN | Description                           |
|-----|---------------------------------------|
| 0x2 | Two-byte system common messages       |
| 0x3 | Three-byte system common messages     |
| 0x4 | SysEx starts or continues             |
| 0x5 | SysEx ends with 1 byte                |
| 0x6 | SysEx ends with 2 bytes               |
| 0x7 | SysEx ends with 3 bytes               |
| 0x8 | Note Off                              |
| 0x9 | Note On                               |
| 0xA | Poly-KeyPress                         |
| 0xB | Control Change                        |
| 0xC | Program Change                        |
| 0xD | Channel Pressure                      |
| 0xE | Pitch Bend                            |
| 0xF | Single Byte                           |

### Minimal Set of Examples
* Note On (C4, velocity 100, channel 1)
  ```
  0x09 0x90 0x3C 0x64
   │    │    │    └─ Velocity: 100
   │    │    └────── Note: 60 (C4)
   │    └─────────── Status: Note On, Channel 1
   └──────────────── Cable=0, CIN=9 (Note On, 3 bytes valid)
  ```
* Program Change (program 42, channel 1)
  ```
  0x0C 0xC0 0x2A 0x00
   │    │    │    └─ Padding (not used)
   │    │    └────── Program: 42
   │    └─────────── Status: Program Change, Channel 1
   └──────────────── Cable=0, CIN=C (Program Change, 2 bytes valid)
  ```
* Longer messages, e.g. a 10-byte SysEx message: `F0 41 10 42 12 00 01 02 03 F7` gets split into four USB MIDI packets:
  ```
  Packet 1: 0x04 0xF0 0x41 0x10  (CIN=4: Start, 3 bytes valid)
  Packet 2: 0x04 0x42 0x12 0x00  (CIN=4: Continue, 3 bytes valid)
  Packet 3: 0x04 0x01 0x02 0x03  (CIN=4: Continue, 3 bytes valid)
  Packet 4: 0x05 0xF7 0x00 0x00  (CIN=5: End with 1 byte)
  ```

## Known Limitations

1. **USB HAL Not Integrated**: Requires Flipper firmware USB MIDI support
2. **SysEx Not Fully Supported**: Large SysEx messages may need special handling
3. **No MIDI Output**: Currently receive-only
4. **Limited History**: Only 8 messages stored (adjustable via MAX_MIDI_MESSAGES)

## References
- [USB MIDI Specification](https://www.usb.org/sites/default/files/midi10.pdf)
- [MIDI 1.0 Specification](https://www.midi.org/specifications)
- [Flipper Zero USB HAL](https://github.com/flipperdevices/flipperzero-firmware)

## Version history
See [changelog.md](changelog.md)

