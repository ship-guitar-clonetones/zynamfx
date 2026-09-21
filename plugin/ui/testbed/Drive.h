// Interaction helpers for scenario drives, mirroring the Playwright steps in
// ui/local/screenshots/scenarios.mjs. The web drives locate controls by
// their `data-help` text; native controls carry the same copy as their
// Component help text, so the same prefixes work here.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "views/PluginRoot.h"

namespace t3k::ui::testbed::drive {

// Depth-first search for the first descendant matching `pred`.
juce::Component* find(juce::Component& root,
                      const std::function<bool(juce::Component&)>& pred);

// `[data-help^="prefix"]`
juce::Component* byHelpPrefix(juce::Component& root, const juce::String& prefix);
// `getByRole('button', { name })`
juce::Button* buttonNamed(juce::Component& root, const juce::String& name);
// `input[placeholder="…"]`
juce::TextEditor* inputWithPlaceholder(juce::Component& root, const juce::String& placeholder);

// Playwright's click moves the pointer onto the target first, so the hint
// bar shows the control's help in the shot; hover() reproduces that half.
void hover(PluginRoot& root, juce::Component& target);
// Move the pointer to a point inside the component first, for controls whose
// hint depends on where the pointer is (a meter's clip LED).
void hoverAt(PluginRoot& root, juce::Component& target, juce::Point<int> local);
// Point at whatever is under a root-space position.
void hoverPoint(PluginRoot& root, juce::Point<int> rootPos);
// The pointer is no longer over any hinted control (a modal came up under it).
void unhover(PluginRoot& root);
// Playwright scrolls an off-screen target the shortest distance into its
// scroller's view before pointing at it; the same for every enclosing
// juce::Viewport.
void scrollIntoView(juce::Component& target);
// Synthetic press + release at the component's centre (right = context click),
// after scrollIntoView() and a pointer move onto the target; if the target
// disappears, whatever is now under the pointer gets the hover.
void click(PluginRoot& root, juce::Component& target, bool right = false);
void clickByHelp(PluginRoot& root, const juce::String& helpPrefix, bool right = false);
// Type into a field (replaces its contents, fires change callbacks).
void fill(PluginRoot& root, const juce::String& placeholder, const juce::String& text);
// fill() then Return.
void submit(PluginRoot& root, const juce::String& placeholder, const juce::String& text);

// Pump the message loop.
void wait(int ms);

// The Settings takeover via the account menu, and its System tab.
void openSettings(PluginRoot& root);
void openSystemSettings(PluginRoot& root);
// Scroll the Settings page so the section headed `label` sits at the top.
void scrollSettingsTo(PluginRoot& root, const juce::String& label);

}  // namespace t3k::ui::testbed::drive
