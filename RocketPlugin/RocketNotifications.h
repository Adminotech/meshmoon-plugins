/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "RocketSceneWidget.h"
#include "ui_RocketNotificationWidget.h"

#include "FrameworkFwd.h"
#include "AssetFwd.h"
#include "UiFwd.h"

#include <QObject>
#include <QWidget>
#include <QString>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QMessageBox>
#include <QBrush>
#include <QMetaType>

/// Provides possibility to show notifications in the Rocket UI.
/** See RocketSceneWidget for provided signals. */
class RocketNotificationWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketNotificationWidget(Framework *framework);
    ~RocketNotificationWidget();
    /// @endcond

public slots:
    /// Closes the notification.
    void Close();
    
    /// Makes the notification automatically hide after @c msecs time.
    /** @param Milliseconds until notification should be hidden. */
    void SetAutoHide(int msecs = 5000);
    
    /// If auto hide was requested before, this function will stop the timer counting to it.
    void StopAutoHide();
    
    /// Sets warning theme. Affect color etc. This is the default on creation.
    void SetInfoTheme();
    
    /// Sets warning theme. Affect color etc.
    void SetWarningTheme();
    
    /// Sets error theme. Affect color etc.
    void SetErrorTheme();
    
    /// Sets message to the notification
    /** @param Message to show */
    void SetMessage(const QString &message);
    
    /// Sets pixmap. 
    /** @param Pixmap resource can be absolute disk path or URL to the image. 
        If URL it will be fetched from the web or used from cache if exists.
        @note The image will be force scaled to 32x32px if bigger. */
    void SetPixmap(const QString &pixmapResource);
    
    /// Add button.
    /** @param If the notification should be closed when this button is pressed.
        @param Button text.
        @return Created button. */
    QPushButton *AddButton(bool closeNotificationOnPress, const QString &text);
    
    /// Add check box button.
    /** @param Button text.
        @param If check box should be checked.
        @return Created check box button. */
    QCheckBox *AddCheckBox(const QString &text, bool checked = false);
    
    /// Add radio button.
    /** @param Button text.
        @param If radio button should be checked.
        @return Created radio button. */
    QRadioButton *AddRadioButton(const QString &text, bool checked = false);

private slots:
    void OnPixmapLoaded(AssetPtr asset);
    
protected:
    virtual void showEvent(QShowEvent *e);

private:
    Ui::RocketNotificationWidget ui_;
    QString originalStyle_;
    QTimer *closeTimer_;
    int msecsAutoHide_;
};
Q_DECLARE_METATYPE(RocketNotificationWidget*)

typedef QList<RocketNotificationWidget*> RocketNotificationWidgetList;

/// Provides UI notifications.
/** @ingroup MeshmoonRocket */
class RocketNotifications : public QObject
{
Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketNotifications(RocketPlugin *plugin);
    ~RocketNotifications();
    
    friend class RocketPlugin;
    /// @endcond
    
    /// Closes all notifications.
    void CloseNotifications();
    
public slots:
    /// Show a modal splash dialog with text message. Calling this function will not block the main loop.
    /** @param Message to show in the dialog.
        @param Path to a image you want to show as the graphic. This cannot be a URL, needs to be a existing file path to the image.
        @param Buttons to show on the dialog.
        @param Default button to select for when user just hits enter etc.
        @param If the dialog should be automatically centered to parent widget (main window if parent is null) and shown.
        @note Will have OK button by default but you can manipulate the returned QMessageBox*. The dialog will be centered to the Tundra main window.
        @note Do not store this pointer for later usage. The dialog will be deleted when closed. */
    QMessageBox *ShowSplashDialog(const QString &message, const QString &pixmapPath = QString(), QMessageBox::StandardButtons buttons = QMessageBox::Ok, 
                                  QMessageBox::StandardButton defaultButton = QMessageBox::NoButton, QWidget *parent = 0, bool autoPositionAndShow = true);

    /// @overload
    /** @param Message to show in the dialog.
        @param Image to show in the dialog.
        @param Buttons to show on the dialog.
        @param Default button to select for when user just hits enter etc.
        @param If the dialog should be automatically centered to parent widget (main window if parent is null) and shown.
        @note Will have OK button by default but you can manipulate the returned QMessageBox*. The dialog will be centered to the Tundra main window.
        @note Do not store this pointer for later usage. The dialog will be deleted when closed. */
    QMessageBox *ShowSplashDialog(const QString &message, const QPixmap &pixmap, QMessageBox::StandardButtons buttons = QMessageBox::Ok, 
                                  QMessageBox::StandardButton defaultButton = QMessageBox::NoButton, QWidget *parent = 0, bool autoPositionAndShow = true);

    /// Execs a modal splash dialog with text message. Calling this function will block the main loop and wont return until dialog is closed.
    /** @param Message to show in the dialog.
        @param Path to a image you want to show as the graphic. This cannot be a URL, needs to be a existing file path to the image.
        @param Buttons to show on the dialog.
        @param Default button to select for when user just hits enter etc.
        @return Pressed button. */
    QMessageBox::StandardButton ExecSplashDialog(const QString &message, const QString &pixmapPath = QString(), QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

    /// @overload
    /** @param Message to show in the dialog.
        @param Image to show in the dialog.
        @param Buttons to show on the dialog.
        @param Default button to select for when user just hits enter etc.
        @return Pressed button. */
    QMessageBox::StandardButton ExecSplashDialog(const QString &message, const QPixmap &pixmap, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

    /// Show a notification in the UI.
    /** @param Message to show in the notification.
        @param Optional image resource, can be file path or URL to image.
        @return Notification widget. Use its API to add buttons and to customize the appearance.
        @note Do not store this pointer for later usage. The widget will be deleted when closed. */
    RocketNotificationWidget *ShowNotification(const QString &message, const QString &pixmapResource = QString());

    /// Sets graphics scene foreground to @c brush.
    /** @see ClearForeground
        @param The brush to set as foreground. Call without parameters to clear foreground. */
    void SetForeground(const QBrush &brush);

    /// Checks if @c brush foreground is already set to the graphics scene.
    bool HasForeground(const QBrush &brush) const;
    
    /// Checks if @c brush foreground is already dimmed.
    /** @see DimForeground */
    bool IsForegroundDimmed() const;

    /// Dims the graphics scene foreground, same as calling SetForeground with QColor(23,23,23,75).
    /** @see IsForegroundDimmed */
    void DimForeground();

    /// Clears the graphics scene foreground, same as calling SetForeground with Qt::NoBrush.
    void ClearForeground();
    
    /// Returns our main window rect.
    /** @note This diffect from UiMainWindow::rect() or ::geometry() in that position is actually set
        to the main windows position. UiMainWindow::rect() will always have (0,0) as the position! */
    QRect MainWindowRect() const;

    /// Returns the center position of our main window. Can be used to center dialogs etc.
    QPoint MainWindowCenter() const;

    /// Centers widget to main window.
    /** @note Only call this with top level @c widget ptrs. This won't handle proxied widgets or widgets in layouts correctly.
        @param Widget to center to the main window. */
    void CenterToMainWindow(QWidget *widget);
    
    /// Centers widget to another widget.
    /** @param The widget will be centered to this window. If this window/widget has a parentWidget() 
        TopMostParent will be called to detect the parent window which will then be used.
        @param The widget to center. */
    static void CenterToWindow(QWidget *window, QWidget *widget);
    
    /// Returns the top most parent for the widget.
    /** This will check @c widget and its parent chain until a
        Qt::Window, Qt::Dialog or Qt::Tool window is found. */
    static QWidget *TopMostParent(QWidget *widget);
    
    /// Ensure that the given geometry is within the desktop.
    static void EnsureGeometryWithinDesktop(QRect &rect);
    
private slots:
    void OnSceneResized(const QRectF &rect);
    void ProcessNextNotification();

protected:
    RocketNotificationWidgetList Notifications();

private:
    RocketPlugin *plugin_;
    Framework *framework_;
    
    RocketNotificationWidgetList notifications_;
    QPointer<RocketNotificationWidget> activeNotification_;
    
    QString dialogButtonStyle_;
};
Q_DECLARE_METATYPE(RocketNotifications*)
