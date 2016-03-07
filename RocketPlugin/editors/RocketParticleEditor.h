/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "IRocketAssetEditor.h"
#include "Win.h"
#include "OgreModuleFwd.h"

#include "storage/MeshmoonStorageItem.h"

#include <QColorDialog>
#include <QColor>
#include <QPixmap>
#include <QMovie>
#include <QMenu>
#include <QTextStream>

#include <OgreStringInterface.h>

#define MATH_OGRE_INTEROP
#include "Math/float3.h"
#include "Color.h"

#include "ui_RocketParticleEditorWidget.h"
#include "ui_RocketParticleEmitterWidget.h"
#include "ui_RocketParticleAffectorWidget.h"

class RocketParticleEmitterWidget;
class RocketParticleAffectorWidget;

namespace Ogre 
{
    class ParameterDef;
    class BillboardParticleRenderer;
    class ParticleEmitter;
}

/// Particle editor that supports all of the Ogre particle script features.
class RocketParticleEditor : public IRocketAssetEditor
{
    Q_OBJECT

public:
    RocketParticleEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource);
    ~RocketParticleEditor();
    
    /// Get all supported suffixes
    /** @note The suffixes wont have the first '.' prefix, as "png". */
    static QStringList SupportedSuffixes();

    /// Returns editor name.
    static QString EditorName();

public slots:
    // Load particle from asset ptr.
    void LoadParticle(AssetPtr asset);
    
    void SetMaterial(const QString &assetRef);

    QString Material() const;
    QString MaterialUrl() const;

private slots:
    // Load helpers
    void OnGeneratedMaterialDependencyFailed(IAssetTransfer *transfer, QString reason);
    
    // Help web page openers
    void OnHelpConfiguration();
    void OnHelpEmitters();
    void OnHelpAffectors();
    
    // Emitters and affectors
    void ClearEmitterTabs();
    void ClearAffectorTabs();
    
    void OnAddNewEmitter();
    void OnAddNewAffector();
    
    void OnRemoveEmitterRequest(int tabIndex);
    void OnRemoveEmitterConfirmed(QAbstractButton *button);
    void OnRemoveAffectorRequest(int tabIndex);
    void OnRemoveAffectorConfirmed(QAbstractButton *button);

    // Material
    void SetMaterialState(IRocketAssetEditor::DependencyState state);
    void OnMaterialChanged();
    void OnMaterialTransferCompleted(AssetPtr asset);
    void OnMaterialTransferFailed(IAssetTransfer *transfer, QString reason);
    
    QString OgreMaterialToAssetReference(QString materialRef);
    std::string OgreMaterialToAssetReference(std::string materialRef);
    
    void OnOpenMaterialEditor();
    void OnCloseMaterialEditor();
    
    void OnSelectMaterialFromStorage();
    void OnMaterialSelectedFromStorage(const MeshmoonStorageItem &item);
    void OnStorageSelectionClosed();
    
    // Ogre::ParticleSystem attributes
    void OnQuotaChanged(int quota, bool apply = true);
    void OnParticleWidthChanged(double width, bool apply = true);
    void OnParticleHeightChanged(double height, bool apply = true);
    void OnIterationIntervalChanged(double interval, bool apply = true);
    void OnNonVisibleUpdateTimeoutChanged(double timeout, bool apply = true);
    void OnCullEachChanged(bool cullEach, bool apply = true);
    // Currently only 'billboard' renderer is supported
    //void OnRendererChanged(const QString &renderer, bool apply = true);
    void OnSortedChanged(bool sorted, bool apply = true);
    void OnLocalSpaceChanged(bool localSpace, bool apply = true);
    
    // Ogre::BillboardParticleRenderer attributes
    void OnAccurateFacingChanged(bool accurateFacing, bool apply = true);
    void OnBillboardTypeChanged(int type, bool apply = true);
    void OnBillboardOriginChanged(int type, bool apply = true);
    void OnBillboardRotationTypeChanged(int type, bool apply = true);
    
    void OnEmitterOrAffectorTouched() { Touched(); }
    
    /// Get the underlying Ogre::Particle system of OgreParticleAsset.
    Ogre::ParticleSystem *GetSystem() const;
    
    /// Get the underlying Ogre::ParticleSystemRenderer of OgreParticleAsset.
    /** @note Currently we only support the billboard renderer, so this is hard coded to it. */
    Ogre::BillboardParticleRenderer *GetSystemRenderer() const;

protected:
    /// Implementations must handle the initial resource data download.
    /** In most cases this is when you update/create the user interface
        to edit the resource. 
        @param Resource raw data. */
    void ResourceDownloaded(QByteArray data);

    /// Implementations must return the current raw data for the resource.
    /** The data will be used to upload a new version of the resource to storage. 
        @return Current raw data of the edited resource. */
    IRocketAssetEditor::SerializationResult ResourceData();

private:
    /// Utility functions for writing particle script
    void WriteStartPracet(QTextStream &s, int indentLevel);
    void WriteEndPracet(QTextStream &s, int indentLevel);
    void WriteParametersSection(QTextStream &s, Ogre::StringInterface *si, const QString &sectionName, int indentLevel);
    void WriteParameters(QTextStream &s, Ogre::StringInterface *si, int indentLevel);
    void WriteParameter(QTextStream &s, const std::string &name, const std::string &value, int indentLevel, bool writePostStartPracet = false);
    void WriteParameter(QTextStream &s, const QString &name, const QString &value, int indentLevel, bool writePostStartPracet = false);
    void WriteIndent(QTextStream &s, int indentLevel);
    bool EqualsDefaultValue(Ogre::StringInterface *si, Ogre::ParameterDef *def);
    
    QString LC;

    weak_ptr<OgreParticleAsset> particle_;
    
    IRocketAssetEditor::DependencyState materialState_;
    QString lastVerifiedMaterialRef_;

    QPixmap pixmapOK_;
    QPixmap pixmapError_;
    QMovie *movieLoading_;

    Ui::RocketParticleEditorWidget ui_;
    QWidget *editor_;
    QMenu *menu_;
    
    MeshmoonStorageItem lastStorageDir_;
    
    QPointer<IRocketAssetEditor> materialEditor_;
    QPointer<QMessageBox> confirmDialog_;
};

// RocketParticleEmitterWidget

class RocketParticleEmitterWidget : public QWidget
{
    Q_OBJECT
    
public:
    RocketParticleEmitterWidget(RocketPlugin *plugin, Ogre::ParticleEmitter *emitter = 0, QWidget *parent = 0);
    ~RocketParticleEmitterWidget();

    /// Reset editor.
    void Reset();

    /// Load emitter to the editor.
    void LoadEmitter(Ogre::ParticleEmitter* emitter);
    
    /// Returns current emitter.
    Ogre::ParticleEmitter *Emitter() const;
    
    /// Returns if this emitter has a property of @c name
    bool HasParameter(const QString &name) const;
    
    /// Returns if this emitter has 'width', 'height' and 'depth' attributes.
    bool HasSizeParameters() const;
    
    /// Returns if this emitter has 'inner_width' and 'inner_height' attributes.
    bool HasInnerSizeParameters() const;
    
    /// Returns if this particle has 'inner_depth' attribute.
    /** @note Can return false even when HasInnerSizeParameters returns true. */
    bool HasInnerDepthParameter() const;

signals:
    void Touched();

protected:
    bool eventFilter(QObject *watched, QEvent *e);

private slots:
    void ReadSizeParameters();   
    void OnSizeWidthChanged(double value, bool apply = true);
    void OnSizeHeightChanged(double value, bool apply = true);
    void OnSizeDepthChanged(double value, bool apply = true);
    
    void ReadInnerSizeParameters();
    void OnInnerSizeWidthChanged(double value, bool apply = true);
    void OnInnerSizeHeightChanged(double value, bool apply = true);
    void OnInnerSizeDepthChanged(double value, bool apply = true);

    void SetDirectionFromCamera();
    void OnDirectionChanged(float3 dir, bool apply = true);
    void OnDirectionXChanged(double value, bool apply = true);
    void OnDirectionYChanged(double value, bool apply = true);
    void OnDirectionZChanged(double value, bool apply = true);
    
    void OnPositionChanged(float3 pos, bool apply = true);
    void OnPositionXChanged(double value, bool apply = true);
    void OnPositionYChanged(double value, bool apply = true);
    void OnPositionZChanged(double value, bool apply = true);
    
    void OnEmissionRateChanged(double value, bool apply = true);
    void OnAngleChanged(int value, bool apply = true);

    void OnPickColor(QString type = "");
    void OnRestoreDefaultColor();
    void OnIncreaseOrDecreaseColor();
    void OnMultiplyColor(const QString &type, float scalar);

    void OnColorRangeStartChanged(const QColor &color, bool apply = true);
    void OnColorRangeEndChanged(const QColor &color, bool apply = true);
    
    void OnTtlMinChanged(double value, bool apply = true);
    void OnTtlMaxChanged(double value, bool apply = true);
    
    void OnVelocityMinChanged(double value, bool apply = true);
    void OnVelocityMaxChanged(double value, bool apply = true);
    
    void OnDurationMinChanged(double value, bool apply = true);
    void OnDurationMaxChanged(double value, bool apply = true);
    
    void OnRepeatDelayMinChanged(double value, bool apply = true);
    void OnRepeatDelayMaxChanged(double value, bool apply = true);
    
private:
    Ui::RocketParticleEmitterWidget ui_;
    QPointer<QColorDialog> colorPicker_;

    QMenu *colorMenu_;
    QMenu *increaseColorMenu_;
    QMenu *decreaseColorMenu_;
    
    RocketPlugin *plugin_;
    Ogre::ParticleEmitter *emitter_;
};

// RocketParticleAffectorWidget

class RocketParticleAffectorWidget : public QWidget
{
    Q_OBJECT

public:
    RocketParticleAffectorWidget(RocketPlugin *plugin, Ogre::ParticleAffector *affector = 0, QWidget *parent = 0);
    ~RocketParticleAffectorWidget();
    
    /// Reset editor.
    void Reset();

    /// Load emitter to the editor.
    void LoadAffector(Ogre::ParticleAffector* affector);

    /// Returns current emitter.
    Ogre::ParticleAffector *Affector() const;

signals:
    void Touched();
    
private slots:
    void CreateDescription();
    void CreateParameterWidget(const QString &name, const QString &value, Ogre::ParameterType type);
    QDoubleSpinBox *CreateRealWidget(double min = 0.0, double max = 1000000.0, double step = 0.1, int decimals = 2);
    QSpinBox *CreateIntWidget(int min = 0, int max = 1000000, int step = 1);
    
    QStringList StringParameterOptions(const QString &name);
    
    float3 DirFromCamera();
    
    void OnBoolParameterChanged(bool value);
    void OnIntParameterChanged(int value);
    void OnRealParameterChanged(double value);
    void OnStringParameterChanged(QString value);
    void OnCameraDirToParameter();
    void OnPickColorParameter();
    void OnColorParameterChanged(const QColor &color);
    
    Ogre::ParameterDef GetSenderDef();

private:
    Ui::RocketParticleAffectorWidget ui_;
    QPointer<QColorDialog> colorPicker_;

    RocketPlugin *plugin_;
    Ogre::ParticleAffector *affector_;
};
