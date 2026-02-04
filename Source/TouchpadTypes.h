#pragma once

struct TouchpadContact {
  int contactId = 0;
  int x = 0;
  int y = 0;
  float normX = 0.0f;
  float normY = 0.0f;
  bool tipDown = true; // false when finger lifted (Tip Switch 0x42 cleared)
};
