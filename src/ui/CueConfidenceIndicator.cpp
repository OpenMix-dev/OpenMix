#include "CueConfidenceIndicator.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/CueValidator.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>

namespace OpenMix {

CueConfidenceIndicator::CueConfidenceIndicator(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void CueConfidenceIndicator::setCue(const Cue* cue, const CueList* cueList) {
    m_cue = cue;
    m_cueList = cueList;
    validate();
}

void CueConfidenceIndicator::clearCue() {
    m_cue = nullptr;
    m_cueList = nullptr;
    m_level = ConfidenceLevel::Unknown;
    m_tooltipText.clear();
    update();
}

void CueConfidenceIndicator::setValidator(CueValidator* validator) {
    m_validator = validator;
    if (m_cue) {
        validate();
    }
}

void CueConfidenceIndicator::validate() {
    if (!m_cue) {
        m_level = ConfidenceLevel::Unknown;
        m_tooltipText = tr("No cue selected");
        update();
        return;
    }

    if (!m_validator) {
        m_level = ConfidenceLevel::Unknown;
        m_tooltipText = tr("Validator not available");
        update();
        return;
    }

    ValidationResult result = m_validator->validate(*m_cue, m_cueList);
    updateFromValidation(result);
    emit validationCompleted(m_level);
}

void CueConfidenceIndicator::setConfidence(ConfidenceLevel level, const QString& tooltip) {
    m_level = level;
    m_tooltipText = tooltip;
    setToolTip(tooltip);
    update();
    emit validationCompleted(m_level);
}

QString CueConfidenceIndicator::tooltipText() const { return m_tooltipText; }

QSize CueConfidenceIndicator::sizeHint() const { return QSize(16, 16); }

QSize CueConfidenceIndicator::minimumSizeHint() const { return QSize(12, 12); }

QColor CueConfidenceIndicator::colorForLevel(ConfidenceLevel level) {
    switch (level) {
    case ConfidenceLevel::Good:
        return QColor(76, 175, 80); // green
    case ConfidenceLevel::Warning:
        return QColor(255, 193, 7); // amber
    case ConfidenceLevel::Error:
        return QColor(244, 67, 54); // red
    case ConfidenceLevel::Unknown:
        return QColor(158, 158, 158); // gray
    }
    return QColor(158, 158, 158);
}

QString CueConfidenceIndicator::iconForLevel(ConfidenceLevel level) {
    switch (level) {
    case ConfidenceLevel::Good:
        return QString::fromUtf8("\u2713"); // check mark
    case ConfidenceLevel::Warning:
        return QString::fromUtf8("\u26A0"); // warning sign
    case ConfidenceLevel::Error:
        return QString::fromUtf8("\u2717"); // x mark
    case ConfidenceLevel::Unknown:
        return QString::fromUtf8("?");
    }
    return "?";
}

void CueConfidenceIndicator::paintEvent([[maybe_unused]] QPaintEvent* event) {

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color = colorForLevel(m_level);

    // draw filled circle
    int size = std::min(width(), height());
    int margin = 2;
    QRect circleRect(margin, margin, size - margin * 2, size - margin * 2);

    if (m_hovered) {
        // slightly larger when hovered
        circleRect.adjust(-1, -1, 1, 1);
    }

    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(circleRect);

    // draw border
    QColor borderColor = color.darker(120);
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(circleRect);
}

void CueConfidenceIndicator::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();

        // show tooltip w/ details
        if (!m_tooltipText.isEmpty()) {
            QToolTip::showText(event->globalPosition().toPoint(), m_tooltipText, this);
        }
    }
    QWidget::mousePressEvent(event);
}

void CueConfidenceIndicator::enterEvent(QEnterEvent* event) {
    m_hovered = true;
    setToolTip(m_tooltipText);
    update();
    QWidget::enterEvent(event);
}

void CueConfidenceIndicator::leaveEvent(QEvent* event) {
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void CueConfidenceIndicator::updateFromValidation(const ValidationResult& result) {
    if (result.hasErrors()) {
        m_level = ConfidenceLevel::Error;
    } else if (result.hasWarnings()) {
        m_level = ConfidenceLevel::Warning;
    } else {
        m_level = ConfidenceLevel::Good;
    }

    // build tooltip text
    QStringList lines;
    if (m_cue) {
        lines.append(tr("Cue %1: %2")
                         .arg(m_cue->number(), 0, 'f', 1)
                         .arg(m_cue->name().isEmpty() ? tr("(unnamed)") : m_cue->name()));
    }

    if (result.issues.isEmpty()) {
        lines.append(tr("No issues detected"));
    } else {
        int errors = result.errorCount();
        int warnings = result.warningCount();

        if (errors > 0) {
            lines.append(tr("%n error(s)", "", errors));
        }
        if (warnings > 0) {
            lines.append(tr("%n warning(s)", "", warnings));
        }

        lines.append(QString()); // empty line

        // add first few issues
        int shown = 0;
        for (const ValidationIssue& issue : result.issues) {
            if (shown >= 5) {
                lines.append(tr("...and %n more", "", result.issues.size() - shown));
                break;
            }
            QString prefix = issue.isError() ? tr("[Error]") : tr("[Warning]");
            lines.append(QString("%1 %2").arg(prefix, issue.message));
            shown++;
        }
    }

    m_tooltipText = lines.join('\n');
    update();
}

} // namespace OpenMix
