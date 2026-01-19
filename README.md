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

### 5. Code Index Number (CIN) Reference

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

## MIDI Message Display Format

The app formats messages as follows:

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

