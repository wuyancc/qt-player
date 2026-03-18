#include "urldialog.h"
#include "ui_urldialog.h"

UrlDialog::UrlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UrlDialog)
{
    ui->setupUi(this);
}

UrlDialog::~UrlDialog()
{
    delete ui;
}
QString UrlDialog::GetUrl()
{
    return ui->urlLineEdit->text();
}
