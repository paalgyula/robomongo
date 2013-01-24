#include "robomongo/gui/widgets/workarea/ScriptWidget.h"

#include <QDebug>
#include <QFont>
#include <QRect>
#include <QVBoxLayout>
#include <QPainter>
#include <QIcon>
#include <QMovie>
#include <QCompleter>
#include <Qsci/qscilexerjavascript.h>

#include "robomongo/core/domain/MongoShell.h"
#include "robomongo/core/domain/MongoServer.h"
#include "robomongo/core/settings/ConnectionSettings.h"
#include "robomongo/gui/GuiRegistry.h"
#include "robomongo/gui/editors/JSLexer.h"
#include "robomongo/gui/editors/PlainJavaScriptEditor.h"
#include <QStringListModel>
#include <QMessageBox>

using namespace Robomongo;

ScriptWidget::ScriptWidget(MongoShell *shell) :
    _shell(shell),
    _completionLineBookmark(0),
    _completionIndexBookmark(0)
{
    setStyleSheet("QFrame {background-color: rgb(255, 255, 255); border: 0px solid #c7c5c4; border-radius: 0px; margin: 0px; padding: 0px;}");

    _queryText = new RoboScintilla;
    _topStatusBar = new TopStatusBar(shell);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(5, 1, 5, 5);
    layout->addWidget(_topStatusBar, 0, Qt::AlignTop);
    layout->addWidget(_queryText, 1, Qt::AlignTop);
    setLayout(layout);

    // Query text widget
    prepareFont();
    _configureQueryText();
    _queryText->setFixedHeight(10);
    ui_queryLinesCountChanged();
    _queryText->setFocus();

    _queryText->installEventFilter(this);

    _completer = new QCompleter(this);
    _completer->setWidget(_queryText);
    _completer->setCompletionMode(QCompleter::PopupCompletion);
    _completer->setCaseSensitivity(Qt::CaseInsensitive);
    _completer->setMaxVisibleItems(20);
    _completer->setWrapAround(false);
    _completer->popup()->setFont(_textFont);
    connect(_completer, SIGNAL(activated(QString)), this, SLOT(onCompletionActivated(QString)));

    QStringListModel *model = new QStringListModel(_completer);
    _completer->setModel(model);
}

bool ScriptWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == _queryText) {

        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                _queryText->setIgnoreEnterKey(false);
                return false;
            }
            return false;
        } else {
            return false;
        }

    } else {
        return QFrame::eventFilter(obj, event);
    }

}

void ScriptWidget::setup(const MongoShellExecResult &execResult)
{
    setCurrentDatabase(execResult.currentDatabase(), execResult.isCurrentDatabaseValid());
    setCurrentServer(execResult.currentServer(), execResult.isCurrentServerValid());
}

void ScriptWidget::setText(const QString &text)
{
    _queryText->setText(text);
}

void ScriptWidget::setTextCursor(const CursorPosition &cursor)
{
    if (cursor.isNull()) {
        _queryText->setCursorPosition(15, 1000);
        return;
    }

    int column = cursor.column();
    if (column < 0) {
        column = _queryText->text(cursor.line()).length() + column;
    }

    _queryText->setCursorPosition(cursor.line(), column);
}

QString ScriptWidget::text() const
{
    return _queryText->text();
}

QString ScriptWidget::selectedText() const
{
    return _queryText->selectedText();
}

void ScriptWidget::selectAll()
{
    _queryText->selectAll();
}

void ScriptWidget::setScriptFocus()
{
    _queryText->setFocus();
}

void ScriptWidget::setCurrentDatabase(const QString &database, bool isValid)
{
    _topStatusBar->setCurrentDatabase(database, isValid);
}

void ScriptWidget::setCurrentServer(const QString &address, bool isValid)
{
    _topStatusBar->setCurrentServer(address, isValid);
}

void ScriptWidget::showProgress()
{
    _topStatusBar->showProgress();
}

void ScriptWidget::hideProgress()
{
    _topStatusBar->hideProgress();
}

void ScriptWidget::showAutocompletion(const QStringList &list, const QString &prefix)
{
    // do not show single autocompletion which is identical to existing prefix
    if (list.count() == 1) {
        if (list.at(0) == prefix)
            return;
    }

    QStringListModel * model = static_cast<QStringListModel *>(_completer->model());
    model->setStringList(list);

    QFontMetrics m(_queryText->font());
    int lineHight = m.lineSpacing() + 1;

    int row = 0;
    int col = 0;
    _queryText->getCursorPosition(&row, &col);
    int firstVisibleLine = _queryText->firstVisibleLine();
    int totalLines = _queryText->lines();

    int visualLine = row - firstVisibleLine;

    //QRect rect(40, visualLine * lineHight, 330, 330);

//    _queryText->rect()

    int stop = 0;
    sanitizeForAutocompletion(&stop);
    stop++;

    QRect rect = _queryText->rect();
    rect.setWidth(350);
    rect.setHeight((visualLine + 1) * lineHight + 8);
    rect.adjust(m.averageCharWidth() * stop + 1, 0, 0, 0);

    //_queryText->
    //_queryText->lineIndexFromPosition();

    _completer->complete(rect);
    _queryText->setIgnoreEnterKey(true);
    _completer->popup()->setCurrentIndex(_completer->completionModel()->index(0, 0));
}

void ScriptWidget::showAutocompletion()
{
    QString statement = sanitizeForAutocompletion();

    if (statement.isEmpty()) {
        hideAutocompletion();
        return;
    }

    _shell->autocomplete(statement);
}

void ScriptWidget::hideAutocompletion()
{
    _completer->popup()->hide();
    _queryText->setIgnoreEnterKey(false);
}

void ScriptWidget::ui_queryLinesCountChanged()
{
    setUpdatesEnabled(false);
    int pos = _queryText->fontInfo().pointSize();
    int pis = _queryText->fontInfo().pixelSize();
    int teh = _queryText->textHeight(0);
    int exa = _queryText->extraAscent();
    int exd = _queryText->extraDescent();

    QFontMetrics m(_queryText->font());
    int lineHeight = m.lineSpacing();

#if defined(Q_OS_UNIX)
    // this fix required to calculate correct height in Linux.
    // not the best way, but for now it tested on Ubuntu.
    lineHeight++;
#endif

    int numberOfLines = _queryText->lines();

    int height = numberOfLines * lineHeight + 8;

    int maxHeight = 18 * lineHeight + 8;
    if (height > maxHeight)
        height = maxHeight;

    _queryText->setFixedHeight(height);

    // this line helps eliminate UI flicks (because of background redraw)
    layout()->activate();

    setUpdatesEnabled(true);
}

void ScriptWidget::onTextChanged()
{
    showAutocompletion();
}

void ScriptWidget::onCompletionActivated(QString text)
{
    QString line = _queryText->text(_completionLineBookmark);
    int len = line.length();

    // do not select trailing new-line symbols
    if (line.endsWith("\r\n"))
        len = len - 2;
    else if (line.endsWith("\n"))
        --len;

    _queryText->setSelection(_completionLineBookmark, _completionIndexBookmark,
                             _completionLineBookmark, len);
    _queryText->replaceSelectedText(text);
}

/*
** Configure QsciScintilla query widget
*/
void ScriptWidget::_configureQueryText()
{
    QsciLexerJavaScript *javaScriptLexer = new JSLexer(this);
    javaScriptLexer->setFont(_textFont);

    _queryText->setFixedHeight(23);
    _queryText->setAutoIndent(true);
    _queryText->setIndentationsUseTabs(false);
    _queryText->setIndentationWidth(4);
    _queryText->setUtf8(true);
    _queryText->installEventFilter(this);
    _queryText->setMarginWidth(1, 0); // to hide left gray column
    _queryText->setBraceMatching(QsciScintilla::StrictBraceMatch);
    _queryText->setFont(_textFont);
    _queryText->setPaper(QColor(255, 0, 0, 127));
    _queryText->setLexer(javaScriptLexer);
    _queryText->setCaretForegroundColor(QColor("#FFFFFF"));
    _queryText->setMatchedBraceBackgroundColor(QColor(48, 10, 36));
    _queryText->setMatchedBraceForegroundColor(QColor("#1AB0A6"));
    _queryText->setWrapMode((QsciScintilla::WrapMode)QsciScintilla::SC_WRAP_NONE);
    _queryText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _queryText->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    //_queryText->SendScintilla(QsciScintilla::SCI_SETFONTQUALITY, QsciScintilla::SC_EFF_QUALITY_LCD_OPTIMIZED);
    //_queryText->SendScintilla (QsciScintillaBase::SCI_SETKEYWORDS, "db");

    _queryText->setStyleSheet("QFrame { background-color: rgb(48, 10, 36); border: 1px solid #c7c5c4; border-radius: 4px; margin: 0px; padding: 0px;}");
    connect(_queryText, SIGNAL(linesChanged()), SLOT(ui_queryLinesCountChanged()));
    connect(_queryText, SIGNAL(textChanged()), SLOT(onTextChanged()));
}

void ScriptWidget::prepareFont()
{
    _textFont = font();
#if defined(Q_OS_MAC)
    _textFont.setPointSize(12);
    _textFont.setFamily("Monaco");
#elif defined(Q_OS_UNIX)
    _textFont.setFamily("Monospace");
    _textFont.setFixedPitch(true);
#elif defined(Q_OS_WIN)
    _textFont.setPointSize(font().pointSize() + 2);
    _textFont.setFamily("Courier");
#endif
}

QString ScriptWidget::sanitizeForAutocompletion(int *stopOut)
{
    int row = 0;
    int col = 0;
    _queryText->getCursorPosition(&row, &col);
    QString line = _queryText->text(row);

    int stop = -1;
    for (int i = line.length() - 1; i >= 0; --i) {
        const QChar ch = line.at(i);

        // if line contains ' or " - do not show autocompletion for this line
        if (ch == '\"' ||  ch == '\'')
            return QString();

        if (ch == '='  ||  ch == ';'  ||
            ch == '('  ||  ch == ')'  ||
            ch == '{'  ||  ch == '}'  ||
            ch == '-'  ||  ch == '/'  ||
            ch == '+'  ||  ch == '*'  ||
            ch == ' ' ) {
            stop = i;
            break;
        }
    }

    _completionLineBookmark = row;
    _completionIndexBookmark = stop + 1;

    if (stopOut)
        *stopOut = stop;

    QString final = line.mid(stop + 1).trimmed();
    return final;
}

void ElidedLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    return;
    QPainter painter(this);
    QFontMetrics metrics(font());
    QString elided = metrics.elidedText(text(), Qt::ElideRight, width());
    painter.drawText(rect(), alignment(), elided);
}

QSize ElidedLabel::minimumSizeHint() const
{
    QSize defaultMinSizeHint = QLabel::minimumSizeHint();
    return QSize(0, defaultMinSizeHint.height());
}

QSize ElidedLabel::sizeHint() const
{
    QSize defaultSizeHint = QLabel::sizeHint();
    return defaultSizeHint;
}


TopStatusBar::TopStatusBar(MongoShell *shell) :
    _shell(shell)
{
    setContentsMargins(0, 0, 0, 0);
    _textColor = palette().text().color().lighter(200);

    QIcon dbIcon = GuiRegistry::instance().databaseIcon();
    QPixmap dbPixmap = dbIcon.pixmap(16, 16, QIcon::Disabled);
    QLabel *dbIconLabel = new QLabel;
    dbIconLabel->setPixmap(dbPixmap);

    QIcon serverIcon = GuiRegistry::instance().serverIcon();
    QPixmap serverPixmap = serverIcon.pixmap(16, 16, QIcon::Disabled);
    QLabel *serverIconLabel = new QLabel;
    serverIconLabel->setPixmap(serverPixmap);

    _currentServerLabel = new ElidedLabel(QString("<font color='%1'>%2</font>").arg(_textColor.name()).arg(_shell->server()->connectionRecord()->getFullAddress()));
    _currentServerLabel->setDisabled(true);

    _currentDatabaseLabel = new ElidedLabel();
    _currentDatabaseLabel->setDisabled(true);

    QMovie *movie = new QMovie(":robomongo/icons/loading.gif", QByteArray(), this);
    _progressLabel = new QLabel();
    _progressLabel->setMovie(movie);
    _progressLabel->hide();
    movie->start();

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(2, 7, 2, 3);
    topLayout->addWidget(serverIconLabel, 0, Qt::AlignLeft);
    topLayout->addWidget(_currentServerLabel, 0, Qt::AlignLeft);
    topLayout->addSpacing(10);
    topLayout->addWidget(dbIconLabel, 0, Qt::AlignLeft);
    topLayout->addWidget(_currentDatabaseLabel, 0, Qt::AlignLeft);
    topLayout->addStretch(1);
    topLayout->addWidget(_progressLabel, 0, Qt::AlignLeft);

    setLayout(topLayout);
}

void TopStatusBar::setCurrentDatabase(const QString &database, bool isValid)
{
    QString color = isValid ? _textColor.name() : "red";

    QString text = QString("<font color='%1'>%2</font>")
            .arg(color)
            .arg(database);

    _currentDatabaseLabel->setText(text);
}

void TopStatusBar::setCurrentServer(const QString &address, bool isValid)
{
    QString color = isValid ? _textColor.name() : "red";

    QString text = QString("<font color='%1'>%2</font>")
            .arg(color)
            .arg(address);

    _currentServerLabel->setText(text);
}

void TopStatusBar::showProgress()
{
    _progressLabel->show();
}

void TopStatusBar::hideProgress()
{
    _progressLabel->hide();
}