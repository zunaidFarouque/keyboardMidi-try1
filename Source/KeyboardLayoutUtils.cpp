#include "KeyboardLayoutUtils.h"

static std::map<int, KeyGeometry> createKeyboardLayout() {
  std::map<int, KeyGeometry> layout;

  // Row 0: 1 through = (Virtual Codes 0x31..0xBB)
  // 1 2 3 4 5 6 7 8 9 0 - =
  const int row0Codes[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0xBD, 0xBB};
  const char* row0Labels[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
  for (int i = 0; i < 12; ++i) {
    layout[row0Codes[i]] = {0, static_cast<float>(i), 1.0f, juce::String(row0Labels[i])};
  }

  // Row 1: Q through ] (Codes 0x51..0xDD), Offset 1.5
  // Q W E R T Y U I O P [ ]
  const int row1Codes[] = {0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0xDB, 0xDD};
  const char* row1Labels[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]"};
  for (int i = 0; i < 12; ++i) {
    layout[row1Codes[i]] = {1, 1.5f + static_cast<float>(i), 1.0f, juce::String(row1Labels[i])};
  }

  // Row 2: A through ' (Codes 0x41..0xDE), Offset 1.8
  // A S D F G H J K L ; '
  const int row2Codes[] = {0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0xBA, 0xDE};
  const char* row2Labels[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"};
  for (int i = 0; i < 11; ++i) {
    layout[row2Codes[i]] = {2, 1.8f + static_cast<float>(i), 1.0f, juce::String(row2Labels[i])};
  }

  // Row 3: Z through / (Codes 0x5A..0xBF), Offset 2.3
  // Z X C V B N M , . /
  const int row3Codes[] = {0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D, 0xBC, 0xBE, 0xBF};
  const char* row3Labels[] = {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"};
  for (int i = 0; i < 10; ++i) {
    layout[row3Codes[i]] = {3, 2.3f + static_cast<float>(i), 1.0f, juce::String(row3Labels[i])};
  }

  // Row 4: Spacebar (Code 0x20)
  layout[0x20] = {4, 6.0f, 6.0f, "Space"};

  return layout;
}

const std::map<int, KeyGeometry> &KeyboardLayoutUtils::getLayout() {
  static const std::map<int, KeyGeometry> layout = createKeyboardLayout();
  return layout;
}

juce::Rectangle<float> KeyboardLayoutUtils::getKeyBounds(int keyCode, float keySize, float padding) {
  const auto &layout = getLayout();
  auto it = layout.find(keyCode);
  
  if (it == layout.end()) {
    // Key not found - return empty rectangle
    return juce::Rectangle<float>();
  }

  const auto &geom = it->second;
  
  // Calculate position
  float x = padding + (geom.col * keySize);
  float y = padding + (geom.row * keySize * 1.2f); // 1.2 multiplier for vertical spacing
  
  // Calculate size
  float width = geom.width * keySize;
  float height = keySize;

  return juce::Rectangle<float>(x, y, width, height);
}
