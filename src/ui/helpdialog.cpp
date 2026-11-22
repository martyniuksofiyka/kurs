#include "helpdialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Допомога"));
    resize(760, 640);

    auto *browser = new QTextBrowser;
    browser->setOpenExternalLinks(true);
    QFile f(":/help.html");
    if (f.open(QIODevice::ReadOnly))
        browser->setHtml(QString::fromUtf8(f.readAll()));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(browser);
    lay->addWidget(buttons);
}
