#pragma once
#include <QLineEdit>
#include <QCompleter>
#include <QStringList>
#include <QKeyEvent>
#include <QObject>

#pragma once
#include <QLineEdit>
#include <QCompleter>

class CommandLine : public QLineEdit {
    Q_OBJECT
public:
    explicit CommandLine(QWidget *parent = nullptr);
    ~CommandLine();

    void setAvailableCommands(const QStringList &commands);
    QString getHelpText() const;

signals:
    void commandEntered(const QString &command);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QStringList commandHistory;
    QStringList availableCommands;
    QCompleter *completer;
    int historyIndex;
};
