// Settings takeover (Settings.tsx): a full-window black page with a 480px
// column (title + close, then the form) that scrolls as one. The standalone
// app adds a tab bar between System Settings (audio device + MIDI hardware;
// first, because setup is the main abandon risk) and Plugin Settings; hosted
// builds have one page and no tab bar. Mounted only while open, so page
// state resets for free each time.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

#include "PluginSettingsPage.h"
#include "SystemSettingsPage.h"
#include "services/Services.h"
#include "widgets/DragScroller.h"
#include "widgets/form/FormItem.h"

namespace t3k::ui {

class SettingsScreen : public juce::Component, public FormHost {
public:
  enum class Tab { system, plugin };

  static constexpr int kMaxWidth = 480;
  static constexpr int kPadTop = 28, kPadX = 24, kPadBottom = 40;
  static constexpr int kHeaderGap = 20, kTabBarGap = 28;

  // `initialTab` only matters in the standalone app (hosted = plugin page).
  SettingsScreen(Services& services, Tab initialTab = Tab::system);
  ~SettingsScreen() override;

  std::function<void()> onClose;

  void setTab(Tab tab);

  // Scroll so the section headed `label` sits at the top of the page (under
  // the top padding), the way a reader lands on it; `centre` puts it mid-
  // viewport instead (scrollIntoViewIfNeeded).
  void scrollToHeading(const juce::String& label, bool centre = false);
  juce::Viewport& viewport() { return viewport_; }

  void paint(juce::Graphics& g) override;
  void resized() override;
  void itemHeightChanged() override { layoutColumn(); }

private:
  class Header;
  class TabBar;

  void layoutColumn();
  // The column's left edge and width at the current window width.
  juce::Rectangle<int> columnBounds() const;

  bool standalone_;
  Tab tab_;
  DragScroller viewport_{DragScroller::Axis::vertical};
  juce::Component content_;
  FormStack stack_;
  std::unique_ptr<Header> header_;
  std::unique_ptr<TabBar> tabBar_;
  std::unique_ptr<SystemSettingsPage> system_;
  PluginSettingsPage plugin_;
};

}  // namespace t3k::ui
