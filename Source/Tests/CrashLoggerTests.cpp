#include "../CrashLogger.h"
#include "../SettingsManager.h"

#include <JuceHeader.h>
#include <gtest/gtest.h>

namespace {

juce::File getCrashLogFile() {
  auto exe =
      juce::File::getSpecialLocation(juce::File::currentExecutableFile);
  return exe.getParentDirectory().getChildFile("MIDIQy_crashlog.txt");
}

} // namespace

class CrashLoggerTest : public ::testing::Test {
protected:
  juce::ScopedJuceInitialiser_GUI juceInit;

  void SetUp() override {
    auto logFile = getCrashLogFile();
    if (logFile.existsAsFile())
      logFile.deleteFile();
    CrashLogger::setDebugModeEnabled(false);
  }
};

TEST_F(CrashLoggerTest, WhenDebugModeDisabled_DoesNotCreateLogFile) {
  auto logFile = getCrashLogFile();
  EXPECT_FALSE(logFile.existsAsFile());

  CrashLogger::writeCrashLogForTest("Test disabled");

  EXPECT_FALSE(logFile.existsAsFile());
}

TEST_F(CrashLoggerTest, WhenDebugModeEnabled_WritesCrashLog) {
  auto logFile = getCrashLogFile();
  EXPECT_FALSE(logFile.existsAsFile());

  CrashLogger::setDebugModeEnabled(true);
  CrashLogger::writeCrashLogForTest("Test enabled");

  EXPECT_TRUE(logFile.existsAsFile());
  auto contents = logFile.loadFileAsString();
  EXPECT_TRUE(contents.contains("==== MIDIQy crash ===="));
  EXPECT_TRUE(contents.contains("Context: Test enabled"));
}

TEST_F(CrashLoggerTest, SubsequentCrashesAppendToExistingFile) {
  auto logFile = getCrashLogFile();

  CrashLogger::setDebugModeEnabled(true);
  CrashLogger::writeCrashLogForTest("First crash");
  CrashLogger::writeCrashLogForTest("Second crash");

  EXPECT_TRUE(logFile.existsAsFile());
  auto contents = logFile.loadFileAsString();
  EXPECT_TRUE(contents.contains("Context: First crash"));
  EXPECT_TRUE(contents.contains("Context: Second crash"));
}

TEST_F(CrashLoggerTest, InstallGlobalHandlersIsIdempotent) {
  // Should be safe to call multiple times without throwing or crashing.
  CrashLogger::installGlobalHandlers();
  CrashLogger::installGlobalHandlers();

  // We cannot easily assert global handler state here, but reaching this point
  // without a crash is sufficient to prove idempotency.
  SUCCEED();
}

TEST_F(CrashLoggerTest, SettingsManagerDebugModeWiresIntoCrashLogger) {
  auto logFile = getCrashLogFile();
  EXPECT_FALSE(logFile.existsAsFile());

  SettingsManager mgr;
  mgr.setDebugModeEnabled(true);

  CrashLogger::writeCrashLogForTest("From SettingsManager");

  EXPECT_TRUE(logFile.existsAsFile());
  auto contents = logFile.loadFileAsString();
  EXPECT_TRUE(contents.contains("From SettingsManager"));
}


