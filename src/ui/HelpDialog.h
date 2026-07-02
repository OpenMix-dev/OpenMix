#pragma once

#include <QDialog>
#include <QString>

namespace OpenMix {

// Simple in-app help viewer: renders a title + HTML body in a scrollable browser.
// Content is supplied by the caller (Quick Start, Feature Guide).
class HelpDialog : public QDialog {
    Q_OBJECT

  public:
    HelpDialog(const QString& title, const QString& html, QWidget* parent = nullptr);
};

} // namespace OpenMix
