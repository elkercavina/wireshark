/* funnel_text_dialog.cpp
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "funnel_text_dialog.h"
#include <ui_funnel_text_dialog.h>

#include <QPushButton>
#include <QRegExp>
#include <QTextCharFormat>
#include <QTextCursor>

#include <ui/qt/utils/qt_ui_utils.h>
#include "wireshark_application.h"

// To do:
// - Add "Find next" to highlighting.
// - Add rich text support?
// - Zoom text?

static QHash<QObject *, funnel_bt_t*> text_button_to_funnel_button_;

FunnelTextDialog::FunnelTextDialog(const QString &title) :
    GeometryStateDialog(NULL),
    ui(new Ui::FunnelTextDialog),
    close_cb_(NULL),
    close_cb_data_(NULL)
{
    ui->setupUi(this);
    if (!title.isEmpty()) {
        loadGeometry(0, 0, QString("Funnel %1").arg(title));
    }
    setWindowTitle(wsApp->windowTitleString(title));

    funnel_text_window_.funnel_text_dialog = this;

    ui->textEdit->setFont(wsApp->monospaceFont());
    ui->textEdit->setReadOnly(true);
    ui->textEdit->setAcceptRichText(false);
}

FunnelTextDialog::~FunnelTextDialog()
{
    delete ui;
}

void FunnelTextDialog::reject()
{
    QDialog::reject();

    if (close_cb_) {
        close_cb_(close_cb_data_);
    }

    QHash<QObject *, funnel_bt_t*>::iterator i;
    for (i = text_button_to_funnel_button_.begin(); i != text_button_to_funnel_button_.end(); ++i) {
        funnel_bt_t *funnel_button = i.value();
        if (funnel_button->free_data_fcn) {
            funnel_button->free_data_fcn(funnel_button->data);
        }
        if (funnel_button->free_fcn) {
            funnel_button->free_fcn(funnel_button);
        }
    }
    text_button_to_funnel_button_.clear();

    disconnect();
    deleteLater();
}

struct _funnel_text_window_t *FunnelTextDialog::textWindowNew(const QString title)
{
    FunnelTextDialog *ftd = new FunnelTextDialog(title);
    ftd->show();
    return &ftd->funnel_text_window_;
}

void FunnelTextDialog::setText(const QString text)
{
    ui->textEdit->setText(text);
}

void FunnelTextDialog::appendText(const QString text)
{
    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(text);
}

void FunnelTextDialog::prependText(const QString text)
{
    ui->textEdit->moveCursor(QTextCursor::Start);
    ui->textEdit->insertPlainText(text);
}

void FunnelTextDialog::clearText()
{
    ui->textEdit->clear();
}

const char *FunnelTextDialog::getText()
{
    return qstring_strdup(ui->textEdit->toPlainText());
}

void FunnelTextDialog::setCloseCallback(text_win_close_cb_t close_cb, void *close_cb_data)
{
    close_cb_ = close_cb;
    close_cb_data_ = close_cb_data;
}

void FunnelTextDialog::setTextEditable(gboolean editable)
{
    ui->textEdit->setReadOnly(!editable);
}

void FunnelTextDialog::addButton(funnel_bt_t *funnel_button, QString label)
{
    // Use "&&" to get a real ampersand in the button.
    label.replace('&', "&&");

    QPushButton *button = new QPushButton(label);
    ui->buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    text_button_to_funnel_button_[button] = funnel_button;
    connect(button, &QPushButton::clicked, this, &FunnelTextDialog::buttonClicked);
}

void FunnelTextDialog::buttonClicked()
{
    if (text_button_to_funnel_button_.contains(sender())) {
        funnel_bt_t *funnel_button = text_button_to_funnel_button_[sender()];
        if (funnel_button) {
            funnel_button->func(&funnel_text_window_, funnel_button->data);
        }
    }
}

void FunnelTextDialog::on_findLineEdit_textChanged(const QString &pattern)
{
    QRegExp re(pattern, Qt::CaseInsensitive);
    QTextCharFormat plain_fmt, highlight_fmt;
    highlight_fmt.setBackground(Qt::yellow);
    QTextCursor csr(ui->textEdit->document());
    int position = csr.position();

    setUpdatesEnabled(false);

    // Reset highlighting
    csr.movePosition(QTextCursor::Start);
    csr.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    csr.setCharFormat(plain_fmt);

    // Apply new highlighting
    if (!pattern.isEmpty()) {
        int match_pos = 0;
        while ((match_pos = re.indexIn(ui->textEdit->toPlainText(), match_pos)) > -1) {
            csr.setPosition(match_pos, QTextCursor::MoveAnchor);
            csr.setPosition(match_pos + re.matchedLength(), QTextCursor::KeepAnchor);
            csr.setCharFormat(highlight_fmt);
            match_pos += re.matchedLength();
        }
    }

    // Restore cursor and anchor
    csr.setPosition(position, QTextCursor::MoveAnchor);
    setUpdatesEnabled(true);
}

struct _funnel_text_window_t* text_window_new(const char* title)
{
    return FunnelTextDialog::textWindowNew(title);
}

void text_window_set_text(funnel_text_window_t *ftw, const char* text)
{
    if (ftw) {
        ftw->funnel_text_dialog->setText(text);
    }
}

void text_window_append(funnel_text_window_t* ftw, const char* text)
{
    if (ftw) {
        ftw->funnel_text_dialog->appendText(text);
    }
}

void text_window_prepend(funnel_text_window_t *ftw, const char* text)
{
    if (ftw) {
        ftw->funnel_text_dialog->prependText(text);
    }
}

void text_window_clear(funnel_text_window_t* ftw)
{
    if (ftw) {
        ftw->funnel_text_dialog->clearText();
    }
}

const char *text_window_get_text(funnel_text_window_t *ftw)
{
    if (ftw) {
        return ftw->funnel_text_dialog->getText();
    }
    return NULL;
}

void text_window_set_close_cb(funnel_text_window_t *ftw, text_win_close_cb_t close_cb, void *close_cb_data)
{
    if (ftw) {
        ftw->funnel_text_dialog->setCloseCallback(close_cb, close_cb_data);
    }
}

void text_window_set_editable(funnel_text_window_t *ftw, gboolean editable)
{
    if (ftw) {
        ftw->funnel_text_dialog->setTextEditable(editable);
    }
}

void text_window_destroy(funnel_text_window_t *ftw)
{
    if (ftw) {
        ftw->funnel_text_dialog->close();
    }
}

void text_window_add_button(funnel_text_window_t *ftw, funnel_bt_t *funnel_button, const char *label)
{
    if (ftw) {
        ftw->funnel_text_dialog->addButton(funnel_button, label);
    }
}
