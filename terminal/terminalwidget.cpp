#include "terminalwidget.h"
#include "softkeyboard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent), m_process(nullptr)
{
    setWindowTitle("终端");
    setFocusPolicy(Qt::StrongFocus);

    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    QFont monoFont("WenQuanYi Zen Hei", 12);
    QFont btnFont("WenQuanYi Zen Hei", 14, QFont::Bold);

    m_output = new QPlainTextEdit(this);
    m_output->setFont(monoFont);
    m_output->setReadOnly(true);
    m_output->setFocusPolicy(Qt::NoFocus);
    m_output->setStyleSheet(
        "QPlainTextEdit { background: #0a0a14; color: #00e676; border: none;"
        "  selection-background: #2a2a4e; padding: 6px; }");
    m_output->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_output->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_input = new QLineEdit(this);
    m_input->setFont(monoFont);
    m_input->setPlaceholderText("输入命令...");
    m_input->setStyleSheet(
        "QLineEdit { background: #0a0a14; color: #00e676; border: none;"
        "  border-top: 1px solid #1a1a2e; padding: 8px 12px; }");

    m_clearBtn = new QPushButton("清屏", this);
    m_clearBtn->setFixedSize(80, 34);
    m_clearBtn->setFont(btnFont);
    m_clearBtn->setFocusPolicy(Qt::NoFocus);
    m_clearBtn->setStyleSheet(
        "QPushButton { background: #555; color: white; border: none; border-radius: 4px; }"
        "QPushButton:pressed { background: #333; }");

    m_exitBtn = new QPushButton("退出", this);
    m_exitBtn->setFixedSize(80, 34);
    m_exitBtn->setFont(btnFont);
    m_exitBtn->setFocusPolicy(Qt::NoFocus);
    m_exitBtn->setStyleSheet(
        "QPushButton { background: white; color: #333; border: none; border-radius: 4px; }"
        "QPushButton:pressed { background: #ccc; }");
    connect(m_exitBtn, &QPushButton::clicked, qApp, &QApplication::quit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);
    btnRow->addWidget(m_input, 1);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_exitBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_output, 1);
    mainLayout->addLayout(btnRow);

    m_keyboard = new SoftKeyboard(this);
    m_keyboard->setVisible(false);
    mainLayout->addWidget(m_keyboard);
    connect(m_keyboard, &SoftKeyboard::keyPressed, this, &TerminalWidget::onKeyPressed);

    setStyleSheet("background: #0a0a14;");

    connect(m_input, &QLineEdit::returnPressed, this, &TerminalWidget::onCommandEntered);
    connect(m_clearBtn, &QPushButton::clicked, this, &TerminalWidget::onClear);

    m_output->installEventFilter(this);
    m_clearBtn->installEventFilter(this);
    m_exitBtn->installEventFilter(this);
    m_input->installEventFilter(this);

    startShell();
}

TerminalWidget::~TerminalWidget()
{
    if (m_process && m_process->state() != QProcess::NotRunning) 
    {
        m_process->terminate();
        m_process->waitForFinished(3000);
    }
}

void TerminalWidget::startShell()
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalWidget::onReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalWidget::onReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalWidget::onProcessFinished);

    m_process->start("/bin/sh", QStringList());
    if (!m_process->waitForStarted(3000)) 
    {
        appendOutput("Failed to start shell\n", QColor("#ff4444"));
    }

    m_input->setFocus();
}

void TerminalWidget::onCommandEntered()
{
    QString cmd = m_input->text();
    if (cmd.isEmpty())
        return;

    appendOutput("$ " + cmd + "\n", QColor("#FFD54F"));
    m_input->clear();

    sendCommand(cmd);
}

void TerminalWidget::sendCommand(const QString &cmd)
{
    if (!m_process || m_process->state() != QProcess::Running)
        return;

    QByteArray data = cmd.toLocal8Bit() + "\n";
    m_process->write(data);
}

void TerminalWidget::onReadStdout()
{
    QByteArray data = m_process->readAllStandardOutput();
    appendOutput(QString::fromLocal8Bit(data), QColor("#00e676"));
}

void TerminalWidget::onReadStderr()
{
    QByteArray data = m_process->readAllStandardError();
    appendOutput(QString::fromLocal8Bit(data), QColor("#ff4444"));
}

void TerminalWidget::onProcessFinished(int exitCode)
{
    appendOutput(QString("\n[进程结束，退出码: %1]\n").arg(exitCode), QColor("#888"));
    startShell();
}

void TerminalWidget::onClear()
{
    m_output->clear();
}

void TerminalWidget::appendOutput(const QString &text, const QColor &color)
{
    QTextCharFormat fmt;
    fmt.setForeground(color);

    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, fmt);

    QScrollBar *bar = m_output->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        qApp->quit();
        return;
    }
    m_input->setFocus();
    QWidget::keyPressEvent(event);
}

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
}

bool TerminalWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::MouseButtonPress) 
    {
        m_keyboard->setVisible(!m_keyboard->isVisible());
        return true;
    }
    if (obj == m_output && event->type() == QEvent::MouseButtonPress) 
    {
        m_keyboard->setVisible(false);
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalWidget::onKeyPressed(const QString &text)
{
    if (text == "\b") 
    {
        m_input->backspace();
    } else if (text == "\n") 
    {
        m_keyboard->setVisible(false);
        onCommandEntered();
    } else 
    {
        m_input->insert(text);
    }
    m_input->setFocus();
}