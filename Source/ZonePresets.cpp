#include "ZonePresets.h"
#include <algorithm>
 
namespace {

// Columns >= this are the navigation cluster / numpad on the right-hand side.
// Limit presets to the main typing block (tilde through backslash / RShift).
constexpr float kMainBlockMaxCol = 14.0f;
 
// Treat any single printable ASCII character as a playable key.
// This includes letters, digits, and punctuation like [ ] ; ' , . / - =.
bool isPlayableLabel(const juce::String &label) {
  if (label.length() != 1)
    return false;
  const juce::juce_wchar c = label[0];
  return c >= 33 && c <= 126;
}
 
std::vector<int> collectRowKeys(const std::vector<int> &rows) {
  const auto &layout = KeyboardLayoutUtils::getLayout();
  std::vector<std::pair<float, int>> keyed;
 
  for (const auto &kv : layout) {
    const auto &geom = kv.second;
    if (std::find(rows.begin(), rows.end(), geom.row) == rows.end())
      continue;

    // Skip navigation cluster and numpad to the right of the main block.
    if (geom.col >= kMainBlockMaxCol)
      continue;
 
    const auto &label = geom.label;
    if (!isPlayableLabel(label))
      continue;
 
    keyed.emplace_back(geom.col, kv.first);
  }
 
  std::sort(keyed.begin(), keyed.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
 
  std::vector<int> out;
  out.reserve(keyed.size());
  for (const auto &p : keyed)
    out.push_back(p.second);
  return out;
}

std::shared_ptr<Zone> makeBaseZone(const juce::String &name) {
  auto z = std::make_shared<Zone>();
  z->name = name;
  z->layerID = 0;
  z->keyboardGroupId = 0;
  z->targetAliasHash = 0; // Global (All Devices)
  z->rootNote = 60;
  z->scaleName = "Major";
  z->chromaticOffset = 0;
  z->degreeOffset = 0;
  z->ignoreGlobalTranspose = false;
  z->ignoreGlobalSustain = false;
  z->polyphonyMode = PolyphonyMode::Poly;
  z->midiChannel = 1;
  return z;
}

} // namespace

namespace ZonePresets {

std::vector<std::shared_ptr<Zone>> createPresetZones(ZonePresetId presetId) {
  std::vector<std::shared_ptr<Zone>> zones;

  switch (presetId) {
  case ZonePresetId::BottomRowChords: {
    auto z = makeBaseZone("Bottom Row - Chords");
    // Bottom letter row (row 3), letters only (Z..M).
    z->inputKeyCodes = collectRowKeys({3});
    z->layoutStrategy = Zone::LayoutStrategy::Linear;
    z->chordType = ChordUtilities::ChordType::Triad;
    z->instrumentMode = Zone::InstrumentMode::Piano;
    z->pianoVoicingStyle = Zone::PianoVoicingStyle::Close;
    zones.push_back(std::move(z));
    break;
  }
  case ZonePresetId::BottomTwoRowsPiano: {
    auto z = makeBaseZone("Bottom 2 Rows - Piano");
    // Home row (2) + bottom row (3), letters only.
    z->inputKeyCodes = collectRowKeys({2, 3});
    z->layoutStrategy = Zone::LayoutStrategy::Piano;
    z->chordType = ChordUtilities::ChordType::None;
    zones.push_back(std::move(z));
    break;
  }
  case ZonePresetId::QwerNumbersPiano: {
    auto z = makeBaseZone("QWER + 1234 - Piano");
    // Number row (0) digits only + QWERTY row (1) letters only.
    auto numberKeys = collectRowKeys({0});
    auto letterKeys = collectRowKeys({1});
    z->inputKeyCodes.reserve(numberKeys.size() + letterKeys.size());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), numberKeys.begin(),
                            numberKeys.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), letterKeys.begin(),
                            letterKeys.end());
    z->layoutStrategy = Zone::LayoutStrategy::Piano;
    z->chordType = ChordUtilities::ChordType::None;
    zones.push_back(std::move(z));
    break;
  }
  case ZonePresetId::JankoFourRows: {
    auto z = makeBaseZone("Janko - 4 Rows");
    // Rows 0–3: numbers + QWERTY + ASDF + ZXCV, letters and digits only.
    auto row0 = collectRowKeys({0});
    auto row1 = collectRowKeys({1});
    auto row2 = collectRowKeys({2});
    auto row3 = collectRowKeys({3});
    z->inputKeyCodes.reserve(row0.size() + row1.size() + row2.size() +
                             row3.size());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row0.begin(), row0.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row1.begin(), row1.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row2.begin(), row2.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row3.begin(), row3.end());
    z->layoutStrategy = Zone::LayoutStrategy::Janko;
    z->chordType = ChordUtilities::ChordType::None;
    zones.push_back(std::move(z));
    break;
  }
  case ZonePresetId::IsomorphicFourRows: {
    auto z = makeBaseZone("Isomorphic Grid - 4 Rows");
    // Same 4-row block, but use Grid layout with 5-semitone row interval.
    auto row0 = collectRowKeys({0});
    auto row1 = collectRowKeys({1});
    auto row2 = collectRowKeys({2});
    auto row3 = collectRowKeys({3});
    z->inputKeyCodes.reserve(row0.size() + row1.size() + row2.size() +
                             row3.size());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row0.begin(), row0.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row1.begin(), row1.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row2.begin(), row2.end());
    z->inputKeyCodes.insert(z->inputKeyCodes.end(), row3.begin(), row3.end());
    z->layoutStrategy = Zone::LayoutStrategy::Grid;
    z->gridInterval = 5;
    z->chordType = ChordUtilities::ChordType::None;
    zones.push_back(std::move(z));
    break;
  }
  }

  return zones;
}

} // namespace ZonePresets

