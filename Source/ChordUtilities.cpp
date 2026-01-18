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
  
  // Step 1: Identify candidate notes for ghost filling
  std::vector<int> candidates;
  
  if (strictHarmony) {
    // Strict mode: Use Root (1) and 5th only
    int root = ScaleUtilities::calculateMidiNote(zoneAnchor, scaleIntervals, degreeIndex);
    int fifth = ScaleUtilities::calculateMidiNote(zoneAnchor, scaleIntervals, degreeIndex + 4);
    candidates.push_back(root);
    candidates.push_back(fifth);
    // Also include 3rd if it's not already in the chord
    if (type == ChordType::Triad || type == ChordType::Seventh || type == ChordType::Ninth) {
      int third = ScaleUtilities::calculateMidiNote(zoneAnchor, scaleIntervals, degreeIndex + 2);
      candidates.push_back(third);
    }
  } else {
    // Loose mode: Use harmonic extensions
    if (type == ChordType::Triad) {
      // For triads, add 7th
      int seventh = ScaleUtilities::calculateMidiNote(zoneAnchor, scaleIntervals, degreeIndex + 6);
      candidates.push_back(seventh);
    } else if (type == ChordType::Seventh) {
      // For 7ths, add 9th
      int ninth = ScaleUtilities::calculateMidiNote(zoneAnchor, scaleIntervals, degreeIndex + 8);
      candidates.push_back(ninth);
    }
  }
  
  // Step 2: Analyze gaps in current voicing
  // Sort notes by pitch
  std::sort(notes.begin(), notes.end(), [](const ChordNote& a, const ChordNote& b) {
    return a.pitch < b.pitch;
  });
  
  // Find largest gap
  int largestGap = 0;
  size_t gapStartIdx = 0;
  
  for (size_t i = 0; i < notes.size() - 1; ++i) {
    int gap = notes[i + 1].pitch - notes[i].pitch;
    if (gap > largestGap && gap > 7) { // Only consider gaps > 7 semitones
      largestGap = gap;
      gapStartIdx = i;
    }
  }
  
  // Step 3: Try to place a ghost note in the largest gap
  if (largestGap > 7) {
    int gapStart = notes[gapStartIdx].pitch;
    int gapEnd = notes[gapStartIdx + 1].pitch;
    int gapCenter = (gapStart + gapEnd) / 2;
    
    // Try each candidate in different octaves around the gap center
    for (int candidate : candidates) {
      // Try candidate in different octaves to fit in gap
      for (int octaveOffset = -12; octaveOffset <= 12; octaveOffset += 12) {
        int testPitch = candidate + octaveOffset;
        
        // Check if testPitch is in the gap
        if (testPitch > gapStart && testPitch < gapEnd) {
          // Check for minor 2nd clashes (avoid notes within 1 semitone)
          bool hasClash = false;
          for (const auto& cn : notes) {
            if (std::abs(cn.pitch - testPitch) <= 1) {
              hasClash = true;
              break;
            }
          }
          
          if (!hasClash) {
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
  }
  
  // Also check for headroom/floor gaps (between highest/lowest note and range limits)
  // This is optional and can be added later if needed
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
