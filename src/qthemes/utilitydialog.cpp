// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilitydialog.h"
// Anoncoin-config.h has been loaded...
#include "ui_aboutdialog.h"
#include "ui_helpmessagedialog.h"

#include "anoncoingui.h"
#include "clientmodel.h"
#include "guiutil.h"

#include "clientversion.h"
#include "init.h"

#include <stdio.h>

#include <QCloseEvent>
#include <QLabel>
//  #include <QRegExp>
#include <QVBoxLayout>

/** "About" dialog box */
AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Set current copyright year for all the crypos we're involved in mining and algos from...
    ui->copyrightLabel->setText(tr("Copyright") + QString(" &copy; 2013-%1 ").arg(COPYRIGHT_YEAR) + tr("The Anoncoin Core developers")
        + QString("<br>") + tr("Copyright") + QString(" &copy; 2009-%1 ").arg(2015) + tr("The Bitcoin Core developers")
        + QString("<br>") + tr("Copyright") + QString(" &copy; 2011-%1 ").arg(2014) + tr("The Litecoin Core developers") );
}

void AboutDialog::setModel(ClientModel *model)
{
    if(model)
    {
        QString version = model->formatFullVersion();
        /* On x86 add a bit specifier to the version so that users can distinguish between
         * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambigious.
         */
#if defined(__x86_64__)
        version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
        version += " " + tr("(%1-bit)").arg(32);
#endif
        ui->versionLabel->setText(version);
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_buttonBox_accepted()
{
    close();
}

/** "Help message" dialog box */
HelpMessageDialog::HelpMessageDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HelpMessageDialog)
{
    ui->setupUi(this);
    GUIUtil::restoreWindowGeometry("nHelpMessageDialogWindow", this->size(), this);

    QString version = tr("Anoncoin Core") + " " + tr("version") + " " + QString::fromStdString(FormatFullVersion());
    /* On x86 add a bit specifier to the version so that users can distinguish between
     * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambigious.
     */
#if defined(__x86_64__)
    version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
    version += " " + tr("(%1-bit)").arg(32);
#endif
    bool about = false;                 // ToDo: Finish combining about and help first...
    if (about)
    {
        setWindowTitle(tr("About Anoncoin Core"));

        /// HTML-format the license message from the core
        // QString licenseInfo = QString::fromStdString(LicenseInfo());
        QString licenseInfo = "";
        QString licenseInfoHTML = licenseInfo;
        // Make URLs clickable
        // QRegExp uri("<(.*)>", Qt::CaseSensitive, QRegExp::RegExp2);
        // uri.setMinimal(true); // use non-greedy matching
        // licenseInfoHTML.replace(uri, "<a href=\"\\1\">\\1</a>");
        // Replace newlines with HTML breaks
        // licenseInfoHTML.replace("\n\n", "<br><br>");

        ui->helpMessageLabel->setTextFormat(Qt::RichText);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        text = version + "\n" + licenseInfo;
        ui->helpMessageLabel->setText(version + "<br><br>" + licenseInfoHTML);
        ui->helpMessageLabel->setWordWrap(true);
    } else {
        setWindowTitle(tr("Command-line options"));
        QString header = tr("Usage:") + "\n" +
            "  anoncoin-qt [" + tr("command-line options") + "]                     " + "\n";

        QString coreOptions = QString::fromStdString(HelpMessage(HMM_ANONCOIN_QT));

        QString uiOptions = tr("UI options") + ":\n" +
            "  -choosedatadir            " + tr("Choose data directory on startup (default: 0)") + "\n" +
            "  -lang=<lang>              " + tr("Set language, for example \"de_DE\" (default: system locale)") + "\n" +
            "  -min                      " + tr("Start minimized") + "\n" +
            "  -rootcertificates=<file>  " + tr("Set SSL root certificates for payment request (default: -system-)") + "\n" +
            "  -splash                   " + tr("Show splash screen on startup (default: 1)");

        QFont aFont = GUIUtil::anoncoinAddressFont();
        aFont.setPointSize(10);
        ui->helpMessageLabel->setFont(aFont);
        text = version + "\n" + header + "\n" + coreOptions + "\n" + uiOptions;
        ui->helpMessageLabel->setText(text);    // Set help message text
    }
}

HelpMessageDialog::~HelpMessageDialog()
{
    GUIUtil::saveWindowGeometry("nHelpMessageDialogWindow", this);
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    fprintf(stdout, "%s\n", qPrintable(text));
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // On other operating systems, print help text to console
    printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    close();
}


/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget *parent, Qt::WindowFlags f):
    QWidget(parent, f)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(
        tr("Anoncoin Core is shutting down...") + "<br /><br />" +
        tr("Do not shut down the computer until this window disappears.")));
    setLayout(layout);
}

void ShutdownWindow::showShutdownWindow(AnoncoinGUI *window)
{
    if (!window)
        return;

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new ShutdownWindow();
    // We don't hold a direct pointer to the shutdown window after creation, so use
    // Qt::WA_DeleteOnClose to make sure that the window will be deleted eventually.
    shutdownWindow->setAttribute(Qt::WA_DeleteOnClose);
    shutdownWindow->setWindowTitle(window->windowTitle());

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
}

void ShutdownWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
}
