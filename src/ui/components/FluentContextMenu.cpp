#include "FluentContextMenu.h"

#include "FluentPopupMenu.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRect>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>

namespace {

enum class Cmd { Cut, Copy, Paste, SelectAll };

const wchar_t* cmdName(Cmd cmd) {
    switch (cmd) {
    case Cmd::Cut: return L"\u526a\u5207";
    case Cmd::Copy: return L"\u590d\u5236";
    case Cmd::Paste: return L"\u7c98\u8d34";
    case Cmd::SelectAll: return L"\u5168\u9009";
    }
    return L"";
}

QList<Cmd> availableCommands(bool hasSelection, bool readOnly,
    bool emptyContent) {
    QList<Cmd> cmds;
    const bool clipboardHasText = !QGuiApplication::clipboard()->text().isEmpty();
    if (hasSelection && !readOnly) cmds << Cmd::Cut;
    if (hasSelection) cmds << Cmd::Copy;
    if (!readOnly && clipboardHasText) cmds << Cmd::Paste;
    if (!emptyContent) cmds << Cmd::SelectAll;
    return cmds;
}

int execMenu(QWidget* parent, const QPoint& globalPos, const QList<Cmd>& cmds) {
    if (cmds.isEmpty()) return -1;
    QStringList items;
    for (Cmd cmd : cmds) {
        items << QString::fromWCharArray(cmdName(cmd));
    }
    return FluentPopupMenu::exec(parent, QRect(globalPos, QSize(0, 0)), items);
}

}  // namespace

namespace FluentContextMenu {

void popupForLineEdit(QLineEdit* edit, const QPoint& globalPos) {
    if (!edit) return;
    const QList<Cmd> cmds = availableCommands(edit->hasSelectedText(),
        edit->isReadOnly(), edit->text().isEmpty());
    const int selected = execMenu(edit, globalPos, cmds);
    if (selected < 0 || selected >= cmds.size()) return;
    switch (cmds.at(selected)) {
    case Cmd::Cut: edit->cut(); break;
    case Cmd::Copy: edit->copy(); break;
    case Cmd::Paste: edit->paste(); break;
    case Cmd::SelectAll: edit->selectAll(); break;
    }
}

void popupForPlainTextEdit(QPlainTextEdit* edit, const QPoint& globalPos) {
    if (!edit) return;
    const QList<Cmd> cmds = availableCommands(edit->textCursor().hasSelection(),
        edit->isReadOnly(), edit->document()->isEmpty());
    const int selected = execMenu(edit, globalPos, cmds);
    if (selected < 0 || selected >= cmds.size()) return;
    switch (cmds.at(selected)) {
    case Cmd::Cut: edit->cut(); break;
    case Cmd::Copy: edit->copy(); break;
    case Cmd::Paste: edit->paste(); break;
    case Cmd::SelectAll: edit->selectAll(); break;
    }
}

}  // namespace FluentContextMenu
