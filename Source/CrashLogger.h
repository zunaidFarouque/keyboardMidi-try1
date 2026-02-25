#pragma once

#include <atomic>

namespace juce {
class String;
}

// Utility for global crash logging controlled by a Debug mode flag.
class CrashLogger {
public:
  // Install global handlers (std::terminate and OS-level where available).
  // Safe to call multiple times; handlers will only be installed once.
  static void installGlobalHandlers();

  // Enable/disable debug mode for crash logging at runtime.
  static void setDebugModeEnabled(bool enabled);

  // Add a lightweight breadcrumb describing a recent high-level action.
  // Safe to call from normal code paths; at crash time we dump the last N.
  static void addBreadcrumb(const juce::String &message);

  // Update UI/context snapshot for better crash diagnostics.
  // This is best-effort and should be called from the message thread.
  static void setUiContext(int mainTabIndex, const juce::String &mainTabName,
                           bool studioMode, bool midiMode, int activeLayer,
                           int zonesSelectedIndex, int touchpadSelectedRow);

  // Test-only helper: invoke the logging path without triggering a real crash.
  // This writes to the same crash log file that would be used during a crash.
  static void writeCrashLogForTest(const juce::String &context);

private:
  static void writeCrashLog(const juce::String &context);
};

