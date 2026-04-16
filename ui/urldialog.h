#ifndef URLDIALOG_H
#define URLDIALOG_H

#include <QDialog>

namespace Ui {
class UrlDialog;
}

class UrlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UrlDialog(QWidget *parent = 0);
    ~UrlDialog();
    QString GetUrl();
private:
    Ui::UrlDialog *ui;
};

#endif // URLDIALOG_H
