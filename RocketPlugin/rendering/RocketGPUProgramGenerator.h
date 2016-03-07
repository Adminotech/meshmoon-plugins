/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPlugin.h
    @brief  The Meshmoon Rocket client plugin. */

#pragma once

#include "RocketFwd.h"
#include "RocketGPUProgramConstant.h"

#include <QString>
#include <QStringList>

/// @cond PRIVATE

struct MeshmoonShaderInfo;
typedef QList<MeshmoonShaderInfo> MeshmoonShaderInfoList;

class RocketGPUProgramGenerator
{
public:
    RocketGPUProgramGenerator(Framework* framework);
    ~RocketGPUProgramGenerator();
    
    // Create shaders to Ogre
    static void CreateShaders(Framework* framework);
    
    // Returns full names of Meshmoon shaders.
    static MeshmoonShaderInfoList MeshmoonShaders(Framework* framework);

    typedef QMap<std::string, std::string> ShaderNameMap;
    typedef QMap<std::string, QList<std::string> > ExcludeConstantMap;
    typedef QMap<std::string, QList<std::string> > DefineMap;
    typedef QMap<std::string, int> ShaderOrderMap;
    typedef QList<QList<int> > CombinationListInt;
    typedef QList<QList<std::string> > CombinationList;

private:
    // Generates shaders to Ogre
    void Generate();

    // Generates all possible combinations of shader defines.
    CombinationList GenerateShaderDefineCombinations();
    CombinationListInt GenerateCombinations(CombinationListInt& combinations, const QList<int>& values);
    void GenerateAdditionalShaderDefineCombinations(QList<QList<std::string> > &shaderDefines);

    std::string CompilerArguments(const QList<std::string> &defines, int maxLightCount, bool useOpenGL);
    void SetConstants(const QList<std::string> &defines, Ogre::GpuProgramParametersSharedPtr programParameters, bool isFragmentShader = false);
    
    const std::string ShaderName(int index);
    
    QString OldMeshmoonShaderAlias(QString newShaderName);
    QString RexShaderAlias(QString oldMeshmoonShaderName);

    bool IsConstantExluded(const std::string& name, const QList<std::string>& defines);
    
    //bool HasAllDependencyDefines(int defineIndex, QList<int> &allDefineIndexes);
    //void InjectDependencyDefines(int defineIndex, QList<int> &allDefineIndexes);
    void ExcludeConflictingDefines(int defineIndex, QList<int> &allDefineIndexes);

    std::string                 _shaderSource;

    std::string                 _fragmentShaderEntryPoint;
    std::string                 _vertexShaderEntryPoint;

    std::string                 _vertexShaderProfiles;
    std::string                 _fragmentShaderProfiles;

    int                         _maxLightCount;

    RocketGPUProgramConstantMap _vertexShaderParameters;
    RocketGPUProgramConstantMap _fragmentShaderParameters;

    ShaderNameMap               _shaderNames;
    ExcludeConstantMap          _excludeConstants;
    DefineMap                   _excludeDefines;
    DefineMap                   _defineDependencies;
    ShaderOrderMap              _shaderOrders;

    int                         _shadowQuality;
    bool                        _useCsm;
    bool                        _useOpenGL;

    Framework*                  _framework;
};

// MeshmoonShaderInfo

struct MeshmoonShaderInfo
{
    QString name;
    QStringList aliases;
    QStringList compileArguments;

    MeshmoonShaderInfo(const QString &_name = "", const QStringList &_compileArguments = QStringList());

    QString VertexProgramName() const;
    QString FragmentProgramName() const;

    bool Matches(QString shaderName) const;
    bool MatchesAlias(QString shaderName) const;

    QString TextureType(const QString &compileArgument, int /*textureUnitIndex*/) const;
    
    /// @note Ignores shadow maps.
    static QString TextureTypeFromTextureUnit(const QString &textureUnitName, const QString &textureUnitAlias);

    QString TextureUnitName(const QString compileArgument, int textureUnitIndex) const;
    static QString TextureUnitName(const QStringList &compileArguments, const QString compileArgument, int textureUnitIndex);

    QStringList FloatTypeNamedParameters() const;
    float FloatTypeNamedParameterDefaultValue(const QString &namedParameter) const;
    
    static QString TilingNamedParameterName(const QString &compileArgument);
    static QString UserFriendlyNamedParameterName(const QString &namedParameter);
    
    QString UserFriendlyShaderName() const;
};

/// @endcond
