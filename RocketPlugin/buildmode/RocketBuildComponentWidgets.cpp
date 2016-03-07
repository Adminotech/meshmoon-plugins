/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildComponentWidgets.cpp
    @brief  RocketBuildComponentWidgets provide widgets for component creation/configuration. */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketBuildComponentWidgets.h"
#include "RocketPlugin.h"
#include "ConfigAPI.h"

#include "Framework.h"
#include "SceneAPI.h"
#include "EC_WebBrowser.h"
#include "EC_MediaBrowser.h"

#include <QFormLayout>
#include <QSignalMapper>

#include "MemoryLeakCheck.h"

// IRocketComponentWidget

IRocketComponentWidget::IRocketComponentWidget(RocketPlugin *plugin, QWidget *parent, Qt::WindowType windowFlags) :
    QWidget(parent, windowFlags),
    plugin_(plugin),
    form_(0),
    mapper_(0)
{
    hide();
}

IRocketComponentWidget::~IRocketComponentWidget()
{
    SaveConfig();
}

void IRocketComponentWidget::SetComponent(const ComponentPtr &component)
{
    component_ = ComponentWeakPtr(component);
    
    CreateUI();
}

void IRocketComponentWidget::ClearComponent()
{
    SaveConfig();
    component_.reset();
}

u32 IRocketComponentWidget::ComponentTypeId()
{
    if (component_.lock().get())
        return component_.lock()->TypeId();
    return 0;
}

QString IRocketComponentWidget::ReadConfig(IAttribute *attribute)
{
    if (!attribute || !attribute->Owner())
        return "";
        
    // If the component is parented, this is a live component already in the scene.
    if (attribute->Owner()->ParentEntity())
    {
        if (!attribute->ToString().isEmpty())
            return "";
    }
    
    ConfigData config("rocket-build-mode", 
        IComponent::EnsureTypeNameWithoutPrefix(plugin_->GetFramework()->Scene()->ComponentTypeNameForTypeId(ComponentTypeId())), 
        attribute->Id(), "", "");

    return plugin_->GetFramework()->Config()->Read(config).toString();
}

void IRocketComponentWidget::SaveConfig()
{
    if (!component_.lock().get())
        return;

    ConfigData config("rocket-build-mode", IComponent::EnsureTypeNameWithoutPrefix(plugin_->GetFramework()->Scene()->ComponentTypeNameForTypeId(ComponentTypeId())));
    foreach(const QString &attributeId, targetAttributeIds_)
    {
        IAttribute *iAttr = component_.lock()->AttributeById(attributeId);
        if (iAttr)
        {
            const QString value = iAttr->ToString();
            if (!value.isEmpty())
                plugin_->GetFramework()->Config()->Write(config, attributeId, value);
        }
    }
}

void IRocketComponentWidget::OnChanged(const QString &attributeId)
{
    if (component_.lock().get())
        UpdateValue(component_.lock()->AttributeById(attributeId));
}

void IRocketComponentWidget::UpdateValue(IAttribute *attribute)
{
    if (!form_ || !attribute || !attribute->Owner())
        return;

    // Disconnected if this is a dummy non-parented component. Othewise inherit from parent with Default.
    AttributeChange::Type change = attribute->Owner()->ParentEntity() ? AttributeChange::Default : AttributeChange::Disconnected;

    for (int i=0; i<form_->rowCount(); ++i)
    {
        QLayout *l = form_->itemAt(i, QFormLayout::FieldRole)->layout();
        if (!l) continue;

        QString widgetId = l->property("tundraAttributeId").toString();
        if (widgetId.compare(attribute->Id(), Qt::CaseInsensitive) != 0)
            continue;

        // Update the attribute
        switch(attribute->TypeId())
        {
            case cAttributeString:
            {
                Attribute<QString> *a = checked_static_cast<Attribute<QString>*>(attribute);
                if (!a || !l->itemAt(0)) return;
                QLineEdit *w = qobject_cast<QLineEdit*>(l->itemAt(0)->widget());
                if (w) a->Set(w->text(), change);
                break;
            }
            case cAttributeUInt:
            {
                Attribute<uint> *a = checked_static_cast<Attribute<uint>*>(attribute);
                if (!a || !l->itemAt(0)) return;
                QSpinBox *w = qobject_cast<QSpinBox*>(l->itemAt(0)->widget());
                if (w) a->Set(static_cast<uint>(w->value()), change);
                break;
            }
            case cAttributeInt:
            {
                Attribute<int> *a = checked_static_cast<Attribute<int>*>(attribute);
                if (!a || !l->itemAt(0)) return;
                QSpinBox *w = qobject_cast<QSpinBox*>(l->itemAt(0)->widget());
                if (w) a->Set(w->value(), change);
                break;
            }
            case cAttributeBool:
            {
                Attribute<bool> *a = checked_static_cast<Attribute<bool>*>(attribute);
                if (!a || !l->itemAt(0)) return;
                QCheckBox *w = qobject_cast<QCheckBox*>(l->itemAt(0)->widget());
                if (w) a->Set(w->isChecked(), change);
                break;
            }
            case cAttributeQPoint:
            {
                Attribute<QPoint> *a = checked_static_cast<Attribute<QPoint>*>(attribute);
                if (!a) return;
                QPoint p;
                if (l->itemAt(0))
                {
                    QSpinBox *w = qobject_cast<QSpinBox*>(l->itemAt(0)->widget());
                    if (w) p.setX(w->value());
                }
                if (l->itemAt(2))
                {
                    QSpinBox *w = qobject_cast<QSpinBox*>(l->itemAt(2)->widget());
                    if (w) p.setY(w->value());
                }
                a->Set(p, change);
                break;
            }
            default:
                return;
        }

        ConfigData config("rocket-build-mode", IComponent::EnsureTypeNameWithoutPrefix(plugin_->GetFramework()->Scene()->ComponentTypeNameForTypeId(ComponentTypeId())));
        plugin_->GetFramework()->Config()->Write(config, attribute->Id(), attribute->ToString());
        break;
    }
}

void IRocketComponentWidget::CreateUI()
{
    if (form_ || !component_.lock().get())
        return;
        
    mapper_ = new QSignalMapper(this);
    connect(mapper_, SIGNAL(mapped(const QString&)), SLOT(OnChanged(const QString&)));
    
    form_ = new QFormLayout(this);
    setLayout(form_);

    const AttributeVector &attributes = component_.lock()->Attributes();
    for(AttributeVector::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
    {
        IAttribute *attribute = (*iter);
        if (!targetAttributeIds_.contains(attribute->Id(), Qt::CaseInsensitive))
            continue;

        QLayout *l = CreateWidget(attribute);
        if (!l) continue;

        l->setProperty("tundraAttributeId", attribute->Id());
        form_->addRow(attribute->Name(), l);
    }
}

QLayout *IRocketComponentWidget::CreateWidget(IAttribute *attribute)
{
    if (!attribute || !attribute->Owner())
        return 0;

    QHBoxLayout *l = new QHBoxLayout();
    
    // Disconnected if this is a dummy non-parented component. Othewise inherit from parent with Default.
    AttributeChange::Type change = attribute->Owner()->ParentEntity() ? AttributeChange::Default : AttributeChange::Disconnected;

    switch(attribute->TypeId())
    {
        case cAttributeString:
        {
            Attribute<QString> *a = checked_static_cast<Attribute<QString>*>(attribute);
            if (!a) return 0;

            QString value = ReadConfig(attribute);
            if (!value.isEmpty())
                a->FromString(value, change);

            QLineEdit *w = new QLineEdit();
            w->setText(a->Get());
            l->addWidget(w);

            mapper_->setMapping(w, attribute->Id());
            connect(w, SIGNAL(editingFinished()), mapper_, SLOT(map()));
            break;
        }
        case cAttributeUInt:
        {
            Attribute<uint> *a = checked_static_cast<Attribute<uint>*>(attribute);
            if (!a) return 0;

            const QString value = ReadConfig(attribute);
            if (!value.isEmpty())
                a->FromString(value, change);

            QSpinBox *w = new QSpinBox();
            w->setRange(0, INT_MAX); // UINT_MAX wont work, QSpinBox works with int range.
            w->setValue(a->Get());
            l->addWidget(w);

            mapper_->setMapping(w, attribute->Id());
            connect(w, SIGNAL(valueChanged(int)), mapper_, SLOT(map()));
            break;
        }
        case cAttributeInt:
        {
            Attribute<int> *a = checked_static_cast<Attribute<int>*>(attribute);
            if (!a) return 0;

            const QString value = ReadConfig(attribute);
            if (!value.isEmpty())
                a->FromString(value, change);

            QSpinBox *w = new QSpinBox();
            w->setRange(INT_MIN, INT_MAX);
            w->setValue(a->Get());
            l->addWidget(w);

            mapper_->setMapping(w, attribute->Id());
            connect(w, SIGNAL(valueChanged(int)), mapper_, SLOT(map()));
            break;
        }
        case cAttributeBool:
        {
            Attribute<bool> *a = checked_static_cast<Attribute<bool>*>(attribute);
            if (!a) return 0;

            const QString value = ReadConfig(attribute);
            if (!value.isEmpty())
                a->FromString(value, change);

            QCheckBox *w = new QCheckBox();
            w->setChecked(a->Get());
            l->addWidget(w);

            mapper_->setMapping(w, attribute->Id());
            connect(w, SIGNAL(clicked()), mapper_, SLOT(map()));
            break;
        }
        case cAttributeQPoint:
        {
            Attribute<QPoint> *a = checked_static_cast<Attribute<QPoint>*>(attribute);
            if (!a) return 0;

            const QString value = ReadConfig(attribute);
            if (!value.isEmpty())
                a->FromString(value, change);

            QSpinBox *x = new QSpinBox();
            x->setRange(INT_MIN, INT_MAX);
            x->setValue(a->Get().x());
            l->addWidget(x);

            l->addWidget(new QLabel("x"));

            QSpinBox *y = new QSpinBox();
            y->setRange(INT_MIN, INT_MAX);
            y->setValue(a->Get().y());
            l->addWidget(y);

            mapper_->setMapping(x, attribute->Id());
            connect(x, SIGNAL(valueChanged(int)), mapper_, SLOT(map()));
            mapper_->setMapping(y, attribute->Id());
            connect(y, SIGNAL(valueChanged(int)), mapper_, SLOT(map()));
            break;
        }
        default:
            SAFE_DELETE(l);
            break;

        /*
        case cAttributeReal: return new ECAttributeEditor<float>(browser, attribute, editor);
        case cAttributeColor: return new ECAttributeEditor<Color>(browser, attribute, editor);
        case cAttributeFloat2: return new ECAttributeEditor<float2>(browser, attribute, editor);
        case cAttributeFloat3: return new ECAttributeEditor<float3>(browser, attribute, editor);
        case cAttributeFloat4: return new ECAttributeEditor<float4>(browser, attribute, editor);
        case cAttributeQuat: return new ECAttributeEditor<Quat>(browser, attribute, editor);
        case cAttributeAssetReference: return  new AssetReferenceAttributeEditor(browser, attribute, editor); // Note: AssetReference uses own special case editor.
        case cAttributeAssetReferenceList: return new AssetReferenceListAttributeEditor(browser, attribute, editor); // Note: AssetReferenceList uses own special case editor.
        case cAttributeEntityReference: return new ECAttributeEditor<EntityReference>(browser, attribute, editor);
        case cAttributeQVariant: return new ECAttributeEditor<QVariant>(browser, attribute, editor);
        case cAttributeQVariantList: return new ECAttributeEditor<QVariantList>(browser, attribute, editor);
        case cAttributeTransform: return new ECAttributeEditor<Transform>(browser, attribute, editor);
        */
    }
    return l;
}

// EC_WebBrowserWidget

EC_WebBrowserWidget::EC_WebBrowserWidget(RocketPlugin *plugin, QWidget *parent, Qt::WindowType windowFlags) :
    IRocketComponentWidget(plugin, parent, windowFlags)
{
    targetAttributeIds_ << "url" << "size" << "renderSubmeshIndex";
}

// EC_MediaBrowserWidget

EC_MediaBrowserWidget::EC_MediaBrowserWidget(RocketPlugin *plugin, QWidget *parent, Qt::WindowType windowFlags) :
    IRocketComponentWidget(plugin, parent, windowFlags)
{
    targetAttributeIds_ << "audioOnly" << "playOnLoad" << "sourceRef" << "renderSubmeshIndex" << "looping";
}

// RocketComponentConfigurationDialog

RocketComponentConfigurationDialog::RocketComponentConfigurationDialog(RocketPlugin *plugin, QWidget *parent, Qt::WindowType windowFlags) :
    QWidget(parent, windowFlags),
    configWidget_(0),
    plugin_(plugin)
{
    ui.setupUi(this);
    connect(ui.buttonClose, SIGNAL(clicked()), SLOT(close()), Qt::QueuedConnection);

#ifndef Q_WS_X11
    setWindowFlags(windowFlags|Qt::WindowStaysOnTopHint);
#else
    setWindowFlags(windowFlags|Qt::WindowStaysOnTopHint|Qt::X11BypassWindowManagerHint);
#endif
    setAttribute(Qt::WA_DeleteOnClose, true);
}

RocketComponentConfigurationDialog::~RocketComponentConfigurationDialog()
{
    ClearComponent();
}

void RocketComponentConfigurationDialog::Popup(const QPoint &globalScreenPos)
{
    if (!isVisible())
        show();
    move(globalScreenPos);
}

void RocketComponentConfigurationDialog::SetComponent(const ComponentPtr &component)
{
    if (!component.get())
    {    
        ClearComponent();
        return;
    }
    
    if (configWidget_ && configWidget_->ComponentTypeId() != component->TypeId())
        RemoveConfigurationWidget();

    if (!configWidget_)
    {
        if (component->TypeId() == EC_WebBrowser::TypeIdStatic())
            configWidget_ = new EC_WebBrowserWidget(plugin_, this);
        else if (component->TypeId() == EC_MediaBrowser::TypeIdStatic())
            configWidget_ = new EC_MediaBrowserWidget(plugin_, this);
        else
        {
            ClearComponent();
            return;
        }
    }

    configWidget_->SetComponent(component);
    ui.contextWidgetLayout->addWidget(configWidget_);
    configWidget_->show();
    
    ui.labelTitle->setText(userFriendlyName_);
}

void RocketComponentConfigurationDialog::ClearComponent()
{
    RemoveConfigurationWidget();
}

void RocketComponentConfigurationDialog::RemoveConfigurationWidget()
{
    if (configWidget_)
        configWidget_->ClearComponent();

    QLayoutItem *item = ui.contextWidgetLayout->takeAt(0);
    if (item)
    {
        QWidget *w = item->widget();
        SAFE_DELETE(item);
        SAFE_DELETE(w);
    }
    configWidget_ = 0;
    
    QTimer::singleShot(1, this, SLOT(OnResize()));
}

void RocketComponentConfigurationDialog::OnResize()
{
    updateGeometry();
    resize(width(),1);
    updateGeometry();
}

void RocketComponentConfigurationDialog::OnToggled(bool checked)
{
    emit EnabledToggled(checked, componentTypeId_);
}

void RocketComponentConfigurationDialog::SetComponentType(const QString &userFriendlyName, u32 componentTypeId)
{
    userFriendlyName_ = userFriendlyName; 
    componentTypeId_ = componentTypeId;
}
