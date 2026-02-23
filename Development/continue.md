Thank you very much!

Now, we need the exact logic for SmartScaleBend.
Exact replica of PitchBend, with these added:
1. Follow global scale check mark.
2. if follow global is true, disable but if false, enable these: scale selection + editing button (look for existing implementation at zones. same thing here). make them always visible but enable/disable them based on the switch
3. the scale based pb calculation (I guess current implementation already has the logic...? I dont know so you check please.)


===============


Migrate from Mappings Tab to Touchpad Tab.
> already discussing

use: @touchpad Mappings (current implementations).md file to add exact features in the touchpad tab.


===

We need to add a new type of touchpad layout: Encoders.
Encoders are important for many navigation stuff and other stuff.
We need to implement it

===

Currently, there is no way to visualize the other layers of mapping if not physically went there.
So, create a switch/checkbox that when enabled, shows the specific layer that has been selected.

example:
if this checkbox is selected:
if currently in the mappings tab, visualizer will show the currently selected layer in that tab
if currently in the zones tab, visualizer will show the layer which currently selected zone is bound to

so, add the checkbox/toggle swtich in appropriate place...


===

We need to add groups in keyboard mapping too.
hold a key to enable that group.