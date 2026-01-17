#include "MusicalKeyboardComponent.h"
#include "MidiNoteUtilities.h"

MusicalKeyboardComponent::MusicalKeyboardComponent() {
  activeIntervals.resize(12, false);
  activeIntervals[0] = true; // Root is always on
  // 7 white keys * width
  setSize(7 * whiteKeyWidth, whiteKeyHeight);
}

MusicalKeyboardComponent::~MusicalKeyboardComponent() {
}

void MusicalKeyboardComponent::setRootNote(int rootNote) {
  this->rootNote = rootNote % 12;
  repaint();
}

void MusicalKeyboardComponent::setActiveIntervals(const std::vector<int>& intervals) {
  // Reset all intervals
  for (int i = 0; i < 12; ++i) {
    activeIntervals[i] = false;
  }
  
  // Set active intervals
  for (int interval : intervals) {
    if (interval >= 0 && interval < 12) {
      activeIntervals[interval] = true;
    }
  }
  
  // Root (interval 0) is always on
  activeIntervals[0] = true;
  
  repaint();
}

std::vector<int> MusicalKeyboardComponent::getActiveIntervals() const {
  std::vector<int> intervals;
  for (int i = 0; i < 12; ++i) {
    if (activeIntervals[i]) {
      intervals.push_back(i);
    }
  }
  return intervals;
}

void MusicalKeyboardComponent::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff1a1a1a));

  // Draw white keys first (in order: C, D, E, F, G, A, B)
  int whiteKeyIntervals[] = {0, 2, 4, 5, 7, 9, 11};
  int whiteKeyIndex = 0;
  
  for (int interval : whiteKeyIntervals) {
    auto keyBounds = getKeyBounds(interval);
    
    // Determine color based on whether this interval is active
    if (activeIntervals[interval]) {
      g.setColour(juce::Colours::lightblue.withAlpha(0.6f));
      g.fillRect(keyBounds);
    } else {
      g.setColour(juce::Colours::white);
      g.fillRect(keyBounds);
    }
    
    // Draw key border
    g.setColour(juce::Colours::grey.darker(0.3f));
    g.drawRect(keyBounds, 1);
    
    // Draw note name
    int midiNote = getNoteFromInterval(interval);
    juce::String noteName = MidiNoteUtilities::getMidiNoteName(midiNote);
    g.setColour(activeIntervals[interval] ? juce::Colours::darkblue : juce::Colours::black);
    g.setFont(11.0f);
    g.drawText(noteName, keyBounds, juce::Justification::centred);
  }
  
  // Draw black keys on top (in order: C#, D#, F#, G#, A#)
  int blackKeyIntervals[] = {1, 3, 6, 8, 10};
  
  for (int interval : blackKeyIntervals) {
    auto keyBounds = getKeyBounds(interval);
    
    // Determine color based on whether this interval is active
    if (activeIntervals[interval]) {
      g.setColour(juce::Colours::lightblue.withAlpha(0.8f));
      g.fillRect(keyBounds);
    } else {
      g.setColour(juce::Colours::darkgrey);
      g.fillRect(keyBounds);
    }
    
    // Draw key border
    g.setColour(juce::Colours::black);
    g.drawRect(keyBounds, 1);
    
    // Draw note name
    int midiNote = getNoteFromInterval(interval);
    juce::String noteName = MidiNoteUtilities::getMidiNoteName(midiNote);
    g.setColour(juce::Colours::white);
    g.setFont(9.0f);
    g.drawText(noteName, keyBounds, juce::Justification::centred);
  }
}

void MusicalKeyboardComponent::resized() {
  // Component size is fixed based on number of keys
  // But we can adjust if needed
}

void MusicalKeyboardComponent::mouseDown(const juce::MouseEvent& e) {
  int interval = getKeyAtPosition(e.x, e.y);
  
  if (interval >= 0 && interval < 12) {
    // Root (interval 0) cannot be toggled
    if (interval == 0)
      return;
    
    // Toggle the interval
    activeIntervals[interval] = !activeIntervals[interval];
    repaint();
    
    // Notify callback
    if (onIntervalToggled) {
      onIntervalToggled(interval);
    }
  }
}

int MusicalKeyboardComponent::getKeyAtPosition(int x, int y) const {
  // Check black keys first (they're on top)
  for (int interval = 0; interval < 12; ++interval) {
    if (isBlackKey(interval)) {
      auto bounds = getKeyBounds(interval);
      if (bounds.contains(x, y)) {
        return interval;
      }
    }
  }
  
  // Then check white keys
  for (int interval = 0; interval < 12; ++interval) {
    if (!isBlackKey(interval)) {
      auto bounds = getKeyBounds(interval);
      if (bounds.contains(x, y)) {
        return interval;
      }
    }
  }
  
  return -1; // No key at this position
}

bool MusicalKeyboardComponent::isBlackKey(int note) const {
  // Black keys are at intervals: 1 (C#), 3 (D#), 6 (F#), 8 (G#), 10 (A#)
  return (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);
}

juce::Rectangle<int> MusicalKeyboardComponent::getKeyBounds(int interval) const {
  // Map intervals to keyboard positions
  // White keys: 0(C)=0, 2(D)=1, 4(E)=2, 5(F)=3, 7(G)=4, 9(A)=5, 11(B)=6
  // Black keys: 1(C#)=between 0-1, 3(D#)=between 1-2, 6(F#)=between 3-4, 8(G#)=between 4-5, 10(A#)=between 5-6
  
  if (isBlackKey(interval)) {
    // Black key positions: which white key index they're positioned after
    int blackKeyWhiteKeyIndex[] = {-1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1};
    // Index:                     0   1  2   3  4   5   6  7   8  9  10 11
    //                            C  C#  D  D#  E   F  F#  G  G#  A  A#  B
    
    int whiteKeyIndex = blackKeyWhiteKeyIndex[interval];
    if (whiteKeyIndex < 0) {
      return juce::Rectangle<int>(0, 0, blackKeyWidth, blackKeyHeight);
    }
    
    // Position black key between white keys (offset to the right of the white key)
    int x = whiteKeyIndex * whiteKeyWidth + whiteKeyWidth - blackKeyWidth / 2;
    return juce::Rectangle<int>(x, 0, blackKeyWidth, blackKeyHeight);
  } else {
    // White key positions
    int whiteKeyPositions[] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    // Index:                 0   1  2   3  4  5   6  7   8  9  10 11
    //                        C  C#  D  D#  E  F  F#  G  G#  A  A#  B
    
    int whiteKeyIndex = whiteKeyPositions[interval];
    if (whiteKeyIndex < 0) {
      return juce::Rectangle<int>(0, 0, whiteKeyWidth, whiteKeyHeight);
    }
    
    int x = whiteKeyIndex * whiteKeyWidth;
    return juce::Rectangle<int>(x, 0, whiteKeyWidth, whiteKeyHeight);
  }
}

int MusicalKeyboardComponent::getNoteFromInterval(int interval) const {
  // Return MIDI note for this interval relative to root
  // For display purposes, we'll use C4 (60) as the base
  int baseNote = 60; // C4
  return baseNote + interval;
}
