/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPlugin.h
    @brief  The Meshmoon Rocket client plugin. */

#include "StableHeaders.h"

#include "RocketPlugin.h"
#include "RocketGPUProgramGenerator.h"
#include "RocketGPUProgramConstant.h"
#include "Math/float2.h"
#include "Math/float4.h"

#include "Framework.h"
#include "ConfigAPI.h"
#include "RocketSettings.h"
#include "Renderer.h"

#include <OgreResourceGroupManager.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreGpuProgramManager.h>
#include <OgreGpuProgramParams.h>

#include <qvariant.h>

namespace
{
    // SPECULAR_LIGHTING
    const QString cNamedParameterSpecularPower("specularPower");
    const QString cNamedParameterSpecularHardness("specularHardness");
    
    // LIGHT_MAPPING
    const QString cNamedParameterLightMapPower("lightMapPower");

    // *_MAPPING tiling
    const QString cNamedParameterDiffuseMapTiling("diffuseMapTiling");
    const QString cNamedParameterDiffuseMap2Tiling("diffuseMap2Tiling");
    const QString cNamedParameterNormalMapTiling("normalMapTiling");
    const QString cNamedParameterSpecularMapTiling("specularMapTiling");
    const QString cNamedParameterLightMapTiling("lightMapTiling");
    
    const QString cNamedParameterWeightMapTiling("weightMapTiling");
    const QString cNamedParameterWeightDiffuseMapTilings("diffuseMapTilings");
}

// RocketGPUProgramGenerator static

// Sorts defined by the texture unit index order the material should have.
bool SortMeshmoonShaderDefines(const QString &s1, const QString &s2)
{
    /** This can be prioritized blindly as it wont be used together with
        other diffuse defines. It also trumps anything else the shader 
        might have eg. lightmap. */
    if (s1 == "WEIGHTED_DIFFUSE")
        return true;
    else if (s2 == "WEIGHTED_DIFFUSE")
        return false;

    // 1. Diffuse
    if (s1 == "DIFFUSE_MAPPING")
        return true;
    // 2. Multi diffuse (2nd diffuse)
    else if (s1 == "MULTI_DIFFUSE")
    {
        if (s2 == "DIFFUSE_MAPPING")
            return false;
    }
    // 3. Normal map
    else if (s1 == "NORMAL_MAPPING")
    {
        if (s2 == "MULTI_DIFFUSE" || s2 == "DIFFUSE_MAPPING")
            return false;
    }
    // 4. Specular
    else if (s1 == "SPECULAR_MAPPING")
    {
        if (s2 == "MULTI_DIFFUSE" || s2 == "DIFFUSE_MAPPING" || s2 == "NORMAL_MAPPING")
            return false;
    }
    // 5. Lightmap
    else if (s1 == "LIGHT_MAPPING")
    {
        if (s2 == "MULTI_DIFFUSE" || s2 == "DIFFUSE_MAPPING" || s2 == "NORMAL_MAPPING" || s2 == "SPECULAR_MAPPING")
            return false;
    }
    // 6. Shadow maps
    else if (s1 == "SHADOW_MAPPING")
    {
        if (s2 == "MULTI_DIFFUSE" || s2 == "DIFFUSE_MAPPING" || s2 == "NORMAL_MAPPING" || s2 == "SPECULAR_MAPPING" || s2 == "LIGHT_MAPPING")
            return false;
    }
    else
        return false;
    return true;
}

void RocketGPUProgramGenerator::CreateShaders(Framework* framework)
{
    if (!framework)
        return;

    LogDebug("[RocketGPUProgramGenerator]: Generating shader programs");
    RocketGPUProgramGenerator generator(framework);
    generator.Generate();
}

MeshmoonShaderInfoList RocketGPUProgramGenerator::MeshmoonShaders(Framework* framework)
{
    if (!framework)
        return MeshmoonShaderInfoList();

    RocketGPUProgramGenerator generator(framework);

    QList<QList<std::string> > shaderDefines = generator.GenerateShaderDefineCombinations();
    generator.GenerateAdditionalShaderDefineCombinations(shaderDefines);

    MeshmoonShaderInfoList shaderInfos;
    foreach(QList<std::string> defines, shaderDefines)
    {
        // Contstruct name and store defines
        QStringList compileArguments;
        std::string programName = "meshmoon/";
        foreach(const std::string& define, defines)
        {
            programName += generator._shaderNames[define];
            compileArguments << QString::fromStdString(define);
        }
        
        // Sort compile arguments into the texture unit index order.
        qSort(compileArguments.begin(), compileArguments.end(), SortMeshmoonShaderDefines);

        MeshmoonShaderInfo info(QString::fromStdString(programName), compileArguments);
        
        // Find aliases
        QString oldMeshmoonAlias = generator.OldMeshmoonShaderAlias(info.name);
        if (!oldMeshmoonAlias.isEmpty())
        {
            info.aliases << oldMeshmoonAlias;
            QString rexAlias = generator.RexShaderAlias(oldMeshmoonAlias);
            if (!oldMeshmoonAlias.isEmpty())
                info.aliases << rexAlias;
        }
        shaderInfos << info;
    }
    return shaderInfos;
}

// RocketGPUProgramGenerator

RocketGPUProgramGenerator::RocketGPUProgramGenerator(Framework* framework) :
    _framework(framework)
{
    ConfigData config("adminotech", "clientplugin");

    ClientSettings::GraphicsMode graphicsMode = static_cast<ClientSettings::GraphicsMode>(
        framework->Config()->Read(config, "graphicsmode", ClientSettings::GraphicsLow).toInt());

     _shaderSource = "MeshmoonShader.cg";
    
    _vertexShaderEntryPoint = "mainVS";
    _fragmentShaderEntryPoint = "mainPS";

    _vertexShaderProfiles = "vs_3_0 arbvp1";
    _fragmentShaderProfiles = "ps_3_0 arbfp1";

    switch(graphicsMode)
    {
    case ClientSettings::GraphicsLow:
        _maxLightCount = 3;
        break;
    case ClientSettings::GraphicsMedium:
        _maxLightCount = 4;
        break;
    case ClientSettings::GraphicsHigh:
        _maxLightCount = 8;
        break;
    case ClientSettings::GraphicsUltra:
    case ClientSettings::GraphicsCustom:
        _maxLightCount = 10;
        break;
    case ClientSettings::GraphicsNone:
        LogError("[RocketGPUProgramGenerator]: Graphics is set to GraphicsNone mode.");
        return;
    }

    const ConfigData renderingCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING);
    _shadowQuality = framework->Config()->Read(renderingCfg, "shadow quality", 0).toInt();
    _useCsm = _shadowQuality < 2 ? false : true;
    
    _useOpenGL = false;
#ifndef Q_OS_WIN
    _useOpenGL = true;
#endif
    if(framework->HasCommandLineParameter("--opengl"))
        _useOpenGL = true;

    // Default
    _vertexShaderParameters["DEFAULT"] << new RocketGPUProgramAutoConstant("worldViewProjMatrix", Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
    _vertexShaderParameters["DEFAULT"] << new RocketGPUProgramAutoConstant("worldMatrix", Ogre::GpuProgramParameters::ACT_WORLD_MATRIX);

    if(!_useOpenGL)
        _fragmentShaderParameters["DEFAULT"] << new RocketGPUProgramAutoConstant("worldMatrix", Ogre::GpuProgramParameters::ACT_WORLD_MATRIX);
    //_fragmentShaderParameters["DEFAULT"] << new RocketGPUProgramAutoConstant("inverseWorldMatrix", Ogre::GpuProgramParameters::ACT_INVERSE_WORLD_MATRIX);

    // Normal mapping
    //vertexShaderParameters["NORMAL_MAPPING"] << new RocketGPUProgramAutoConstant("inverseWorldMatrix", Ogre::GpuProgramParameters::ACT_INVERSE_WORLD_MATRIX);

    // Shadows
    //_vertexShaderParameters["SHADOW_MAPPING"] << new RocketGPUProgramAutoConstant("shadowTextureProj0", Ogre::GpuProgramParameters::ACT_TEXTURE_WORLDVIEWPROJ_MATRIX, 0);

    // Default
    _fragmentShaderParameters["DEFAULT"] << new RocketGPUProgramAutoConstant("ambientLight", Ogre::GpuProgramParameters::ACT_AMBIENT_LIGHT_COLOUR);

    // Diffuse lighting
    _fragmentShaderParameters["DIFFUSE_LIGHTING"] << new RocketGPUProgramAutoConstant("lightDirections", Ogre::GpuProgramParameters::ACT_LIGHT_POSITION_ARRAY, _maxLightCount);
    _fragmentShaderParameters["DIFFUSE_LIGHTING"] << new RocketGPUProgramAutoConstant("lightAttenuations", Ogre::GpuProgramParameters::ACT_LIGHT_ATTENUATION_ARRAY, _maxLightCount);
    _fragmentShaderParameters["DIFFUSE_LIGHTING"] << new RocketGPUProgramAutoConstant("lightDiffuseColors", Ogre::GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR_ARRAY, _maxLightCount);

    if(!_useOpenGL)
    {
        _fragmentShaderParameters["DIFFUSE_LIGHTING"] << new RocketGPUProgramAutoConstant("spotLightParams", Ogre::GpuProgramParameters::ACT_SPOTLIGHT_PARAMS_ARRAY, _maxLightCount);
        _fragmentShaderParameters["DIFFUSE_LIGHTING"] << new RocketGPUProgramAutoConstant("spotLightDirections", Ogre::GpuProgramParameters::ACT_LIGHT_DIRECTION_OBJECT_SPACE_ARRAY, _maxLightCount);
    }
    //fragmentShaderParameters["DIFFUSE_MAPPING"] << new RocketGPUProgramAutoConstant("lightCount", Ogre::GpuProgramParameters::ACT_LIGHT_COUNT);

    // Specular lighting
    _fragmentShaderParameters["SPECULAR_LIGHTING"] << new RocketGPUProgramAutoConstant("cameraPosition", Ogre::GpuProgramParameters::ACT_CAMERA_POSITION);
    _fragmentShaderParameters["SPECULAR_LIGHTING"] << new RocketGPUProgramAutoConstant("lightSpecularColors", Ogre::GpuProgramParameters::ACT_LIGHT_SPECULAR_COLOUR_ARRAY, _maxLightCount);
    _fragmentShaderParameters["SPECULAR_LIGHTING"] << new RocketGPUProgramConstant<float>("specularHardness", 10.f);
    _fragmentShaderParameters["SPECULAR_LIGHTING"] << new RocketGPUProgramConstant<float>("specularPower", 1.0f);

    // Diffuse mapping
    _fragmentShaderParameters["DIFFUSE_MAPPING"] << new RocketGPUProgramConstant<float2>("diffuseMapTiling", float2(1.0f, 1.0f));

    // Normal mapping
    _fragmentShaderParameters["NORMAL_MAPPING"] << new RocketGPUProgramConstant<float2>("normalMapTiling", float2(1.0f, 1.0f));

    // Specular mapping
    _fragmentShaderParameters["SPECULAR_MAPPING"] << new RocketGPUProgramConstant<float2>("specularMapTiling", float2(1.0f, 1.0f));

    // Light mapping
    _fragmentShaderParameters["LIGHT_MAPPING"] << new RocketGPUProgramConstant<float2>("lightMapTiling", float2(1.0f, 1.0f));
    _fragmentShaderParameters["LIGHT_MAPPING"] << new RocketGPUProgramConstant<float>("lightMapPower", 1.0f);

    /*_fragmentShaderParameters["SHADOW_MAPPING"] << new RocketGPUProgramConstant<float4>("fixedDepthBias", float4(0.000005f, 0.000005f, 0.000005f, 0.000005f));
    _fragmentShaderParameters["SHADOW_MAPPING"] << new RocketGPUProgramConstant<float4>("gradientScaleBias", float4(0.00005f, 0.00005f, 0.00005f, 0.00005f));
    
    _fragmentShaderParameters["SHADOW_MAPPING"] << new RocketGPUProgramConstant<float>("shadowFadeDist", 900.0f);
    _fragmentShaderParameters["SHADOW_MAPPING"] << new RocketGPUProgramConstant<float>("shadowMaxDist", 1000.0f);

    float4 shadowMapSize(1024, 1024, 1024, 1024);
    _fragmentShaderParameters["CSM_SHADOWS"] << new RocketGPUProgramConstant<float4>("shadowMapSize", shadowMapSize);

    float4 inverseShadowMapSize(1.0f / shadowMapSize.x, 1.0f / shadowMapSize.y, 1.0f / shadowMapSize.z, 1.0f / shadowMapSize.w);
    _fragmentShaderParameters["CSM_SHADOWS"] << new RocketGPUProgramConstant<float4>("invShadowMapSize", inverseShadowMapSize);*/

    _fragmentShaderParameters["JITTER_SHADOWS"] << new RocketGPUProgramConstant<float>("jitterSize", 1.0f);

    // Weighted diffuse
    float4 diffuseMapTiling[2];
    for(int i = 0; i < 2; ++i)
        diffuseMapTiling[i] = float4(10, 10, 10, 10);
    _fragmentShaderParameters["WEIGHTED_DIFFUSE"] << new RocketGPUProgramArrayConstant<float4>("diffuseMapTilings", diffuseMapTiling, 2);
    _fragmentShaderParameters["WEIGHTED_DIFFUSE"] << new RocketGPUProgramConstant<float2>("weightMapTiling", float2(1.0f, 1.0f));

    // Shader names
    /*
    _shaderNames["SPECULAR_LIGHTING"] = "SpecularLighting";
    _shaderNames["AMBIENT_COLOR"] = "AmbientColor";
    _shaderNames["DIFFUSE_COLOR"] = "DiffuseColor";
    _shaderNames["SPECULAR_COLOR"] = "SpecularColor";
    _shaderNames["EMISSIVE_COLOR"] = "EmissiveColor";
    */

    _shaderNames["DIFFUSE_MAPPING"] = "Diffuse";
    _shaderNames["MULTI_DIFFUSE"] = "Multi";
    _shaderNames["NORMAL_MAPPING"] = "NormalMap";
    _shaderNames["SPECULAR_MAPPING"] = "SpecularMap";
    _shaderNames["LIGHT_MAPPING"] = "LightMap";
    _shaderNames["SHADOW_MAPPING"] = "Shadow";

    // Exclude parameters
    _excludeConstants["LIGHT_MAPPING"] << "ambientLight";
    
    /*
    _excludeConstants["AMBIENT_COLOR"] << "ambientLight";
    _excludeConstants["SPECULAR_COLOR"] << "lightSpecularColors";

    _excludeDefines["DIFFUSE_COLOR"] << "DIFFUSE_MAPPING" << "MULTI_DIFFUSE" << "WEIGHTED_DIFFUSE";
    _excludeDefines["EMISSIVE_COLOR"] << "LIGHT_MAPPING";

    _defineDependencies["SPECULAR_MAPPING"] << "SPECULAR_LIGHTING";
    */

    // Shader define orders

    /*
    _shaderOrders["SPECULAR_LIGHTING"] = 0;
    _shaderOrders["AMBIENT_COLOR"] = 3;
    _shaderOrders["DIFFUSE_COLOR"] = 4;
    _shaderOrders["SPECULAR_COLOR"] = 5;
    _shaderOrders["EMISSIVE_COLOR"] = 6;
    */

    _shaderOrders["DIFFUSE_MAPPING"] = 0;
    _shaderOrders["MULTI_DIFFUSE"] = 1;
    _shaderOrders["NORMAL_MAPPING"] = 2;
    _shaderOrders["SPECULAR_MAPPING"] = 3;
    _shaderOrders["LIGHT_MAPPING"] = 4;
    _shaderOrders["SHADOW_MAPPING"] = 5;
}

RocketGPUProgramGenerator::~RocketGPUProgramGenerator()
{
    // Delete constants
    foreach(const QList<IRocketGPUProgramConstant*>& constants, _vertexShaderParameters)
    {
        for(int i = 0; i < constants.size(); ++i)
        {
            if(!constants[i])
                continue;
            delete constants[i];
        }
    }

    foreach(const QList<IRocketGPUProgramConstant*>& constants, _fragmentShaderParameters)
    {
        for(int i = 0; i < constants.size(); ++i)
        {
            if(!constants[i])
                continue;
            delete constants[i];
        }
    }

    _vertexShaderParameters.clear();
    _fragmentShaderParameters.clear();
}

std::string RocketGPUProgramGenerator::CompilerArguments(const QList<std::string> &defines, int maxLightCount, bool useOpenGL)
{
    std::string compileDefines;
    foreach(const std::string& define, defines)
        compileDefines += " -D" + define;

    return QString("%1 -DMAX_LIGHT_COUNT=%2 %3")
        .arg(QString::fromStdString(compileDefines))
        .arg(maxLightCount)
        .arg(useOpenGL ? "-profileopts MaxLocalParams=224" : "").trimmed().toStdString();
}

void RocketGPUProgramGenerator::GenerateAdditionalShaderDefineCombinations(QList<QList<std::string> > &shaderDefines)
{
    _shaderNames["WEIGHTED_DIFFUSE"] = "WeightedDiffuse";

    QList<std::string> additionalDefines;
    additionalDefines << "DIFFUSE_LIGHTING"
                      << "WEIGHTED_DIFFUSE";

    shaderDefines << additionalDefines;
    additionalDefines.clear();

    additionalDefines << "DIFFUSE_LIGHTING"
                      << "WEIGHTED_DIFFUSE"
                      << "LIGHT_MAPPING";

    shaderDefines << additionalDefines;
    additionalDefines.clear();
    
    additionalDefines << "DIFFUSE_LIGHTING"
                      << "WEIGHTED_DIFFUSE"
                      << "SHADOW_MAPPING";

    shaderDefines << additionalDefines;
    additionalDefines.clear();

    additionalDefines << "DIFFUSE_LIGHTING"
                      << "WEIGHTED_DIFFUSE"
                      << "LIGHT_MAPPING"
                      << "SHADOW_MAPPING";

    shaderDefines << additionalDefines;
}

void RocketGPUProgramGenerator::Generate()
{
    QList<QList<std::string> > shaderDefines = GenerateShaderDefineCombinations();
    GenerateAdditionalShaderDefineCombinations(shaderDefines);

    // Output definitions to JSON to be used in Shaderor.
    if (_framework->HasCommandLineParameter("--outputshadervariants"))
    {
        QFile definitionFile("meshmoonShaderVariants.json");
        if (definitionFile.open(QIODevice::WriteOnly))
        {
            QTextStream stream(&definitionFile);

            stream << "{\n";

            stream << "\t\"Variants\" : [\n";

            int currentVariant = 0;
            foreach(const QList<std::string>& defines, shaderDefines)
            {
                stream << "\t\t{\n";

                QString programName = "meshmoon/";
                foreach(const std::string& define, defines)
                    programName += QString::fromStdString(_shaderNames[define]);

                stream << "\t\t\t\"Name\" : \"" << programName << "\",\n";
                stream << "\t\t\t\"Defines\" : [\n";

                int currentDefine = 0;
                foreach(const std::string& define, defines)
                {
                    stream << "\t\t\t\t\"" << QString::fromStdString(define) << "\"" << ((currentDefine < defines.size() - 1) ? ",\n" : "\n");
                    ++currentDefine;
                }

                stream << "\t\t\t]\n";
                stream << "\t\t}" << ((currentVariant < shaderDefines.size() - 1) ? ",\n\n" : "\n");

                ++currentVariant;
            }

            stream << "\t]\n";
            stream << "}\n";
        }
    }

    foreach(QList<std::string> defines, shaderDefines)
    {
        std::string programName = "meshmoon/";
        foreach(const std::string& define, defines)
            programName += _shaderNames[define];

        // Compile arguments
        std::string compileArguments = CompilerArguments(defines, _maxLightCount, _useOpenGL);

        if (IsLogChannelEnabled(LogChannelDebug))
        {
            QString section = "";
            
            qDebug() << endl << programName.c_str();
            foreach(QString arg, QString::fromStdString(compileArguments).split(" "))
            {   
                if (arg.startsWith("-D", Qt::CaseSensitive))
                    arg = arg.mid(2);

                // Define section
                if ((arg == "DIFFUSE_LIGHTING" || arg == "SPECULAR_LIGHTING") && section != "Lighting")
                {
                    section = "Lighting";
                    qDebug() << " -" << qPrintable(section);
                }
                else if (arg.endsWith("_COLOR") && section != "Coloring")
                {
                    section = "Coloring";
                    qDebug() << " -" << qPrintable(section);
                }
                else if (arg.endsWith("_MAPPING") && section != "Texturing")
                {
                    section = "Texturing";
                    qDebug() << " -" << qPrintable(section);
                }
                else if (arg.contains("_COUNT") && section != "Limits")
                {
                    section = "Limits";
                    qDebug() << " -" << qPrintable(section);
                }
                
                qDebug() << "    " << qPrintable(arg);
            }
        }

        // Add default
        defines << "DEFAULT";

        if(!_useOpenGL)
            compileArguments += " -DSPOTLIGHTS_ENABLED";

        if (defines.contains("SHADOW_MAPPING"))
        {
            defines << "JITTER_SHADOWS";
            if (_useCsm)
            {
                defines << "CSM_SHADOWS";
                compileArguments += " -DCSM_SHADOWS";
            }
        }

        try
        {
            // Create vertex shader
            Ogre::HighLevelGpuProgramPtr vertexProgram = Ogre::HighLevelGpuProgramManager::getSingleton().createProgram(programName + "VP", RocketPlugin::MESHMOON_RESOURCE_GROUP, "cg", Ogre::GPT_VERTEX_PROGRAM);

            vertexProgram->setSourceFile(_shaderSource);
            vertexProgram->setParameter("entry_point", _vertexShaderEntryPoint);
            vertexProgram->setParameter("profiles", _vertexShaderProfiles);
            vertexProgram->setParameter("compile_arguments", compileArguments);

            Ogre::GpuProgramParametersSharedPtr vertexParams = vertexProgram->getDefaultParameters();

            if (defines.contains("SHADOW_MAPPING"))
                vertexParams->addSharedParameters("params_shadowTextureMatrix");

            SetConstants(defines, vertexParams);

            // Load vertex shader
            vertexProgram->load();
        }
        catch(const Ogre::Exception& e)
        {
            LogError("[RocketGPUProgramGenerator]: Exception while creating GPU vertex shader: " + e.getFullDescription());
        }

        try
        {
            // Create fragment shader
            Ogre::HighLevelGpuProgramPtr fragmentShader = Ogre::HighLevelGpuProgramManager::getSingleton().createProgram(programName + "FP", RocketPlugin::MESHMOON_RESOURCE_GROUP, "cg", Ogre::GPT_FRAGMENT_PROGRAM);

            fragmentShader->setSourceFile(_shaderSource);
            fragmentShader->setParameter("entry_point", _fragmentShaderEntryPoint);
            fragmentShader->setParameter("profiles", _fragmentShaderProfiles);

            if (!_useOpenGL && defines.contains("CSM_SHADOWS"))
                compileArguments += " -DTEX2D_FORCE_ZERO_GRAD_IN_BRANCH";

            LogDebug(programName + "FP");
            LogDebug("  " + compileArguments);
            fragmentShader->setParameter("compile_arguments", compileArguments);

            Ogre::GpuProgramParametersSharedPtr fragmentParams = fragmentShader->getDefaultParameters();

            if (defines.contains("SHADOW_MAPPING"))
            {
                fragmentParams->addSharedParameters("params_shadowMatrixScaleBias");
                fragmentParams->addSharedParameters("params_shadowParams");
            }

            SetConstants(defines, fragmentParams, true);

            // Load fragment shader
            fragmentShader->load();
        }
        catch(const Ogre::Exception& e)
        {
            LogError("[RocketGPUProgramGenerator]: Exception while creating GPU fragment shader: " + e.getFullDescription());
        }
    }
}

void RocketGPUProgramGenerator::SetConstants(const QList<std::string> &defines, Ogre::GpuProgramParametersSharedPtr programParameters, bool isFragmentShader)
{
    foreach(const std::string &define, defines)
    {
        if (!isFragmentShader && _vertexShaderParameters.find(define) == _vertexShaderParameters.end())
            continue;
        else if (isFragmentShader && _fragmentShaderParameters.find(define) == _fragmentShaderParameters.end())
            continue;

        foreach(IRocketGPUProgramConstant* constant, (!isFragmentShader ? _vertexShaderParameters[define] : _fragmentShaderParameters[define]))
        {
            if (!constant || IsConstantExluded(constant->Name(), defines))
                continue;

            try
            {
                constant->SetOgreConstant(programParameters);
            }
            catch(const Ogre::Exception& e)
            {
                LogWarning(QString("[RocketGPUProgramGenerator]: Exception when setting Ogre Constant '%1': %2")
                    .arg(QString::fromStdString(constant->Name())).arg(QString::fromStdString(e.getFullDescription())));
            }
        }
    }
}

struct ShaderDefineComparer
{
public:
    ShaderDefineComparer(const QMap<std::string, int>& order) :
       _order(order)
    {}

    bool operator()(const std::string& left, const std::string& right)
    {
        if(!_order.contains(left))
            return false;
        else if(!_order.contains(right))
            return true;
        return _order[left] < _order[right];
    }
private:
    QMap<std::string, int> _order;
};

RocketGPUProgramGenerator::CombinationList RocketGPUProgramGenerator::GenerateShaderDefineCombinations()
{
    QList<QList<std::string> > result;

    // Add diffuse mapping only as default
    QList<std::string> defaultDefines;
    defaultDefines << "DIFFUSE_LIGHTING";
    defaultDefines << "SPECULAR_LIGHTING";
    defaultDefines << "DIFFUSE_MAPPING";

    result << defaultDefines;

    QList<std::string> shaderDefines;
    foreach(const std::string& define, _shaderNames.keys())
    {
        if (!defaultDefines.contains(define))
            shaderDefines << define;
    }

    QList<int> values;
    foreach(const std::string& key, _shaderOrders.keys())
    {
        if (shaderDefines.contains(key))
            values << _shaderOrders[key];
    }
    qSort(values);

    // Generate all possible combinations from range [1 ... *]
    CombinationListInt finalCombinations;
    CombinationListInt lastCombinations;

    while(true)
    {
        lastCombinations = GenerateCombinations(lastCombinations, values);
        if (lastCombinations.size() == 0)
            break;
        finalCombinations << lastCombinations;
    }

    CombinationList stringCombinations;
    stringCombinations << defaultDefines;

    foreach(const QList<int> &combination, finalCombinations)
    {
        QList<std::string> stringCombination;
        stringCombination << defaultDefines;
 
        foreach(int i, combination)
            stringCombination << ShaderName(i);
        stringCombinations << stringCombination;
    }

    return stringCombinations;
}

bool HasMoreVariations(const QList<int>& combination, const QList<int>& values)
{
    foreach(int v, values)
        if(combination.last() < v)
            return true;
    return false;
}

bool HasMoreVariations(const RocketGPUProgramGenerator::CombinationListInt& combinations, const QList<int>& values)
{
    int newVariations = 0;
    foreach(const QList<int>& combination, combinations)
    {
        QString str;
        foreach(int i, combination)
            str += QString::number(i) + " ";
        newVariations += HasMoreVariations(combination, values) ? 1 : 0;
    }
    return newVariations > 0;
}

bool IsBiggestValue(const QList<int>& combination, int value)
{
    int max = -1;
    foreach(int i, combination)
        if(i > max)
            max = i;
    if(value > max)
        return true;
    return false;
}

int FirstVariation(const RocketGPUProgramGenerator::CombinationListInt& combinations, const QList<int>& values)
{
    if(combinations.size() <= 0)
        return -1;

    const QList<int>& combination = combinations.last();
    foreach(int v, values)
    {
        if(v > combination.last())
            return v;
    }
    return -1;
}

RocketGPUProgramGenerator::CombinationListInt RocketGPUProgramGenerator::GenerateCombinations(CombinationListInt& combinations, const QList<int>& values)
{
    CombinationListInt result;

    int newCombinations = 0;
    // Check if there is more variations at all
    foreach(const QList<int>& combination, combinations)
        newCombinations += HasMoreVariations(combination, values) ? 1 : 0;
    if (newCombinations == 0 && combinations.size() > 0)
        return CombinationListInt();

    if (combinations.size() > 0)
    {
        foreach(const QList<int>& combination, combinations)
        {
            bool skipCombo = false;

            CombinationListInt newCombinations;
            if (HasMoreVariations(combination, values))
            {
                QList<int> newCombination = combination;
                foreach(int v, values)
                {      
                    if (!newCombination.contains(v) && IsBiggestValue(newCombination, v))
                    {
                        // Exclude other indexes if v has rules about them.
                        ExcludeConflictingDefines(v, newCombination);

                        // Dependency check
                        /*if (!HasAllDependencyDefines(v, newCombination))
                            skipCombo = true;*/
                        
                        if (!skipCombo)
                            newCombination << v;
                        break;
                    }
                }
                
                
                if (!skipCombo && newCombination.size() == (combination.size() + 1))
                    newCombinations << newCombination;
            }
            if (skipCombo)
                continue;

            int firstVariation = FirstVariation(newCombinations, values);
            while(firstVariation > -1)
            {
                QString str = "";
                foreach(int i, combination)
                    str += QString::number(i) + " ";
                QList<int> newCombination = combination;
                
                newCombination << firstVariation;
                
                str = "";
                foreach(int i, newCombination)
                    str += QString::number(i) + " ";
                if(newCombination.size() == (combination.size() + 1))
                    newCombinations << newCombination;
                firstVariation = FirstVariation(newCombinations, values);
            }
            result << newCombinations;
        }
    }
    else
    {
        // No previous combinations.
        foreach(int v, values)
        {
            QList<int> combination;
            combination << v;
            result << combination;
        }
    }
    return result;
}

const std::string RocketGPUProgramGenerator::ShaderName(int index)
{
    ShaderOrderMap::iterator it = _shaderOrders.begin();
    for(; it != _shaderOrders.end(); ++it)
    {
        if(it.value() == index)
            return it.key();
    }
    return "";
}

bool RocketGPUProgramGenerator::IsConstantExluded(const std::string& name, const QList<std::string>& defines)
{
    foreach(const std::string& define, defines)
    {
        if (!_excludeConstants.contains(define))
            continue;
        foreach(const std::string& constantName, _excludeConstants[define])
        {
            if (constantName == name)
                return true;
        }
    }
    return false;
}

/*
bool RocketGPUProgramGenerator::HasAllDependencyDefines(int defineIndex, QList<int> &allDefineIndexes)
{
    const std::string defineName = _shaderOrders.key(defineIndex);
    if (_defineDependencies.contains(defineName))
    {
        qDebug() << "     Checking deps for" << defineName.c_str();
        foreach(const std::string &neededKey, _defineDependencies[defineName])
        {
            int neededDefineIndex = _shaderOrders[neededKey];
            if (!allDefineIndexes.contains(neededDefineIndex))
            {
                qDebug() << "         Missing" << neededKey.c_str();
                return false;
            }
        }
    }
    return true;
}

void RocketGPUProgramGenerator::InjectDependencyDefines(int defineIndex, QList<int> &allDefineIndexes)
{
    const std::string defineName = _shaderOrders.key(defineIndex);
    if (_defineDependencies.contains(defineName))
    {
        qDebug() << "     Checking deps for" << defineName.c_str();
        foreach(const std::string &neededKey, _defineDependencies[defineName])
        {
            int neededDefineIndex = _shaderOrders[neededKey];
            if (!allDefineIndexes.contains(neededDefineIndex))
            {
                qDebug() << "         Injecting" << neededKey.c_str();
                if (allDefineIndexes.size() > 2)
                    allDefineIndexes.insert(2, neededDefineIndex);
                else
                    allDefineIndexes << neededDefineIndex;
            }
        }
    }
}
*/

void RocketGPUProgramGenerator::ExcludeConflictingDefines(int defineIndex, QList<int> &allDefineIndexes)
{
    const std::string defineName = _shaderOrders.key(defineIndex);
    if (_excludeDefines.contains(defineName))
    {
        qDebug() << "     Excluding with" << defineName.c_str();
        foreach (const std::string &excludeDefineName, _excludeDefines[defineName])
        {
            int excludeDefineIndex = _shaderOrders[excludeDefineName];
            if (allDefineIndexes.contains(excludeDefineIndex))
            {
                qDebug() << "         " << excludeDefineName.c_str();
                allDefineIndexes.removeAll(excludeDefineIndex);
            }
        }
    }
}

QString RocketGPUProgramGenerator::OldMeshmoonShaderAlias(QString newShaderName)
{
    QString original = newShaderName;

    if (newShaderName.contains("DiffuseMulti"))
        newShaderName = newShaderName.replace("DiffuseMulti", "MultiDiff", Qt::CaseSensitive);
    if (newShaderName.contains("Diffuse"))
        newShaderName = newShaderName.replace("Diffuse", "Diff", Qt::CaseSensitive);
    if (newShaderName.contains("SpecularMap"))
        newShaderName = newShaderName.replace("SpecularMap", "Specmap", Qt::CaseSensitive);
    if (newShaderName.contains("NormalMap"))
        newShaderName = newShaderName.replace("NormalMap", "Normal", Qt::CaseSensitive);
    if (newShaderName.contains("LightMap"))
        newShaderName = newShaderName.replace("LightMap", "Lightmap", Qt::CaseSensitive);

    // Switcharoooo
    if (newShaderName.contains("NormalSpecmap"))
        newShaderName = newShaderName.replace("NormalSpecmap", "SpecmapNormal", Qt::CaseSensitive);
    
    if (newShaderName != original)
    {
        Ogre::ResourcePtr alias = Ogre::HighLevelGpuProgramManager::getSingletonPtr()->getByName(newShaderName.toStdString() + "VP");
        if (!alias.get())
        {
            //LogWarning(QString("Failed to find Meshmoon alias: %1 for %2").arg(newShaderName).arg(original));
            newShaderName = "";
        }
    }
    else
        newShaderName = "";
    return newShaderName;
}

QString RocketGPUProgramGenerator::RexShaderAlias(QString oldMeshmoonShaderName)
{
    if (oldMeshmoonShaderName.startsWith("meshmoon/", Qt::CaseSensitive))
    {
        QString original = oldMeshmoonShaderName;
        oldMeshmoonShaderName  = "rex/" + oldMeshmoonShaderName.mid(9);
        Ogre::ResourcePtr alias = Ogre::HighLevelGpuProgramManager::getSingletonPtr()->getByName(oldMeshmoonShaderName.toStdString() + "VP");
        if (!alias.get())
        {
            //LogWarning(QString("Failed to find realXtend alias: %1 for %2").arg(oldMeshmoonShaderName).arg(original));
            oldMeshmoonShaderName = "";
        }
    }
    else
        oldMeshmoonShaderName = "";
    return oldMeshmoonShaderName;
}

// MeshmoonShaderInfo

MeshmoonShaderInfo::MeshmoonShaderInfo(const QString &_name, const QStringList &_compileArguments) :
    name(_name),
    compileArguments(_compileArguments)
{
}

QString MeshmoonShaderInfo::VertexProgramName() const
{
    return name + "VP";
}

QString MeshmoonShaderInfo::FragmentProgramName() const
{
    return name + "FP";
}

bool MeshmoonShaderInfo::Matches(QString shaderName) const
{
    if (shaderName.endsWith("VP") || shaderName.endsWith("FP"))
        shaderName = shaderName.left(shaderName.length()-2);

    return (name.compare(shaderName, Qt::CaseSensitive) == 0);
}

bool MeshmoonShaderInfo::MatchesAlias(QString shaderName) const
{
    if (shaderName.endsWith("VP") || shaderName.endsWith("FP"))
        shaderName = shaderName.left(shaderName.length()-2);

    if (aliases.contains(shaderName, Qt::CaseSensitive))
        return true;    
    return false;
}

QString MeshmoonShaderInfo::TextureType(const QString &compileArgument, int /*textureUnitIndex*/) const
{
    if (compileArgument == "WEIGHTED_DIFFUSE")
        return "Weighted Diffuse";
    else if (compileArgument == "DIFFUSE_MAPPING")
        return !compileArguments.contains("MULTI_DIFFUSE", Qt::CaseSensitive) ? "Diffuse" : "Diffuse 1";
    else if (compileArgument == "MULTI_DIFFUSE")
        return "Diffuse 2";
    else if (compileArgument == "NORMAL_MAPPING")
        return "Normal";
    else if (compileArgument == "SPECULAR_MAPPING")
        return "Specular";
    else if (compileArgument == "LIGHT_MAPPING")
        return "Light map";
    else if (compileArgument == "SHADOW_MAPPING")
        return "Shadow map";
    return "";
}

QString MeshmoonShaderInfo::TextureTypeFromTextureUnit(const QString &textureUnitName, const QString &textureUnitAlias)
{
    if (textureUnitName.compare("baseMap", Qt::CaseInsensitive) == 0 || textureUnitAlias.compare("baseMap", Qt::CaseInsensitive) == 0 ||
        textureUnitName.contains("diffuse", Qt::CaseInsensitive) || textureUnitAlias.contains("diffuse", Qt::CaseInsensitive))
        return "Diffuse";
    else if (textureUnitName.compare("specularMap", Qt::CaseInsensitive) == 0 || textureUnitAlias.compare("specularMap", Qt::CaseInsensitive) == 0 ||
        textureUnitName.contains("specular", Qt::CaseInsensitive) || textureUnitAlias.contains("specular", Qt::CaseInsensitive))
        return "Specular";
    else if (textureUnitName.compare("normalMap", Qt::CaseInsensitive) == 0 || textureUnitAlias.compare("normalMap", Qt::CaseInsensitive) == 0 ||
        textureUnitName.contains("normal", Qt::CaseInsensitive) || textureUnitAlias.contains("normal", Qt::CaseInsensitive))
        return "Normal";
    else if (textureUnitName.compare("lightMap", Qt::CaseInsensitive) == 0 || textureUnitAlias.compare("lightMap", Qt::CaseInsensitive) == 0 ||
        textureUnitName.contains("light", Qt::CaseInsensitive) || textureUnitAlias.contains("light", Qt::CaseInsensitive))
        return "Light map";
    return "";
}

QString MeshmoonShaderInfo::TextureUnitName(const QString compileArgument, int textureUnitIndex) const
{
    return TextureUnitName(compileArguments, compileArgument, textureUnitIndex);
}

QString MeshmoonShaderInfo::TextureUnitName(const QStringList &compileArguments, const QString compileArgument, int textureUnitIndex)
{
    if (compileArgument == "WEIGHTED_DIFFUSE")
        return (textureUnitIndex == 0 ? "weightMap" : QString("diffuseMap%1").arg(textureUnitIndex));
    else if (compileArgument == "DIFFUSE_MAPPING")
        return !compileArguments.contains("MULTI_DIFFUSE", Qt::CaseSensitive) ? "diffuseMap" : "diffuseMap1";
    else if (compileArgument == "MULTI_DIFFUSE")
        return "diffuseMap2";
    else if (compileArgument == "NORMAL_MAPPING")
        return "normalMap";
    else if (compileArgument == "SPECULAR_MAPPING")
        return "specularMap";
    else if (compileArgument == "LIGHT_MAPPING")
        return "lightMap";
    else if (compileArgument == "SHADOW_MAPPING")
        return QString("shadowMap%1").arg(textureUnitIndex);
    return "";
}

QString MeshmoonShaderInfo::TilingNamedParameterName(const QString &compileArgument)
{
    // diffuseMapTilings is not handled by this function.
    if (compileArgument == "WEIGHTED_DIFFUSE")
        return cNamedParameterWeightMapTiling;
    if (compileArgument == "DIFFUSE_MAPPING")
        return cNamedParameterDiffuseMapTiling;
    else if (compileArgument == "MULTI_DIFFUSE")
        return cNamedParameterDiffuseMap2Tiling;
    else if (compileArgument == "NORMAL_MAPPING")
        return cNamedParameterNormalMapTiling;
    else if (compileArgument == "SPECULAR_MAPPING")
        return cNamedParameterSpecularMapTiling;
    else if (compileArgument == "LIGHT_MAPPING")
        return cNamedParameterLightMapTiling;
    return "";
}

QStringList MeshmoonShaderInfo::FloatTypeNamedParameters() const
{
    /** Float type named parameters this shader has
        SPECULAR_LIGHTING
        - uniform float specularPower
        - uniform float specularHardness
        LIGHT_MAPPING
        - uniform float lightMapPower */
    QStringList floatNamedParameters;
    if (compileArguments.contains("SPECULAR_LIGHTING", Qt::CaseSensitive))
        floatNamedParameters << cNamedParameterSpecularPower << cNamedParameterSpecularHardness;
    if (compileArguments.contains("LIGHT_MAPPING", Qt::CaseSensitive))
        floatNamedParameters << cNamedParameterLightMapPower;
    return floatNamedParameters;
}

float MeshmoonShaderInfo::FloatTypeNamedParameterDefaultValue(const QString &namedParameter) const
{
    if (namedParameter == cNamedParameterSpecularPower)
        return 1.0f;
    else if (namedParameter == cNamedParameterSpecularHardness)
        return 10.0f;
    else if (namedParameter == cNamedParameterLightMapPower)
        return 1.0f;
    return 1.0f;
}

QString MeshmoonShaderInfo::UserFriendlyNamedParameterName(const QString &namedParameter)
{
    if (namedParameter == cNamedParameterSpecularPower)
        return "Specular Power";
    else if (namedParameter == cNamedParameterSpecularHardness)
        return "Specular Hardness";
    else if (namedParameter == cNamedParameterLightMapPower)
        return "Light Map Power";
    return namedParameter;
}

QString MeshmoonShaderInfo::UserFriendlyShaderName() const
{
    QString result = name;
    if (result.contains("/"))
        result = result.mid(result.indexOf("/")+1);

    QStringList parts = result.split(QRegExp("(?=[A-Z])"), QString::SkipEmptyParts);
    if (!parts.isEmpty())
    {
        for(int i=0; i<parts.size(); ++i)
        {
            if (parts[i] == "Map")
            {
                parts.removeAt(i);
                --i;
            }
        }

        // Throw "Map" away from each that has it as a postfix
        QStringList resultParts;
        int combined = 0;

        if (parts.contains("Weighted", Qt::CaseInsensitive) && parts.contains("Diffuse", Qt::CaseInsensitive))
        {
            resultParts << "Weighted Diffuse";
            combined++;
        }
        else if (parts.contains("Multi", Qt::CaseInsensitive) && parts.contains("Diffuse", Qt::CaseInsensitive))
        {
            resultParts << "Multi Diffuse";
            combined++;
        }
        else if (parts.contains("Diffuse", Qt::CaseInsensitive))
            resultParts << "Diffuse";
        if (parts.contains("Normal", Qt::CaseInsensitive))
            resultParts << "Normal";
        if (parts.contains("Specular", Qt::CaseInsensitive))
            resultParts << "Specular";
        if (parts.contains("Light", Qt::CaseInsensitive))
            resultParts << "Light";
        if (parts.contains("Shadow", Qt::CaseInsensitive))
            resultParts << "Shadows";

        // If we could not resolve a common sense name for each part, 
        // don't do anything as we don't want to lose information.
        if (resultParts.size() + combined == parts.size())
            result = resultParts.join(" + ");
    }
    return result;
}
