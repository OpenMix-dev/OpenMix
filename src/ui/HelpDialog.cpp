#include "HelpDialog.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace OpenMix {

HelpDialog::HelpDialog(const QString& title, const QString& html, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    resize(640, 620);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QTextBrowser* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setHtml(html);
    layout->addWidget(browser);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace OpenMix
