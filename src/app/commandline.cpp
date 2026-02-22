#include "src/app/commandline.h"
#include <QKeyEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QKeyEvent>

CommandLine::CommandLine(QWidget *parent)
    : QLineEdit(parent), historyIndex(-1) {
    setPlaceholderText("Enter command...");

    // 初始化自动补全
    completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);

    // 设置默认的可用命令
    availableCommands << "line"<< "circle" << "cube" << "rectangle" << "cylinder" << "sphere" << "zoom in" << "zoom out" << "reset" << "help" << "clear";
    completer->setModel(new QStringListModel(availableCommands, this));
    setCompleter(completer);
}

CommandLine::~CommandLine() {}

void CommandLine::setAvailableCommands(const QStringList &commands) {
    availableCommands = commands;
    completer->setModel(new QStringListModel(availableCommands, this));
}

QString CommandLine::getHelpText() const {
    return
        "<b>Available Commands:</b><br><br>"
        "<b>line</b>: Draw a line. Click to set the start point, then click to set the end point.<br>"
        "<b>circle</b>: Draw a circle. Click to set the center, then click to set the radius.<br>"
        "<b>rectangle</b>: Draw a rectangle. Click to set the first corner, then click to set the opposite corner.<br>"
        "<b>zoom in</b>: Zoom in the view.<br>"
        "<b>zoom out</b>: Zoom out the view.<br>"
        "<b>reset</b>: Reset the view to default.<br>"
        "<b>clear</b>: Clear all drawings.<br>"
        "<b>help</b>: Show this help message.<br>"
        "<br>"
        "<b>Examples:</b><br>"
        "- <i>line</i>: Start drawing a line.<br>"
        "- <i>circle</i>: Start drawing a circle.<br>"
        "- <i>rectangle</i>: Start drawing a rectangle.<br>"
        "- <i>zoom in</i>: Zoom in the view.<br>"
        "- <i>reset</i>: Reset the view.<br>"
        "- <i>clear</i>: Clear all drawings.<br>";
}

void CommandLine::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QString command = text().trimmed();
        if (!command.isEmpty()) {
            commandHistory.append(command);
            historyIndex = commandHistory.size();
            emit commandEntered(command);
            clear();
        }
    }
    else if (event->key() == Qt::Key_Up) {
        if (!commandHistory.isEmpty() && historyIndex > 0) {
            setText(commandHistory.at(--historyIndex));
        }
    }
    else if (event->key() == Qt::Key_Down) {
        if (!commandHistory.isEmpty() && historyIndex < commandHistory.size() - 1) {
            setText(commandHistory.at(++historyIndex));
        } else {
            historyIndex = commandHistory.size();
            clear();
        }
    }
    else if (event->key() == Qt::Key_Tab) {
        if (completer->completionPrefix().isEmpty()) {
            completer->setCompletionPrefix(text());
        }
        if (completer->completionCount() == 1) {
            setText(completer->currentCompletion());
        } else {
            QStringList completions;
            QStringListModel *model = qobject_cast<QStringListModel*>(completer->model());
            if (model) {
                completions = model->stringList();
                QString prefix = completer->completionPrefix();
                QStringList matches;
                for (const QString &str : completions) {
                    if (str.startsWith(prefix, Qt::CaseInsensitive)) {
                        matches.append(str);
                    }
                }
                if (!matches.isEmpty()) {
                    setText(matches.first());
                }
            }
        }
    }
    else {
        QLineEdit::keyPressEvent(event);
    }
}
