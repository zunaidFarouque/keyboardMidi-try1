#include "PercentageSplitLayout.h"

namespace PercentageSplitLayout {

void apply(int x, int y, int width, int height, bool vertically,
           float divider1Fraction, float divider2Fraction,
           juce::Component &panel0, juce::Component &bar1,
           juce::Component &panel2, juce::Component &bar2,
           juce::Component &panel4, int barSize, int min0, int min2, int min4) {
  int total = vertically ? width : height;
  if (total <= 0)
    return;

  float d1 = juce::jlimit(0.0f, 1.0f, divider1Fraction);
  float d2 = juce::jlimit(0.0f, 1.0f, divider2Fraction);
  if (d1 >= d2)
    d2 = juce::jmin(1.0f, d1 + 0.01f);

  int pos1 = juce::roundToInt(d1 * total);
  int pos2 = juce::roundToInt(d2 * total);

  pos1 = juce::jmax(min0, juce::jmin(pos1, pos2 - barSize - min2));
  pos2 = juce::jmax(pos1 + barSize + min2, juce::jmin(pos2, total - barSize - min4));

  if (vertically) {
    panel0.setBounds(x, y, pos1, height);
    bar1.setBounds(x + pos1, y, barSize, height);
    panel2.setBounds(x + pos1 + barSize, y, pos2 - pos1 - barSize, height);
    bar2.setBounds(x + pos2, y, barSize, height);
    panel4.setBounds(x + pos2 + barSize, y, width - pos2 - barSize, height);
  } else {
    panel0.setBounds(x, y, width, pos1);
    bar1.setBounds(x, y + pos1, width, barSize);
    panel2.setBounds(x, y + pos1 + barSize, width, pos2 - pos1 - barSize);
    bar2.setBounds(x, y + pos2, width, barSize);
    panel4.setBounds(x, y + pos2 + barSize, width, height - pos2 - barSize);
  }
}

} // namespace PercentageSplitLayout
