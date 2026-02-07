We need to create a new type of control using touchpad.
currently it has some things e.g. touch down and up, axis X and Y... mappable to Note, CC, Pitchbend...

now we will create a new type of control: "touchpad as mixer"
user can divice the touchpad into n mixer sliders (imaginary)
suppose I choose 5. then my touchpad will be responsible for 5 different faders.

control (quick mode and precision mode) x (absolute mode and relative mode) x (lock mode and free mode):
1.a) quick mode: put one finger to directly apply midi things.
1.b) precision mode: put one finger to get a semitransparent visualization of touchpad with the current touch location and the visualization of the faders. users can swipe and go to proper location for proper control (swipe with the finger to go there. but no midi apply yet). put second finger to apply midi things.
2.a) absolute mode: just like absolute pitch bend. the layout is static, finger position on the static canvas dictates the value
2.b) relative mode: like the relative pitch bend. here, finger movement is more important than finger position. user can start anywhere.
3.a) lock mode: the first fader a user touches is the one to target until the user lifts the finger and touches another one.
3.b) free mode: whatever the first touch fader was, user can simply swipe to the next fader and apply something there.

this will be called touchpad mixer/fader/something like that. easy to set up, just like zones. but it will only be able to send CC messages.
it will have many things from zones e.g. channel, CC number starting point (user can start at CC50 and have 5 faders so it will be like CC50 - CC54)
there will be range mapping too. to increase/reduce sensitivity.
on the bottom of the sliders there could be buttons (e.g. mute buttons) which can be activated/deactivated by the second finger tap.

---
**Touchpad Mixer – implementation status (from plan above)**
- [x] N faders, CC start, channel, name, layer
- [x] Quick / Precision (one-finger direct vs overlay + second-finger apply)
- [x] Absolute / Relative
- [x] Lock / Free (first-fader lock vs free swipe)
- [x] Input Y range and Output range (sensitivity)
- [x] Mute buttons (second-finger tap toggles mute; sends CC 0 when muted)
- [x] UI: list + editor, visualizer overlay when layer matches selected strip
- [x] Persistence: session (StartupManager); strips cleared when loading a new session
- [ ] Optional later: save/load touchpad mixer strips with preset (currently session-only)
- [ ] Optional later: dedicated “mute strip” region at bottom of touchpad (currently second-finger tap anywhere on fader toggles that fader’s mute)
