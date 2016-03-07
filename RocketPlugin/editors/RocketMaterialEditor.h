/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "IRocketAssetEditor.h"
#include "OgreModuleFwd.h"

#include "rendering/RocketGPUProgramGenerator.h"
#include "storage/MeshmoonStorageItem.h"

#define MATH_OGRE_INTEROP
#include "Math/float2.h"
#include "Color.h"

#include <QColorDialog>

#include "ui_RocketMaterialEditorWidget.h"
#include "ui_RocketMaterialTextureWidget.h"

#include <OgreSceneManager.h>

class RocketMaterialTextureWidget;
struct MeshmoonShaderInfo;

class OgreMaterialAsset;

/// Material editor with various material, texture unit and shader editing capabilities.
class RocketMaterialEditor : public IRocketAssetEditor
{
    Q_OBJECT

public:
    RocketMaterialEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource);
    ~RocketMaterialEditor();

    /// Get all supported suffixes
    /** @note The suffixes wont have the first '.' prefix, as "png". */
    static QStringList SupportedSuffixes();
    
    /// Returns editor name.
    static QString EditorName();
    
    /// QObject override.
    bool eventFilter(QObject *watched, QEvent *e);

public slots:
    // Load material from asset ptr.
    void LoadMaterial(AssetPtr asset);
    
    // Load pass from tech in the current material.
    void LoadPass(int techIndex, int passIndex);

private slots:
    // Init.
    void Initialize();
    void InitializeUi();
    
    // Frame updates
    void OnUpdate(float frametime);
    
    // Open Ogre materials manual webpage.
    void OnHelpMaterials();
    
    // Pass change handler
    void OnPassChanged(int index);
    
    // Color helpers
    void OnPickColor(int type);

    void OnRestoreDefaultColor();
    void OnIncreaseOrDecreaseColor();
    void OnMultiplyColor(int type, float scalar);
    
    // Colors
    void OnAmbientChanged(const QColor &color, bool apply = true);
    void OnDiffuseChanged(const QColor &color, bool apply = true);
    void OnSpecularChanged(const QColor &color, bool apply = true);
    void OnEmissiveChanged(const QColor &color, bool apply = true);

    // Blending
    void OnSourceBlendingChanged(int mode, bool apply = true);
    void OnDestinationBlendingChanged(int mode, bool apply = true);
    void OnCullingChanged(int mode, bool apply = true);
    void OnLightingChanged(bool enabled, bool apply = true);
    void OnDepthCheckChanged(bool enabled, bool apply = true);
    void OnDepthWriteChanged(bool enabled, bool apply = true);
    void OnDepthBiasChanged(double value, bool apply = true);
    void OnAlphaRejectionFunctionChanged(int value, bool apply = true);
    void OnAlphaRejectionValueChanged(int value, bool apply = true);
    
    // Shaders
    void OnShaderChanged(const QString &shaderName);
    void OnShaderNamedParameterChanged(double value, bool apply = true);

    void SelectShadingCheckbox(QCheckBox *target, bool checked, bool enabled, bool apply = false);
    void SelectShadingSpecLighting(bool checked, bool enabled = true, bool apply = false);
    void SelectShadingSpecMap(bool checked, bool enabled = true, bool apply = false);
    void SelectShadingMultiDiffuse(bool checked, bool enabled = true, bool apply = false);
    void SelectShadingNormalMap(bool checked, bool enabled = true, bool apply = false);
    void SelectShadingLightMap(bool checked, bool enabled = true, bool apply = false);
    void SelectShadingShadows(bool checked, bool enabled = true, bool apply = false);
    
    void LoadShaderAsSelection(const MeshmoonShaderInfo &shader, bool readOnly);
    void LoadShaderFromSelection(bool readOnly = false);
    void LoadShader(const QString &shaderName, bool readOnly);

    bool PrepareTextureUnit(int textureIndex, const QStringList &compileArguments, const QString &compileArgument, OgreMaterialAsset *material, RocketMaterialTextureWidget *editor, bool readOnly);
    bool PrepareShadowMaps(int textureIndex, const QString &compileArgument, OgreMaterialAsset *material, bool readOnly);
    
    void HookTextureEditorSignals(RocketMaterialTextureWidget *editor);
    
    // Textures
    void OnTextureChanged(int textureUnitIndex, const QString &textureRef);
    void OnTextureUVChanged(int textureUnitIndex, int index);
    void OnTextureTilingChanged(int textureUnitIndex, float2 tiling);
    void OnTextureAddressingChanged(int textureUnitIndex, int addressing);
    
    /// @note @c filtering is a Ogre::TextureFilterOptions as int.
    void OnTextureFilteringChanged(int textureUnitIndex, int filtering);

    void OnTextureMaxAnisotropyChanged(int textureUnitIndex, uint maxAnisotropy);
    
    /// @todo Deprecated, remove
    void OnTextureColorOperationChanged(int textureUnitIndex, int operation);
       
    // Shader named parameters
    bool WriteShaderNamedParameter(const QString &namedParameter, const QString &shaderType, QVariant value);
    
    /// @todo Currently only supports QVariant::Double not float4 etc. Add when needed.
    QVariant ReadShaderNamedParameter(const QString &namedParameter, const QString &shaderType, QVariant::Type type, size_t size = 0);
    
    // Shader named parameter cache
    QVariant ReadShaderNamedParameterCache(const QString &namedParameter) const;
    void WriteShaderNamedParameterCache(const QString &namedParameter, QVariant value, bool overwriteExisting = true);

    // String helpers
    QString OgreTextureToAssetReference(const QString &materialRef, QString textureRef);
    
    // Load helpers
    void OnGeneratedMaterialDependencyFailed(IAssetTransfer *transfer, QString reason);

    // Preview
    void CreateCubePreview();
    void CreateSpherePreview();
    void CreatePlanePreview();
    void CreatePreviewMesh(Ogre::SceneManager::PrefabType type);

protected:
    /// IRocketAssetEditor override.
    void ResourceDownloaded(QByteArray data);

    /// IRocketAssetEditor override.
    IRocketAssetEditor::SerializationResult ResourceData();

    /// IRocketAssetEditor override.
    void AboutToClose();
    
signals:
    void CloseTextureEditors();

private:
    enum ColorType
    {
        AmbientColor = 0,
        DiffuseColor,
        SpecularColor,
        EmissiveColor
    };
    enum MaterialDataRole
    {
        TechniqueRole = Qt::UserRole+1,
        PassRole,
        ShaderNameRole
    };

    /// Loads the available and acceptable shader library into the editor.
    void LoadShaders();
    
    /// Caches compatible texture units from the current material/tech/pass.
    void CacheCompatibleTextureUnits();
    
    bool ActivateColorDialog(int type);
    static QString ColorTypeToString(int type);
    
    QByteArray PostProcessMaterialScript(QByteArray &script);
    
    Ui::RocketMaterialEditorWidget ui_;
    QWidget *editor_;
    
    RocketOgreSceneRenderer *sceneRenderer_;
    Ogre::Entity *previewMeshEntity_;
    weak_ptr<OgreMaterialAsset> material_;
    
    int iTech_;
    int iPass_;

    QList<QPointer<QColorDialog> > colorDialogs_;
    QList<Color> colorCache_;
    
    QMenu *colorMenu_;
    QMenu *increaseColorMenu_;
    QMenu *decreaseColorMenu_;
    
    QList<MeshmoonShaderInfo> shaders_;
    QHash<QString, QVariant> shaderNamedParameterCache_;
    QHash<QString, QString> textureCache_;
};

// RocketMaterialTextureWidget

class RocketMaterialTextureWidget : public QWidget
{
    Q_OBJECT
    
public:
    RocketMaterialTextureWidget(RocketPlugin *plugin, const QString &materialRef, const MeshmoonShaderInfo &info, 
                                const QString &textureType, int textureUnitIndex, QWidget *parent = 0);
    ~RocketMaterialTextureWidget();

    enum TextureState
    {
        TS_OK = 0,
        TS_Error,
        TS_VerifyingTexture
    };

    TextureState State() const;
    QString TextureType() const;
    int TextureUnitIndex() const;
    
    const Ui::RocketMaterialTextureWidget &Ui() const;
    const MeshmoonShaderInfo &ShaderInfo() const;

public slots:
    /// Get the texture reference.
    /** This may be a relative reference to the material,
        if you want and working URL ref use Url() instead. */
    QString Texture() const;

    /// Get the absolute texture reference.
    QString Url() const;

    /// Set texture ref
    void SetTexture(const QString &textureRef, bool apply = true);

    /// Set UV coord.
    void SetUV(int index, bool apply = true);

    /// Set addressing mode.
    /** Call with Ogre::TextureUnitState::TextureAddressingMode
        as int. */
    void SetAddressing(int addressing, bool apply = true);

    /// Set color operation
    /** @todo Deprecated, remove. */
    void SetColorOperation(int operation, bool apply = true);

    /// Set tiling.
    void SetTilingX(double tiling, bool apply = true);
    void SetTilingY(double tiling, bool apply = true);
    void SetTiling(float2 tiling, bool apply = true);

    /// Call with Ogre::FilterOptions as int with all of the Ogre::FilterType.
    void SetFilteringComplex(int min, int mag, int mip, bool apply = true);
    
    /// Call with Ogre::TextureFilterOptions as int.
    void SetFiltering(int ogreFiltering, bool apply = true);

    /// Only useful/applies with Ogre::TFO_ANISOTROPIC filtering.
    void SetMaxAnisotropy(uint maxAnisotropy, bool apply = true);
    
    /// Returns UV.
    int UV() const;

    /// Returns Ogre::TextureUnitState::TextureAddressingMode as int.
    int Addressing() const;

    /** Returns Ogre::LayerBlendOperationEx as int,
        up to Ogre::LBX_BLEND_CURRENT_ALPHA.
        @todo Deprecated, remove. */
    int ColorOperation() const;

    /// Returns tiling.
    float2 Tiling() const;

    /// Returns Ogre::TextureFilterOptions as int.
    int Filtering() const;

    /** Returns max anisotropy. Only useful/applies 
        with Ogree::TFO_ANISOTROPIC filtering. Returned range is 1-9. */
    uint MaxAnisotropy() const;
    
    Ogre::TextureFilterOptions FilteringFromComplex(int min, int mag, int mip);

private slots:
    void OnOpenTextureViewer();
    void OnSelectTextureFromStorage();
    void OnTextureSelectedFromStorage(const MeshmoonStorageItem &item);
    void OnStorageSelectionClosed();

    void OnTextureChanged();
    void OnTextureTransferCompleted(AssetPtr asset);
    void OnTextureTransferFailed(IAssetTransfer *transfer, QString error);

    void OnCloseTextureEditors();

    void SetFilteringFromComboBoxIndex(int index, bool apply = true);
    void SetMaxAnisotropyFromComboBoxIndex(int index, bool apply = true);

signals:
    void TextureChanged(int textureUnitIndex, const QString &textureRef);
    void UVChanged(int textureUnitIndex, int index);
    void TilingChanged(int textureUnitIndex, float2 tiling);
    
    /// @c addressing is a Ogre::TextureUnitState::TextureAddressingMode as int
    void AddressingChanged(int textureUnitIndex, int addressing);
    
    /** Emits Ogre::LayerBlendOperationEx as int,
        up to Ogre::LBX_BLEND_CURRENT_ALPHA.
        @todo Deprecated, remove. */
    void ColorOperationChanged(int textureUnitIndex, int index);

    /// @c filtering is a Ogre::TextureFilterOptions as int.
    void FilteringChanged(int textureUnitIndex, int filtering);
    
    /// @c maxAnisotropy is limited to range 1-9.
    void MaxAnisotropyChanged(int textureUnitIndex, uint maxAnisotropy);

protected:
    void dragEnterEvent(QDragEnterEvent *e);
    void dragLeaveEvent(QDragLeaveEvent *e);
    void dropEvent(QDropEvent *e);
    
private:
    void SetState(TextureState state);

    RocketPlugin *plugin_;
    
    MeshmoonShaderInfo info_;

    Ui::RocketMaterialTextureWidget ui_;
    int textureUnitIndex_;
    QString materialRef_;
    QString materialBase_;
    QString textureType_;
    QString lastVerifiedTextureRef_;

    TextureState state_;
    QPixmap pixmapOK_;
    QPixmap pixmapError_;
    QMovie *movieLoading_;

    MeshmoonStorageItem lastStorageDir_;

    QPointer<IRocketAssetEditor> textureViewer_;
};
