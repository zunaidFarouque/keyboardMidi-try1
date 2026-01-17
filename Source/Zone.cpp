#include "Zone.h"
#include "ScaleUtilities.h"
#include "KeyboardLayoutUtils.h"
#include <algorithm>

Zone::Zone()
    : name("Untitled Zone"),
      targetAliasHash(0),
      rootNote(60),
      scale(ScaleUtilities::ScaleType::Major),
      chromaticOffset(0),
      degreeOffset(0),
      isTransposeLocked(false),
      layoutStrategy(LayoutStrategy::Linear),
      gridInterval(5) {
}

std::optional<MidiAction> Zone::processKey(InputID input, int globalChromTrans, int globalDegTrans) {
  // Check 1: Does input.deviceHandle match targetAliasHash?
  if (input.deviceHandle != targetAliasHash)
    return std::nullopt;

  int degree = 0;

  if (layoutStrategy == LayoutStrategy::Linear) {
    // Linear mode: Find index of input.keyCode in inputKeyCodes
    int index = -1;
    for (size_t i = 0; i < inputKeyCodes.size(); ++i) {
      if (inputKeyCodes[i] == input.keyCode) {
        index = static_cast<int>(i);
        break;
      }
    }

    if (index < 0)
      return std::nullopt;

    degree = index;
  } else {
    // Grid mode: Calculate degree based on keyboard geometry
    const auto& layout = KeyboardLayoutUtils::getLayout();
    
    // Check if current key is in the layout
    auto currentKeyIt = layout.find(input.keyCode);
    if (currentKeyIt == layout.end())
      return std::nullopt;

    // Get anchor key (first key in inputKeyCodes)
    if (inputKeyCodes.empty())
      return std::nullopt;

    auto anchorKeyIt = layout.find(inputKeyCodes[0]);
    if (anchorKeyIt == layout.end())
      return std::nullopt; // Anchor key not in layout - fallback to Linear?

    const auto& currentKey = currentKeyIt->second;
    const auto& anchorKey = anchorKeyIt->second;

    // Calculate delta
    int deltaCol = static_cast<int>(currentKey.col) - static_cast<int>(anchorKey.col);
    int deltaRow = currentKey.row - anchorKey.row;

    // Degree = deltaCol + (deltaRow * gridInterval)
    degree = deltaCol + (deltaRow * gridInterval);
  }

  // Calculate effective transposition (respect isTransposeLocked)
  int effDegTrans = isTransposeLocked ? 0 : globalDegTrans;
  int effChromTrans = isTransposeLocked ? 0 : globalChromTrans;

  // Calculate final degree and note
  int finalDegree = degree + degreeOffset + effDegTrans;
  int baseNote = ScaleUtilities::calculateMidiNote(rootNote, scale, finalDegree);
  int finalNote = baseNote + chromaticOffset + effChromTrans;

  // Clamp to valid MIDI range
  finalNote = juce::jlimit(0, 127, finalNote);

  // Return MIDI action (Type: Note, Channel: 1, Data1: finalNote, Data2: 100)
  MidiAction action;
  action.type = ActionType::Note;
  action.channel = 1;
  action.data1 = finalNote;
  action.data2 = 100;

  return action;
}

void Zone::removeKey(int keyCode) {
  inputKeyCodes.erase(
    std::remove(inputKeyCodes.begin(), inputKeyCodes.end(), keyCode),
    inputKeyCodes.end()
  );
}
