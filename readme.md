<div align="center">
  <img src="assets/logo.svg" width="150" height="150" alt="MIDIQy Logo">
  <h1>MIDIQy</h1>
  <p><strong>The QWERTY Performance Engine.</strong></p>
  <p>Turn the hardware you have into the instrument you need.</p>
  
  <img src="https://img.shields.io/badge/C++-20-00599C?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows" alt="Windows">
  <img src="https://img.shields.io/badge/Audio-JUCE-88CC00" alt="JUCE">
</div>

---

## What is MIDIQy?

**MIDIQy** is a high-performance Windows application that transforms your computer keyboard, mouse, and trackpad into a highly expressive, low-latency MIDI controller.

Unlike basic keyboard-to-MIDI scripts, MIDIQy is built on a **raw input engine**. It distinguishes between multiple keyboards (one for drums, one for chords), handles complex harmonization, and ensures stage-safe performance.

Whether you are a producer traveling with just a laptop, or a technical user expanding your studio setup, MIDIQy lets you **type beats** and **perform nuances** without dedicated hardware.

## Key Features

### 🎹 The Performance Engine
* **Raw Input Handling:** Distinguishes between your laptop keyboard and external USB keyboards. Map them separately!
* **9-Layer Stack:** Base layer, plus 8 overlay layers (Chords, Drums, FX, Transport).
* **Stage-Safe Mode:** Locks the cursor to the window to prevent accidental clicks during performance.

### 🎼 Smart Expressiveness
* **Musical Zones:** Define key ranges for specific Scales, Chords, or Strumming.
* **Trackpad X/Y Pad:** Turn your mouse cursor into a Korg Kaoss Pad-style controller for Pitch Bend and Modulation.
* **Velocity Emulation:** Humanize your inputs with smart velocity logic.

### 🛠 Technical Precision
* **C++20 & JUCE Architecture:** Engineered for speed.
* **Lock-Free Audio Thread:** Guarantees no audio glitches or stuck notes.
* **Visualizer:** See exactly what your rig is doing with a real-time event log and mapping overlay.

## Getting Started
[Download the latest release] | [Build from Source]

---
*"This isn't a script; it's a raw engine."*