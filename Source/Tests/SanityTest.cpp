#include "../MappingTypes.h"
#include "../ScaleUtilities.h" // Example of reaching into Core
#include <gtest/gtest.h>


// 1. Basic Math Check (Infrastructure Test)
TEST(SanityCheck, MathWorks) { EXPECT_EQ(1 + 1, 2); }

// 2. Core Linkage Check (Dependency Test)
TEST(SanityCheck, CanAccessCoreEnums) {
  // Just verify we can use types defined in OmniKey_Core
  ActionType t = ActionType::Note;
  EXPECT_EQ(t, ActionType::Note);
}

// 3. Logic Check (ScaleUtilities Test)
TEST(SanityCheck, ScaleLogicFunction) {
  // C Major (0, 2, 4, 5, 7, 9, 11)
  std::vector<int> majorScale = {0, 2, 4, 5, 7, 9, 11};
  int root = 60; // C4

  // Degree 0 should be 60
  EXPECT_EQ(ScaleUtilities::calculateMidiNote(root, majorScale, 0), 60);

  // Degree 1 should be 62 (D4)
  EXPECT_EQ(ScaleUtilities::calculateMidiNote(root, majorScale, 1), 62);
}
