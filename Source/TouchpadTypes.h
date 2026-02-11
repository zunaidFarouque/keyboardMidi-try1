#pragma once
#include <algorithm>
#include <vector>

struct TouchpadContact {
  int contactId = 0;
  int x = 0;
  int y = 0;
  float normX = 0.0f;
  float normY = 0.0f;
  bool tipDown = true; // false when finger lifted (Tip Switch 0x42 cleared)
};

/// Returns true if any contact that was tipDown in prev is now lifted in curr
/// (missing or tipDown == false). Used to prioritize lift events over throttle.
inline bool touchpadContactsHaveLift(const std::vector<TouchpadContact> &prev,
                                     const std::vector<TouchpadContact> &curr) {
  for (const auto &p : prev) {
    if (!p.tipDown) continue;
    auto it = std::find_if(curr.begin(), curr.end(),
                           [id = p.contactId](const TouchpadContact &c) {
                             return c.contactId == id;
                           });
    if (it == curr.end()) return true;  // contact gone => lifted
    if (!it->tipDown) return true;      // contact now up => lifted
  }
  return false;
}
