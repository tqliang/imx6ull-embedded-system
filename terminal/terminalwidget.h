#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>

class SoftKeyboard;

class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onCommandEntered();
    void onReadStdout();
    void onReadStderr();
    void onProcessFinished(int exitCode);
    void onClear();
    void onKeyPressed(const QString &text);

private:
    void startShell();
    void sendCommand(const QString &cmd);
    void appendOutput(const QString &text, const QColor &color);

    QPlainTextEdit *m_output;
    QLineEdit      *m_input;
    QProcess       *m_process;
    QPushButton    *m_clearBtn;
    QPushButton    *m_exitBtn;
    SoftKeyboard   *m_keyboard;
};

#endif