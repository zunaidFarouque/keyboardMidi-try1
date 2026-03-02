## Manual Smoke Checklist – UI Standardization

Run this after a full standardization pass, before a visual overhaul, or before a release.

- [ ] Build Debug and ensure tests pass:
  - `cmake --build build --config Debug`
  - `build\Debug\MIDIQy_Tests.exe --gtest_brief=1`

- [ ] Open the app and load a preset (or factory default).

- [ ] **Zones**
  - [ ] Select a zone.
  - [ ] Change root note, play mode, and one toggle (e.g. useGlobalRoot).
  - [ ] Switch to another zone and back; values remain correct.
  - [ ] Save preset; reload preset; values for that zone are still correct.

- [ ] **Keyboard mappings**
  - [ ] Select a mapping in the Keyboard Mappings tab.
  - [ ] Change type (Note/Expression/Command) and one combo (e.g. sustain style).
  - [ ] Save preset; reload; mapping values are correct.

- [ ] **Touchpad**
  - [ ] In the Touchpad tab, select a layout; change one layout property (e.g. rows/columns or region).
  - [ ] Select a touchpad mapping; change one mapping property.
  - [ ] Save preset; reload; both layout and mapping values are correct.

- [ ] **Settings**
  - [ ] Change pitch bend range or one checkbox (e.g. remember UI state, show touchpad in mini window).
  - [ ] Restart the app or reload settings; changed values persist.

- [ ] **Stability**
  - [ ] No crashes or assertions during the above.
  - [ ] Log contains no unexpected errors.

