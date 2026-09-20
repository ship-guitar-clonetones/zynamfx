// Anchored floating panel (the save popover, preset browser, account menu,
// tile menus…). Lives in the root's overlay layer so it paints above
// everything, is positioned relative to an anchor component, and dismisses
// on a press outside itself/its anchor or on Escape (useDismissable.ts).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace t3k::ui {

// Implemented by PluginRoot: the full-size, click-through layer popovers
// are parented to.
class OverlayHost {
public:
  virtual ~OverlayHost() = default;
  virtual juce::Component& overlayLayer() = 0;
};

class Popover : public juce::Component {
public:
  enum class Align { left, right };
  enum class Placement { below, above };

  Popover();
  ~Popover() override;

  // Show below (or above) `anchor`, `gap` px from its edge, with the panel's
  // left (or right) edge offset `inset` px from the anchor's matching edge.
  // The panel must already have its size set.
  void open(juce::Component& anchor, Align align, int gap, int inset = 0,
            Placement placement = Placement::below);
  // Show with the panel's top-left at a point given in `context`'s local
  // coordinates (a right-click's position): context menus. Any press outside
  // the panel dismisses; the panel is kept inside the overlay.
  void openAt(juce::Component& context, juce::Point<int> point);
  void close();
  // close() + onDismiss (a row pick, or the owner backing out for the user).
  void dismiss();
  bool isOpen() const { return getParentComponent() != nullptr; }

  // Called after the panel is removed (outside press, Escape, or close()).
  std::function<void()> onDismiss;
  // Ignore non-primary presses (panels toggled by right-click).
  bool primaryOnly = false;
  // A press on the anchor dismisses too (the anchor is a control, not the
  // panel's toggle).
  bool dismissOnAnchorPress = false;

  bool keyPressed(const juce::KeyPress& key) override;

  // Every panel draws a 1px border; CSS padding starts inside it.
  static constexpr int kBorder = 1;
  juce::Rectangle<int> contentBounds() const { return getLocalBounds().reduced(kBorder); }

protected:
  // Reposition against the anchor (call after a size change while open).
  void reposition();

private:
  // Global mouse listener: a press anywhere outside the panel and its
  // anchor dismisses. Separate object because Component is itself a
  // MouseListener for its own events.
  class OutsidePressWatcher : public juce::MouseListener {
  public:
    explicit OutsidePressWatcher(Popover& owner) : owner_(owner) {}
    void mouseDown(const juce::MouseEvent& e) override { owner_.outsidePress(e); }

  private:
    Popover& owner_;
  };

  void outsidePress(const juce::MouseEvent& e);

  OutsidePressWatcher watcher_{*this};
  juce::Component::SafePointer<juce::Component> anchor_;
  Align align_ = Align::left;
  Placement placement_ = Placement::below;
  int gap_ = 0, inset_ = 0;
};

}  // namespace t3k::ui
