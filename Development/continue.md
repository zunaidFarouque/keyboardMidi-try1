now, we have to do an important thing.

currently the software detects all keystrokes. whether the application is in focus or not.
but we have to do this:
1. do a global shortcut binding 
2. e.g. Caps lock on = Midi mode. that specific keyboard device will be intercepted, and only midi will be sent. Caps Lock off = no midi. no interception. keyboard works as expected,

is it possible?
and will it harm my performance optimizations?



---

===

---

pitch bend conflict handling...

scenario:
```
when another pitch bend is pressed (A) (from normal pitch bend or smart scale bend), and we press on another one (B), the B overrides A.
if B is released while A is held, the pitch bend goes back to A.

OR

if A is released while B is held, A should not reset the pitch bend (it used to do it) because another key is held, and that has more priority.
```


---

===

---

THE Window uses 10% of my CPU at all the time. even if I dont use it. this is huge! why is that?