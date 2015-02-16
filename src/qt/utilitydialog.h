// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UTILITYDIALOG_H
#define UTILITYDIALOG_H

#include <QDialog>
#include <QObject>

class AnoncoinGUI;
class ClientModel;

namespace Ui {
    class AboutDialog;
    class HelpMessageDialog;
}

/** "About" dialog box */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent);
    ~AboutDialog();

    void setModel(ClientModel *model);

private:
    Ui::AboutDialog *ui;

private slots:
    void on_buttonBox_accepted();
};

/** "Help message" dialog box */
class HelpMessageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpMessageDialog(QWidget *parent);
    ~HelpMessageDialog();

    void printToConsole();
    void showOrPrint();

private:
    Ui::HelpMessageDialog *ui;
    QString text;

private slots:
    void on_okButton_accepted();
};


/** "Shutdown" window */
class ShutdownWindow : public QWidget
{
    Q_OBJECT

public:
    ShutdownWindow(QWidget *parent=0, Qt::WindowFlags f=0);
    static void showShutdownWindow(AnoncoinGUI *window);

protected:
    void closeEvent(QCloseEvent *event);
};

#endif // UTILITYDIALOG_H
