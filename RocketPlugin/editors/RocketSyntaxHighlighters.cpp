/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketSyntaxHighlighters.h"

// IRocketSyntaxHighlighter

IRocketSyntaxHighlighter::IRocketSyntaxHighlighter(QTextDocument *doc) :
    QSyntaxHighlighter(doc),
    commentRulesEnabled_(false),
    doubleQuoteRulesEnabled_(false),
    doubleQuoteMultilineRulesEnabled_(false),
    singleQuoteRulesEnabled_(false),
    singleQuoteMultilineRulesEnabled_(false)
{   
    formatLightGrey_.setForeground(QColor(117, 113, 94));
    formatBlue_.setForeground(QColor(102, 217, 239));
    formatPink_.setForeground(QColor(249, 38, 114));
    formatGreen_.setForeground(QColor(166, 226, 46));
    formatYellow_.setForeground(QColor(230, 219, 116));
    formatOrange_.setForeground(QColor(253, 151, 31));
    formatPurple_.setForeground(QColor(174, 129, 255));
}

IRocketSyntaxHighlighter *IRocketSyntaxHighlighter::TryCreateHighlighter(QString suffix, QTextDocument *target)
{
    if (!target)
        return 0;

    if (suffix.startsWith("."))
        suffix = suffix.mid(1);
    suffix = suffix.toLower();

    if (suffix == "material")
        return new RocketMaterialHighlighter(target);
    else if (suffix == "js" || suffix == "webrocketjs")
        return new RocketJavaScriptHighlighter(target);
    else if (suffix == "obj")
        return new RocketObjHighlighter(target);
    else if (suffix == "mtl")
        return new RocketMtlHighlighter(target);
    else if (suffix == "xml" || suffix == "txml" || suffix == "dae")
        return new RocketXmlHighlighter(target);
    return 0;
}

void IRocketSyntaxHighlighter::AddRule(const QRegExp &pattern, const QTextCharFormat &format)
{   
    HighlightRule rule;
    rule.pattern = pattern;
    rule.format = format;
    rules_ << rule;
}

void IRocketSyntaxHighlighter::AddRule(const HighlightRule &rule)
{
    rules_ << rule;
}

void IRocketSyntaxHighlighter::AddQuotedStringRules(bool doubleQuotes, bool singleQuotes)
{
    if (!doubleQuoteRulesEnabled_ && doubleQuotes)
    {
        AddRule(QRegExp("(\"\")|(\\\"[^\"]*\\\")"), formatYellow_);
        doubleQuoteRulesEnabled_ = true;
        
        /* Figure this out...
        if (multiLinedStringsAllowed)
        {   
            doubleQuoteStartExpression_ = QRegExp("(\\\"[^\\\"]*[^\\\"/>]$)");
            doubleQuoteEndExpression_   = QRegExp("([^\\\"]*\\\")");
            doubleQuoteMultilineRulesEnabled_ = true;
        }*/
    }
    
    if (!singleQuoteRulesEnabled_ && singleQuotes)
    {
        AddRule(QRegExp("('')|('[^']*')"), formatYellow_);
        singleQuoteRulesEnabled_ = true;

        /* Figure this out...
        if (multiLinedStringsAllowed)
        {
        }*/        
    }
}

void IRocketSyntaxHighlighter::AddCommentRules(bool includeHashSignComment)
{
    if (commentRulesEnabled_)
        return;
    commentRulesEnabled_ = true;

    AddRule(QRegExp("((?:\\s+)(\\/\\/[^\n]*)|^\\/\\/[^\n]*)"), formatLightGrey_);
    if (includeHashSignComment)
        AddRule(QRegExp("((?:\\s+)(#[^\n]*)|^#[^\n]*)"), formatLightGrey_);

    commentStartExpression_ = QRegExp("/\\*");
    commentEndExpression_   = QRegExp("\\*/");
}

void IRocketSyntaxHighlighter::highlightBlock(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

    foreach (const HighlightRule &rule, rules_)
    {
        QRegExp expression(rule.pattern);
        int index = expression.indexIn(text);
        while (index > -1)
        {
            int count = expression.captureCount();
            int advance = expression.matchedLength();

            if (advance > 0)
            {
                if (count > 0)
                {
                    for (int ci=1,ciLen=expression.captureCount(); ci<=ciLen; ++ci)
                    {
                        int len = expression.cap(ci).length();
                        if (len > 0)
                        {
                            int pos = expression.pos(ci);
                            setFormat(pos, len, rule.format);
                        }
                    }
                }
                else
                    setFormat(index, advance, rule.format);
            }
            else
                advance = 1;

            index = expression.indexIn(text, index + advance);
        }
    }
    setCurrentBlockState(0);
    
    if (commentRulesEnabled_)
    {
        int STATE_MULTILINE = 1;
        int startIndex = 0;
        if (previousBlockState() != STATE_MULTILINE)
            startIndex = commentStartExpression_.indexIn(text);

        while (startIndex >= 0)
        {
            int endIndex = commentEndExpression_.indexIn(text, startIndex);
            int commentLength;
            if (endIndex == -1)
            {
                setCurrentBlockState(STATE_MULTILINE);
                commentLength = text.length() - startIndex;
            }
            else
                commentLength = endIndex - startIndex + commentEndExpression_.matchedLength();

            setFormat(startIndex, commentLength, formatLightGrey_);
            startIndex = commentStartExpression_.indexIn(text, startIndex + commentLength);
        }
    }
    
    /* Figure this out...
    STATE_MULTILINE = 2;
    
    if (doubleQuoteMultilineRulesEnabled_)
    {
        int startIndex = 0;
        if (previousBlockState() != STATE_MULTILINE)
            startIndex = doubleQuoteStartExpression_.indexIn(text);

        while (startIndex >= 0)
        {
            int endIndex = doubleQuoteEndExpression_.indexIn(text, startIndex);
            int stringLength;
            if (endIndex == -1)
            {
                setCurrentBlockState(STATE_MULTILINE);
                stringLength = text.length() - startIndex;
                qDebug() << "CONTINUE" << text << text.mid(startIndex, stringLength);
            }
            else
            {
                stringLength = endIndex - startIndex + doubleQuoteEndExpression_.matchedLength();
                qDebug() << "STRING  " << text << text.mid(startIndex, stringLength);
            }

            setFormat(startIndex, stringLength, formatYellow_);
            if (stringLength == 0)
                stringLength = 1;
            startIndex = doubleQuoteStartExpression_.indexIn(text, startIndex + stringLength);
        }
    }*/
}

// RocketMaterialHighlighter

RocketMaterialHighlighter::RocketMaterialHighlighter(QTextDocument *doc) :
    IRocketSyntaxHighlighter(doc)
{
    QStringList keywords;
    keywords << "ambient"
             << "diffuse"
             << "specular"
             << "emissive"
             << "scene_blend"
             << "separate_scene_blend"
             << "scene_blend_op"
             << "separate_scene_blend_op"
             << "depth_check"
             << "depth_write"
             << "depth_func"
             << "depth_bias"
             << "iteration_depth_bias"
             << "alpha_rejection"
             << "alpha_to_coverage"
             << "light_scissor"
             << "light_clip_planes"
             << "illumination_stage"
             << "transparent_sorting"
             << "normalise_normals"
             << "cull_hardware"
             << "cull_software"
             << "lighting"
             << "shading"
             << "polygon_mode"
             << "polygon_mode_overrideable"
             << "fog_override"
             << "colour_write"
             << "max_lights"
             << "start_light"
             << "iteration"
             << "point_size"
             << "point_sprites"
             << "point_size_attenuation"
             << "point_size_min"
             << "point_size_max";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatBlue_);

    keywords.clear();
    keywords << "material" << "technique" << "pass";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatPurple_);

    keywords.clear();
    keywords << "vertex_program_ref" << "fragment_program_ref";
    
    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatGreen_);
    
    keywords.clear();
    keywords << "texture_unit";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatOrange_);

    AddRule(QRegExp("(?:\\s+)(http(s)?:\\/\\/[a-zA-Z0-9\\-_]+\\.[a-zA-Z]+(.)+)"), formatYellow_);

    AddCommentRules();
}

// RocketJavaScriptHighlighter

RocketJavaScriptHighlighter::RocketJavaScriptHighlighter(QTextDocument *doc) :
    IRocketSyntaxHighlighter(doc)
{  
    QStringList regexps;
    regexps << "(?:\\s+)(if)(?:\\s*\\()"            // @todo These all have a bug that is they are the start of the line it wont highlight.
            << "(?:\\s+|})(else)(?:\\s+|{|\\n)"     // \s* is not good as then it will hit inside all words. Need to add some kind of ^ trick.
            << "(?:\\s+|})(else if)(?:\\s*\\()"
            << "(?:\\s+)(for)(?:\\s*\\()"
            << "(?:\\s+)(while)(?:\\s*\\()"
            << "(?:\\s+)(do)(?:\\s*\\()"
            << "(?:\\s+)(switch)(?:\\s*\\()"
            << "(!{0,1}={1,3})"
            << "(!{1})"
            << "(\\+{1,2}={0,1})"
            << "(\\-{1,2}={0,1})";
            
    QStringList keywords;
    keywords << "break"
             << "case"
             << "catch"
             << "continue"
             << "debugger"
             << "default"
             << "delete"
             << "finally"
             << "in"
             << "instanceof"
             << "new"
             << "return"
             << "throw"
             << "try"
             << "typeof"
             << "with";
            
    foreach (const QString &regexp, regexps)
        AddRule(QRegExp(regexp), formatPink_);
    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatPink_);
        
    regexps.clear();
    regexps << "(function)(?:\\s*\\w*\\s*\\()";
    
    keywords.clear();
    keywords << "var"
             << "void"
             << "interface";

    foreach (const QString &regexp, regexps)
        AddRule(QRegExp(regexp), formatBlue_);
    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatBlue_);

    regexps.clear();
    regexps << "(?:\\W)(\\d*\\.{0,1}\\d+)";

    keywords.clear();
    keywords << "null"
             << "false"
             << "true";

    foreach (const QString &regexp, regexps)
        AddRule(QRegExp(regexp), formatPurple_);
    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("\\b(" + keyword + ")\\b"), formatPurple_);

    // Lastly "object function" that will override the above added function regexp color
    AddRule(QRegExp("(\\w+)(?:\\s*(?::|=)\\s*(?:function))"), formatGreen_);
    AddRule(QRegExp("(?:\\s*function\\s*)(\\w+)"), formatGreen_);

    AddQuotedStringRules();
    AddCommentRules();
}

// RocketObjHighlighter

RocketObjHighlighter::RocketObjHighlighter(QTextDocument *doc) :
    IRocketSyntaxHighlighter(doc)
{
    // Grouping
    QStringList regexps;
    regexps << "(^\\g|^s|^o|^mg [^\\n]*)";

    foreach (const QString &regexp, regexps)
        AddRule(QRegExp(regexp), formatGreen_);

    // Vertex data
    QStringList keywords;
    keywords << "v" << "vt" << "vn" << "vp" << "cstype" << "deg" << "bmat" << "step";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("^(" + keyword + ")\\b"), formatOrange_);

    // Elements
    keywords.clear();
    keywords << "p" << "l" << "f" << "curv" << "curv2" << "surf";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("^(" + keyword + ") \\b"), formatBlue_);

    // Display/render attributes
    keywords.clear();
    keywords << "bevel" << "c_interp" << "d_interp" << "lod"
             << "usemtl" << "mtllib"
             << "shadow_obj" << "trace_obj" << "ctech" << "stech";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("^(" + keyword + ") \\b"), formatPurple_);

    AddRule(QRegExp("(?:^usemtl\\s*)([\\w-\\.]+)"), formatYellow_);
    AddRule(QRegExp("(?:^mtllib\\s*)([\\w-\\.]+)"), formatYellow_);
    
    // Comments, including "#".
    AddCommentRules(true);
}

// RocketMtlHighlighter

RocketMtlHighlighter::RocketMtlHighlighter(QTextDocument *doc) :
    IRocketSyntaxHighlighter(doc)
{
    // Material
    QStringList regexps;
    regexps << "(^newmtl) ";

    foreach (const QString &regexp, regexps)
        AddRule(QRegExp(regexp), formatPurple_);

    // Colors
    QStringList keywords;
    keywords << "Ka" << "Kd" << "Ks" << "Tf";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("^(" + keyword + ")\\b"), formatOrange_);

    keywords.clear();
    keywords << "illum" << "d" << "Ns" << "sharpness" << "Ni";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("^(" + keyword + ") \\b"), formatPink_);

    // Textures
    keywords.clear();
    keywords << "map_Ka" << "map_Kd" << "map_Ks" << "map_Ns" << "map_d" << "disp" << "decal" << "bump" << "refl";

    foreach (const QString &keyword, keywords)
        AddRule(QRegExp("^(" + keyword + ") \\b"), formatBlue_);

    AddRule(QRegExp("(?:^newmtl\\s*)([\\w-\\.]+)"), formatYellow_);

    // Comments, including "#".
    AddCommentRules(true);
}

// RocketXmlHighlighter

RocketXmlHighlighter::RocketXmlHighlighter(QTextDocument *doc) :
    IRocketSyntaxHighlighter(doc)
{   
    // Capture word after starting "<"
    AddRule(QRegExp("(?:^\\s*<(?!!)\\/?)(\\w*)(?:.*>)"), formatPink_);
    AddRule(QRegExp("(\\w*)(?:=)"), formatGreen_);
    
    AddQuotedStringRules();
}
