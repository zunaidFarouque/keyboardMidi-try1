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

**Pros**: You play EVERYTHING. No scale change/reconfigure on song change. Most varsatile.\
**Cons**: most keyboards cannot handle multiple keypresses/key holds at once. it is their physical limitation. this sometimes might affect performance.

Map keyboard rows:\
`1 2 3 ... - +` and\
`q w e ... [ ]`\
to right hand piano part. No scale mapping or stuff. pure piano.

Similarly, map keyboard rows:\
`a s d ... ; '` and\
`z x c ... . /`\
to left hand piano part. No chord mapping or stuff. pure piano.

#### Tempo Related

`PgUp` = Tempo Up

`PgDn` = Tempo Down

#### Transpose

We will Only add transpose settings in the MIDIQy, not Giglad.

`Left Arrow` = Transpose Down

`Right Arrow` = Transpose up

### Layer 6 (Giglad 1)

`Shift` = Momentary switch to Layer 7 (Giglad 2)

#### Player

`Backspace` = Start

`Home` = Tap Tempo

`Page Up` = Fade in

`Page Down` = Fade out

`End` = Stop

`Enter` = Synchro Start

#### Style Sections

_Intro and Ending_

`Z X` = Intro 1 & 2

`C` = Fade in (Manually Mapped)

`B` = Fade out (Manually Mapped)

`N M` = Ending 2 & 1

_Main and Fills_

Row `A S D ... K` = Main 1-8 (Auto-filled)

> Note: You need to change a setting in Giglad.
>
> Settings window -> (Modules) Arranger -> (Style section Switch)\
> Auto Fill = `Yes (Delayed)`
>
> This will do this:
>
> 1. Press `S` to transition to Main 2 WITH a Fill (plays Fill 2, then plays Main 2)
> 2. Double Press `S` to directly play Main 2 WITHOUT any Fill.

#### Bank Memories and other triggers

`Q W E ... I` = Bank Memories 1-8

Shift + `Q W E ... I` = Bank Memories 9-16\
(Bound to Layer 7)

`1 2 3 4` = Pad 1-4

Shift + `1 2 3 4` = Pad Synchro 1-4\
(Bound to Layer 7)

`5 6 7 8` = Style Memory 1-4

Shift + `5 6 7 8` = ?

#### Style Track & Melody Track Layers

Shift + `Z X C ... ,` = T1-8 Mute\
(Bound to Layer 7)

> TODO: Volume control using touchpad

Shift + `A S D F` = Melody tracks Mute\
(Bound to Layer 7)

Shift + `H J K` = Melody tracks octave up (ignore RH3)\
(Bound to Layer 7)

Shift + `L ; '` = Melody tracks octave down (ignore RH3)\
(Bound to Layer 7)

#### Lyrics

`Up Arrow` = Lyrics Scroll Up

`Down Arrow` = Lyrics Scroll Down

`\` = Show/Hide Lyrics

### Layer 8 (Giglad 3)

`1` = Styles

`2` = Instruments

`3` = Presets

`4` = Banks

`5` = Songs

`6` = Audio

`7` = Lyrics

`8` = Patterns

`9` = Melody

`0` = Physical

`-` = Pads

`+` = Lyrics Viewer

`Backspace` = Back

`[` = Page Prev

`]` = Page Next

> TODO: Try to migrate on encoders navigation.
> like this: hold a button to make arrow buttons the encoder. left and right should normally navigate, up button to press on encoder. Select with enter, back using backspace. It is not a layer, it is a very specific group.
