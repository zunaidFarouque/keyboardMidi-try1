#include "ChordUtilities.h"
#include "MidiNoteUtilities.h"
#include "ScaleUtilities.h"
#include <algorithm>
#include <sstream>

// Helper: Convert int vector to ChordNote vector (all non-ghost)
static std::vector<ChordUtilities::ChordNote>
intsToChordNotes(const std::vector<int> &ints) {
  std::vector<ChordUtilities::ChordNote> result;
  for (int n : ints) {
    result.emplace_back(n, false);
  }
  return result;
}

std::vector<ChordUtilities::ChordNote> ChordUtilities::generateChord(
    int rootNote, const std::vector<int> &scaleIntervals, int degreeIndex,
    ChordType type, bool strictGhostHarmony) {
  std::vector<int> notes;

  if (type == ChordType::None) {
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                 degreeIndex);
    return {ChordNote(note, false)};
  }

  int root =
      ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                  degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 8);

  if (type == ChordType::Triad) {
    notes = {root, third, fifth};
  } else if (type == ChordType::Seventh) {
    notes = {root, third, fifth, seventh};
  } else if (type == ChordType::Ninth) {
    notes = {root, third, fifth, seventh, ninth};
  } else if (type == ChordType::Power5) {
    notes = {root, fifth};
  }

  std::sort(notes.begin(), notes.end());
  return intsToChordNotes(notes);
}

void ChordUtilities::addGhostNotes(std::vector<ChordNote> &notes,
                                   int zoneAnchor, ChordType type,
                                   const std::vector<int> &scaleIntervals,
                                   int degreeIndex, bool strictHarmony) {
  if (notes.empty())
    return;

  // Sort notes by pitch for gap analysis
  std::sort(
      notes.begin(), notes.end(),
      [](const ChordNote &a, const ChordNote &b) { return a.pitch < b.pitch; });

  // Step 1: Define candidate degrees (scale degree offsets from chord root)
  std::vector<int> candidateDegrees;

  if (strictHarmony) {
    // Strict mode: ONLY Root (degreeIndex) and 5th (degreeIndex + 4)
    candidateDegrees = {degreeIndex, degreeIndex + 4}; // Root, 5th
  } else {
    // Loose mode: Use harmonic extensions (7th and 9th)
    if (type == ChordType::Triad) {
      // For triads, add 7th (degreeIndex + 6)
      candidateDegrees = {degreeIndex + 6};
    } else if (type == ChordType::Seventh) {
      // For 7ths, add 9th (degreeIndex + 8)
      candidateDegrees = {degreeIndex + 8};
    } else {
      // For other types, no candidates
      return;
    }
  }

  // Step 2: Analyze gaps in current voicing
  // Find largest gap between adjacent notes
  int largestGap = 0;
  size_t gapStartIdx = 0;
  int gapStart = 0;
  int gapEnd = 0;

  for (size_t i = 0; i < notes.size() - 1; ++i) {
    int gap = notes[i + 1].pitch - notes[i].pitch;
    if (gap > largestGap && gap > 7) { // Only consider gaps > 7 semitones
      largestGap = gap;
      gapStartIdx = i;
      gapStart = notes[i].pitch;
      gapEnd = notes[i + 1].pitch;
    }
  }

  // Step 3: Try to place a ghost note in the largest gap
  if (largestGap > 7) {
    // Try each candidate degree
    for (int candidateDegree : candidateDegrees) {
      // Calculate the base MIDI note for this candidate degree using
      // ScaleUtilities This ensures the note is diatonic (in key)
      int baseCandidatePitch = ScaleUtilities::calculateMidiNote(
          zoneAnchor, scaleIntervals, candidateDegree);

      // Find the octave that places this pitch in the gap
      // Try different octaves to find one that fits
      // Start from a low octave and work up
      for (int octaveOffset = -24; octaveOffset <= 24; octaveOffset += 12) {
        int testPitch = baseCandidatePitch + octaveOffset;

        // Clamp to valid MIDI range
        if (testPitch < 0 || testPitch > 127)
          continue;

        // Check if testPitch is in the gap
        if (testPitch > gapStart && testPitch < gapEnd) {
          // Check for minor 2nd clashes (avoid notes within 1 semitone of
          // existing notes)
          bool hasClash = false;
          for (const auto &cn : notes) {
            if (std::abs(cn.pitch - testPitch) <= 1) {
              hasClash = true;
              break;
            }
          }

          // Additional clash check: ensure distance from gap boundaries > 1
          // semitone
          if (!hasClash && std::abs(testPitch - gapStart) > 1 &&
              std::abs(gapEnd - testPitch) > 1) {
            // Valid ghost note - insert it
            ChordNote ghost(testPitch, true);
            notes.insert(notes.begin() + gapStartIdx + 1, ghost);
            // Re-sort after insertion
            std::sort(notes.begin(), notes.end(),
                      [](const ChordNote &a, const ChordNote &b) {
                        return a.pitch < b.pitch;
                      });
            return; // Only add one ghost note per call
          }
        }
      }
    }

    // Strict mode fallback: If no candidate fit, do NOT try loose candidates
    // Just return the original chord (already handled by the loop above)
  }

  // No ghost note added - return original chord
}

// Gravity Well: generate all inversions, pick the one whose average pitch is
// closest to center
std::vector<int> ChordUtilities::applyGravityWell(const std::vector<int> &notes,
                                                  int centerPitch) {
  if (notes.empty())
    return notes;
  std::vector<int> sorted = notes;
  std::sort(sorted.begin(), sorted.end());
  const int n = static_cast<int>(sorted.size());
  double bestDist = 1e9;
  std::vector<int> bestInv = sorted;
  for (int shift = 0; shift < n; ++shift) {
    std::vector<int> inv;
    inv.reserve(n);
    for (int i = 0; i < n; ++i)
      inv.push_back(sorted[(shift + i) % n]);
    int first = inv[0];
    while (first > centerPitch + 6)
      first -= 12;
    while (first < centerPitch - 18)
      first += 12;
    inv[0] = first;
    for (size_t i = 1; i < inv.size(); ++i) {
      int p = inv[i];
      while (p > inv[i - 1] + 12)
        p -= 12;
      while (p < inv[i - 1])
        p += 12;
      inv[i] = p;
    }
    std::sort(inv.begin(), inv.end());
    double avg = 0;
    for (int x : inv)
      avg += x;
    avg /= n;
    double dist = std::abs(avg - centerPitch);
    if (dist < bestDist) {
      bestDist = dist;
      bestInv = std::move(inv);
    }
  }
  return bestInv;
}

// Alternating Grip: odd scale degree (I, iii, V, vii) -> root position; even
// (ii, iv, vi) -> 2nd inversion
std::vector<int>
ChordUtilities::applyAlternatingGrip(const std::vector<int> &notes,
                                     int degreeIndex) {
  if (notes.size() < 3)
    return notes;
  bool useSecondInv = ((degreeIndex % 2) == 0); // even degree -> 2nd inversion
  if (!useSecondInv)
    return notes; // root position
  std::vector<int> sorted = notes;
  std::sort(sorted.begin(), sorted.end());
  int n = static_cast<int>(sorted.size());
  // 2nd inversion: 5th in bass. sorted = [root, 3rd, 5th, (7th)] -> [5th, 7th,
  // root+12, 3rd+12]
  int fifthIdx = (n >= 4) ? 2 : 2; // index of 5th in sorted triad/7th
  int bassNote = sorted[fifthIdx];
  std::vector<int> out;
  out.push_back(bassNote);
  for (int i = fifthIdx + 1; i < n; ++i) {
    int p = sorted[i];
    while (p <= bassNote)
      p += 12;
    out.push_back(p);
  }
  for (int i = 0; i < fifthIdx; ++i) {
    int p = sorted[i];
    while (p <= out.back())
      p += 12;
    out.push_back(p);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Drop-2: for 4+ note chord, take 2nd note from top, drop one octave. Triad:
// drop middle note.
std::vector<int> ChordUtilities::applyDrop2(std::vector<int> notes) {
  std::sort(notes.begin(), notes.end());
  if (notes.size() == 3) {
    notes[1] -= 12; // 2nd from top = middle (3rd); drop down
    std::sort(notes.begin(), notes.end());
    return notes;
  }
  if (notes.size() < 4)
    return notes;
  size_t idx = notes.size() - 2; // 2nd from top
  notes[idx] -= 12;
  std::sort(notes.begin(), notes.end());
  return notes;
}

// Smart Flow: Triad -> Gravity Well; 7th/9th -> Alternating Grip
std::vector<int> ChordUtilities::applySmartFlow(const std::vector<int> &notes,
                                                ChordType type, int degreeIndex,
                                                int centerPitch) {
  if (type == ChordType::Triad || type == ChordType::Power5)
    return applyGravityWell(notes, centerPitch);
  if (type == ChordType::Seventh || type == ChordType::Ninth)
    return applyAlternatingGrip(notes, degreeIndex);
  return notes;
}

std::vector<ChordUtilities::ChordNote> ChordUtilities::generateChordForPiano(
    int rootNote, const std::vector<int> &scaleIntervals, int degreeIndex,
    ChordType type, PianoVoicingStyle style, bool strictGhostHarmony,
    int magnetSemitones) {
  if (type == ChordType::None) {
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                 degreeIndex);
    return {ChordNote(note, false)};
  }

  int root =
      ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                  degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 8);

  std::vector<int> notes;
  if (type == ChordType::Triad) {
    notes = {root, third, fifth};
  } else if (type == ChordType::Seventh) {
    notes = {root, third, fifth, seventh};
  } else if (type == ChordType::Ninth) {
    notes = {root, third, fifth, seventh, ninth};
  } else if (type == ChordType::Power5) {
    notes = {root, fifth};
  } else {
    return intsToChordNotes(notes);
  }

  // Magnet: center of voicing = rootNote + magnetSemitones (relative, no
  // octave). 0 = root.
  magnetSemitones = juce::jlimit(-6, 6, magnetSemitones);
  const int centerPitch = rootNote + magnetSemitones;

  if (style == PianoVoicingStyle::Block) {
    std::sort(notes.begin(), notes.end());
    return intsToChordNotes(notes);
  }

  if (style == PianoVoicingStyle::Open) {
    notes = applyDrop2(notes);
  }

  notes = applySmartFlow(notes, type, degreeIndex, centerPitch);
  std::sort(notes.begin(), notes.end());
  return intsToChordNotes(notes);
}

// Guitar: 6 strings standard tuning (E2=40, A2=45, D3=50, G3=55, B3=59, E4=64)
static const int kGuitarBaseMidi[] = {40, 45, 50, 55, 59, 64};
static const int kGuitarNumStrings = 6;

std::vector<ChordUtilities::ChordNote> ChordUtilities::generateChordForGuitar(
    int rootNote, const std::vector<int> &scaleIntervals, int degreeIndex,
    ChordType type, int fretMin, int fretMax) {
  if (type == ChordType::None) {
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                 degreeIndex);
    return {ChordNote(note, false)};
  }

  int root =
      ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                  degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals,
                                                degreeIndex + 8);

  std::vector<int> chordTones;
  if (type == ChordType::Triad) {
    chordTones = {root, third, fifth};
  } else if (type == ChordType::Seventh) {
    chordTones = {root, third, fifth, seventh};
  } else if (type == ChordType::Ninth) {
    chordTones = {root, third, fifth, seventh, ninth};
  } else if (type == ChordType::Power5) {
    chordTones = {root, fifth};
  } else {
    return {ChordNote(root, false)};
  }

  fretMin = std::max(0, fretMin);
  fretMax = std::min(24, fretMax);
  if (fretMin > fretMax)
    return intsToChordNotes(chordTones);

  // For each chord tone, which strings can play it? (string index, fret) ->
  // pitch
  auto canPlay = [&](int pitch, int stringIdx) -> int {
    int fret = pitch - kGuitarBaseMidi[stringIdx];
    return (fret >= fretMin && fret <= fretMax) ? fret : -1;
  };

  // Assign root first to lowest string that can play it
  int rootString = -1;
  for (int s = 0; s < kGuitarNumStrings; ++s) {
    if (canPlay(root, s) >= 0) {
      rootString = s;
      break;
    }
  }
  if (rootString < 0)
    return intsToChordNotes(chordTones);

  // Bass isolation: root on A (string 1) -> mute string 6 (0); root on D
  // (string 2) -> mute 6,5
  int minString = 0;
  if (rootString == 1)
    minString = 1; // don't use string 6
  else if (rootString == 2)
    minString = 2; // don't use string 6 or 5

  std::vector<int> used(kGuitarNumStrings, -1);
  std::vector<int> pitches;
  for (size_t i = 0; i < chordTones.size(); ++i) {
    int tone = chordTones[i];
    int start = (i == 0) ? 0 : minString; // root can use any string; others
                                          // respect bass isolation
    for (int s = start; s < kGuitarNumStrings; ++s) {
      if (used[s] >= 0)
        continue;
      int f = canPlay(tone, s);
      if (f >= 0) {
        used[s] = f;
        pitches.push_back(kGuitarBaseMidi[s] + f);
        break;
      }
    }
  }

  if (pitches.empty())
    return intsToChordNotes(chordTones); // fallback

  std::sort(pitches.begin(), pitches.end());
  return intsToChordNotes(pitches);
}

void ChordUtilities::dumpDebugReport(juce::File targetFile) {
  const std::vector<int> cMajorIntervals = {0, 2, 4, 5, 7, 9, 11};
  const int zoneAnchor = 60;

  juce::String report;
  report += "=== MIDIQy Chord Debug Report ===\n";
  report += "Scale: C Major, Root: C4 (MIDI 60)\n\n";

  auto repeatString = [](const juce::String &str, int count) {
    juce::String result;
    for (int i = 0; i < count; ++i)
      result += str;
    return result;
  };

  const ChordType types[] = {ChordType::Triad, ChordType::Seventh,
                             ChordType::Ninth};
  const juce::String typeNames[] = {"Triad", "Seventh", "Ninth"};

  for (int typeIdx = 0; typeIdx < 3; ++typeIdx) {
    ChordType type = types[typeIdx];
    report += repeatString("-", 60) + "\n";
    report += "Chord Type: " + typeNames[typeIdx] + "\n";
    report += repeatString("-", 60) + "\n\n";

    for (int degree = 0; degree <= 6; ++degree) {
      std::vector<ChordNote> chordNotes =
          generateChord(zoneAnchor, cMajorIntervals, degree, type, true);

      juce::String midiStr = "[";
      juce::String noteNamesStr = "(";
      for (size_t i = 0; i < chordNotes.size(); ++i) {
        if (i > 0) {
          midiStr += ", ";
          noteNamesStr += ", ";
        }
        midiStr += juce::String(chordNotes[i].pitch);
        noteNamesStr += MidiNoteUtilities::getMidiNoteName(chordNotes[i].pitch);
      }
      midiStr += "]";
      noteNamesStr += ")";

      const juce::String degreeNames[] = {
          "C (I)", "D (II)", "E (III)", "F (IV)", "G (V)", "A (VI)", "B (VII)"};
      juce::String degreeName =
          (degree < 7) ? degreeNames[degree]
                       : juce::String("Degree ") + juce::String(degree);
      report += "  Degree " + juce::String(degree) + " (" + degreeName +
                "): " + midiStr + " " + noteNamesStr + "\n";
    }
    report += "\n";
  }

  report += repeatString("=", 70) + "\nEnd of Report\n";

  if (targetFile.getParentDirectory().createDirectory()) {
    targetFile.replaceWithText(report);
  }
}
