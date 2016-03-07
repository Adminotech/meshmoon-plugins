/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildComponentWidgets.h
    @brief  RocketBuildComponentWidgets provide widgets for component creation/configuration. */

#pragma once

#include <QWidget>

#include "RocketFwd.h"
#include "SceneFwd.h"
#include "IAttribute.h"

#include "ui_RocketComponentConfigurationDialog.h"

#include <QStringList>

class QFormLayout;
class QSignalMapper;

// IRocketComponentWidget

class IRocketComponentWidget : public QWidget
{
Q_OBJECT

public:
    IRocketComponentWidget(RocketPlugin *plugin, QWidget *parent, Qt::WindowType windowFlags);
    virtual ~IRocketComponentWidget();
    
public slots:
    /// Set component.
    void SetComponent(const ComponentPtr &component);
    
    /// Clear component.
    void ClearComponent();
    
    /// Returns current component type id, 0 if no component is set.
    u32 ComponentTypeId();

private slots:
    void OnChanged(const QString &attributeId);

protected:
    /// Implementation should return default value for attribute, from config etc.
    QString ReadConfig(IAttribute *attribute);
    void SaveConfig();

    RocketPlugin *plugin_;
    ComponentWeakPtr component_;

    QStringList targetAttributeIds_;

private:
    void CreateUI();
    QLayout *CreateWidget(IAttribute *attribute);
    void UpdateValue(IAttribute *attribute);

    QFormLayout *form_;
    QSignalMapper *mapper_;
};

// EC_WebBrowserWidget

class EC_WebBrowserWidget : public IRocketComponentWidget
{
Q_OBJECT

public:
    EC_WebBrowserWidget(RocketPlugin *plugin, QWidget *parent = 0, Qt::WindowType windowFlags = Qt::Widget);
};

// EC_MediaBrowserWidget

class EC_MediaBrowserWidget : public IRocketComponentWidget
{
Q_OBJECT

public:
    EC_MediaBrowserWidget(RocketPlugin *plugin, QWidget *parent = 0, Qt::WindowType windowFlags = Qt::Widget);
};

// RocketComponentConfigurationDialog

class RocketComponentConfigurationDialog : public QWidget
{
Q_OBJECT

public:
    RocketComponentConfigurationDialog(RocketPlugin *plugin, QWidget *parent = 0, Qt::WindowType windowFlags = Qt::SplashScreen);
    ~RocketComponentConfigurationDialog();

    Ui::RocketComponentConfigurationDialog ui;

public slots:
    void Popup(const QPoint &globalScreenPos);
    void SetComponentType(const QString &userFriendlyName, u32 componentTypeId);
    void SetComponent(const ComponentPtr &component);
    void ClearComponent();

signals:
    void EnabledToggled(bool enabled, u32 componentTypeId);

private slots:
    void OnToggled(bool checked);
    void OnResize();
    
private:
    void RemoveConfigurationWidget();

    IRocketComponentWidget *configWidget_;
    RocketPlugin *plugin_;
    
    u32 componentTypeId_;
    QString userFriendlyName_;
};