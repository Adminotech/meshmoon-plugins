/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "MeshmoonStorageItem.h"
#include "MeshmoonStorage.h"

#include "editors/RocketTextEditor.h"
#include "editors/RocketTextureEditor.h"
#include "editors/RocketMaterialEditor.h"
#include "editors/RocketParticleEditor.h"
#include "editors/RocketMeshEditor.h"
#include "RocketPlugin.h"

#include <QFileInfo>

#include "kNet/Network.h"

MeshmoonStorageItem::MeshmoonStorageItem() :
    isDirectory(false),
    isFile(false)
{
}

MeshmoonStorageItem::MeshmoonStorageItem(const MeshmoonStorageItemWidget *storageItem, const MeshmoonStorage *storage) :
    path(""),
    isDirectory(false),
    isFile(false)
{
    if (!storageItem)
        return;

    path = storageItem->data.key;
    filename = storageItem->filename;
    isDirectory = storageItem->data.isDir;
    isFile = !isDirectory;

    if (storage)
    {
        fullAssetRef = storage->UrlForItem(path);
        relativeAssetRef = storage->RelativeRefForKey(path);
    }
}

MeshmoonStorageItem::MeshmoonStorageItem(const QString storageKey, const MeshmoonStorage *storage) :
    path(storageKey),
    isDirectory(false),
    isFile(false)
{
    if (path.trimmed().isEmpty())
        return;

    isDirectory = path.endsWith("/");
    isFile = !isDirectory;

    QFileInfo fileInfo(path);
    if (!isDirectory)
        filename = fileInfo.fileName();
    else
        filename = fileInfo.path().split("/", QString::SkipEmptyParts).last();

    if (storage)
    {
        fullAssetRef = storage->UrlForItem(path);
        relativeAssetRef = storage->RelativeRefForKey(path);
    }
}

MeshmoonStorageItem MeshmoonStorageItem::GenerateChildFileItem(const QString &filename) const
{
    MeshmoonStorageItem item;
    if (!isDirectory)
        return item;

    QString filepath = filename.trimmed();
    if (filepath.trimmed().startsWith("/"))
        filepath = filepath.mid(1);

    item.path = path;
    if (!item.path.isEmpty() && !item.path.endsWith("/"))
        item.path += "/";
    item.path += filepath;
    item.filename = filepath;

    item.isDirectory = item.filename.endsWith("/");
    item.isFile = !item.isDirectory;

    item.fullAssetRef = fullAssetRef;
    if (!item.fullAssetRef.endsWith("/"))
        item.fullAssetRef += "/";
    item.fullAssetRef += filepath;

    item.relativeAssetRef = relativeAssetRef;
    if (!item.relativeAssetRef.isEmpty() && !item.relativeAssetRef.endsWith("/"))
        item.relativeAssetRef += "/";
    item.relativeAssetRef += filepath;

    return item;
}

bool MeshmoonStorageItem::IsNull() const
{
    return path.trimmed().isEmpty();
}

// MeshmoonStorageItemWidget

MeshmoonStorageItemWidget::MeshmoonStorageItemWidget(RocketPlugin *plugin, const QS3Object &data_) :
    data(data_),
    parent(0),
    plugin_(plugin),
    hasTextEditor(false),
    hasVisualEditor(false),
    canCreateInstances(false)
{
    QFileInfo fileInfo(data.key);
    if (!data.isDir)
        filename = fileInfo.fileName();
    else
        filename = fileInfo.path().split("/", QString::SkipEmptyParts).last();

    setText(filename);
    setData(Qt::WhatsThisRole, data.key);
    SetFormat(fileInfo.suffix().toLower(), fileInfo.completeSuffix().toLower());
}

MeshmoonStorageItemWidget::MeshmoonStorageItemWidget(RocketPlugin *plugin, const QString key) :
    parent(0),
    plugin_(plugin),
    hasTextEditor(false),
    hasVisualEditor(false),
    canCreateInstances(false)
{
    if (key == ".." || key.endsWith("/"))
        data.isDir = true;
    data.key = key;

    setText(key);
    setData(Qt::WhatsThisRole, data.key);
    SetFormat();
}

MeshmoonStorageItemWidget::MeshmoonStorageItemWidget(RocketPlugin *plugin, const MeshmoonStorageItemWidget *other, MeshmoonStorageItemWidget *parent_) :
    parent(0),
    plugin_(plugin),
    hasTextEditor(false),
    hasVisualEditor(false),
    canCreateInstances(false)
{
    if (!other)
        return;

    data = other->data;
    filename = other->filename;
    suffix = other->suffix;
    completeSuffix = other->completeSuffix;
    visualEditorName = other->visualEditorName;

    hasTextEditor = other->hasTextEditor;
    hasVisualEditor = other->hasVisualEditor;
    canCreateInstances = other->canCreateInstances;

    parent = (parent_ != 0 ? parent_ : other->parent);
    foreach(MeshmoonStorageItemWidget *child, other->children)
        children << new MeshmoonStorageItemWidget(plugin_, child, this);

    setText(filename);
    setData(Qt::WhatsThisRole, data.key);
    SetFormat(other->suffix, other->completeSuffix);
}

MeshmoonStorageItemWidget::~MeshmoonStorageItemWidget()
{
    DeleteChildren();
}

void MeshmoonStorageItemWidget::OnDownload()
{
    emit DownloadRequest(this);
}

void MeshmoonStorageItemWidget::OnDelete()
{
    emit DeleteRequest(this);
}

void MeshmoonStorageItemWidget::OnCopyAssetReference()
{
    emit CopyAssetReferenceRequest(this);
}

void MeshmoonStorageItemWidget::OnCopyUrl()
{
    emit CopyUrlRequest(this);
}

void MeshmoonStorageItemWidget::OnCreateCopy()
{
    emit CreateCopyRequest(this);
}

bool MeshmoonStorageItemWidget::OpenVisualEditor()
{
    // This is set in SetFormat() depending on file suffix.
    if (!hasVisualEditor)
        return false;

    // Select the appropriate visual editor
    IRocketAssetEditor *editor = 0;
    
    // Material editor.
    QStringList suffixes = RocketMaterialEditor::SupportedSuffixes();
    if (suffixes.contains(suffix, Qt::CaseInsensitive) || suffixes.contains(completeSuffix, Qt::CaseInsensitive))
        editor = new RocketMaterialEditor(plugin_, this);
    // Particle editor.
    if (!editor)
    {
        suffixes = RocketParticleEditor::SupportedSuffixes();
        if (suffixes.contains(suffix, Qt::CaseInsensitive) || suffixes.contains(completeSuffix, Qt::CaseInsensitive))
            editor = new RocketParticleEditor(plugin_, this);
    }
    // Texture editor.
    if (!editor)
    {
        suffixes = RocketTextureEditor::SupportedSuffixes();
        if (suffixes.contains(suffix, Qt::CaseInsensitive) || suffixes.contains(completeSuffix, Qt::CaseInsensitive))
            editor = new RocketTextureEditor(plugin_, this);
    }
    // Mesh editor.
    if (!editor)
    {
        suffixes = RocketMeshEditor::SupportedSuffixes();
        if (suffixes.contains(suffix, Qt::CaseInsensitive) || suffixes.contains(completeSuffix, Qt::CaseInsensitive))
            editor = new RocketMeshEditor(plugin_, this);
    }
    
    if (editor)
        emit EditorOpened(editor);
    return (editor != 0);
}

bool MeshmoonStorageItemWidget::OpenTextEditor()
{
    // This is set in SetFormat() depending on file suffix.
    if (!hasTextEditor)
        return false;
        
    IRocketAssetEditor *editor = new RocketTextEditor(plugin_, this);
    emit EditorOpened(editor);
    return true;
}

void MeshmoonStorageItemWidget::OnInstantiate()
{
    emit InstantiateRequest(this);
}

void MeshmoonStorageItemWidget::OnRestoreBackup()
{
    emit RestoreBackupRequest(this);
}

void MeshmoonStorageItemWidget::SetFormat(const QString &suffix_, const QString &completeSuffix_)
{
    if (data.isDir)
        setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    else
        setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsDragEnabled);

    suffix = suffix_;
    completeSuffix = completeSuffix_;
    
    // Detect text editor capabilities.
    if (RocketTextEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive) || RocketTextEditor::SupportedSuffixes().contains(completeSuffix, Qt::CaseInsensitive))
        hasTextEditor = true;

    // Detect visual editor capabilities.
    if (RocketMaterialEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive) || RocketMaterialEditor::SupportedSuffixes().contains(completeSuffix, Qt::CaseInsensitive))
    {
        visualEditorName = RocketMaterialEditor::EditorName();
        hasVisualEditor = true;
    }
    else if (RocketParticleEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive) || RocketParticleEditor::SupportedSuffixes().contains(completeSuffix, Qt::CaseInsensitive))
    {
        visualEditorName = RocketParticleEditor::EditorName();
        hasVisualEditor = true;
    }
    else if (RocketTextureEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive) || RocketTextureEditor::SupportedSuffixes().contains(completeSuffix, Qt::CaseInsensitive))
    {
        visualEditorName = RocketTextureEditor::EditorName();
        hasVisualEditor = true;
    }
    else if (RocketMeshEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive) || RocketMeshEditor::SupportedSuffixes().contains(completeSuffix, Qt::CaseInsensitive))
    {
        visualEditorName = RocketMeshEditor::EditorName();
        hasVisualEditor = true;
    }

    // Detect instancing capability.
    QStringList createInstancesSuffixes; 
    createInstancesSuffixes << "mesh" << "obj" << "dae" << "ply" << "txml" << "js";
    if (createInstancesSuffixes.contains(suffix, Qt::CaseInsensitive) || createInstancesSuffixes.contains(completeSuffix, Qt::CaseInsensitive))
        canCreateInstances = true;
    
    // Set item icon
    QString imagePath = data.isDir ? ":/images/icon-folder-white.png" : ":/images/icon-file-white.png";
    if (!suffix.isEmpty())
    {
        QString customIconPath = IconImagePath(suffix, completeSuffix);
        if (!customIconPath.isEmpty())
            imagePath = customIconPath;
    }
    setIcon(QIcon(imagePath));
}

QString MeshmoonStorageItemWidget::IconImagePath(QString suffix, QString completeSuffix)
{
    suffix = suffix.toLower().trimmed();
    if (suffix.startsWith("."))
        suffix = suffix.mid(1);
    if (suffix.isEmpty())
        return "";
        
    completeSuffix = completeSuffix.toLower().trimmed();
    if (completeSuffix.startsWith("."))
        completeSuffix = completeSuffix.mid(1);

    // Generic file
    if (suffix == "file")
        return ":/images/icon-file-white.png";
    // Generic folder
    else if (suffix == "folder")
        return ":/images/icon-folder-white.png";
    // Mesh
    else if (suffix == "mesh" || suffix == "obj" || suffix == "dae" || suffix == "ply")
        return ":/images/icon-box.png";
    // Skeleton
    else if (suffix == "skeleton")
        return ":/images/icon-skeleton.png";
    // Material / Particle
    else if (suffix == "material" || suffix == "particle")
        return ":/images/icon-material.png";
    // Scene description
    else if (suffix == "txml" || suffix == "tbin" || completeSuffix == "txml.server.log")
        return ":/images/icon-world.png";
    // Texture
    else if (suffix == "jpg" || suffix == "jpeg" || suffix == "png" || suffix == "gif" || suffix == "bmp" || suffix == "crn" || suffix == "dds" || suffix == "tga")
        return ":/images/icon-image.png";
    // Script
    else if (suffix == "js" || suffix == "webrocketjs" || suffix == "py")
        return ":/images/icon-gear.png";
    // Archive
    else if (suffix == "zip" || suffix == "7z" || suffix == "rar" || suffix == "tar" || completeSuffix == "tar.gz")
        return ":/images/icon-archive.png";            
    // JSON
    else if (suffix == "json")
        return ":/images/icon-json.png";
        
    return "";
}

QString MeshmoonStorageItemWidget::FormattedSize() const
{
    if (data.isDir || data.size == 0)
        return "";

    QString sizeStr = QString::fromStdString(kNet::FormatBytes(static_cast<u64>(data.size)));
    if (sizeStr.lastIndexOf(".") >= sizeStr.lastIndexOf(" ")-3) sizeStr.insert(sizeStr.lastIndexOf(" "), "0");
    return sizeStr;
}

MeshmoonStorageItem MeshmoonStorageItemWidget::Item() const
{
    return MeshmoonStorageItem(this, plugin_->Storage());
}

void MeshmoonStorageItemWidget::AddChild(MeshmoonStorageItemWidget *item)
{
    if (!item)
        return;
    item->parent = this;
    children << item;
}

void MeshmoonStorageItemWidget::DeleteChildren()
{
    foreach(MeshmoonStorageItemWidget *child, children)
        SAFE_DELETE(child);
    children.clear();
}

MeshmoonStorageItemWidgetList MeshmoonStorageItemWidget::Folders()
{
    if (children.isEmpty())
        return MeshmoonStorageItemWidgetList();
    MeshmoonStorageItemWidgetList folders;
    foreach(MeshmoonStorageItemWidget *item, children)
        if (item->data.isDir)
            folders << item;
    return folders;
}

MeshmoonStorageItemWidgetList MeshmoonStorageItemWidget::Files(bool recursive)
{
    if (children.isEmpty())
        return MeshmoonStorageItemWidgetList();
    MeshmoonStorageItemWidgetList files;
    foreach(MeshmoonStorageItemWidget *item, children)
    {
        if (!item->data.isDir)
            files << item;
        else if (recursive)
            files << item->Files(recursive);
    }
    return files;
}

MeshmoonStorageItemWidgetList MeshmoonStorageItemWidget::FoldersAndFiles()
{
    return children;
}

bool MeshmoonStorageItemWidget::FindParent(MeshmoonStorageItemWidget *child, MeshmoonStorageItemWidget *parentCandidate)
{
    if (!child || !parentCandidate)
        return false;

    bool found = false;
    if (child->data.key.startsWith(parentCandidate->data.key))
    {
        if (child->data.isDir)
        {
            QString childKey = child->data.key;
            QString parentKey = parentCandidate->data.key;
            QString noParent = childKey.right(childKey.length()-parentKey.length());
            if (noParent.count("/") == 1 && noParent.endsWith("/"))
            {
                parentCandidate->AddChild(child);
                found = true;
            }
        }
        else
        {
            QFileInfo childInfo(child->data.key);
            QFileInfo parentInfo(parentCandidate->data.key);
            if (childInfo.dir().path() == parentInfo.path())
            {
                parentCandidate->AddChild(child);
                found = true;
            }
        }
        if (!found)
        {
            foreach(MeshmoonStorageItemWidget *folder, parentCandidate->Folders())
            {
                found = FindParent(child, folder);
                if (found)
                    break;
            }
        }
    }
    return found;
}

bool MeshmoonStorageItemWidget::IsProtected() const
{
    if (!plugin_ || !plugin_->Storage())
        return false;
    QString relativeRef = plugin_->Storage()->RelativeRefForKey(data.key);
    return relativeRef.startsWith("backup", Qt::CaseSensitive);
}

QString MeshmoonStorageItemWidget::RelativeAssetReference()
{
    if (!plugin_ || !plugin_->Storage())
        return "";
    return plugin_->Storage()->RelativeRefForKey(data.key);
}

QString MeshmoonStorageItemWidget::AbsoluteAssetReference()
{
    if (!plugin_ || !plugin_->Storage())
        return "";
    return plugin_->Storage()->UrlForItem(data.key);
}
