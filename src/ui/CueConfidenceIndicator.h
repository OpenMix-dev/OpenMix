#pragma once

#include <QColor>
#include <QWidget>

namespace StageBlend {

class Cue;
class CueList;
class CueValidator;
struct ValidationResult;

enum class ConfidenceLevel {
    Good,    // validated, no issues
    Warning, // warnings present
    Error,   // errors present
    Unknown  // not validated
};

class CueConfidenceIndicator : public QWidget {
    Q_OBJECT

  public:
    explicit CueConfidenceIndicator(QWidget* parent = nullptr);

    // set the cue & cue list for validation
    void setCue(const Cue* cue, const CueList* cueList);
    void clearCue();

    // set the validator to use
    void setValidator(CueValidator* validator);

    // get current confidence level
    ConfidenceLevel confidenceLevel() const { return m_level; }

    // manually trigger validation
    void validate();

    // get tooltip text describing issues
    QString tooltipText() const;

    // sizing
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // colors for each level
    static QColor colorForLevel(ConfidenceLevel level);
    static QString iconForLevel(ConfidenceLevel level);

  signals:
    void clicked();
    void validationCompleted(ConfidenceLevel level);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    void updateFromValidation(const ValidationResult& result);

    const Cue* m_cue = nullptr;
    const CueList* m_cueList = nullptr;
    CueValidator* m_validator = nullptr;

    ConfidenceLevel m_level = ConfidenceLevel::Unknown;
    QString m_tooltipText;
    bool m_hovered = false;
};

} // namespace StageBlend
