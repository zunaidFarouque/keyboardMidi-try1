#include "KeyboardLayoutUtils.h"

static std::map<int, KeyGeometry> createKeyboardLayout() {
  std::map<int, KeyGeometry> layout;

  // Function Row (Row -1) - Full top row: Esc, F1-F12, PrintScreen, ScrollLock,
  // Pause Esc at x=0, then F1-F12 continuous
  layout[0x1B] = KeyGeometry(-1, 0.0f, 1.0f, "Esc");

  // F1-F12 continuous starting at x=1
  const int fKeysCodes[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75,
                            0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B};
  const char *fKeysLabels[] = {"F1", "F2", "F3", "F4",  "F5",  "F6",
                               "F7", "F8", "F9", "F10", "F11", "F12"};
  for (int i = 0; i < 12; ++i) {
    layout[fKeysCodes[i]] = KeyGeometry(-1, 1.0f + static_cast<float>(i), 1.0f,
                                        juce::String(fKeysLabels[i]));
  }

  // PrintScreen, ScrollLock, Pause starting at x=13 (after F12)
  layout[0x2C] = KeyGeometry(-1, 13.0f, 1.0f, "PrtSc"); // PrintScreen
  layout[0x91] = KeyGeometry(-1, 14.0f, 1.0f, "ScrLk"); // ScrollLock
  layout[0x13] = KeyGeometry(-1, 15.0f, 1.0f, "Pause"); // Pause

  // Row 0: Tilde, 1 through =, Backspace (Width 2.0)
  // ` 1 2 3 4 5 6 7 8 9 0 - = Backspace
  layout[0xC0] = KeyGeometry(0, 0.0f, 1.0f, "`"); // Tilde
  const int row0Codes[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                           0x37, 0x38, 0x39, 0x30, 0xBD, 0xBB};
  const char *row0Labels[] = {"1", "2", "3", "4", "5", "6",
                              "7", "8", "9", "0", "-", "="};
  for (int i = 0; i < 12; ++i) {
    layout[row0Codes[i]] =
        KeyGeometry(0, 1.0f + static_cast<float>(i), 1.0f,
                    juce::String(row0Labels[i])); // Shifted right by 1
  }
  layout[0x08] =
      KeyGeometry(0, 13.0f, 2.0f, "Bksp"); // Backspace (shifted right by 1)

  // Row 1: Tab (Width 1.5), Q through ], \ (Width 1.5)
  // Tab Q W E R T Y U I O P [ ] Backslash
  layout[0x09] = KeyGeometry(1, 0.0f, 1.5f, "Tab");  // Tab
  const int row1Codes[] = {0x51, 0x57, 0x45, 0x52, 0x54, 0x59,
                           0x55, 0x49, 0x4F, 0x50, 0xDB, 0xDD};
  const char *row1Labels[] = {"Q", "W", "E", "R", "T", "Y",
                              "U", "I", "O", "P", "[", "]"};
  for (int i = 0; i < 12; ++i) {
    layout[row1Codes[i]] = KeyGeometry(1, 1.5f + static_cast<float>(i), 1.0f,
                                       juce::String(row1Labels[i]));
  }
  layout[0xDC] = KeyGeometry(1, 13.5f, 1.5f, "\\"); // Backslash

  // Row 2: Caps (Width 1.75), A through ', Enter (Width 2.25)
  // Caps A S D F G H J K L ; ' Enter
  const int row2Codes[] = {0x41, 0x53, 0x44, 0x46, 0x47, 0x48,
                           0x4A, 0x4B, 0x4C, 0xBA, 0xDE};
  const char *row2Labels[] = {"A", "S", "D", "F", "G", "H",
                              "J", "K", "L", ";", "'"};
  layout[0x14] = KeyGeometry(2, 0.0f, 1.75f, "Caps"); // Caps Lock
  for (int i = 0; i < 11; ++i) {
    layout[row2Codes[i]] = KeyGeometry(2, 1.8f + static_cast<float>(i), 1.0f,
                                       juce::String(row2Labels[i]));
  }
  layout[0x0D] = KeyGeometry(2, 12.8f, 2.25f, "Enter"); // Enter

  // Row 3: LShift (Width 2.25), Z through /, RShift (Width 2.75)
  // LShift Z X C V B N M , . / RShift
  const int row3Codes[] = {0x5A, 0x58, 0x43, 0x56, 0x42,
                           0x4E, 0x4D, 0xBC, 0xBE, 0xBF};
  const char *row3Labels[] = {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"};
  layout[0xA0] = KeyGeometry(3, 0.0f, 2.25f, "LShift"); // Left Shift
  for (int i = 0; i < 10; ++i) {
    layout[row3Codes[i]] = KeyGeometry(3, 2.3f + static_cast<float>(i), 1.0f,
                                       juce::String(row3Labels[i]));
  }
  layout[0xA1] = KeyGeometry(3, 12.3f, 2.75f, "RShift"); // Right Shift

  // Row 4: Modifiers and Spacebar
  // LCtrl LWin LAlt Space RAlt RWin Menu RCtrl
  layout[0xA2] = KeyGeometry(4, 0.0f, 1.25f, "LCtrl");   // Left Ctrl
  layout[0x5B] = KeyGeometry(4, 1.25f, 1.25f, "LWin");   // Left Windows
  layout[0xA4] = KeyGeometry(4, 2.5f, 1.25f, "LAlt");    // Left Alt
  layout[0x20] = KeyGeometry(4, 3.75f, 6.25f, "Space");  // Spacebar
  layout[0xA5] = KeyGeometry(4, 10.0f, 1.25f, "RAlt");   // Right Alt
  layout[0x5C] = KeyGeometry(4, 11.25f, 1.25f, "RWin");  // Right Windows
  layout[0x5D] = KeyGeometry(4, 12.5f, 1.25f, "Menu");   // Menu/Context
  layout[0xA3] = KeyGeometry(4, 13.75f, 1.25f, "RCtrl"); // Right Ctrl

  // Navigation Cluster
  // Row 0: Insert, Home, PgUp at x=15.5
  layout[0x2D] = KeyGeometry(0, 15.5f, 1.0f, "Ins");  // Insert
  layout[0x24] = KeyGeometry(0, 16.5f, 1.0f, "Home"); // Home
  layout[0x21] = KeyGeometry(0, 17.5f, 1.0f, "PgUp"); // Page Up

  // Row 1: Del, End, PgDn at x=15.5
  layout[0x2E] = KeyGeometry(1, 15.5f, 1.0f, "Del");  // Delete
  layout[0x23] = KeyGeometry(1, 16.5f, 1.0f, "End");  // End
  layout[0x22] = KeyGeometry(1, 17.5f, 1.0f, "PgDn"); // Page Down

  // Row 3: Arrow Up at x=16.5
  layout[0x26] = KeyGeometry(3, 16.5f, 1.0f, "Up"); // Arrow Up

  // Row 4: Arrow Left, Down, Right at x=15.5
  layout[0x25] = KeyGeometry(4, 15.5f, 1.0f, "Left");  // Arrow Left
  layout[0x28] = KeyGeometry(4, 16.5f, 1.0f, "Down");  // Arrow Down
  layout[0x27] = KeyGeometry(4, 17.5f, 1.0f, "Right"); // Arrow Right

  // Numpad (Starting x=19)
  // Row 0: NumLock, /, *, -
  layout[0x90] = KeyGeometry(0, 19.0f, 1.0f, "Num"); // NumLock
  layout[0x6F] = KeyGeometry(0, 20.0f, 1.0f, "/");   // Numpad /
  layout[0x6A] = KeyGeometry(0, 21.0f, 1.0f, "*");   // Numpad *
  layout[0x6D] = KeyGeometry(0, 22.0f, 1.0f, "-");   // Numpad -

  // Row 1: 7, 8, 9, + (Tall, Height 2.0)
  layout[0x67] = KeyGeometry(1, 19.0f, 1.0f, "7"); // Numpad 7
  layout[0x68] = KeyGeometry(1, 20.0f, 1.0f, "8"); // Numpad 8
  layout[0x69] = KeyGeometry(1, 21.0f, 1.0f, "9"); // Numpad 9
  // Numpad + (tall, height 2.0)
  KeyGeometry plusKey(1, 22.0f, 1.0f, "+");
  plusKey.height = 2.0f;
  layout[0x6B] = plusKey;

  // Row 2: 4, 5, 6
  layout[0x64] = KeyGeometry(2, 19.0f, 1.0f, "4"); // Numpad 4
  layout[0x65] = KeyGeometry(2, 20.0f, 1.0f, "5"); // Numpad 5
  layout[0x66] = KeyGeometry(2, 21.0f, 1.0f, "6"); // Numpad 6

  // Row 3: 1, 2, 3, Enter (Tall, Height 2.0)
  layout[0x61] = KeyGeometry(3, 19.0f, 1.0f, "1"); // Numpad 1
  layout[0x62] = KeyGeometry(3, 20.0f, 1.0f, "2"); // Numpad 2
  layout[0x63] = KeyGeometry(3, 21.0f, 1.0f, "3"); // Numpad 3
  // Numpad Enter (tall, height 2.0)
  // Note: Same VK code (0x0D) as main Enter, so this will overwrite the main
  // Enter entry In practice, Windows distinguishes them via extended key flag,
  // but our map can only have one per VK code
  KeyGeometry numpadEnterKey(3, 22.0f, 1.0f, "Ent");
  numpadEnterKey.height = 2.0f;
  layout[0x0D] = numpadEnterKey; // Numpad Enter (overwrites main Enter in map)

  // Row 4: 0 (Width 2.0), .
  layout[0x60] = KeyGeometry(4, 19.0f, 2.0f, "0"); // Numpad 0 (2 units wide)
  layout[0x6E] = KeyGeometry(4, 21.0f, 1.0f, "."); // Numpad .

  return layout;
}

const std::map<int, KeyGeometry> &KeyboardLayoutUtils::getLayout() {
  static const std::map<int, KeyGeometry> layout = createKeyboardLayout();
  return layout;
}

juce::Rectangle<float>
KeyboardLayoutUtils::getKeyBounds(int keyCode, float keySize, float padding) {
  const auto &layout = getLayout();
  auto it = layout.find(keyCode);

  if (it == layout.end()) {
    // Key not found - return empty rectangle
    return juce::Rectangle<float>();
  }

  const auto &geom = it->second;

  // Calculate position
  // Row -1 (function row) needs special handling - place it above row 0
  float rowOffset = (geom.row == -1) ? -1.2f : static_cast<float>(geom.row);
  float x = padding + (geom.col * keySize);
  float y = padding +
            (rowOffset * keySize * 1.2f); // 1.2 multiplier for vertical spacing

  // Calculate size
  float width = geom.width * keySize;
  float height = geom.height * keySize;

  return juce::Rectangle<float>(x, y, width, height);
}
