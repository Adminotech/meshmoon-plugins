/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "IRocketAssetEditor.h"

#include <QTextEdit>
#include <QPlainTextEdit>

class RocketPlainTextEdit;
class RocketPlainTextLineNumberArea;

class QCheckBox;
class QLineEdit;
class QLabel;
class QFrame;

/// Text editor with basic editing capabilities. Including search, goto line and converting tabs to spaces.
class RocketTextEditor : public IRocketAssetEditor
{
    Q_OBJECT

public:
    RocketTextEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource);
    virtual ~RocketTextEditor();

    /// Get all supported suffixes
    /** @note The suffixes wont have the first '.' prefix, as "png". */
    static QStringList SupportedSuffixes();

    /// Returns editor name.
    static QString EditorName();

protected:
    virtual void ResourceDownloaded(QByteArray data); ///< IRocketAssetEditor override.
    virtual IRocketAssetEditor::SerializationResult ResourceData(); ///< IRocketAssetEditor override.
    bool eventFilter(QObject *watched, QEvent *e); ///< QObject override.
    void resizeEvent(QResizeEvent *e); ///< QWidget override.

private slots:
    // These could be made public maybe?
    void SetAutoIndentationEnabled(bool enabled);
    void SetShowWhitespace(bool);
    void SetWordWrapEnabled(bool);

    void IncreaseFontSize();
    void DecreaseFontSize();
    void ResetFontSize();
    void SetFontPointSize(int pointSize);
    
    void OnFindPressed();
    void OnGoToPressed();
    void OnConvertTabsToSpaces();

    void OnDocumentEdited();
    void OnInputTextChanged(const QString &text);
    void OnInputCheckBoxChange(bool checked);

    void UpdateInputState();
    void ClearInputState();
    void UpdateInputGeometry();

private:
    enum InputMode
    {
        None = 0,
        Find,
        GoTo
    };
    
    RocketPlainTextEdit *editor_;
    
    InputMode inputMode_;
    QLineEdit *inputLine_;
    QFrame *inputFrame_;
    QLabel *inputInfo_;
    QLabel *inputStatus_;
    QCheckBox *inputCheckBox_;
    bool autoIndentationEnabled_;

    QPair<QString, QList<QTextEdit::ExtraSelection> > findResults_;
    int iFind_;
};

// RocketPlainTextEdit

class RocketPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit RocketPlainTextEdit(const QString &suffix, QWidget *parent = 0);

    QLabel *lineInfo;
    bool shouldHighlight;
    
    RocketPlainTextLineNumberArea *LineNumberArea() const;
    void SetFontPointSize(int pointSize);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

public slots:
    void updateLineNumberAreaWidth();
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

protected:
    void resizeEvent(QResizeEvent *event);

private:
    RocketPlainTextLineNumberArea *lineNumberArea;
    IRocketSyntaxHighlighter *highlighter_;
    
    QColor highlightColor_;
    QFontMetrics currentFontMetric_;
};

// RocketPlainTextLineNumberArea

class RocketPlainTextLineNumberArea : public QWidget
{
    Q_OBJECT

public:
    explicit RocketPlainTextLineNumberArea(RocketPlainTextEdit *editor);

    QSize sizeHint() const;

    QColor colorBg;
    QColor colorText;

protected:
    void paintEvent(QPaintEvent *event);

private:
    RocketPlainTextEdit *editor_;
};
