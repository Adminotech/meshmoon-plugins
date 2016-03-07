/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketTextEditor.h"
#include "RocketSyntaxHighlighters.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "ConfigAPI.h"
#include "Math/MathFunc.h"

#include "MemoryLeakCheck.h"

static const QString submlimeThemeStyleSheet =
"QPlainTextEdit#RocketPlainTextEdit { \
    background-color: rgb(39, 40, 34); \
    color: rgb(248, 248, 242); \
    font-family: Consolas, Courier New; \
    border: 0px; \
    selection-background-color: rgb(255, 231, 146); \
    selection-color: rgb(0, 0, 0); \
}";

static const int defaultFontSize = 11;

RocketTextEditor::RocketTextEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource) :
    IRocketAssetEditor(plugin, resource, "Text Editor"),
    inputMode_(None)
{
    // Editor
    editor_ = new RocketPlainTextEdit(suffix, this);
    editor_->setStyleSheet(submlimeThemeStyleSheet);
    editor_->setFrameShape(QFrame::NoFrame);
    editor_->setFrameShadow(QFrame::Plain);
    editor_->setLineWidth(0);
    editor_->setEnabled(false);
    editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    editor_->installEventFilter(this);
    if (editor_->verticalScrollBar())
        editor_->verticalScrollBar()->installEventFilter(this);
    if (editor_->horizontalScrollBar())
        editor_->horizontalScrollBar()->installEventFilter(this);
    editor_->setCenterOnScroll(true);
    editor_->lineInfo = ui.labelMessage;

    // Input
    inputFrame_ = new QFrame(editor_);
    inputFrame_->setObjectName("inputFrame");
    inputFrame_->setFixedHeight(30);
    
    inputInfo_ = new QLabel(inputFrame_);
    inputInfo_->setObjectName("labelInputInfo");
    
    inputStatus_ = new QLabel(inputFrame_);
    inputStatus_->setObjectName("labelInputStatus");
    
    inputCheckBox_ = new QCheckBox("Match case");
    inputCheckBox_->setObjectName("checkboxInput");
    inputCheckBox_->setChecked(false);

    inputLine_ = new QLineEdit(inputFrame_);
    inputLine_->setObjectName("inputLine");
    inputLine_->installEventFilter(this);
    inputLine_->hide();

    connect(inputCheckBox_, SIGNAL(toggled(bool)), SLOT(OnInputCheckBoxChange(bool)));
    connect(inputLine_, SIGNAL(returnPressed()), SLOT(UpdateInputState()));
    connect(inputLine_, SIGNAL(textEdited(const QString&)), SLOT(OnInputTextChanged(const QString&)));

    QHBoxLayout *l = new QHBoxLayout();
    l->setSpacing(6);
    l->setContentsMargins(6,3,6,3);
    l->addWidget(inputInfo_);
    l->addWidget(inputLine_);
    l->addWidget(inputStatus_);
    l->addWidget(inputCheckBox_);
    inputFrame_->setLayout(l);
    inputFrame_->hide();

    // Tools
    QAction *action = ToolsMenu()->addAction("Find...");
    action->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_F));
    action->setShortcutContext(Qt::WindowShortcut);
    connect(action, SIGNAL(triggered()), SLOT(OnFindPressed()));

    action = ToolsMenu()->addAction("Go to line...");
    action->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_G));
    action->setShortcutContext(Qt::WindowShortcut);
    connect(action, SIGNAL(triggered()), SLOT(OnGoToPressed()));

    action = ToolsMenu()->addAction("Convert tabs to spaces");
    connect(action, SIGNAL(triggered()), SLOT(OnConvertTabsToSpaces()));

    // Settings
    QAction *autoIndentation = SettingsMenu()->addAction("Auto indentation");
    autoIndentationEnabled_ = plugin_->Fw()->Config()->DeclareSetting("rocket-asset-editors", "text-editor", "auto indentation", true).toBool();
    autoIndentation->setCheckable(true);
    autoIndentation->setChecked(autoIndentationEnabled_);
    connect(autoIndentation, SIGNAL(toggled(bool)), SLOT(SetAutoIndentationEnabled(bool)));

    const bool viewWhitespace = plugin_->Fw()->Config()->DeclareSetting("rocket-asset-editors", "text-editor", "show whitespace", false).toBool();
    QAction *showWhitespace = SettingsMenu()->addAction("View whitespace");
    showWhitespace->setCheckable(true);
    showWhitespace->setChecked(viewWhitespace);
    connect(showWhitespace, SIGNAL(toggled(bool)), SLOT(SetShowWhitespace(bool)));
    SetShowWhitespace(viewWhitespace);

    const bool wordWrap = plugin_->Fw()->Config()->DeclareSetting("rocket-asset-editors", "text-editor", "word wrap", true).toBool();
    QAction *enableWordWrap = SettingsMenu()->addAction("Word wrap");
    enableWordWrap->setCheckable(true);
    enableWordWrap->setChecked(wordWrap);
    connect(enableWordWrap, SIGNAL(toggled(bool)), SLOT(SetWordWrapEnabled(bool)));
    SetWordWrapEnabled(wordWrap);

    editor_->SetFontPointSize(plugin_->Fw()->Config()->DeclareSetting("rocket-asset-editors", "text-editor", "font size", defaultFontSize).toInt());

    QMenu *fontMenu = SettingsMenu()->addMenu("Font size");
    QAction *fontAct = fontMenu->addAction("Larger (Cltr + Wheel up)");
    connect(fontAct, SIGNAL(triggered()), SLOT(IncreaseFontSize()));
    fontAct = fontMenu->addAction("Smaller (Cltr + Wheel down)");
    connect(fontAct, SIGNAL(triggered()), SLOT(DecreaseFontSize()));
    fontMenu->addSeparator();
    fontAct = fontMenu->addAction("Reset");
    connect(fontAct, SIGNAL(triggered()), SLOT(ResetFontSize()));

    ui.contentLayout->addWidget(editor_);

    if (suffix == "js" || suffix == "webrocketjs")
        SetResourceDescription("JavaScript");
    else if (suffix == "material")
        SetResourceDescription("Ogre Material Script");
    else if (suffix == "particle")
        SetResourceDescription("Ogre Particle Script");
    else if (suffix == "json")
        SetResourceDescription("JSON");
    else if (suffix == "txt")
        SetResourceDescription("Plain Text");
    else if (suffix == "py")
        SetResourceDescription("Python");
    else if (suffix == "xml")
        SetResourceDescription("XML");
    else if (suffix == "obj")
        SetResourceDescription("Wavefront OBJ");
    else if (suffix == "mtl")
        SetResourceDescription("Wavefront Material");
    else if (suffix == "dae")
        SetResourceDescription("COLLADA XML");
    else if (suffix == "txml")
    {
        SetResourceDescription("Meshmoon Scene Description (read only)");
        SetSaveEnabled(false);
    }
        
    if (!RestoreGeometryFromConfig("text-editor", true))
        resize(900, 550);
}

RocketTextEditor::~RocketTextEditor()
{
}

QStringList RocketTextEditor::SupportedSuffixes()
{
    QStringList suffixes;
    suffixes << "material" << "particle" << "js" << "webrocketjs" << "py" 
             << "avatar" << "meshmoonavatar" << "dae" << "obj" << "mtl"
             << "json" << "txt" << "xml" << "txml"; // Well known text formats
    return suffixes;
}

QString RocketTextEditor::EditorName()
{
    return "Text Editor";
}

void RocketTextEditor::ResourceDownloaded(QByteArray data)
{
    editor_->setEnabled(true);   
    editor_->setPlainText(QString(data));
    editor_->updateLineNumberAreaWidth();
    update();
    
    // Connect the 'Touched' signal now that the initial content has been loaded.
    connect(editor_, SIGNAL(textChanged()), SLOT(OnDocumentEdited()), Qt::UniqueConnection);
}

IRocketAssetEditor::SerializationResult RocketTextEditor::ResourceData()
{
    IRocketAssetEditor::SerializationResult result;
    result.first = true;
    result.second = editor_->toPlainText().toUtf8();
    return result;
}

void RocketTextEditor::OnConvertTabsToSpaces()
{
    QByteArray editorData = editor_->toPlainText().toUtf8();
    editorData.replace('\t', "    ");
    editor_->setPlainText(QString(editorData));
}

void RocketTextEditor::OnDocumentEdited()
{
    Touched();
}

void RocketTextEditor::SetAutoIndentationEnabled(bool enabled)
{
    autoIndentationEnabled_ = enabled;
    plugin_->Fw()->Config()->Write("rocket-asset-editors", "text-editor", "auto indentation", enabled);
}

void RocketTextEditor::IncreaseFontSize()
{
    SetFontPointSize(editor_->font().pointSize() + 1);
}

void RocketTextEditor::DecreaseFontSize()
{
    SetFontPointSize(editor_->font().pointSize() - 1);
}

void RocketTextEditor::ResetFontSize()
{
    SetFontPointSize(defaultFontSize);
}

void RocketTextEditor::SetFontPointSize(int pointSize)
{
    if (pointSize < 1)
        pointSize = 1;

    editor_->SetFontPointSize(pointSize);
    plugin_->Fw()->Config()->Write("rocket-asset-editors", "text-editor", "font size", pointSize);
}

void RocketTextEditor::OnFindPressed()
{
    if (inputMode_ != None)
        ClearInputState();

    inputMode_ = Find;
    
    editor_->clearFocus();
    editor_->releaseKeyboard();

    inputFrame_->show();
    inputLine_->show();
    inputCheckBox_->show();
    inputStatus_->hide();
    inputInfo_->show();
    inputInfo_->setText("Find");  
    
    inputLine_->clear();
    inputLine_->setFocus(Qt::MouseFocusReason);
    inputLine_->grabKeyboard();
    
    UpdateInputGeometry();
    
    if (!findResults_.first.isEmpty())
    {
        if (!findResults_.second.isEmpty())
        {
            editor_->shouldHighlight = false;
            editor_->setExtraSelections(findResults_.second);
            iFind_--;
        }
        inputLine_->setText(findResults_.first);
        UpdateInputState();
    }
}

void RocketTextEditor::OnGoToPressed()
{
    if (inputMode_ != None)
        ClearInputState();
    
    inputMode_ = GoTo;
    
    editor_->clearFocus();
    editor_->releaseKeyboard();

    inputFrame_->show();
    inputLine_->show();
    inputCheckBox_->hide();
    inputStatus_->hide();
    inputInfo_->show();
    inputInfo_->setText("Go to line");

    inputLine_->clear();
    inputLine_->setFocus(Qt::MouseFocusReason);
    inputLine_->grabKeyboard();

    UpdateInputGeometry();
}

void RocketTextEditor::OnInputTextChanged(const QString &text)
{
    if (inputMode_ == Find)
    {
        if (findResults_.first.compare(text, Qt::CaseInsensitive) != 0)
            UpdateInputState();
    }
}

void RocketTextEditor::OnInputCheckBoxChange(bool /*checked*/)
{
    if (inputMode_ == Find)
    {
        findResults_.first = "";
        UpdateInputState();
    }
}

void RocketTextEditor::UpdateInputState()
{
    if (inputMode_ == GoTo)
    {
        bool ok = false;
        int linenum = inputLine_->text().trimmed().toInt(&ok);
        if (!ok)
        {
            SetMessage("Go to line number input is invalid");
            return;
        }
        if (linenum < 0)
            linenum = 0;
        if (linenum > editor_->blockCount())
            linenum = editor_->blockCount();
        
        // Find cursor index for this line
        int lineNow = 0; int pos = 0;
        QTextBlock b = editor_->document()->begin();
        while (b.isValid())
        {
            lineNow++;
            if (lineNow >= linenum)
                break;
            pos += b.length();
            b = b.next();
        }
        
        // Set cursor
        QTextCursor c = editor_->textCursor();
        c.setPosition(pos);
        editor_->setTextCursor(c);
        
        // Reset any errors if ones were given
        ClearMessage();
        
        // Select the text so user can just write a new number
        inputLine_->selectAll();
    }
    else if (inputMode_ == Find)
    {
        QString text = inputLine_->text();
        if (text.trimmed().isEmpty() || findResults_.first.compare(text, Qt::CaseInsensitive) != 0)
        {
            findResults_.first = text;
            findResults_.second.clear();
            iFind_ = 0;
            
            if (text.trimmed().isEmpty())
            {
                editor_->setExtraSelections(findResults_.second);
                inputStatus_->setText("");
                inputStatus_->hide();
                return;
            }
            
            int docPos = 0;
            int textLen = text.length();
            
            Qt::CaseSensitivity matchCase = (inputCheckBox_->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive);
            
            QTextCharFormat f;
            f.setBackground(QColor(39, 40, 42));
            f.setForeground(QColor(255, 231, 146));

            QTextCursor c = editor_->textCursor();
            QTextBlock b = editor_->document()->begin();
            
            while (b.isValid())
            {
                QString blockText = b.text();
                int i = blockText.indexOf(text, 0, matchCase);
                while (i != -1)
                {
                    QTextEdit::ExtraSelection selection;
                    selection.format = f;
                    selection.cursor = c;
                    selection.cursor.clearSelection();
                    selection.cursor.setPosition(docPos + i);
                    selection.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, textLen);

                    findResults_.second.append(selection);
                    
                    i = blockText.indexOf(text, i+textLen, matchCase);
                }
                docPos += b.length();
                b = b.next();
            }
            
            editor_->setExtraSelections(findResults_.second);

            if (!findResults_.second.isEmpty())
            {
                QTextEdit::ExtraSelection &first = findResults_.second.first();
                editor_->shouldHighlight = false;
                editor_->setTextCursor(first.cursor);
                
                inputStatus_->setText(QString("%1/%2").arg(1).arg(findResults_.second.size()));
            }
            else
            {
                inputStatus_->setText("");
                inputStatus_->hide();
            }
        }
        else if (!findResults_.second.isEmpty())
        {           
            if (iFind_ < (findResults_.second.size() - 1))
                iFind_++;
            else
                iFind_ = 0;

            QTextEdit::ExtraSelection &next = findResults_.second[iFind_];
            editor_->setTextCursor(next.cursor);
            
            inputStatus_->setText(QString("%1/%2").arg(iFind_+1).arg(findResults_.second.size()));
        }
        
        if (!inputStatus_->isVisible() && !inputStatus_->text().trimmed().isEmpty())
            inputStatus_->show();
    }
}

void RocketTextEditor::ClearInputState()
{
    inputMode_ = None;

    inputLine_->clear();
    inputLine_->clearFocus();
    inputLine_->releaseKeyboard();
    inputStatus_->setText("");
    inputFrame_->hide();
       
    editor_->setFocus(Qt::MouseFocusReason);
    editor_->grabKeyboard();
    editor_->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    editor_->shouldHighlight = true;
    editor_->highlightCurrentLine();
    
    ClearMessage();
}

void RocketTextEditor::UpdateInputGeometry()
{
    if (!inputFrame_ || !inputFrame_->isVisible())
        return;
    if (inputMode_ == None && inputFrame_->isVisible())
    {
        inputFrame_->hide();
        return;
    }
    
    int scrollBarWidth = 0;
    if (editor_->verticalScrollBar())
        scrollBarWidth = editor_->verticalScrollBar()->width();
    
    qreal width = editor_->width() * 0.4;
    inputFrame_->setFixedWidth(width);
    inputFrame_->move(editor_->width() - width - scrollBarWidth, 0);
}

void RocketTextEditor::SetShowWhitespace(bool show)
{
    plugin_->Fw()->Config()->Write("rocket-asset-editors", "text-editor", "show whitespace", show);
    QTextOption option = editor_->document()->defaultTextOption();
    if (show)
        option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
    else
        option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);
    editor_->document()->setDefaultTextOption(option);
}

void RocketTextEditor::SetWordWrapEnabled(bool enabled)
{
    plugin_->Fw()->Config()->Write("rocket-asset-editors", "text-editor", "word wrap", enabled);
    editor_->setWordWrapMode(enabled ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
}

void RocketTextEditor::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    UpdateInputGeometry();
}

static QString TextCursorSelect(QTextCursor &cursor, QTextCursor::MoveOperation op, int count)
{
    int prePos = cursor.position();
    
    cursor.movePosition(op, QTextCursor::KeepAnchor, count);
    QString text = cursor.selectedText();
    cursor.setPosition(prePos);
    return text;
}

static int TextCursorWhitespaceLineStart(QTextCursor &cursor, bool accountPostCurrentPos = true)
{
    int prePos = cursor.position();
    int posInBlock = cursor.positionInBlock();
    int whitespace = 0;

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    QString prevChar = TextCursorSelect(cursor, QTextCursor::Right, 1);
    if (prevChar == " " || prevChar == "\t")
    {
        cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
        whitespace += cursor.selectedText().length();
        if (!accountPostCurrentPos && whitespace > posInBlock)
            whitespace = posInBlock;
    }
    
    cursor.setPosition(prePos);
    return whitespace;
}

static int TextCursorRemove(QTextCursor &cursor, QTextCursor::MoveOperation op, int count)
{
    int removed = 0;
    if (cursor.movePosition(op, QTextCursor::KeepAnchor, count) && cursor.hasSelection())
    {
        removed = cursor.selectedText().length();
        cursor.removeSelectedText();
    }
    cursor.clearSelection();
    return removed;
}

static bool TextCursorOnlyWhitespace(QTextCursor &cursor, QTextCursor::MoveOperation op)
{
    if (op != QTextCursor::Left && op != QTextCursor::Right)
        return false;

    int prePos = cursor.position();

    // Check character at a time
    for(;;)
    {
        if (op == QTextCursor::Left && cursor.atBlockStart())
            break;
        else if (op == QTextCursor::Right && cursor.atBlockEnd())
            break;

        QString character = TextCursorSelect(cursor, op, 1);
        if (character != " " && character != "\t")
        {
            cursor.setPosition(prePos);
            return false;
        }
        else if (character.isEmpty())
            break;
        cursor.movePosition(op, QTextCursor::MoveAnchor);
    }
    cursor.setPosition(prePos);
    return true;
}

static void TextCursorInsert(QTextCursor &cursor, const QString &text, int count)
{
    for(int i=0; i<count; ++i)
        cursor.insertText(text);
}

static bool TextCursorIndent(QTextCursor &cursor, bool forward)
{
    int prePos = cursor.position();
    int start = cursor.anchor();
    int stop = prePos;
    if (start > stop)
        Swap(start, stop);
        
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    int startBlock = cursor.block().blockNumber();

    cursor.setPosition(stop, QTextCursor::MoveAnchor);
    int endBlock = cursor.block().blockNumber();

    cursor.setPosition(start, QTextCursor::MoveAnchor);

    // Single line. Only allow indentation if there is only whitespace before the cursor.
    // If not at start, inject 4 spaces there!
    bool singleLine = (endBlock == startBlock);
    if (singleLine)
    {
        if (!TextCursorOnlyWhitespace(cursor, QTextCursor::Left))
        {
            if (forward)
                TextCursorInsert(cursor, " ", 4);
            return true;
        }
    }
    
    int firstLineMoved = 0;

    cursor.beginEditBlock();
    for(int i=0; i<=(endBlock-startBlock); ++i)
    {
        if (forward)
        {
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            int spacecount = 4;
            int whitespace = TextCursorWhitespaceLineStart(cursor);
            if (whitespace > 0 && whitespace % 4 != 0)
                spacecount -= (whitespace % 4);
            TextCursorInsert(cursor, " ", spacecount);
            if (i == 0)
                firstLineMoved = spacecount;
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        }
        else
        {
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            int whitespace = TextCursorWhitespaceLineStart(cursor);
            if (whitespace > 0)
            {
                if (whitespace % 4 != 0)
                    whitespace = whitespace % 4;
                else if (whitespace > 4)
                    whitespace = 4;
                int removed = TextCursorRemove(cursor, QTextCursor::Right, whitespace);
                if (i == 0)
                    firstLineMoved = removed;
            }
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        }
    }
    cursor.endEditBlock();

    cursor.setPosition(start);
    if (!singleLine && (endBlock - cursor.block().blockNumber()) > 0)
    {
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, endBlock - cursor.block().blockNumber());
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    else if (firstLineMoved > 0)
        cursor.movePosition(forward ? QTextCursor::Right : QTextCursor::Left, QTextCursor::MoveAnchor, firstLineMoved);
    return true;
}

bool RocketTextEditor::eventFilter(QObject *watched, QEvent *e)
{
    if (e->type() == QEvent::Wheel)
    {        
        if (watched == editor_ || watched == editor_->horizontalScrollBar() || watched == editor_->verticalScrollBar())
        {
            QWheelEvent *we = static_cast<QWheelEvent*>(e);
            if (we->modifiers() == Qt::ControlModifier)
            {
                if (we->delta() > 0)
                    IncreaseFontSize();
                else
                    DecreaseFontSize();
                e->accept();
                return true;
            }
        }
        return false;
    }

    if (watched == editor_)
    {
        if (e->type() == QEvent::FocusIn)
        {
            if (inputLine_->hasFocus() || inputLine_->isVisible())
            {
                ClearInputState();
                return true;
            }
        }
        else if (e->type() == QEvent::KeyPress)
        {
            if (inputLine_->hasFocus() || inputLine_->isVisible())
                return true;
            
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            int key = ke->key();
            if (key == Qt::Key_Escape)
            {
                ClearInputState();
                return true;
            }
            else if (key == Qt::Key_Home)
            {
                // Move to post line whitespace on first press, then go to start of line.
                QTextCursor cursor = editor_->textCursor();
                int whitespace = TextCursorWhitespaceLineStart(cursor);
                if (cursor.positionInBlock() > whitespace)
                {
                    QTextCursor::MoveMode mode = (ke->modifiers() == Qt::ShiftModifier ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                    cursor.movePosition(QTextCursor::StartOfBlock, mode);
                    cursor.movePosition(QTextCursor::Right, mode, whitespace);
                    editor_->setTextCursor(cursor);
                    return true;
                }
            }
            else if (autoIndentationEnabled_ && (key == Qt::Key_Return || key == Qt::Key_Enter))
            {
                QTextCursor cursor = editor_->textCursor();
                cursor.clearSelection();

                int startPos = cursor.position();                
                QString previousChar = TextCursorSelect(cursor, QTextCursor::Left, 1);

                int whitespace = TextCursorWhitespaceLineStart(cursor, false);
                if (previousChar == "{")
                    whitespace += 4;

                // No whitespace at the start of line. Let Qt handle the line change.
                if (whitespace == 0)
                    return false;

                QString whitespaceStr = "";
                for (int i=0; i<whitespace; ++i)
                    whitespaceStr.append(" ");

                cursor.setPosition(startPos);
                cursor.insertText("\n" + whitespaceStr);
                return true;
            }
            else if (key == Qt::Key_Tab && ke->modifiers() != Qt::ShiftModifier)
            {
                QTextCursor cursor = editor_->textCursor();
                TextCursorIndent(cursor, true);
                editor_->setTextCursor(cursor);
                editor_->update();
                return true;
            }
            else if (key == Qt::Key_Backtab || (key == Qt::Key_Tab && ke->modifiers() == Qt::ShiftModifier))
            {
                QTextCursor cursor = editor_->textCursor();
                TextCursorIndent(cursor, false);
                editor_->setTextCursor(cursor);
                editor_->update();
                return true;
            }
        }
    }
    else if (watched == inputLine_)
    {
        if (e->type() == QEvent::KeyPress)
        {
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_Escape)
            {
                ClearInputState();
                return true;
            }
        }
    }
    return false;
}

// RocketPlainTextEdit

RocketPlainTextEdit::RocketPlainTextEdit(const QString &suffix, QWidget *parent) :
    QPlainTextEdit(parent),
    highlighter_(0),
    lineInfo(0),
    shouldHighlight(true),
    currentFontMetric_(QFont("Consolas, Courier New", defaultFontSize))
{
    setObjectName("RocketPlainTextEdit");
    
    highlighter_ = IRocketSyntaxHighlighter::TryCreateHighlighter(suffix, document());
    lineNumberArea = new RocketPlainTextLineNumberArea(this);
    
    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumberAreaWidth()));
    connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(updateLineNumberArea(QRect,int)));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(highlightCurrentLine()));
    
    highlightColor_ = QColor(44, 45, 39);
    
    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

int RocketPlainTextEdit::lineNumberAreaWidth()
{
    int numLines = blockCount();
    return (5 + currentFontMetric_.width("0") * (QString::number(numLines).length() + 2));
}

void RocketPlainTextEdit::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void RocketPlainTextEdit::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void RocketPlainTextEdit::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void RocketPlainTextEdit::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextCursor cursor = textCursor();

    if (!isReadOnly()) 
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(highlightColor_);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = cursor;
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    if (lineInfo)
        lineInfo->setText(QString("Line %1 Column %2").arg(cursor.blockNumber()+1).arg(cursor.columnNumber()));

    if (shouldHighlight)
        setExtraSelections(extraSelections);
}

void RocketPlainTextEdit::SetFontPointSize(int pointSize)
{
    QFont f = font();
    f.setPointSize(pointSize);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    setFont(f);
    
    currentFontMetric_ = QFontMetrics(f);
    setTabStopWidth(4 * currentFontMetric_.width(" "));
      
    lineNumberArea->update();
}

RocketPlainTextLineNumberArea *RocketPlainTextEdit::LineNumberArea() const
{
    return lineNumberArea;
}

void RocketPlainTextEdit::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), lineNumberArea->colorBg);
    painter.setPen(lineNumberArea->colorText);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    QRectF blockRect = blockBoundingRect(block);
    int bottom = top + static_cast<int>(blockRect.height());

    QFont f = font();
    f.setPixelSize(static_cast<int>(blockRect.height() * 0.8));
    painter.setFont(f);
    
    int numberTextHeight = currentFontMetric_.height();
    int numberTextWidth = lineNumberArea->width() - 10;

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
            painter.drawText(0, top, numberTextWidth, numberTextHeight, Qt::AlignRight|Qt::AlignVCenter, QString::number(blockNumber + 1));

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// RocketPlainTextLineNumberArea

RocketPlainTextLineNumberArea::RocketPlainTextLineNumberArea(RocketPlainTextEdit *editor) : 
    QWidget(editor),
    editor_(editor)
{
    setObjectName("RocketPlainTextLineNumberArea");

    editor_ = editor;
    colorBg = QColor(39, 40, 34);
    colorText = QColor(143, 144, 138);
}

void RocketPlainTextLineNumberArea::paintEvent(QPaintEvent *event)
{
    editor_->lineNumberAreaPaintEvent(event);
}

QSize RocketPlainTextLineNumberArea::sizeHint() const
{
    return QSize(editor_->lineNumberAreaWidth(), 0);
}
