/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QRegExp>
#include <QTextCharFormat>

/// @cond PRIVATE

// IRocketSyntaxHighlighter

class IRocketSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    IRocketSyntaxHighlighter(QTextDocument *doc);

    /// Returns a higher for file @c suffix. Null if no highlighter exists for this suffix.
    static IRocketSyntaxHighlighter *TryCreateHighlighter(QString suffix, QTextDocument *target);

    /// Highlight rule.
    struct HighlightRule
    {
        QRegExp pattern;
        QTextCharFormat format;
    };
    typedef QList<HighlightRule> HighlightRuleList;

    /// Add a custom highlighting rule.
    void AddRule(const HighlightRule &rule);

    /// @overload
    void AddRule(const QRegExp &pattern, const QTextCharFormat &format);

    /// Call this before AddCommentRules but after all your custom rules.
    void AddQuotedStringRules(bool doubleQuotes = true, bool singleQuotes = true);

    /// Adds rules about "//" and "/* ... */" to be automatically highlighted.
    /** @note You should call this as the last thing in your ctor so that the 
        comment color overrides any of your other highlight rules.
        @param If line starting with "#" should be considered a comment.
        Disabled by default as this is not the common case. */
    void AddCommentRules(bool includeHashSignComment = false);
    
protected:
    virtual void highlightBlock(const QString &text);

    // Basic colors that work well with our sublime theme.
    QTextCharFormat formatLightGrey_;
    QTextCharFormat formatBlue_;
    QTextCharFormat formatPink_;
    QTextCharFormat formatGreen_;
    QTextCharFormat formatYellow_;
    QTextCharFormat formatOrange_;
    QTextCharFormat formatPurple_;

private:
    HighlightRuleList rules_;
    
    bool commentRulesEnabled_;
    bool doubleQuoteRulesEnabled_;
    bool doubleQuoteMultilineRulesEnabled_;
    bool singleQuoteRulesEnabled_;
    bool singleQuoteMultilineRulesEnabled_;

    QRegExp commentStartExpression_;
    QRegExp commentEndExpression_;
    
    QRegExp doubleQuoteStartExpression_;
    QRegExp doubleQuoteEndExpression_;
};

// RocketMaterialHighlighter

class RocketMaterialHighlighter : public IRocketSyntaxHighlighter
{
    Q_OBJECT

public:
    RocketMaterialHighlighter(QTextDocument *doc);
};

// RocketJavaScriptHighlighter

class RocketJavaScriptHighlighter : public IRocketSyntaxHighlighter
{
    Q_OBJECT

public:
    RocketJavaScriptHighlighter(QTextDocument *doc);
};

// RocketObjHighlighter

class RocketObjHighlighter : public IRocketSyntaxHighlighter
{
    Q_OBJECT

public:
    RocketObjHighlighter(QTextDocument *doc);
};

// RocketMtlHighlighter

class RocketMtlHighlighter : public IRocketSyntaxHighlighter
{
    Q_OBJECT

public:
    RocketMtlHighlighter(QTextDocument *doc);
};

// RocketXmlHighlighter

class RocketXmlHighlighter : public IRocketSyntaxHighlighter
{
    Q_OBJECT

public:
    RocketXmlHighlighter(QTextDocument *doc);
};

/// @endcond
