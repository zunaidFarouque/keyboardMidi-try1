# Features Guide

**Global MIDI Mode:**

Locks all keyboards for MIDI tasks. If any keypress happens when any other window is opened/focused, the MIDIQy window (main window/small window) will be focused.
Basically it will focus the window on first key press, and catch the next keypresses to generate MIDI. So, you will not be able to type on any other software until you disable Global MIDI Mode.

**Performance Mode:**

> TODO: Might Rename later...
> TODO: Use Shift F11 to only turn on Performance mode without the Global Midi mode.

Locks your touchpad. Your cursor will be hidden and clipped inside the small window that popped up. that small window will have the current touchpad layout active, and also show your finger positions. So, you will interact with those controls with your fingers. You can turn off it individually to unlock your touchpad.

**Delay MIDI Message (Seconds):**

Some software does not bind to MIDI Message if the window is not focused. Example is Giglad.\
This is a MIDIQy feature that lets you send the MIDI message, which enables you to generate specific MIDI message you want, and then move to your specific software, and click on "bind/map/listen to midi message".

So, simple workflow when binding MIDI message in Giglad:

[Preperation]

1. In Giglad, Go to the settings window where you will click on binding MIDI message to param.
2. In MIDIQy, Set: Delay MIDI Message (Seconds) = 1 second
3. Turn on MIDI mode.

[Binding]

4. Press the specific key in your keyboard.
5. instantly click on the binding option in Giglad.
6. wait, you will see right after 1 second of Step 4, the MIDI message is generated and Giglad has successfully captured it as the Giglad window is focused now.

---

---

# Mapping Guide

For most of the things, **We need to configure MIDIQy and Giglad both.**

The mappings in this document is added like this:

**Mappings:**\
[Key/input in inline code] [tags about where to config] = [What it does] [extra context if needed in parenthesis]

**Tags about where to config**:\
M = MIDIQy\
G = Giglad\
MG = MIDIQy and Giglad

# Settings

Global Pitch Bend = 12 Semitones.
use PRN to send pitch bend range.

> Maybe we need to send that to ALL individual VST instruments manually?

# Mappings

## General Global

`F12` = Global MIDI mode

`F11` = Global performance mode

**Laptop keyboard**

`Del` = MIDI panic\
&emsp; Make this in Layer 0 -> Global (ignores layering system.)

## 1-keyboard, Piano Style

### Layer-agnostic

> Add later

### Layer zero (Base layer)

Pros: You play EVERYTHING. No scale change/reconfigure on song change. Most varsatile.
Cons: most keyboards cannot handle multiple keypresses/key holds at once. it is their physical limitation. this sometimes might affect performance.

Map keyboard rows:\
`1 2 3 ... - +` and\
`q w e ... [ ]`\
to right hand piano part. No scale mapping or stuff. pure piano.

Similarly, map keyboard rows:\
`a s d ... ; '` and\
`z x c ... . /`\
to left hand piano part. No chord mapping or stuff. pure piano.
