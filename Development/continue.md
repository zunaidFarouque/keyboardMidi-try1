Add the zones preset system (just like the touchpad preset system).

we will have:
- bottom row for chords
- bottom two rows real keyboard layout
- `QWER... & 1234...` rows real keyboard layout
- Add janko keyboard layout (total 4 rows)
- isomorphic keyboard (total 4 rows. each row 5 semitones up from row below)

---

add a system to quickly add a mapping so that when it is added, a touchpad group is created with mixers. so user can do this:
1. add a keyboard mapping,
2. select command, select touchpad group
3. in the group selector dropdown, beside that will be the same button that opens the touchpad group manager in touchpad tab. that way, user can easily add the specific group.
it would be even more useful we can just give it an easy button to add presets into the group using that group manager too.

---

if I cancel after pressing "+" in keyboard mapping (when adding new mapping), it crashes instantly. we need to fix it

---

new option needed in
Keyboard mappings -> Commands -> Layer -> remove overrides (that basically clears any overriding layer whether it is toggled on or pressed on. any overriding groups are also removed. It resets back to base layer. Kind of like keyboard layer system panic button)