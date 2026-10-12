#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Brand-dark theme for the few stock JUCE surfaces in the standalone app:
 * the title-bar Options menu, the Save/Load-state file dialogs, and any JUCE
 * AlertWindow. The plugin UI itself (including its audio settings page)
 * paints its own chrome and never consults a LookAndFeel.
 *
 * Installed as the process default by the editor via SharedResourcePointer so
 * multiple plugin instances share one object; the destructor un-installs it
 * when the last instance goes away.
 */
class DarkLookAndFeel : public juce::LookAndFeel_V4 {
public:
  DarkLookAndFeel()
      : juce::LookAndFeel_V4({
            juce::Colour(0xff101115),  // windowBackground
            juce::Colour(0xff232730),  // widgetBackground
            juce::Colour(0xff191c23),  // menuBackground
            juce::Colour(0xff4c5667),  // outline
            juce::Colour(0xffffffff),  // defaultText
            juce::Colour(0xff2c2c2e),  // defaultFill
            juce::Colour(0xff101115),  // highlightedText
            juce::Colour(0xff8e98ff),  // highlightedFill (brand yellow)
            juce::Colour(0xffffffff),  // menuText
        }) {}

  ~DarkLookAndFeel() override {
    // Never leave a dangling default if we're the last instance being torn down.
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == this)
      juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
  }
};
