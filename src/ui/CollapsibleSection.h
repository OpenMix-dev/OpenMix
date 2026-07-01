#pragma once

#include <QWidget>

class QToolButton;
class QLayout;

namespace OpenMix {

// A titled section that collapses to a single chevron header row. Unlike a
// checkable QGroupBox it draws no frame when collapsed and never enables or
// disables its body as a side effect of toggling.
class CollapsibleSection : public QWidget {
    Q_OBJECT

  public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    // takes ownership of the layout and installs it on the body
    void setContentLayout(QLayout* layout);

    void setExpanded(bool expanded);
    [[nodiscard]] bool isExpanded() const;

  private:
    QToolButton* m_toggle;
    QWidget* m_body;
};

} // namespace OpenMix
