/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketMenu.h"
#include "RocketPlugin.h"
#include "RocketLobbyWidgets.h"

#include "Framework.h"
#include "Application.h"
#include "CoreDefines.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "AssetCache.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "LoggingFunctions.h"

#include "ui_AboutMeshmoonDialog.h"
#include "ui_AboutTundraDialog.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>

#include "MemoryLeakCheck.h"

RocketMenu::RocketMenu(RocketPlugin *plugin) :
    plugin_(plugin),
    menuSession_(0),
    menuApplication_(0)
{
    Framework *framework = plugin_->GetFramework();
    if (framework->IsHeadless() || !framework->Ui()->MainWindow() || !framework->Ui()->MainWindow()->menuBar())
        return;
        
    // Hide some of the Tundras "default" menus to replace them with out own.
    QList<QAction*> menus = framework->Ui()->MainWindow()->menuBar()->actions();
    foreach(QAction *menu, menus)
    {
        if (menu->text() == "File" || menu->text() == "&File")
            menu->setVisible(false);
        if (menu->text() == "Help" || menu->text() == "&Help")
        {
            QMenu *menuObj = menu->menu();
            if (menuObj)
            {
                foreach(QAction *helpItem, menuObj->actions())
                    helpItem->setVisible(false);

                QAction *act = menuObj->addAction(QIcon(":/images/logo-meshmoon.png"), "Meshmoon website");
                connect(act, SIGNAL(triggered()), SLOT(OpenMeshmoonWebSite()));
                act = menuObj->addAction(QIcon(":/images/logo-meshmoon.png"), "Meshmoon documentation");
                connect(act, SIGNAL(triggered()), SLOT(OpenMeshmoonDocWebSite()));
                act = menuObj->addAction(QIcon(":/images/icon-copy-32x32.png"), tr("Show Licenses"));
                connect(act, SIGNAL(triggered()), SLOT(ShowLicensesDialog()));
                act = menuObj->addAction(QIcon(":/images/icon-copy-32x32.png"), tr("Show EULA"));
                connect(act, SIGNAL(triggered()), SLOT(ShowEulaDialog()));
                menuObj->addSeparator();
                act = menuObj->addAction(QIcon(":/images/icon-questionmark.png"), tr("About Meshmoon"));
                connect(act, SIGNAL(triggered()), SLOT(ShowAboutMeshmoonDialog()));
                act = menuObj->addAction(QIcon(":/images/icon-questionmark.png"), tr("About realXtend Tundra"));
                connect(act, SIGNAL(triggered()), SLOT(ShowAboutTundraDialog()));
            }
        }
    }
    QAction *secondItem = menus.size() > 1 ? menus.at(1) : 0;
        
    // Session menu
    menuSession_ = new QMenu(tr("Session"));
    menuSession_->setStyleSheet("QMenu::item:disabled { color: rgb(188, 99, 22); }");
    menuSession_->installEventFilter(this);
    framework->Ui()->MainWindow()->menuBar()->insertMenu(secondItem, menuSession_);

    // Init title so it stays on top
    QAction *title = menuSession_->addAction(tr("Session"));
    QFont titleFont("Arial");
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::Bold);
    title->setProperty("menuGroupTitle", true);
    title->setFont(titleFont);
    title->setEnabled(false);
    
    QAction *actExit = new QAction(QIcon(":/images/icon-exit.png"), tr("Exit"), this);
    connect(actExit, SIGNAL(triggered()), framework, SLOT(Exit()));
    AddAction(actExit, tr("Session"));
    
    // Applications
    menuApplication_ = new QMenu(tr("Applications"));
    menuApplication_->installEventFilter(this);
    menuApplication_->setEnabled(false);
    framework->Ui()->MainWindow()->menuBar()->insertMenu(secondItem, menuApplication_);
}

RocketMenu::~RocketMenu()
{
    SAFE_DELETE(menuSession_);
    SAFE_DELETE(menuApplication_);
}

QMenu *RocketMenu::TopLevelMenu(const QString &name)
{
    QList<QAction*> menus = plugin_->GetFramework()->Ui()->MainWindow()->menuBar()->actions();
    foreach(QAction* menu, menus)
    {
        if (menu->text() == name || menu->text() == "&" + name)
            return menu->menu();
    }
    return 0;
}

void RocketMenu::SetActionEnabled(const QString &menuName, const QString &itemName, bool enabled)
{
    QMenu *menu = TopLevelMenu(menuName);
    if (!menu)
        return;

    foreach(QAction* item, menu->actions())
        if (item->text() == itemName || item->text() == "&" + itemName)
            item->setEnabled(enabled);
}

void RocketMenu::OpenMeshmoonWebSite()
{
    QDesktopServices::openUrl(QUrl("http://www.meshmoon.com"));
}

void RocketMenu::OpenMeshmoonDocWebSite()
{
    QDesktopServices::openUrl(QUrl("http://doc.meshmoon.com"));
}

void RocketMenu::ShowAboutMeshmoonDialog()
{
    Ui::AboutMeshmoon ui;
    QDialog *dialog = new QDialog();
    ui.setupUi(dialog);
    ui.labelLink->setOpenExternalLinks(true);
    dialog->setWindowIcon(plugin_->GetFramework()->Ui()->MainWindow()->windowIcon());
    dialog->exec();
    dialog->deleteLater();
}

void RocketMenu::ShowAboutTundraDialog()
{
    Ui::AboutTundra ui;
    QDialog *dialog = new QDialog();
    ui.setupUi(dialog);
    ui.labelIcon->setPixmap(QPixmap(Application::InstallationDirectory() + "/data/ui/images/realxtend_logo.png"));
    ui.labelText->setOpenExternalLinks(true);
    ui.labelLink->setOpenExternalLinks(true);
    dialog->setWindowIcon(plugin_->GetFramework()->Ui()->MainWindow()->windowIcon());
    dialog->exec();
    dialog->deleteLater();
}

void RocketMenu::ShowLicensesDialog()
{
    RocketLicensesDialog *dialog = new RocketLicensesDialog(plugin_);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setModal(true);
    dialog->show();
}

void RocketMenu::ShowEulaDialog()
{
    RocketEulaDialog *dialog = new RocketEulaDialog(plugin_);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setModal(true);
    dialog->show();
}

void RocketMenu::AddAction(QAction *action, QString group)
{
    if (!menuSession_ || !action)
        return;
    if (group.isEmpty())
        group = "Miscellaneous";
    action->setProperty("menuGroup", group);
    
    // Make sure group title is visible if found.
    QAction *existingTitle = TitleAction(group);
    if (existingTitle && !existingTitle->isVisible())
        existingTitle->setVisible(true);

    QAction *insertBefore = existingTitle ? GetNextAction(existingTitle) : GetNextAction(group);
    // Push after the group title item
    if (insertBefore)
        menuSession_->insertAction(insertBefore, action);
    // Create group title item and add action
    else
    {
        if (!existingTitle)
        {
            QAction *title = menuSession_->addAction(group);
            QFont titleFont("Arial");
            titleFont.setWeight(QFont::Bold);
            titleFont.setPixelSize(13);
            title->setProperty("menuGroupTitle", true);
            title->setFont(titleFont);
            title->setEnabled(false);
            
            existingTitle = title;
        }
        
        menuSession_->insertAction(GetNextAction(existingTitle), action);
    }
}

void RocketMenu::AddApplication(QAction *action)
{
    if (!menuApplication_)
        return;
    if (!menuApplication_->isEnabled())
        menuApplication_->setEnabled(true);
    menuApplication_->addAction(action);
}

QAction *RocketMenu::Action(const QString &text) const
{
    if (!menuSession_)
        return 0;

    QList<QAction*> actions = menuSession_->actions();
    for(int i=0; i<actions.size(); ++i)
    {
        if (actions[i]->text() == text)
            return actions[i];
    }

    QList<QAction*> actionsApp = menuApplication_->actions();
    for (int i=0; i<actionsApp.size(); ++i)
    {
        if (actionsApp[i]->text() == text)
            return actionsApp[i];
    }

    return 0;
}

QMenu *RocketMenu::SessionMenu()
{
    return menuSession_;
}

QMenu *RocketMenu::ApplicationMenu()
{
    return menuApplication_;
}

bool RocketMenu::HasOpenMenus() const
{
    if (!plugin_->GetFramework()->Ui()->MainWindow() || !plugin_->GetFramework()->Ui()->MainWindow()->menuBar())
        return false;

    foreach(QAction *action, plugin_->GetFramework()->Ui()->MainWindow()->menuBar()->actions())
    {
        QMenu *menu = action != 0 ? action->menu() : 0;
        if (menu && menu->isVisible())
            return true;
    }
    return false;
}

bool RocketMenu::IsSessionMenuVisible() const
{
    return (menuSession_ ? menuSession_->isVisible() : false);
}

bool RocketMenu::IsApplicationMenuVisible() const
{
    return (menuApplication_ ? menuApplication_->isVisible() : false);
}

bool RocketMenu::IsSessionMenuEmpty() const
{
    return (menuSession_ ? menuSession_->actions().isEmpty() : true);
}

bool RocketMenu::IsApplicationMenuEmpty() const
{
    return (menuApplication_ ? menuApplication_->actions().isEmpty() : true);
}

int RocketMenu::SessionMenuActionCount() const
{
    return (menuSession_ ? menuSession_->actions().size() : 0);
}

int RocketMenu::ApplicationMenuActionCount() const
{
    return (menuApplication_ ? menuApplication_->actions().size() : 0);
}

QAction *RocketMenu::TitleAction(const QString &group) const
{
    if (!menuSession_)
        return 0;
    foreach(QAction *iter, menuSession_->actions())
        if (iter->text() == group && iter->property("menuGroupTitle").toBool())
            return iter;
    return 0;
}

QAction *RocketMenu::GetNextAction(const QString &group) const
{
    if (!menuSession_)
        return 0;
        
    bool next = false;
    QList<QAction*> actions = menuSession_->actions();
    for(int i=0; i<actions.size(); ++i)
    {
        if (next)
            return actions[i];
        if (actions[i]->text() == group && actions[i]->property("menuGroupTitle").toBool())
            next = true;
    }
    return 0;
}

QAction *RocketMenu::GetNextAction(QAction *action) const
{
    if (!menuSession_)
        return 0;
        
    bool next = false;
    QList<QAction*> actions = menuSession_->actions();
    for(int i=0; i<actions.size(); ++i)
    {
        if (next)
            return actions[i];
        if (actions[i] == action)
            next = true;
    }
    return 0;
}

void RocketMenu::OnTaskbarActionAdded(QAction *action)
{
    AddApplication(action);
}

void RocketMenu::HideEmptyApplications()
{
    if (!menuApplication_)
        return;
        
    if (menuApplication_->actions().size() == 0 && menuApplication_->isEnabled())
        menuApplication_->setEnabled(false);
    else if (menuApplication_->actions().size() > 0 && !menuApplication_->isEnabled())
        menuApplication_->setEnabled(true);
}

void RocketMenu::HideEmptyGroups()
{
    if (!menuSession_)
        return;
        
    QMap<QString, QAction*> groups;
    foreach(QAction *iter, menuSession_->actions())
        if (iter->property("menuGroupTitle").toBool())
            groups[iter->text()] = iter;
            
    foreach(QString group, groups.keys())
    {
        QAction *groupAction = groups[group];
        if (!groupAction || !groupAction->isVisible())
            continue;
            
        // If any item is found, don't hide.
        bool shouldHide = true;
        foreach(QAction *iter, menuSession_->actions())
        {
            if (iter->property("menuGroupTitle").toBool())
                continue;
            if (iter->property("menuGroup").toString() == group)
            {
                shouldHide = false;
                break;
            }
        }
        if (shouldHide)
            groupAction->setVisible(false);
    }
}

bool RocketMenu::eventFilter(QObject *obj, QEvent *event)
{
    if (menuSession_ && obj == menuSession_ && event->type() == QEvent::ActionRemoved)
        HideEmptyGroups();
    if (menuApplication_ && obj == menuApplication_ && event->type() == QEvent::ActionRemoved)
        HideEmptyApplications();
    return false;
}
