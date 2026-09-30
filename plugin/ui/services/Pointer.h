// Whether the pointer driving the UI can hover. What a mouse reveals on
// hover (the tile chrome, the branch dots) stays up for a finger, and the
// hint bar says "tap" where its copy says "click". iOS and Android are touch
// from the start and stay so; a desktop build decides at run time, since a
// Windows or Linux tablet is a desktop build with a finger on it and a
// convertible swaps between the two: it seeds from the hardware where the
// OS tells (Windows in slate mode) and then follows the last press or move,
// so the first touch turns the affordances on and the next mouse move turns
// them off again.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui {

class Pointer {
public:
  struct Listener {
    virtual ~Listener() = default;
    virtual void pointerChanged() = 0;
  };

  Pointer();

  // No hover: touch affordances on.
  bool coarse() const { return coarse_; }

  // PointerTracker, on every press and move.
  void sawInput(bool touch);

  void addListener(Listener* l) { listeners_.add(l); }
  void removeListener(Listener* l) { listeners_.remove(l); }

private:
  bool coarse_;
  juce::ListenerList<Listener> listeners_;
};

// Installed on the root as a recursive mouse listener (beside HintTracker):
// feeds the source of every press and move to the Pointer.
class PointerTracker : public juce::MouseListener {
public:
  PointerTracker(Pointer& pointer, juce::Component& root);
  ~PointerTracker() override;

  void mouseMove(const juce::MouseEvent& e) override { pointer_.sawInput(e.source.isTouch()); }
  void mouseDown(const juce::MouseEvent& e) override { pointer_.sawInput(e.source.isTouch()); }

private:
  Pointer& pointer_;
  juce::Component& root_;
};

}  // namespace t3k::ui
