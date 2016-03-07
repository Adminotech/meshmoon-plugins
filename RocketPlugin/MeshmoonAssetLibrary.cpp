/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief  */

#include "StableHeaders.h"
#include "Framework.h"
#include "CoreJsonUtils.h"
#include "ConsoleAPI.h"
#include "LoggingFunctions.h"

#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAssetTransfer.h"
#include "IAsset.h"

#include "UiAPI.h"
#include "UiMainWindow.h"

#include "Math/MathFunc.h"

#include "MeshmoonAssetLibrary.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include <QUuid>
#include <QGridLayout>

// MeshmoonAssetLibrary

MeshmoonAssetLibrary::MeshmoonAssetLibrary(RocketPlugin *plugin) :
    LC("[MeshmoonAssetLibrary]: "),
    plugin_(plugin)
{
    // Add default Meshmoon asset library.
    sources_ << MeshmoonLibrarySource("MeshmoonAssetLibrary", "http://meshmoon.data.s3.amazonaws.com/asset-library/meshmoon-library.json");
}

MeshmoonAssetLibrary::~MeshmoonAssetLibrary()
{
    ClearLibrary();
    sources_.clear();
}

void MeshmoonAssetLibrary::DumpAssetInformation() const
{
    qDebug() << endl << "[MeshmoonAssetLibrary]: Dumping " + QString::number(assets_.size()) + " assets to console" << endl;
    foreach(MeshmoonAsset *asset, assets_)
    {
        qDebug() << asset->name << "-" << asset->ResourceType() << endl
                 << "  Desc :" << asset->description.toStdString().c_str() << endl
                 << "  Ref  :" << asset->assetRef.toStdString().c_str() << endl
                 << "  Img  :" << asset->previewImageUrl.toStdString().c_str();
        if (!asset->metadata.isEmpty())
        {
            qDebug() << "  Metadata";
            foreach(const QString &key, asset->metadata.keys())
                qDebug() << "    " << key.toStdString().c_str() << "=" << asset->metadata[key];
        }
        if (!asset->tags.isEmpty())
        {
            qDebug() << "  Tags";
            foreach(const QString &tag, asset->tags)
                qDebug() << "    " << tag;
        }
    }
}

void MeshmoonAssetLibrary::UpdateLibrary()
{
    // If we already have assets we will trigger the completed signal with a short delay.
    // If the using code wants to update the library from the web, he needs to call ClearLibrary first.
    if (!assets_.isEmpty())
    {
        QTimer::singleShot(1, this, SLOT(EmitUpdated()));
        return;
    }

    // We are already doing a update, wait for it.
    if (!pendingSources_.isEmpty())
        return;
    
    foreach(const MeshmoonLibrarySource source, sources_)
    {
        plugin_->GetFramework()->Asset()->ForgetAsset(source.second, true);
        AssetTransferPtr transfer = plugin_->GetFramework()->Asset()->RequestAsset(source.second, "Binary", true);
        if (transfer.get())
        {
            connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(OnSourceCompleted(AssetPtr)), Qt::UniqueConnection);
            connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), this, SLOT(OnSourceFailed(IAssetTransfer*, QString)), Qt::UniqueConnection);
            pendingSources_ << source;
        }
    }
}

void MeshmoonAssetLibrary::ClearLibrary()
{
    foreach (MeshmoonAsset *asset, assets_)
        SAFE_DELETE(asset);
    assets_.clear();
}

bool MeshmoonAssetLibrary::HasAssets() const
{
    return !assets_.isEmpty();
}

bool MeshmoonAssetLibrary::HasSource(const QString &sourceUrl) const
{
    return !Source(sourceUrl).second.isEmpty();
}

bool MeshmoonAssetLibrary::AddSource(const QString &sourceName, const QString &sourceUrl)
{
    return AddSource(MeshmoonLibrarySource(sourceName, sourceUrl));
}

bool MeshmoonAssetLibrary::AddSource(const MeshmoonLibrarySource &source)
{
    if (source.second.isEmpty())
    {
        LogWarning(LC + "Library source for your '" + source.first + "' is an empty url.");
        return false;
    }
    foreach(const MeshmoonLibrarySource existing, sources_)
    {
        if (existing.second == source.second)
        {
            LogWarning(LC + "Library source '" + source.second + "' already exists as '" + existing.first + "'");
            return false;
        }
    }    
    sources_ << source;

    pendingSources_.clear();
    ClearLibrary();
    UpdateLibrary();

    return true;
}

QStringList MeshmoonAssetLibrary::AvailableAssetTags(const MeshmoonLibrarySource &source) const
{
    QStringList tags;
    foreach (MeshmoonAsset *asset, assets_)
    {
        if (!asset || asset->tags.isEmpty())
            continue;
        bool sourceMatches = (source.second.isEmpty() ? true : asset->source.second == source.second);
        if (sourceMatches)
        {
            foreach(const QString &tag, asset->tags)
            {
                QString tagTrimmed = tag.trimmed().toLower();
                if (!tags.contains(tagTrimmed, Qt::CaseInsensitive))
                    tags << tagTrimmed;
            }
        }
    }
    return tags;
}

MeshmoonLibrarySourceList MeshmoonAssetLibrary::Sources() const
{
    return sources_;
}

MeshmoonAssetList MeshmoonAssetLibrary::Assets() const
{
    return assets_;
}

MeshmoonAssetList MeshmoonAssetLibrary::Assets(MeshmoonAsset::Type type, const MeshmoonLibrarySource &source) const
{
    MeshmoonAssetList ret;
    foreach(MeshmoonAsset *asset, assets_)
    {
        if (!asset)
            continue;
        bool typeMatches = (type == MeshmoonAsset::All ? true : asset->type == type);
        bool sourceMatches = (source.second.isEmpty() ? true : asset->source.second == source.second);
        if (typeMatches && sourceMatches)
            ret << asset;
    }
    return ret;
}

MeshmoonAsset *MeshmoonAssetLibrary::FindAsset(const QString &ref) const
{
    if (ref.trimmed().isEmpty())
        return 0;
    QHash<QString, QPointer<MeshmoonAsset> >::const_iterator it = assetsByRef_.find(ref);
    return (it != assetsByRef_.end() ? it.value() : 0);
}

MeshmoonAssetSelectionDialog *MeshmoonAssetLibrary::ShowSelectionDialog(MeshmoonAsset::Type type, const MeshmoonLibrarySource &source, const QString &forcedTag, QWidget *parent)
{
    if (!dialog_.isNull())
    {
        LogWarning(LC + "Already showing a asset selection dialog!");
        return 0;
    }
    return ShowSelectionDialog(Assets(type, source), forcedTag, parent);
}

MeshmoonAssetSelectionDialog *MeshmoonAssetLibrary::ShowSelectionDialog(const MeshmoonAssetList &assets, const QString &forcedTag, QWidget *parent)
{
    if (!dialog_.isNull())
    {
        LogWarning(LC + "Already showing a asset selection dialog!");
        return 0;
    }
    dialog_ = new MeshmoonAssetSelectionDialog(plugin_, assets, forcedTag, parent);
    return dialog_.data();
}

QPushButton *MeshmoonAssetLibrary::CreateAssetWidget(MeshmoonAsset *asset, const QSize &size, bool createName, bool createDescription, const QString &tooltip,
                                                     const QString &nameOverride, const QString &descriptionOverride, QColor backgroundColor, int fixedNameContainerHeight)
{
    QString name = "";
    QString description = "";
    QString previewImageUrl = "";

    QString id = QUuid::createUuid().toString().replace("{", "").replace("}", "");

    QPushButton *button = new QPushButton();
    button->setProperty("meshmoonLibraryId", id);
    button->setObjectName("button-" + id);
    button->setFixedSize(size);
    button->setMinimumSize(size);
    button->setCheckable(true);
    button->setChecked(true);
    if (!tooltip.trimmed().isEmpty())
        button->setToolTip(tooltip);
    
    if (asset)
    {
        name = asset->name;
        description = asset->description;
        previewImageUrl = asset->previewImageUrl;

        button->setProperty("asset", QVariant::fromValue<MeshmoonAsset*>(asset));
    }
    if (createName && !nameOverride.isEmpty())
        name = nameOverride;
    if (name.isEmpty())
        name = "Click to select...";
    if (createDescription && !descriptionOverride.isEmpty())
        description = descriptionOverride;

    // Skip description if it is empty.
    if (createDescription && description.trimmed().isEmpty())
        createDescription = false;

    QVBoxLayout *frameL = 0;
    QFrame *frame = 0;
    
    // Info container
    if (createName || createDescription)
    {
        frameL = new QVBoxLayout();
        frameL->setContentsMargins(5,2,5,2);
        frameL->setSpacing(2);

        frame = new QFrame(button, Qt::Widget);
        frame->setObjectName("assetFrame" + id);
        frame->setFixedSize(size.width() - 4, fixedNameContainerHeight > 0 ? fixedNameContainerHeight : (createName && createDescription ? 35 : 25));
        frame->move(2, size.height() - frame->height() - 2);
    }

    // Name
    if (createName)
    {
        QLabel *label = new QLabel(name);
        label->setObjectName("nameLabel");
        label->setStyleSheet("color: rgb(70,70,70); font-size: 15px; padding: 0px;");
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        frameL->addWidget(label);
    }

    // Description
    if (createDescription)
    {
        QString descStr = description;
        if (descStr.length() > 35)
            descStr = descStr.left(32) + "...";

        QLabel *desc = new QLabel(descStr);
        desc->setObjectName("descLabel");
        desc->setStyleSheet("color: rgb(90,90,90); font-size: 11px; padding: 0px; padding-left: 1px;");
        desc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        desc->setIndent(0);
        frameL->addWidget(desc);
    }

    if (frame && frameL)
        frame->setLayout(frameL);

    // Preview image
    QString imagePath;
    if (!previewImageUrl.isEmpty())
        imagePath = plugin_->GetFramework()->Asset()->Cache()->FindInCache(previewImageUrl);
    if (imagePath.isEmpty())
    {
        // Look for rgba color in metadata
        if (asset && asset->metadata.contains("rgba"))
        {
            QVariantList rgbaParts = asset->metadata["rgba"].toList();
            if (rgbaParts.size() >= 3)
            {
                QString outputPath = QDir(plugin_->GetFramework()->Asset()->Cache()->CacheDirectory())
                    .absoluteFilePath(QString("asset-library-rgba_%1-%2-%3-%4_%5x%6.png")
                    .arg(rgbaParts[0].toInt()).arg(rgbaParts[1].toInt()).arg(rgbaParts[2].toInt()).arg(rgbaParts.size() >= 4 ? rgbaParts[3].toInt() : 255)
                    .arg(size.width()).arg(size.height()));
                
                if (!QFile::exists(outputPath))
                {
                    QPixmap pixmap(size);
                    pixmap.fill(QColor(rgbaParts[0].toInt(), rgbaParts[1].toInt(), rgbaParts[2].toInt(), rgbaParts.size() >= 4 ? rgbaParts[3].toInt() : 255));
                    if (pixmap.save(outputPath, "PNG"))
                        imagePath = outputPath;
                }
                else
                    imagePath = outputPath;
            }
        }
        else
            imagePath = asset ? ":/images/asset-library-no-preview.png" : "";
    }

    // Image is valid?
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) 
    {
        // Image needs to be scaled?
        if (pixmap.width() != size.width() || pixmap.height() != size.height())
        {
            // Image still valid?
            pixmap = pixmap.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            if (!pixmap.isNull())
            {
                QString outputPath;
                if (imagePath.startsWith(":"))
                    outputPath = QDir(plugin_->GetFramework()->Asset()->Cache()->CacheDirectory()).absoluteFilePath("asset-library-no-preview");
                else
                    outputPath = imagePath.mid(0, imagePath.lastIndexOf("."));
                outputPath += QString("_%1x%2").arg(size.width()).arg(size.height()) + ".png";

                // Scaled version already exists?
                if (!QFile::exists(outputPath))
                {
                    // Saving scaled image ok?
                    if (pixmap.save(outputPath, "PNG"))
                        imagePath = outputPath;
                }
                else 
                    imagePath = outputPath;
            }
            else
                imagePath = ":/images/asset-library-no-preview.png";
        }
    }

    // If a background color was given and we are defaulting to 'no preview' image, reset and use the color.
    if (backgroundColor.isValid() && imagePath.contains("asset-library-no-preview", Qt::CaseSensitive))
        imagePath = "";

    QString backgroundImageProp = (imagePath.isEmpty() ? 
        (backgroundColor.isValid() ? QString("background-color: rgba(%1,%2,%3,%4);").arg(backgroundColor.red()).arg(backgroundColor.green()).arg(backgroundColor.blue()).arg(backgroundColor.alpha()) 
                                   : "background-color: rgba(96,96,96,150);") : "background-image: url('" + imagePath + "'); background-position: center center;");

    button->setStyleSheet(
        // Normal
        QString("QPushButton#%1 { ").arg(button->objectName()) +
        QString("border: 2px solid rgb(207, 207, 207, 200); border-radius: 6px; max-height: %1px; min-height: %1px; max-width: %2px; min-width: %2px;").arg(size.height()-4).arg(size.width()-4) +
        QString(backgroundImageProp + " }") +
        // Hover
        QString("QPushButton#%1:hover { ").arg(button->objectName()) +
        QString("border: 2px solid rgb(170, 170, 170, 200); border-radius: 6px;") +
        QString(backgroundImageProp + " }") +
        // Selected
        QString("QPushButton#%1:checked { ").arg(button->objectName()) +
        QString("border: 2px solid rgb(243, 154, 41); border-radius: 6px;") +
        QString(backgroundImageProp + " }") +
        // Frame
        QString("QFrame#assetFrame%1 { background-color: rgba(200,200,200,200); ").arg(id) +
        QString("border: 0px; border-top: 1px solid rgb(150, 150, 150); border-radius: 0px; }")
    );
        
    return button;
}

void MeshmoonAssetLibrary::DeserializeAssetLibrary(const QString &sourceUrl, const QByteArray &data)
{
    bool ok = false;
    QVariantMap map = TundraJson::Parse(data, &ok).toMap();
    if (!ok)
    {
        LogError(LC + "Failed to parse asset library data from source: " + sourceUrl);
        return;
    }
    if (map.size() == 0)
    {
        LogWarning(LC + "Asset library data does not contain any items: " + sourceUrl);
        return;
    }
    
    MeshmoonLibrarySource source = Source(sourceUrl);
    if (source.second.isEmpty())
    {
        LogError(LC + "Failed to parse find loaded source: " + sourceUrl);
        return;
    }
    QString baseUrl = source.second;
    baseUrl = baseUrl.left(baseUrl.lastIndexOf("/")+1);

    // Meshes
    if (map.contains("meshes"))
    {
        QVariantList meshes = map["meshes"].toList();
        foreach(QVariant meshObj, meshes)
        {
            MeshmoonAsset *asset = DeserializeAsset(MeshmoonAsset::Mesh, source, baseUrl, meshObj);
            if (asset)
            {
                assets_.push_back(asset);
                assetsByRef_.insert(asset->assetRef, asset);
            }
        }
    }

    // Materials
    if (map.contains("materials"))
    {
        QVariantList materials = map["materials"].toList();
        foreach(QVariant materialObj, materials)
        {
            MeshmoonAsset *asset = DeserializeAsset(MeshmoonAsset::Material, source, baseUrl, materialObj);
            if (asset)
            {
                assets_.push_back(asset);
                assetsByRef_.insert(asset->assetRef, asset);
            }
        }
    }

    // Textures
    if (map.contains("textures"))
    {
        QVariantList textures = map["textures"].toList();
        foreach(QVariant textureObj, textures)
        {
            MeshmoonAsset *asset = DeserializeAsset(MeshmoonAsset::Texture, source, baseUrl, textureObj);
            if (asset)
            {
                assets_.push_back(asset);
                assetsByRef_.insert(asset->assetRef, asset);
            }
        }
    }
}

MeshmoonAsset *MeshmoonAssetLibrary::DeserializeAsset(MeshmoonAsset::Type type, const MeshmoonLibrarySource &source, const QString &baseUrl, const QVariant &data)
{
    if (!data.isValid() || data.isNull())
        return 0;

    QVariantMap objData = data.toMap();
    if (objData.isEmpty())
        return 0;

    if (!objData.contains("assetRef") || !objData.contains("name"))
    {
        LogWarning(LC + "Library asset is missing 'assetRef' and/or 'name', skipping...");
        return 0;
    }

    // Create asset
    MeshmoonAsset *asset = new MeshmoonAsset(source, objData["name"].toString(), objData["assetRef"].toString(), type);

    // Desc
    if (objData.contains("description"))
        asset->description = objData["description"].toString();

    // Preview image
    if (objData.contains("preview") && !objData["preview"].toString().isEmpty())
        asset->previewImageUrl = baseUrl + objData["preview"].toString();

    // Tags
    if (objData.contains("tags"))
        asset->tags = objData["tags"].toStringList();

    // Metadata
    if (objData.contains("metadata"))
    {
        asset->metadata = objData["metadata"].toMap();

        // Extract tags from metadata
        if (type == MeshmoonAsset::Mesh)
        {
            if (asset->metadata.contains("submeshes"))
            {
                bool ok = false;
                int submeshCount = asset->metadata["submeshes"].toInt(&ok);
                if (ok && submeshCount >= 1)
                    asset->tags << QString("%1 submesh").arg(submeshCount);
            }
        }
    }

    return asset;
}

void MeshmoonAssetLibrary::SourceCompleted(const QString &sourceUrl)
{
    for(int i=0; i< pendingSources_.size(); ++i)
    {
        MeshmoonLibrarySource &source = pendingSources_[i];
        if (source.second == sourceUrl)
        {
            LogDebug(LC + "Library source update completed: " + sourceUrl);
            pendingSources_.removeAt(i);
            break;
        }
    }
    
    // Emit completed signal when all sources have completed
    if (pendingSources_.isEmpty())
    {
        emit LibraryUpdated(assets_);
    }
}

MeshmoonLibrarySource MeshmoonAssetLibrary::Source(const QString &sourceUrl) const
{
    for(int i=0; i<sources_.size(); ++i)
        if (pendingSources_[i].second == sourceUrl)
            return pendingSources_[i];
    return MeshmoonLibrarySource("", "");
}

MeshmoonLibrarySource MeshmoonAssetLibrary::SourceByName(const QString &sourceName) const
{
    for(int i=0; i< sources_.size(); ++i)
        if (pendingSources_[i].first == sourceName)
            return pendingSources_[i];
    return MeshmoonLibrarySource("", "");
}

void MeshmoonAssetLibrary::OnSourceCompleted(AssetPtr asset)
{
    QByteArray raw = asset->RawData();
    DeserializeAssetLibrary(asset->Name(), raw);
    SourceCompleted(asset->Name());
    
    QString assetRef = asset->Name();
    asset.reset();
    plugin_->GetFramework()->Asset()->ForgetAsset(assetRef, true);
}

void MeshmoonAssetLibrary::OnSourceFailed(IAssetTransfer *transfer, QString reason)
{
    SourceCompleted(transfer->SourceUrl());
}

void MeshmoonAssetLibrary::EmitUpdated()
{
    if (!assets_.isEmpty())
        emit LibraryUpdated(assets_);
}

// MeshmoonAssetSelectionDialog

MeshmoonAssetSelectionDialog::MeshmoonAssetSelectionDialog(RocketPlugin *plugin, const MeshmoonAssetList &_assets, const QString &forcedTag, QWidget *parent) :
    QDialog(parent),
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    assets(_assets),
    selected_(0),
    grid_(0)
{
    ui_.setupUi(this);
    ui_.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui_.lineEditFilter->installEventFilter(this);

    ui_.buttonSelect->setAutoDefault(false);
    ui_.buttonCancel->setAutoDefault(false);

    connect(ui_.buttonSelect, SIGNAL(clicked()), SLOT(OnSelect()), Qt::QueuedConnection);
    connect(ui_.buttonCancel, SIGNAL(clicked()), SLOT(reject()), Qt::QueuedConnection);
    connect(this, SIGNAL(rejected()), SLOT(OnCancel()));
    connect(ui_.lineEditFilter, SIGNAL(textChanged(const QString&)), SLOT(OnFilterChanged(const QString&)));
    
    ui_.comboBoxCategory->addItem("all");
    if (!forcedTag.trimmed().isEmpty() && forcedTag.trimmed().toLower() != "all")
    {
        currentTag_ = forcedTag.trimmed().toLower();
        ui_.comboBoxCategory->addItem(currentTag_);
        ui_.comboBoxCategory->setCurrentIndex(1);
    }
    connect(ui_.comboBoxCategory, SIGNAL(currentIndexChanged(const QString&)), SLOT(OnCategoryChanged(const QString&)));

    // Dialog setup
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowModality(parent != 0 ? Qt::WindowModal : Qt::ApplicationModal);
    setWindowFlags(parent != 0 ? Qt::Tool : Qt::SplashScreen);
    setWindowTitle(parent != 0 ? "Meshmoon Library Picker" : "");
    setModal(true);

    // Center to main window or to parent window.
    if (!parent)
    {
        plugin_->Notifications()->DimForeground();
        
        // Initial resize
        QPoint mainWindowPos = framework_->Ui()->MainWindow()->pos();
        QSize mainWindowSize = framework_->Ui()->MainWindow()->rect().size();
        QRect mainWindowRect(mainWindowPos, mainWindowSize);
        QPointF centerPos = mainWindowRect.center();

        int myWidth = mainWindowSize.width() - 100;

        QSize mySize(myWidth, FloorInt((float)mainWindowSize.height() / 2.0f));
        resize(mySize);
        move(centerPos.x() - mySize.width()/2, centerPos.y() - mySize.height()/2 + 25);
    }
    else
        plugin_->Notifications()->CenterToWindow(parent, this);

    // Show and activate
    show();
    setFocus(Qt::ActiveWindowFocusReason);
    activateWindow();

    // If this is a splash dialog animate opacity
    if (!parent)
    {
        setWindowOpacity(0.0);

        QPropertyAnimation *showAnim = new QPropertyAnimation(this, "windowOpacity", this);
        showAnim->setStartValue(0.0);
        showAnim->setEndValue(1.0);
        showAnim->setDuration(300);
        showAnim->setEasingCurve(QEasingCurve(QEasingCurve::InOutQuad)); 
        showAnim->start();
    }

    CreateAssetWidgets();
    UpdateGeometry();
    UpdateFiltering();
}

MeshmoonAsset *MeshmoonAssetSelectionDialog::Selected() const
{
    return selected_;
}

void MeshmoonAssetSelectionDialog::CreateAssetWidgets()
{    
    itemsPerRow_ = Min<int>(FloorInt((float)width() / 220.0f), 4); // Max items per row == 4
    if (itemsPerRow_ <= 0) itemsPerRow_ = 1;

    grid_ = new QGridLayout(ui_.scrollAreaWidgetContents);
    grid_->setHorizontalSpacing(6);
    grid_->setVerticalSpacing(6);
    ui_.scrollAreaWidgetContents->setLayout(grid_);
    
    int row = 0;
    int col = 0;
    foreach(MeshmoonAsset *asset, assets)
    {
        // Add selectable tags
        foreach(const QString &tag, asset->tags)
        {
            QString trimmedLower = tag.trimmed().toLower();
            if (ui_.comboBoxCategory->findText(trimmedLower, Qt::MatchExactly) == -1)
                ui_.comboBoxCategory->addItem(trimmedLower);
        }
        
        QString id = QUuid::createUuid().toString().replace("{", "").replace("}", "");
        
        QPushButton *button = new QPushButton();
        button->setObjectName("assetButton" + id);
        button->setFixedSize(200, 200);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        
        button->setProperty("assetId", id);
        button->setProperty("asset", QVariant::fromValue<MeshmoonAsset*>(asset));

        button->installEventFilter(this);
        
        connect(button, SIGNAL(toggled(bool)), SLOT(OnSelectionChanged()));

        QVBoxLayout *frameL = new QVBoxLayout();
        frameL->setContentsMargins(9,2,9,2);
        frameL->setSpacing(2);

        // Info container
        QFrame *frame = new QFrame(button, Qt::Widget);
        frame->setLayout(frameL);
        frame->setObjectName("assetFrame" + id);
        frame->setFixedSize(200 - 4, 40);
        frame->move(2, 200 - frame->height() - 2);

        // Name
        QLabel *label = new QLabel(asset->name);
        label->setStyleSheet("color: rgb(70,70,70); font-size: 16px;");
        frameL->addWidget(label);

        // Description
        QString descStr = asset->description;
        if (descStr.length() > 35)
            descStr = descStr.left(32) + "...";

        QLabel *desc = new QLabel(descStr);
        desc->setIndent(0);
        desc->setStyleSheet("color: rgb(90,90,90); font-size: 11px; padding: 0px; padding-left: 1px;");
        frameL->addWidget(desc);
        
        // Preview image
        QString imagePath = ":/images/asset-library-no-preview.png";
        if (!asset->previewImageUrl.isEmpty())
        {
            QString diskSource = framework_->Asset()->Cache()->FindInCache(asset->previewImageUrl);
            if (diskSource.isEmpty())
            {
                AssetTransferPtr imageTransfer = framework_->Asset()->RequestAsset(asset->previewImageUrl, "Binary");
                connect(imageTransfer.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(OnPreviewImageCompleted(AssetPtr)), Qt::UniqueConnection);
            }
            else
                imagePath = diskSource;
        }
        else
        {
            // Look for rgba color in metadata
            if (asset->metadata.contains("rgba"))
            {
                QVariantList rgbaParts = asset->metadata["rgba"].toList();
                if (rgbaParts.size() >= 3)
                {
                    QString outputPath = QDir(framework_->Asset()->Cache()->CacheDirectory())
                        .absoluteFilePath(QString("asset-library-rgba_%1-%2-%3-%4_200x200.png")
                        .arg(rgbaParts[0].toInt()).arg(rgbaParts[1].toInt()).arg(rgbaParts[2].toInt()).arg(rgbaParts.size() >= 4 ? rgbaParts[3].toInt() : 255));

                    if (!QFile::exists(outputPath))
                    {
                        QPixmap pixmap(200,200);
                        pixmap.fill(QColor(rgbaParts[0].toInt(), rgbaParts[1].toInt(), rgbaParts[2].toInt(), rgbaParts.size() >= 4 ? rgbaParts[3].toInt() : 255));
                        if (pixmap.save(outputPath, "PNG"))
                            imagePath = outputPath;
                    }
                    else
                        imagePath = outputPath;
                }
            }
        }
        
        UpdateButtonStyle(button, imagePath);

        grid_->addWidget(button, row, col);
        buttons_ << button;
        
        col++;
        if (col >= itemsPerRow_)
        {
            col = 0;
            row++;
        }
    }
}

void MeshmoonAssetSelectionDialog::UpdateGeometry()
{
    if (!grid_)
        return;
        
    int maxRows = (framework_->Ui()->MainWindow()->size().height() < 3 * 200 + 125 ? 2 : 3);

    int rows = Min<int>(grid_->rowCount(), maxRows); // max rows to show == 3
    int cols = grid_->columnCount();
    QMargins margins = grid_->contentsMargins();

    int dynHeight = (rows * 200) + (rows-1 * grid_->verticalSpacing()) + margins.top() + margins.bottom() + ui_.controlsFrame->height() + 30; // last is extra padding
    int dynWidth = (cols * 200) + (cols-1 * grid_->horizontalSpacing()) + margins.left() + margins.right() + 35; // last is extra padding

    if (dynHeight != height() || dynWidth != width())
    {
        QPoint mainWindowPos = framework_->Ui()->MainWindow()->pos();
        QSize mainWindowSize = framework_->Ui()->MainWindow()->rect().size();
        QRect mainWindowRect(mainWindowPos, mainWindowSize);
        QPointF centerPos = mainWindowRect.center();

        QSize newSize(dynWidth, dynHeight);
        resize(newSize);
        move(centerPos.x() - newSize.width()/2, centerPos.y() - newSize.height()/2 + 25);
    }
}

void MeshmoonAssetSelectionDialog::UpdateButtonStyle(QPushButton *button, const QString &imagePath)
{
    if (!button)
        return;

    QString id = button->property("assetId").toString();

    QString backgroundImageProp = imagePath.isEmpty() ? "" : "background-image: url('" + imagePath + "');";
    QString backgroundProp = "background-color: rgba(200,200,200,200); ";

    button->setStyleSheet(
        // Normal
        QString("QPushButton#%1 { ").arg(button->objectName()) +
        QString("border: 2px solid rgb(207, 207, 207, 200); border-radius: 6px;") +
        QString(backgroundImageProp + " }") +
        // Hover
        QString("QPushButton#%1:hover { ").arg(button->objectName()) +
        QString("border: 2px solid rgb(170, 170, 170, 200); border-radius: 6px;") +
        QString(backgroundImageProp + " }") +
        // Selected
        QString("QPushButton#%1:checked { ").arg(button->objectName()) +
        QString("border: 2px solid rgb(243, 154, 41); border-radius: 6px;") +
        QString(backgroundImageProp + " }") +
        // Frame
        QString("QFrame#assetFrame%1 { " + backgroundProp).arg(id) +
        QString("border: 0px; border-top: 1px solid rgb(150, 150, 150); border-radius: 0px; }")
    );
}

void MeshmoonAssetSelectionDialog::OnPreviewImageCompleted(AssetPtr asset)
{
    QString completedImage = asset->Name();
    QString diskSource = asset->DiskSource();
    if (!QFile::exists(diskSource))
        return;
    
    // Note: Don't break when first is found, many buttons can potentially have same preview image.
    foreach(QPushButton *button, buttons_)
    {
        if (!button)
            continue;
        
        MeshmoonAsset *asset = qvariant_cast<MeshmoonAsset*>(button->property("asset"));
        if (asset && asset->previewImageUrl == completedImage)
            UpdateButtonStyle(button, diskSource);
    }
}

void MeshmoonAssetSelectionDialog::OnFilterChanged(const QString &term)
{   
    currentTerm_ = term.trimmed().toLower();
    UpdateFiltering();
}

void MeshmoonAssetSelectionDialog::OnCategoryChanged(const QString &category)
{
    currentTag_ = category.trimmed().toLower();
    UpdateFiltering();
}

void MeshmoonAssetSelectionDialog::UpdateFiltering()
{
    if (!grid_)
        return;

    foreach(QPushButton *button, buttons_)
    {
        if (button->layout())
            grid_->removeWidget(button);
        button->hide();
    }
       
    int row = 0;
    int col = 0;

    bool doingFiltering = (!currentTerm_.isEmpty() || (!currentTag_.isEmpty() && currentTag_.toLower() != "all"));

    // Always show any selected asset as the first, if filtering is done
    bool selectedAdded = false;
    if (selected_ && doingFiltering)
    {
        foreach(QPushButton *button, buttons_)
        {
            MeshmoonAsset *asset = qvariant_cast<MeshmoonAsset*>(button->property("asset"));
            if (asset && asset == selected_)
            {
                selectedAdded = true;
                
                button->show();
                grid_->addWidget(button, row, col);
                
                col++;
                if (col >= itemsPerRow_)
                {
                    col = 0;
                    row++;
                }
                break;
            }
        }
    }
    
    foreach(QPushButton *button, buttons_)
    {
        if (!button)
            continue;
        MeshmoonAsset *asset = qvariant_cast<MeshmoonAsset*>(button->property("asset"));
        if (!asset)
            continue;

        bool showAsset = true;
        
        // Search term
        if (!currentTerm_.isEmpty())
        {
            if (!asset->name.contains(currentTerm_, Qt::CaseInsensitive) && !asset->description.contains(currentTerm_, Qt::CaseInsensitive))
                showAsset = false;       
        }

        // Category tag
        if (!currentTag_.isEmpty() && currentTag_ != "all")
        {
            if (!asset->ContainsTag(currentTag_))
                showAsset = false;
        }

        // Asset is already added
        if (showAsset && asset == selected_ && selectedAdded)
            showAsset = false; 
            
        if (showAsset)
        {
            button->show();
            grid_->addWidget(button, row, col);
            
            col++;
            if (col >= itemsPerRow_)
            {
                col = 0;
                row++;
            }
        }
    }
}

void MeshmoonAssetSelectionDialog::OnSelectionChanged()
{
    foreach(QPushButton *button, buttons_)
    {
        if (!button || !button->isChecked())
            continue;

        MeshmoonAsset *asset = qvariant_cast<MeshmoonAsset*>(button->property("asset"));
        if (asset)
        {
            selected_ = asset;
            ui_.infoLabel->setText("");
            //ui_.infoLabel->setText(asset->name + " <span style='color:rgb(150, 150, 150); font-size: 8pt;'>" + asset->description + "</span>");
            break;
        }
    }
}

void MeshmoonAssetSelectionDialog::OnSelect()
{
    if (!selected_)
    {
        ui_.infoLabel->setText("<span style='color:red;'>You must select an asset to continue</span>");
        return;
    }
    
    plugin_->Notifications()->ClearForeground();

    emit AssetSelected(selected_);
    accept();
}

void MeshmoonAssetSelectionDialog::OnCancel()
{
    plugin_->Notifications()->ClearForeground();

    emit Canceled();
}

bool MeshmoonAssetSelectionDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui_.lineEditFilter)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *kEvent = dynamic_cast<QKeyEvent*>(event);
            if (kEvent && (kEvent->key() == Qt::Key_Enter || kEvent->key() == Qt::Key_Escape))
            {
                if (kEvent->key() == Qt::Key_Escape)
                    ui_.lineEditFilter->setText("");
                kEvent->accept();
                return true;
            }
        }
        return false;
    }
    
    // Double clicking an asset button == selecting and closing dialog
    QPushButton *button = dynamic_cast<QPushButton*>(obj);
    if (button && event->type() == QEvent::MouseButtonDblClick)
    {
        MeshmoonAsset *asset = qvariant_cast<MeshmoonAsset*>(button->property("asset"));
        if (asset)
        {
            selected_ = asset;
            ui_.infoLabel->setText(asset->name + " <span style='color:rgb(150, 150, 150); font-size: 11px;'>" + asset->description + "</span>");
            ui_.buttonSelect->click();
            return true;
        }
    }
    return false;
}
