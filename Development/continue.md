release mode "Nothing" and "latch" doing exact same thing...
"Nothing" should not send the opposite thing is previous thing is in the state.

example:
intended behaviour for "Nothing"
```
finger 1 down -> midi C5 note on
finger 1 up -> [no midi data sent because "Nothing is chosen"]
finger 1 down -> midi C5 note off and immediately then note on.
```

it is both for touchpad and the keys's intended behaviour... so please properly fix it.
also rename it- "Nothing" -> "Sustain until retrigger".


---

===

---



Root note global and state global.


---

===

---


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
===
