#pragma once

#include <QDialog>

namespace OpenMix {

class Application;

// Read-only report: each referenced input channel, its assigned actor, and the
// cues that use it. Computed on open from the current show.
class ChannelUtilisationDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ChannelUtilisationDialog(Application* app, QWidget* parent = nullptr);
};

} // namespace OpenMix
