/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketAssetSelectionWidget.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include "MeshmoonAsset.h"
#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageDialogs.h"

#include "Framework.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "IAssetTypeFactory.h"

RocketAssetSelectionWidget::RocketAssetSelectionWidget(RocketPlugin *plugin, const QString &assetType, const QString &assetContext, QWidget *parent) :
    QWidget(parent),
    plugin_(plugin),
    assetType_(assetType),
    assetContext_(assetContext),
    state_(IRocketAssetEditor::DS_Verifying)
{
    setupUi(this);

    if (assetContext_.isEmpty())
        assetContext_ = GuaranteeTrailingSlash(plugin_->Storage()->BaseUrl());

    if (!assetContext_.endsWith("/") && assetContext_.contains("/"))
        assetContext_ = assetContext_.mid(0, assetContext_.lastIndexOf("/")+1);

    connect(assetRefLineEdit, SIGNAL(editingFinished()), SLOT(OnEditingFinished()));
    connect(buttonReset, SIGNAL(clicked()), SLOT(ClearAssetRef()));
    connect(buttonStorageSelection, SIGNAL(clicked()), SLOT(PickFromStorage()));
    connect(buttonOpenEditor, SIGNAL(clicked()), SLOT(OpenCurrentInEditor()));
    
    /// @todo Implement editor stuff!
    buttonOpenEditor->hide();

    SetState(IRocketAssetEditor::DS_OK);
}

RocketAssetSelectionWidget::~RocketAssetSelectionWidget()
{
}

QString RocketAssetSelectionWidget::ResolveRelativeOrAbsoluteRef(QString ref)
{
    if (ref.isEmpty())
        return "";

    // 1. Full URL ref.
    ref = AssetAPI::DesanitateAssetRef(ref);

    // 2. Relative ref to context (or storage base url)
    if (!assetContext_.isEmpty())
    {
        if (ref.startsWith(assetContext_, Qt::CaseInsensitive))
            ref = ref.mid(assetContext_.length());
        else
        {
            // 3. Check if relative to S3 variation URL.
            QString variationContext = MeshmoonAsset::S3RegionVariationURL(assetContext_);
            if (!variationContext.endsWith("/"))
                variationContext = variationContext.mid(0, variationContext.lastIndexOf("/")+1);
            if (ref.startsWith(variationContext, Qt::CaseInsensitive))
                ref = ref.mid(variationContext.length());
        }
    }

    return ref; 
}

void RocketAssetSelectionWidget::OnAssetRefChanged(const QString &assetRef, bool emitChange)
{
    QString resolved = ResolveRelativeOrAbsoluteRef(assetRef);

    assetRefLineEdit->blockSignals(true);
    assetRefLineEdit->setText(resolved);
    assetRefLineEdit->setToolTip(!resolved.isEmpty() ? assetRef : "");
    assetRefLineEdit->blockSignals(false);

    if (emitChange)
    {
        // Write full ref to config, the relative world storage ref is not going to be valid in another world.
        if (!configData_.file.isEmpty() && !configData_.section.isEmpty() && !configData_.key.isEmpty())
            plugin_->GetFramework()->Config()->Write(configData_, assetRef);

        if (!resolved.isEmpty())
            emit AssetRefChanged(resolved, plugin_->GetFramework()->Asset()->FindAsset(assetRef));
        else
            emit AssetRefChanged(resolved, AssetPtr());
    }
}

void RocketAssetSelectionWidget::SetAssetRef(const QString &assetRef)
{
    QString resolvedRef = plugin_->GetFramework()->Asset()->ResolveAssetRef(assetContext_, assetRef);

    if (assetRef_ == resolvedRef)
        return;
    assetRef_ = resolvedRef;
        
    // Empty ref
    if (assetRef_.isEmpty())
    {
        OnAssetRefChanged("");
        SetState(IRocketAssetEditor::DS_OK);
        return;
    }

    // 1. Already loaded to AssetAPI
    AssetPtr asset = plugin_->GetFramework()->Asset()->FindAsset(assetRef_);
    if (!asset)
    {
        OnAssetRefChanged(assetRef, false);

        // 2. Request asset now
        AssetTransferPtr transfer = plugin_->GetFramework()->Asset()->RequestAsset(assetRef_, assetType_, true);
        if (transfer.get())
        {
            connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnAssetTransferCompleted(AssetPtr)));
            connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(OnAssetTransferFailed(IAssetTransfer*, QString)));
            SetState(IRocketAssetEditor::DS_Verifying);
        }
        else
            SetState(IRocketAssetEditor::DS_Error);
    }
    else
    {
        if (asset->IsLoaded() && asset->Type() == assetType_)
        {
            OnAssetRefChanged(asset->Name());
            SetState(IRocketAssetEditor::DS_OK);
        }
        else
        {
            if (asset->Type() == assetType_)
                LogError("[RocketAssetSelectionWidget]: Proposed asset is not of correct type: " + assetRef_ + " Type: " + asset->Type() + " Expecting: " + assetType_);
            OnAssetRefChanged(asset->Name(), false);
            SetState(IRocketAssetEditor::DS_Error);
        }
    }
}

void RocketAssetSelectionWidget::SetConfigData(const ConfigData &configData)
{
    configData_ = configData;
    if (configData_.file.isEmpty() || configData_.section.isEmpty() || configData_.key.isEmpty())
        return;
        
    if (plugin_->GetFramework()->Config()->HasValue(configData_))
        SetAssetRef(plugin_->GetFramework()->Config()->Read(configData_).toString());
}

void RocketAssetSelectionWidget::OnAssetTransferCompleted(AssetPtr asset)
{
    QString assetRef = asset->Name();
    QString assetType = asset->Type();
    asset.reset();

    if (assetType == assetType_)
    {
        OnAssetRefChanged(assetRef);
        SetState(IRocketAssetEditor::DS_OK);
    }
    else
    {
        SetState(IRocketAssetEditor::DS_Error);
        LogError("[RocketAssetSelectionWidget]: Proposed asset is not of correct type: " + assetRef + " Type: " + assetType + " Expecting: " + assetType_);
        plugin_->GetFramework()->Asset()->ForgetAsset(assetRef, false);
    }
}

void RocketAssetSelectionWidget::OnAssetTransferFailed(IAssetTransfer* /*transfer*/, QString /*reason*/)
{
    SetState(IRocketAssetEditor::DS_Error);
}

void RocketAssetSelectionWidget::ClearAssetRef()
{
    SetAssetRef("");
}

QString RocketAssetSelectionWidget::CurrentAssetRef()
{
    return assetRefLineEdit->text().trimmed();
}

QString RocketAssetSelectionWidget::CurrentFullAssetRef()
{
    return assetRef_;
}

void RocketAssetSelectionWidget::EmitCurrentAssetRef()
{
    emit AssetRefChanged(CurrentAssetRef(), (!CurrentFullAssetRef().isEmpty() ? plugin_->GetFramework()->Asset()->FindAsset(CurrentFullAssetRef()) : AssetPtr()));
}

void RocketAssetSelectionWidget::PickFromStorage()
{
    if (!plugin_ || !plugin_->Storage())
        return;

    // Get the texture suffixes from the asset factory if possible.
    QStringList suffixes;
    AssetTypeFactoryPtr textureFactory = plugin_->GetFramework()->Asset()->GetAssetTypeFactory(assetType_);
    if (textureFactory.get())
        suffixes = textureFactory->TypeExtensions();
    else
        LogWarning(QString("[RocketAssetSelectionWidget]: Failed to get supported suffixes for asset type '%1'").arg(assetType_));

    QWidget *topMostParent = RocketNotifications::TopMostParent(this);
    RocketStorageSelectionDialog *dialog = plugin_->Storage()->ShowSelectionDialog(suffixes, true, lastStorageDir_, topMostParent);
    if (dialog)
    {
        connect(dialog, SIGNAL(StorageItemSelected(const MeshmoonStorageItem&)), SLOT(OnAssetSelectedFromStorage(const MeshmoonStorageItem&)), Qt::UniqueConnection);
        connect(dialog, SIGNAL(Canceled()), SLOT(OnStorageSelectionClosed()), Qt::UniqueConnection);
    }
}

void RocketAssetSelectionWidget::OnAssetSelectedFromStorage(const MeshmoonStorageItem &item)
{
    OnStorageSelectionClosed();

    if (!item.IsNull())
        SetAssetRef(item.fullAssetRef);
}

void RocketAssetSelectionWidget::OnStorageSelectionClosed()
{
    // Store the last storage location into memory for the next dialog open
    RocketStorageSelectionDialog *dialog = dynamic_cast<RocketStorageSelectionDialog*>(sender());
    if (dialog)
        lastStorageDir_ = dialog->CurrentDirectory();
}

void RocketAssetSelectionWidget::OpenCurrentInEditor()
{

}

void RocketAssetSelectionWidget::SetState(IRocketAssetEditor::DependencyState state)
{
    // Setup ui
    if (state_ == state)
        return;
    state_ = state;

    /* @todo No anims and state icons yet for this widget.
    if (state == IRocketAssetEditor::DS_Verifying)
    {
        if (movieLoading_->isValid())
        {
            ui_.labelIcon->setMovie(movieLoading_);
            movieLoading_->start();
        }
        else
            ui_.labelIcon->clear();
    }
    else if (state == IRocketAssetEditor::DS_Error)
    {
        ui_.labelIcon->setPixmap(pixmapError_);
        if (movieLoading_->isValid()) movieLoading_->stop();
    }
    else if (state == IRocketAssetEditor::DS_OK)
    {
        ui_.labelIcon->setPixmap(pixmapOK_);
        if (movieLoading_->isValid()) movieLoading_->stop();
    }*/

    assetRefLineEdit->setStyleSheet(QString("color: %1;").arg((state == IRocketAssetEditor::DS_OK ? "black" : (state == IRocketAssetEditor::DS_Error ? "red" : "blue"))));
}

void RocketAssetSelectionWidget::OnEditingFinished()
{
    SetAssetRef(assetRefLineEdit->text().trimmed());
}
