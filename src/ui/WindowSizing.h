#pragma once

class QWidget;

namespace OpenMix {

// Sizes top-level windows so their content fits without horizontal scroll bars.
namespace WindowSizing {

// Widens the window (never shrinks, capped to the screen) until no descendant
// scroll surface needs a horizontal scroll bar at the current content.
void widenToContents(QWidget* window);

// Installs a filter that runs widenToContents after every show of the window,
// once layout has settled. One call in the constructor covers the lifetime.
void widenOnShow(QWidget* window);

} // namespace WindowSizing

} // namespace OpenMix
