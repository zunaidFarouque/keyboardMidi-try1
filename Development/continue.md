# Doing

please introduce state banks, and modifier keys.
for example, if I press "Q", it plays "C4". hold shift and play to play "C5". or "C4" in but another channel. or a CC, or command... or anything. or trigger something (e.g. set zone playmode to poly instead of mono)
basically shift or any other button could act as a way to modify.
so, there are a lot of possibilities.
now, we need a way to easily handle, manage and also visualize those modified state mappings...
even we can let users bind a zone to only a modified state. like that zone will be triggered only when in a certain modifier key is pressed.

there could also be banks. banks could be like a state of the layout/bindings. easily switch layout. open specific layout+binding instantly with just one button...

I dont know which will be better to implement first... we need to do brainstorming.

=== AND ===

the keyboard visualizer should have alias-wise keyboard.
for example, I should be able to choose which keyboard mapping is being shown.
e.g. when I select the laptop keyboard to be shown, the keyboard mappings will be shown for laptop And the any/master aliases. that way it is easy to identify conflict.
if I select the external keyboard alias, the keyboard mappings will be shown for external And the any/master aliases.
if I select any/master alias, it will only show mapping for that only.
if I select ALL, it will show from all keyboards (starting from any/master bindings and going down chronologically. it will not try to override if a key already shows as "bound". )

---

===

---

overhawl the configurator window

for CC, it would be useful if we added some previews of the general mapping of CC numbers.


---

===

---

## Regarding mouse/trackpage

what if we do this:
we visualize the trackpad finger position for each finger...?

then, what if we make a system:
1. always keep mouse pointer in a location (e.g. over the midi mini window) so that it cannot go out and click randomly.
2. in the meanwhile we can track fingers.
3. we can exit from that mouse lock state using a mapped command button in keyboard

---

===

---

## Brand:

we need to add icon to our software.



---

===

---

Thank you... now that the compiler is unified and optimized, we need to make the logger optimized and unified too.


---
===
---
===


# DO NOW:



right.
we need to do the musical nuances and customizability that have, implement in core logic and test it.

can you please tell me your detailed understanding of the nuances that I have?
1. I have mappings. this is for singular thing. it has some types. the properties list change based on the type we choose
2. I have zones. this is group thing. assigned keys show similar trait. have A LOT of musical nuances. the chords, the grid system, and so much playing style and stuff. lots of customizability.

I need you to tell me your full understanding of the mappings first. then we will implement the test, code implementation, GUI linking etc all to finish it.
then we will start the same thing with zones.
we go slow, but we go accurate.

so, no prompt, give me detailed understanding of you about the mappings. every type, what the parameters are, what it does etc



---
===
---
===

THINK: if we have Envelope, we dont need CC because envelope can handle it.
lets call it `Expression (ADSR)` or something better name that should contain the things of what it has as options.

rename:
envelope

remove:
- add scroll mapping
- add Trackpad X
- add Trackpad Y

modify:
the plus button should start listening for key (a popup will show that will say - press key to map). it will automatically turn on global midi mode just to capture the key (if not on already. also store the info whether it was turned on or off). there will also be option to skip listening (pop up will go away, a new mapping will be created with default things (exactly what happens now), because user did not want to press the key now).
either it captures the global midi mode, or it is skipped, the listening will stop (only if it was turned off and we turned it on to catch the key)