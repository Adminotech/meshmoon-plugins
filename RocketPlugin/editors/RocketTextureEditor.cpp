/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketTextureEditor.h"

#include "RocketPlugin.h"
#include "MeshmoonAsset.h"
#include "storage/MeshmoonStorageItem.h"
#include "utils/RocketFileSystem.h"

#include "Framework.h"
#include "Application.h"
#include "AssetAPI.h"
#include "IAsset.h"

RocketTextureEditor::RocketTextureEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource) :
    IRocketAssetEditor(plugin, resource, "Texture Viewer", resource->suffix.toUpper()),
    scaleFactor_(1.0),
    mipmaps_(-1),
    firstResizeDone_(false),
    converterProcess_(0)
{
    // If we implement editing capabilities to this editor, remove disabling saving from this overload only!
    SetSaveEnabled(false);
    InitUi();
}

RocketTextureEditor::RocketTextureEditor(RocketPlugin *plugin, MeshmoonAsset *resource) :
    IRocketAssetEditor(plugin, resource, "Texture Viewer", QFileInfo(resource->filename).suffix().toUpper()),
    scaleFactor_(1.0),
    mipmaps_(-1),
    firstResizeDone_(false),
    converterProcess_(0)
{
    SetSaveEnabled(false);
    InitUi();
}

RocketTextureEditor::~RocketTextureEditor()
{
    CleanupConversion();
}

void RocketTextureEditor::InitUi()
{
    imageLabel_ = new QLabel();
    //imageLabel_->setBackgroundRole(QPalette::Base);
    imageLabel_->setStyleSheet("background-color: transparent; border: 1px outset #C4C4C3; border-top: 0px; border-left: 0px;");
    imageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel_->setScaledContents(true);
    imageLabel_->setFrameStyle(QFrame::NoFrame);

    scrollArea_ = new QScrollArea();
    //scrollArea_->setBackgroundRole(QPalette::Dark);
    scrollArea_->setStyleSheet("background-color: rgb(239, 243, 252);");
    scrollArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollArea_->setFrameStyle(QFrame::NoFrame);
    scrollArea_->installEventFilter(this);
    scrollArea_->horizontalScrollBar()->installEventFilter(this);
    scrollArea_->verticalScrollBar()->installEventFilter(this);
    scrollArea_->setWidget(imageLabel_);

    imageLabel_->hide();
    ui.contentLayout->addWidget(scrollArea_);
    resize(525, 570); // Fits 512x512 with some padding

    // Menus
    QMenu *viewMenu = MenuBar()->addMenu("View");
    QAction *zoomIn = viewMenu->addAction("Zoom In");
    QAction *zoomOut = viewMenu->addAction("Zoom Out");
    QAction *zoomReset = viewMenu->addAction("Actual Size");

    zoomIn->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_Plus));
    zoomIn->setShortcutContext(Qt::WindowShortcut);
    zoomOut->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_Minus));
    zoomOut->setShortcutContext(Qt::WindowShortcut);
    zoomReset->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_0));
    zoomReset->setShortcutContext(Qt::WindowShortcut);

    connect(zoomIn, SIGNAL(triggered()), SLOT(ZoomIn()));
    connect(zoomOut, SIGNAL(triggered()), SLOT(ZoomOut()));
    connect(zoomReset, SIGNAL(triggered()), SLOT(ZoomReset()));
}

QStringList RocketTextureEditor::SupportedSuffixes()
{
    QStringList suffixes;
    
    // Qt image reader supported formats.
    foreach(QByteArray qtFormat, QImageReader::supportedImageFormats())
        suffixes << qtFormat.toLower();

    // DDS and CRN with Crunch
    suffixes << "dds" << "crn";

    return suffixes;
}

QString RocketTextureEditor::EditorName()
{
    return "Texture Viewer";
}

void RocketTextureEditor::CleanupConversion()
{
    if (converterProcess_)
    {
        if (converterProcess_->state() != QProcess::NotRunning)
            converterProcess_->kill();
        converterProcess_->deleteLater();
    }
    converterProcess_ = 0;

    if (!tempInPath_.isEmpty() && QFile::exists(tempInPath_))
        QFile::remove(tempInPath_);
    if (!tempOutPath_.isEmpty() && QFile::exists(tempOutPath_))
        QFile::remove(tempOutPath_);
        
    tempInPath_ = "";
    tempOutPath_ = "";
}

void RocketTextureEditor::ResourceDownloaded(QByteArray data)
{
    // Qt image reader supported formats.
    QStringList qtSuffixes;
    foreach(QByteArray qtFormat, QImageReader::supportedImageFormats())
        qtSuffixes << qtFormat.toLower();

    if (qtSuffixes.contains(suffix, Qt::CaseInsensitive) || qtSuffixes.contains(completeSuffix, Qt::CaseInsensitive))
    {
        QByteArray extHint = qtSuffixes.contains(suffix, Qt::CaseInsensitive) ? suffix.toUpper().toAscii() : completeSuffix.toUpper().toAscii();
        image_ = QImage::fromData(data, extHint.constData());

        if (!image_.isNull())
        {
            // Load image
            imageLabel_->setPixmap(QPixmap::fromImage(image_));
            ZoomReset();            
        }
        else
            SetError("Failed to load image");
    }
    else if (suffix == "dds" || suffix == "crn")
    {
        CleanupConversion();

        // This should be set to "DXT1" etc. if detection is possible.
        formatOverride_ = "Unknown";

        SetMessage("Loading texture...");

        /// @todo Enable png when crunch support writing it.
        tempInPath_ = plugin_->GetFramework()->Asset()->GenerateTemporaryNonexistingAssetFilename(filename);
        tempOutPath_ = plugin_->GetFramework()->Asset()->GenerateTemporaryNonexistingAssetFilename(filename + ".bmp");

        // Store storage data to disk.
        QFile file(tempInPath_);
        if (!file.open(QFile::WriteOnly))
        {
            SetError("Failed to load image");
            return;        
        }
        file.write(data);
        file.close();

        // Conversion params
        QStringList params;
        params << "-noprogress";
        params << "-fileformat" << "bmp";
        params << "-file" << QDir::toNativeSeparators(tempInPath_);
        params << "-out"  << QDir::toNativeSeparators(tempOutPath_);

        // Run the process
        converterProcess_ = new QProcess();
        connect(converterProcess_, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(OnCrunchProcessFinished(int, QProcess::ExitStatus)));
        converterProcess_->start(QDir::toNativeSeparators(RocketFileSystem::InternalToolpath(RocketFileSystem::Crunch)), params);
    }
}

void RocketTextureEditor::OnCrunchProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (exitCode != 0 || status == QProcess::CrashExit || !converterProcess_)
    {
        SetError("Failed to load image. Conversion failed.");
        if (IsLogChannelEnabled(LogChannelDebug))
        {
            QByteArray stdoutDump = converterProcess_->readAllStandardOutput();
            qDebug() << "Dumping crunch stdout after failed conversion:" << endl << stdoutDump.data();
        }
        CleanupConversion();
        return;
    }
    
    // Try detecting mipmaps and format from the Crunch stdout.
    QByteArray output = converterProcess_->readAllStandardOutput();
    int iStart = output.indexOf("Input texture:");
    if (iStart != -1)
    {
        int iLevelsStart = output.indexOf("Levels: ", iStart+14);
        int iLevelsStop = iLevelsStart != -1 ? output.indexOf(",", iLevelsStart+8) : -1;
        if (iLevelsStart != -1 && iLevelsStop != -1)
        {
            QByteArray levelsStr = output.mid(iLevelsStart+8, iLevelsStop-(iLevelsStart+8)).trimmed();
            if (!levelsStr.isEmpty())
            {
                bool ok = false;
                mipmaps_ = levelsStr.toInt(&ok);
                if (!ok) mipmaps_ = -1;
            }
        }

        int iFormatStart = output.indexOf("Format: ", iStart+14);
        if (iFormatStart != -1)
        {
            int iFormatStop1 = output.indexOf('\n', iFormatStart+8);
            int iFormatStop2 = output.indexOf("\r\n", iFormatStart+8);
            int iFormatStop = iFormatStop1;
            if (iFormatStop2 != -1 && iFormatStop2 < iFormatStop)
                iFormatStop = iFormatStop2;
            if (iFormatStop != -1)
                formatOverride_ = output.mid(iFormatStart+8, iFormatStop-(iFormatStart+8)).trimmed();
        }
    }

    // Check if output file was crated.
    if (!tempOutPath_.isEmpty() && QFile::exists(QDir::fromNativeSeparators(tempOutPath_)))
    {
        // Open output file that should be now in a Qt supported format and load it.
        QFile outFile(QDir::fromNativeSeparators(tempOutPath_));
        if (outFile.open(QFile::ReadOnly))
        {
            QByteArray outData = outFile.readAll();
            outFile.close();

            QByteArray suffix = QFileInfo(outFile.fileName()).suffix().toUpper().toAscii();
            image_ = QImage::fromData(outData, suffix.constData());

            if (!image_.isNull())
            {
                // Load image
                imageLabel_->setPixmap(QPixmap::fromImage(image_));
                ZoomReset();
            }
            else
                SetError("Failed to load image after conversion");
        }
    }
    else
        SetError("Failed to locate image after conversion");
        
    CleanupConversion();
}

IRocketAssetEditor::SerializationResult RocketTextureEditor::ResourceData()
{
    // Saving is not supported, only a image viewer at this point.
    IRocketAssetEditor::SerializationResult result;
    result.first = false;
    return result;
}

void RocketTextureEditor::keyPressEvent(QKeyEvent *e)
{   
    QWidget::keyPressEvent(e);
}

void RocketTextureEditor::wheelEvent(QWheelEvent *e)
{
    QWidget::wheelEvent(e);
}

bool RocketTextureEditor::eventFilter(QObject *o, QEvent *e)
{
    if (scrollArea_ && (o == scrollArea_ || o == scrollArea_->horizontalScrollBar() || o == scrollArea_->verticalScrollBar()))
    {
        if (e->type() == QEvent::Wheel)
        {
            QWheelEvent *we = dynamic_cast<QWheelEvent*>(e);
            if (we && we->modifiers() == Qt::ControlModifier)
            {
                if (we->delta() > 0)
                    ZoomIn();
                else
                    ZoomOut();
                we->accept();
                return true;
            }
        }
    }
    return false;
}

void RocketTextureEditor::RefreshInformation()
{
    if (!image_.isNull())
    {
        QString colorStart = "<span style='color: rgb(210,210,210);'>";
        QString info = QString(colorStart + "Size</span> %1 x %2 " +
                               colorStart + "Format</span> %3 " +
                               colorStart + "Zoom</span> %4% ")
            .arg(image_.width()).arg(image_.height()).arg(!formatOverride_.isEmpty() ? formatOverride_ : ImageFormatToString(image_.format())).arg(ZoomPercent(), 0, 'f', 0);
        if (mipmaps_ >= 0)
            info += QString(colorStart + "Mipmaps</span> %1").arg(mipmaps_);
        SetMessage(info);
        
        if (!firstResizeDone_)
        {
            firstResizeDone_ = true;
            
            // Apply padding to image size and don't go over the 
            // default set size that fits a 512x512 image nicely.
            QSize newSize(image_.width() + 13, image_.height() + 58);
            if (newSize.width() > width())
                newSize.setWidth(width());
            else if (newSize.width() < 200)
                newSize.setWidth(200);
            if (newSize.height() > height())
                newSize.setHeight(height());            
            else if (newSize.height() < 200)
                newSize.setHeight(200);
            resize(newSize);
        }
    }
    else
        ClearMessage();
}

void RocketTextureEditor::ZoomIn()
{
    ScaleImage(1.25);
}

void RocketTextureEditor::ZoomOut()
{
    ScaleImage(0.8);
}

void RocketTextureEditor::ZoomReset()
{
    scaleFactor_ = 1.0;
    ScaleImage(1.0);
}

double RocketTextureEditor::ZoomPercent()
{
    return scaleFactor_*100.0;
}

void RocketTextureEditor::ScaleImage(double factor)
{
    if (image_.isNull())
        return;

    if (!imageLabel_->isVisible())
        imageLabel_->show();

    // 5% - 1000% range
    if ((factor < 1.0 && ZoomPercent() <= 5) || (factor > 1.0 && ZoomPercent() >= 1000))
        return;

    scaleFactor_ *= factor;
    if (scaleFactor_ > 10.0)
        scaleFactor_ = 10.0;
    else if (scaleFactor_ < 0.05)
        scaleFactor_ = 0.05;

    imageLabel_->resize(scaleFactor_ * imageLabel_->pixmap()->size());

    AdjustScrollBar(scrollArea_->horizontalScrollBar(), scaleFactor_);
    AdjustScrollBar(scrollArea_->verticalScrollBar(), scaleFactor_);

    RefreshInformation();
}

void RocketTextureEditor::AdjustScrollBar(QScrollBar *scrollBar, double factor)
{
    if (scrollBar)
        scrollBar->setValue(int(factor * scrollBar->value() + ((factor - 1) * scrollBar->pageStep()/2)));
}

QString RocketTextureEditor::ImageFormatToString(QImage::Format format)
{
    switch(format)
    {
        case QImage::Format_Invalid:
            return "Invalid";
        case QImage::Format_Mono: 
        case QImage::Format_MonoLSB:
            return "1-bit Mono";
        case QImage::Format_Indexed8:
            return "8-bit";
        case QImage::Format_RGB16:
        case QImage::Format_RGB444:
        case QImage::Format_RGB555:  
            return "16-bit RGB";
        case QImage::Format_ARGB4444_Premultiplied:
            return "16-bit ARGB";
        case QImage::Format_ARGB8565_Premultiplied:
        case QImage::Format_ARGB6666_Premultiplied:
        case QImage::Format_ARGB8555_Premultiplied:
            return "24-bit ARGB";
        case QImage::Format_RGB666:
        case QImage::Format_RGB888:
            return "24-bit RGB";
        case QImage::Format_RGB32:
            return "32-bit RGB";
        case QImage::Format_ARGB32:
        case QImage::Format_ARGB32_Premultiplied:
            return "32-bit ARGB";
        default:
            break;
    }
    return "";
}
