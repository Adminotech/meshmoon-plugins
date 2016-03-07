/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "IRocketAssetEditor.h"

#include <QProcess>

class QLabel;
class QScrollArea;
class QScrollBar;

/// Basic texture viewer.
class RocketTextureEditor : public IRocketAssetEditor
{
    Q_OBJECT

public:
    /// Create from existing and valid storage item.
    RocketTextureEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource);
    
    /// Create view-only editor from a generic asset.
    RocketTextureEditor(RocketPlugin *plugin, MeshmoonAsset *resource);

    /// Dtor.
    ~RocketTextureEditor();

    /// Get all supported suffixes
    /** @note The suffixes wont have the first '.' prefix, as "png". */
    static QStringList SupportedSuffixes();

    /// Returns editor name.
    static QString EditorName();

    /// QObject override.
    virtual bool eventFilter(QObject *o, QEvent *e);

protected:
    /// IRocketAssetEditor override.
    virtual void ResourceDownloaded(QByteArray data);

    /// IRocketAssetEditor override.
    virtual IRocketAssetEditor::SerializationResult ResourceData();

    /// QWidget override
    void keyPressEvent(QKeyEvent *e);

    /// QWidget override
    void wheelEvent(QWheelEvent *e);

private slots:
    void OnCrunchProcessFinished(int exitCode, QProcess::ExitStatus status);

    void RefreshInformation();

    void ZoomIn();
    void ZoomOut();
    void ZoomReset();
    double ZoomPercent();
    
    void ScaleImage(double factor);
    void AdjustScrollBar(QScrollBar *scrollBar, double factor);

    static QString ImageFormatToString(QImage::Format format);

private:
    void InitUi();
    void CleanupConversion();
    
    QLabel *imageLabel_;
    QScrollArea *scrollArea_;
    
    QImage image_;
    QByteArray imageData_;
    
    QString formatOverride_;
    int mipmaps_;
    
    QProcess *converterProcess_;
    
    QString tempInPath_;
    QString tempOutPath_;
    
    double scaleFactor_;
    
    bool firstResizeDone_;
};
