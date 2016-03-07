/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "LoggingFunctions.h"

#include <QObject>
#include <QString>

class QAction;
class QMenu;

/// Provides toolbar/window menu functionality.
/** @ingroup MeshmoonRocket */
class RocketMenu : public QObject
{
Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketMenu(RocketPlugin *plugin);
    ~RocketMenu();
    /// @endcond

public slots:
    /// Adds action to Session menu with group.
    /** @param Action to add.
        @param Group name */
    void AddAction(QAction *action, QString group);

    /// Adds application to Application menu.
    /** @param Action to add. */
    void AddApplication(QAction *action);

    /// Returns Session menu.
    /** @return Session menu. */
    QMenu *SessionMenu();

    /// Returns Application menu.
    /** @return Application menu. */
    QMenu *ApplicationMenu();

    /// Returns a top level menu from the QMenuBar with name.
    /** @note Also looks for "&" + @c name for shortcut enabled strings. 
        @return Found menu or null. */
    QMenu *TopLevelMenu(const QString &name);

    /// Finds and returns a action from the Session and Application menus.
    /** @param Action name.
        @return Found action or null. */
    QAction *Action(const QString &text) const;

    /// Finds and returns a group title action from the Session menu.
    /** @param Group action name.
        @return Found group action or null. */
    QAction *TitleAction(const QString &group) const;

    /// Enabled/disables @c itemName from menu with @c menuName.
    /** @param Menu name.
        @param Item name.
        @param Enabled/disabled. */
    void SetActionEnabled(const QString &menuName, const QString &itemName, bool enabled);

    /// Returns if any menus in the menu bar are currently open.
    /** @return True if any menus are open, false otherwise. */
    bool HasOpenMenus() const;

    /// Returns if the Session menu is visible.
    bool IsSessionMenuVisible() const;

    /// Returns if the Application menu is visible.
    bool IsApplicationMenuVisible() const;

    /// Returns if the Session menu is empty.
    bool IsSessionMenuEmpty() const;

    /// Returns if the Application menu is empty.
    bool IsApplicationMenuEmpty() const;

    /// Returns the Session menu action count.
    int SessionMenuActionCount() const;

    /// Returns the Application menu action count.
    int ApplicationMenuActionCount() const;

    /// Opens the Meshmoon website with default OS browser.
    void OpenMeshmoonWebSite();

    /// Opens the Meshmoon documentation website with default OS browser.
    void OpenMeshmoonDocWebSite();

    /// Shows the about Meshmoon dialog.
    void ShowAboutMeshmoonDialog();

    /// Shows the about realXtend Tundra dialog.
    void ShowAboutTundraDialog();

    /// Shows the licenses dialog.
    void ShowLicensesDialog();

    /// Shows the EULA dialog.
    void ShowEulaDialog();

private slots:
    void OnTaskbarActionAdded(QAction *action);
    
protected:
    /// @cond PRIVATE
    bool eventFilter(QObject *obj, QEvent *event);
    /// @endcond

private:
    void HideEmptyApplications();
    void HideEmptyGroups();
    
    QAction *GetNextAction(const QString &group) const;
    QAction *GetNextAction(QAction *action) const;

    RocketPlugin *plugin_;
    QMenu *menuSession_;
    QMenu *menuApplication_;
};
Q_DECLARE_METATYPE(RocketMenu*)
