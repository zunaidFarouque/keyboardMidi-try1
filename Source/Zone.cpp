#include "Zone.h"
#include "ScaleUtilities.h"

Zone::Zone()
    : name("Untitled Zone"),
      targetAliasHash(0),
      rootNote(60),
      scale(ScaleUtilities::ScaleType::Major),
      chromaticOffset(0),
      degreeOffset(0),
      isTransposeLocked(false) {
}

std::optional<MidiAction> Zone::processKey(InputID input, int globalChromTrans, int globalDegTrans) {
  // Check 1: Does input.deviceHandle match targetAliasHash?
  if (input.deviceHandle != targetAliasHash)
    return std::nullopt;

  // Check 2: Find index of input.keyCode in inputKeyCodes
  int index = -1;
  for (size_t i = 0; i < inputKeyCodes.size(); ++i) {
    if (inputKeyCodes[i] == input.keyCode) {
      index = static_cast<int>(i);
      break;
    }
  }

  if (index < 0)
    return std::nullopt;

  // Calculate effective transposition (respect isTransposeLocked)
  int effDegTrans = isTransposeLocked ? 0 : globalDegTrans;
  int effChromTrans = isTransposeLocked ? 0 : globalChromTrans;

  // Calculate final degree and note
  int finalDegree = index + degreeOffset + effDegTrans;
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
