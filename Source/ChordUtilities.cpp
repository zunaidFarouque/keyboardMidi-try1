#include "ChordUtilities.h"
#include "ScaleUtilities.h"
#include "MidiNoteUtilities.h"
#include <algorithm>
#include <sstream>

// Helper: Convert int vector to ChordNote vector (all non-ghost)
static std::vector<ChordUtilities::ChordNote> intsToChordNotes(const std::vector<int>& ints) {
  std::vector<ChordUtilities::ChordNote> result;
  for (int n : ints) {
    result.emplace_back(n, false);
  }
  return result;
}

std::vector<ChordUtilities::ChordNote> ChordUtilities::generateChord(int rootNote, 
                                                                      const std::vector<int>& scaleIntervals, 
                                                                      int degreeIndex, 
                                                                      ChordType type, 
                                                                      Voicing voicing,
                                                                      bool strictGhostHarmony) {
  std::vector<ChordNote> chordNotes;
  std::vector<int> notes; // Temporary int vector for voicing calculations

  if (type == ChordType::None) {
    // Single note - just return the root
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
    chordNotes.emplace_back(note, false);
    return chordNotes;
  }

  // Step A: Generate Raw Stack
  // Calculate base notes for the chord
  int root = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 8);

  // Build chord based on type
  if (type == ChordType::Triad) {
    notes.push_back(root);
    notes.push_back(third);
    notes.push_back(fifth);
  } else if (type == ChordType::Seventh) {
    notes.push_back(root);
    notes.push_back(third);
    notes.push_back(fifth);
    notes.push_back(seventh);
  } else if (type == ChordType::Ninth) {
    notes.push_back(root);
    notes.push_back(third);
    notes.push_back(fifth);
    notes.push_back(seventh);
    notes.push_back(ninth);
  } else if (type == ChordType::Power5) {
    notes.push_back(root);
    notes.push_back(fifth);
  }

  // Step B: Apply Voicing Strategy
  bool needsGhostNotes = (voicing == Voicing::SmoothFilled || voicing == Voicing::GuitarFilled);
  Voicing baseVoicing = voicing;
  
  // Map filled voicings to their base voicings
  if (voicing == Voicing::SmoothFilled) {
    baseVoicing = Voicing::Smooth;
  } else if (voicing == Voicing::GuitarFilled) {
    baseVoicing = Voicing::GuitarSpread;
  }
  
  if (baseVoicing == Voicing::RootPosition) {
    // Do nothing - keep as 1-3-5 (root position)
    // Notes stay as calculated
  } else if (baseVoicing == Voicing::Smooth) {
    // Smooth voicing: Inversions to minimize movement
    // Target Center = The rootNote argument (Zone Root) for smooth voice leading
    // Cluster ALL notes (including root) around Zone Root for smooth voice leading
    int center = rootNote; // Use Zone Root as specified
    int originalRootPitchClass = root % 12; // Store root pitch class for identification
    
    // Cluster ALL notes around center (±6 semitones)
    for (size_t i = 0; i < notes.size(); ++i) { // Process all notes including root
      // Move note to cluster around center (±6 semitones)
      // Add safety limit to prevent infinite loops
      int iterations = 0;
      const int maxIterations = 20; // Safety limit (should never need more than a few)
      
      while (notes[i] > center + 6 && iterations < maxIterations) {
        notes[i] -= 12;
        ++iterations;
      }
      
      iterations = 0;
      while (notes[i] < center - 6 && iterations < maxIterations) {
        notes[i] += 12;
        ++iterations;
      }
    }
    
    // Step C: Sort by pitch (Low to High)
    std::sort(notes.begin(), notes.end());
    
    // Ensure root (by pitch class) is the lowest note (for correct chord display)
    // Find the root in the sorted array (it might be in a different octave now)
    int rootNoteInArray = -1;
    int rootValue = -1;
    for (size_t i = 0; i < notes.size(); ++i) {
      // Check if this note is the root (modulo 12)
      if ((notes[i] % 12) == originalRootPitchClass) {
        rootNoteInArray = static_cast<int>(i);
        rootValue = notes[i];
        break;
      }
    }
    
    // If root is not the first (lowest) note, move it to be the bass
    // Priority: Root must be lowest for correct chord display
    if (rootNoteInArray > 0 && rootValue >= 0) {
      // Remove root from current position
      notes.erase(notes.begin() + rootNoteInArray);
      
      // Get the current lowest note
      int lowestNote = notes[0];
      
      // Place root one octave below the lowest note
      int targetRoot = rootValue;
      while (targetRoot >= lowestNote) {
        targetRoot -= 12;
      }
      
      // Insert root as the first (lowest) note
      notes.insert(notes.begin(), targetRoot);
    }
    
    // Convert to ChordNote and check for ghost notes
    chordNotes = intsToChordNotes(notes);
    if (needsGhostNotes) {
      addGhostNotes(chordNotes, rootNote, type, scaleIntervals, degreeIndex, strictGhostHarmony);
    }
    return chordNotes;
  } else if (baseVoicing == Voicing::GuitarSpread) {
    // GuitarSpread voicing: Open shapes with octave folding (Phase 18.6)
    // Maintain spread texture, shift octaves to keep in range
    
    // Step 1: Base Generation - Start with standard diatonic stack
    // (notes vector already contains root, third, fifth, etc. from earlier)
    
    // Step 2: Apply Spread (The "Guitar Shape")
    // Move 3rd (index 1) up +12 semitones
    if (notes.size() >= 2) {
      notes[1] += 12; // Move 3rd up one octave
    }
    
    // If 7th/9th exist (index 3+), move them up +12 to keep them above the 3rd
    if (notes.size() >= 4) {
      notes[3] += 12; // Move 7th up one octave
    }
    if (notes.size() >= 5) {
      notes[4] += 12; // Move 9th up one octave
    }
    
    // Step 3: Apply Gravity (Octave Folding)
    // Find the bass note (lowest note)
    int currentBass = *std::min_element(notes.begin(), notes.end());
    int diff = currentBass - rootNote; // rootNote is the Zone Anchor
    
    // High Threshold: If bass > 5 semitones above Zone Root, shift all down -12
    if (diff > 5) {
      for (size_t i = 0; i < notes.size(); ++i) {
        notes[i] -= 12;
      }
    }
    // Low Threshold: If bass < -7 semitones below Zone Root, shift all up +12
    else if (diff < -7) {
      for (size_t i = 0; i < notes.size(); ++i) {
        notes[i] += 12;
      }
    }
    
    // Step 4: Finalize - Sort by pitch (Low to High)
    std::sort(notes.begin(), notes.end());
    
    // Convert to ChordNote and check for ghost notes
    chordNotes = intsToChordNotes(notes);
    if (needsGhostNotes) {
      addGhostNotes(chordNotes, rootNote, type, scaleIntervals, degreeIndex, strictGhostHarmony);
    }
    return chordNotes;
  }

  // Step C: Final Polish - Sort by pitch (Low to High)
  // (Note: Smooth voicing already sorted and returned above)
  std::sort(notes.begin(), notes.end());

  // Convert to ChordNote (no ghost notes for RootPosition)
  chordNotes = intsToChordNotes(notes);
  return chordNotes;
}

void ChordUtilities::addGhostNotes(std::vector<ChordNote>& notes,
                                   int zoneAnchor,
                                   ChordType type,
                                   const std::vector<int>& scaleIntervals,
                                   int degreeIndex,
                                   bool strictHarmony) {
  if (notes.empty())
    return;
  
  // Sort notes by pitch for gap analysis
  std::sort(notes.begin(), notes.end(), [](const ChordNote& a, const ChordNote& b) {
    return a.pitch < b.pitch;
  });
  
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
      // Calculate the base MIDI note for this candidate degree using ScaleUtilities
      // This ensures the note is diatonic (in key)
      int baseCandidatePitch = ScaleUtilities::calculateMidiNote(zoneAnchor, scaleIntervals, candidateDegree);
      
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
          // Check for minor 2nd clashes (avoid notes within 1 semitone of existing notes)
          bool hasClash = false;
          for (const auto& cn : notes) {
            if (std::abs(cn.pitch - testPitch) <= 1) {
              hasClash = true;
              break;
            }
          }
          
          // Additional clash check: ensure distance from gap boundaries > 1 semitone
          if (!hasClash && std::abs(testPitch - gapStart) > 1 && std::abs(gapEnd - testPitch) > 1) {
            // Valid ghost note - insert it
            ChordNote ghost(testPitch, true);
            notes.insert(notes.begin() + gapStartIdx + 1, ghost);
            // Re-sort after insertion
            std::sort(notes.begin(), notes.end(), [](const ChordNote& a, const ChordNote& b) {
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

// Gravity Well: generate all inversions, pick the one whose average pitch is closest to center
std::vector<int> ChordUtilities::applyGravityWell(const std::vector<int>& notes, int centerPitch) {
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
    while (first > centerPitch + 6) first -= 12;
    while (first < centerPitch - 18) first += 12;
    inv[0] = first;
    for (size_t i = 1; i < inv.size(); ++i) {
      int p = inv[i];
      while (p > inv[i - 1] + 12) p -= 12;
      while (p < inv[i - 1]) p += 12;
      inv[i] = p;
    }
    std::sort(inv.begin(), inv.end());
    double avg = 0;
    for (int x : inv) avg += x;
    avg /= n;
    double dist = std::abs(avg - centerPitch);
    if (dist < bestDist) {
      bestDist = dist;
      bestInv = std::move(inv);
    }
  }
  return bestInv;
}

// Alternating Grip: odd scale degree (I, iii, V, vii) -> root position; even (ii, iv, vi) -> 2nd inversion
std::vector<int> ChordUtilities::applyAlternatingGrip(const std::vector<int>& notes, int degreeIndex) {
  if (notes.size() < 3)
    return notes;
  bool useSecondInv = ((degreeIndex % 2) == 0); // even degree -> 2nd inversion
  if (!useSecondInv)
    return notes; // root position
  std::vector<int> sorted = notes;
  std::sort(sorted.begin(), sorted.end());
  int n = static_cast<int>(sorted.size());
  // 2nd inversion: 5th in bass. sorted = [root, 3rd, 5th, (7th)] -> [5th, 7th, root+12, 3rd+12]
  int fifthIdx = (n >= 4) ? 2 : 2; // index of 5th in sorted triad/7th
  int bassNote = sorted[fifthIdx];
  std::vector<int> out;
  out.push_back(bassNote);
  for (int i = fifthIdx + 1; i < n; ++i) {
    int p = sorted[i];
    while (p <= bassNote) p += 12;
    out.push_back(p);
  }
  for (int i = 0; i < fifthIdx; ++i) {
    int p = sorted[i];
    while (p <= out.back()) p += 12;
    out.push_back(p);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Drop-2: for 4+ note chord, take 2nd note from top, drop one octave. Triad: drop middle note.
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
std::vector<int> ChordUtilities::applySmartFlow(const std::vector<int>& notes, ChordType type,
                                                int degreeIndex, int centerPitch) {
  if (type == ChordType::Triad || type == ChordType::Power5)
    return applyGravityWell(notes, centerPitch);
  if (type == ChordType::Seventh || type == ChordType::Ninth)
    return applyAlternatingGrip(notes, degreeIndex);
  return notes;
}

std::vector<ChordUtilities::ChordNote> ChordUtilities::generateChordForPiano(
    int rootNote, const std::vector<int>& scaleIntervals, int degreeIndex,
    ChordType type, PianoVoicingStyle style, bool strictGhostHarmony) {
  if (type == ChordType::None) {
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
    return {ChordNote(note, false)};
  }

  int root = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 8);

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

  const int centerPitch = rootNote; // Zone root as center

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
    int rootNote, const std::vector<int>& scaleIntervals, int degreeIndex,
    ChordType type, int fretMin, int fretMax) {
  if (type == ChordType::None) {
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
    return {ChordNote(note, false)};
  }

  int root = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 8);

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

  // For each chord tone, which strings can play it? (string index, fret) -> pitch
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

  // Bass isolation: root on A (string 1) -> mute string 6 (0); root on D (string 2) -> mute 6,5
  int minString = 0;
  if (rootString == 1)
    minString = 1; // don't use string 6
  else if (rootString == 2)
    minString = 2; // don't use string 6 or 5

  std::vector<int> used(kGuitarNumStrings, -1);
  std::vector<int> pitches;
  for (size_t i = 0; i < chordTones.size(); ++i) {
    int tone = chordTones[i];
    int start = (i == 0) ? 0 : minString; // root can use any string; others respect bass isolation
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
  // Setup: C Major scale intervals
  const std::vector<int> cMajorIntervals = {0, 2, 4, 5, 7, 9, 11};
  const int zoneAnchor = 60; // C4
  
  juce::String report;
  
  report += "=== OmniKey Voicing Debug Report (Phase 18.8) ===\n";
  report += "Scale: C Major\n";
  report += "Zone Anchor/Root: C4 (MIDI 60)\n";
  report += "Input Root: C4 (MIDI 60)\n";
  report += "Ghost Notes marked with [G] or *\n\n";
  
  // Helper to create repeated strings
  auto repeatString = [](const juce::String& str, int count) {
    juce::String result;
    for (int i = 0; i < count; ++i)
      result += str;
    return result;
  };
  
  // Iterate through Strict and Loose harmony modes
  const bool harmonyModes[] = {true, false}; // true = Strict, false = Loose
  const juce::String harmonyModeNames[] = {"ON", "OFF"};
  
  for (int harmonyIdx = 0; harmonyIdx < 2; ++harmonyIdx) {
    bool strictMode = harmonyModes[harmonyIdx];
    report += "\n" + repeatString("=", 70) + "\n";
    report += "=== STRICT HARMONY: " + harmonyModeNames[harmonyIdx] + " ===\n";
    report += repeatString("=", 70) + "\n\n";
    
    // Iterate through all voicing strategies (including filled)
    const Voicing voicings[] = {
      Voicing::RootPosition, 
      Voicing::Smooth, 
      Voicing::GuitarSpread,
      Voicing::SmoothFilled,
      Voicing::GuitarFilled
    };
    const juce::String voicingNames[] = {
      "Root Position", 
      "Smooth", 
      "Guitar Spread",
      "Smooth (Filled)",
      "Guitar (Filled)"
    };
    
    // Iterate through chord types (skip None and Power5 for this report)
    const ChordType types[] = {ChordType::Triad, ChordType::Seventh, ChordType::Ninth};
    const juce::String typeNames[] = {"Triad", "Seventh", "Ninth"};
    
    // Iterate through all voicings
    for (int voicingIdx = 0; voicingIdx < 5; ++voicingIdx) {
      Voicing voicing = voicings[voicingIdx];
      report += "\n" + repeatString("-", 60) + "\n";
      report += "VOICING: " + voicingNames[voicingIdx] + "\n";
      report += repeatString("-", 60) + "\n\n";
      
      for (int typeIdx = 0; typeIdx < 3; ++typeIdx) {
        ChordType type = types[typeIdx];
        report += "  Chord Type: " + typeNames[typeIdx] + "\n";
        report += "  " + repeatString("-", 50) + "\n";
        
        for (int degree = 0; degree <= 6; ++degree) {
          // Generate chord with current harmony mode
          std::vector<ChordNote> chordNotes = generateChord(zoneAnchor, cMajorIntervals, degree, type, voicing, strictMode);
          
          // Format MIDI numbers and note names with ghost indicators
          juce::String midiStr = "[";
          juce::String noteNamesStr = "(";
          for (size_t i = 0; i < chordNotes.size(); ++i) {
            if (i > 0) {
              midiStr += ", ";
              noteNamesStr += ", ";
            }
            midiStr += juce::String(chordNotes[i].pitch);
            if (chordNotes[i].isGhost) {
              midiStr += "*"; // Mark ghost in MIDI numbers
            }
            
            juce::String noteName = MidiNoteUtilities::getMidiNoteName(chordNotes[i].pitch);
            if (chordNotes[i].isGhost) {
              noteName += " [G]"; // Mark ghost in note names
            }
            noteNamesStr += noteName;
          }
          midiStr += "]";
          noteNamesStr += ")";
          
          // Get degree name
          const juce::String degreeNames[] = {"C (I)", "D (II)", "E (III)", "F (IV)", "G (V)", "A (VI)", "B (VII)"};
          juce::String degreeName = (degree < 7) ? degreeNames[degree] : juce::String("Degree ") + juce::String(degree);
          
          report += "    Degree " + juce::String(degree) + " (" + degreeName + "): " 
                 + midiStr + " " + noteNamesStr + "\n";
        }
        report += "\n";
      }
    }
  }
  
  report += "\n" + repeatString("=", 70) + "\n";
  report += "End of Report\n";
  
  // Write to file
  if (targetFile.getParentDirectory().createDirectory()) {
    targetFile.replaceWithText(report);
  }
}
