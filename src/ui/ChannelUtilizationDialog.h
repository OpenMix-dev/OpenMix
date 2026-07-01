#pragma once

#include <QDialog>

namespace OpenMix {

class Application;

// Read-only report: each referenced input channel, its assigned actor, and the
// cues that use it. Computed on open from the current show.
class ChannelUtilizationDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ChannelUtilizationDialog(Application* app, QWidget* parent = nullptr);
};

} // namespace OpenMix
