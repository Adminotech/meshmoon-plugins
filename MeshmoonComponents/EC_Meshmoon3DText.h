/**
    For conditions of distribution and use, see copyright notice in LICENSE

    @file   EC_Meshmoon3DText.h
    @brief  EC_Meshmoon3DText makes the entity a 3D text object.*/

#pragma once

#include "MeshmoonComponentsApi.h"
#include "IComponent.h"
#include "AssetReference.h"
#include "Color.h"

/// Makes the entity a 3D text object.
/** @note Client-side implementation only exists currently on WebRocket. */
class MESHMOON_COMPONENTS_API EC_Meshmoon3DText : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("Meshmoon3DText", 509)

public:
    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_Meshmoon3DText(Scene* scene) :
        IComponent(scene),
        INIT_ATTRIBUTE_VALUE(font, "Font", ""),
        INIT_ATTRIBUTE_VALUE(pointSize, "Point size", 20),
        INIT_ATTRIBUTE_VALUE(style, "Style", Regular),
        INIT_ATTRIBUTE_VALUE(color, "Color", Color::White),
        INIT_ATTRIBUTE_VALUE(materialRef, "Material ref", AssetReference("", "OgreMaterial")),
        INIT_ATTRIBUTE(text, "Text"),
        INIT_ATTRIBUTE(fontRefs, "Font refs"),
        INIT_ATTRIBUTE(webFontRefs, "Web font refs")
    {}
    /// @endcond

    enum FontStyle
    {
        Regular,
        Bold,
        Italic,
        BoldItalic
    };

    /// Font family.
    Q_PROPERTY(QString font READ getfont WRITE setfont);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, font);

    /// Font's point size.
    Q_PROPERTY(uint pointSize READ getpointSize  WRITE setpointSize);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, pointSize);

    /// Font style, see FontStyle.
    Q_PROPERTY(uint style READ getstyle WRITE setstyle);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, style);

    /// Diffuse color for the text mesh, white by default.
    /** The visual look of the text mesh can be overwritten by using the materialRef attribute. */
    Q_PROPERTY(Color color READ getcolor WRITE setcolor);
    DEFINE_QPROPERTY_ATTRIBUTE(Color, color);

    /// Defines the material for the text mesh.
    /** Overwrites the visual look defined by the color attribute. */
    Q_PROPERTY(AssetReference materialRef READ getmaterialRef WRITE setmaterialRef);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReference, materialRef);

    /// Text that is rendered as a mesh.
    Q_PROPERTY(QString text READ gettext WRITE settext);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, text);

    /// Font resources for native clients (TTF).
    Q_PROPERTY(AssetReferenceList fontRefs READ getfontRefs WRITE setfontRefs);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReferenceList, fontRefs);

    /// Font resources for web clients (JS).
    Q_PROPERTY(AssetReferenceList webFontRefs READ getwebFontRefs WRITE setwebFontRefs);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReferenceList, webFontRefs);
};
COMPONENT_TYPEDEFS(Meshmoon3DText);
