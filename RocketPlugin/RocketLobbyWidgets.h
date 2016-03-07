/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketSceneWidget.h"
#include "AssetFwd.h"

#include <QPushButton>
#include <QDialog>
#include <QTextEdit>
#include <QTimer>
#include <QMap>
#include <QString>

#include "ui_RocketServerWidget.h"
#include "ui_RocketPromotedServerWidget.h"
#include "ui_RocketLicensesWidget.h"

// RocketServerWidget

class RocketServerWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    RocketServerWidget(Framework *framework, const Meshmoon::Server &inData);
    ~RocketServerWidget();

    Meshmoon::Server data;
    Meshmoon::ServerScore score;
    Meshmoon::ServerOnlineStats stats; 

    bool filtered;
    bool focused;
    bool selected;

    void SetOnlineStats(const Meshmoon::ServerOnlineStats &onlineStats);
    void ResetOnlineStats();

    void SetFocused(bool focused_);
    void SetSelected(bool selected_);

    void UpdateGridPosition(int row, int column, int itemsPerHeight, int itemsPerWidth, bool isLast);

public slots:
    void FilterAndSuggest(Meshmoon::RelevancyFilter &filter);

private slots:
    void SetZoom(bool zoom);

    void OnTransformAnimationsFinished(RocketAnimationEngine *engine, RocketAnimations::Direction direction);
    void OnProfilePictureLoaded(AssetPtr asset);
    void SetProfileImage(const QString &path = QString());

    void ApplyDataToWidgets();
    void ApplyDataWidgetStyling();
    
protected:
    void InitializeWidget(QWidget *widget);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);

private:
    void CheckTooltip(bool show);
    
    bool ReplaceStyle(QWidget *widget, const QStringList &targetStyles, const QString &replacementStyle);
    bool ReplaceStyle(QWidget *widget, const QString &targetStyle, const QString &replacementStyle);

    Ui::RocketServerWidget ui_;
    bool zoomed_;

signals:
    void FocusRequest(RocketServerWidget *server);
    void UnfocusRequest(RocketServerWidget *server);

    void ConnectRequest(RocketSceneWidget *server);
    void CancelRequest(RocketSceneWidget *server);

    void HoverChange(RocketServerWidget *server, bool hovering);
};

// RocketPromotionWidget

class RocketPromotionWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    RocketPromotionWidget(Framework *framework, const Meshmoon::PromotedServer &inData);
    ~RocketPromotionWidget();

    Meshmoon::PromotedServer data;
    QTimer timer;
    
    bool focused;
    bool promoting;

    void SetFocused(bool focused_);
    void SetPromoting(bool promoting_);
    void SetGeometry(const QSizeF &size, const QPointF &pos);

    void RestartTimer();

public slots:
    /// RocketSceneWidget override.
    void Show();

    /// RocketSceneWidget override.
    void Animate(const QSizeF &newSize, const QPointF &newPos, qreal newOpacity = -1.0, const QRectF &sceneRect = QRectF(), RocketAnimations::Animation anim = RocketAnimations::NoAnimation);

    /// Override.
    void Hide(RocketAnimations::Animation anim = RocketAnimations::NoAnimation, const QRectF &sceneRect = QRectF());

    /// Inform this widget its next in line to be shown. Prepares needed graphics.
    void AboutToBeShown();

private slots:
    void OnPromotionOpacityAnimationFinished();

    void OnSmallImageCompleted(AssetPtr asset);
    void OnLargeImageCompleted(AssetPtr asset);

    void LoadRequiredImage();
    void LoadSmallImage(const QString &path = "");
    void LoadLargeImage(const QString &path = "");

signals:
    void Timeout();
    void Pressed(RocketPromotionWidget *promoted);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);

private:
    Ui::RocketPromotedServerWidget ui_;
    QPropertyAnimation *opacityAnim_;
    QPixmap smallImage_;
    QPixmap largeImage_;
};

// RocketLobbyActionButton

class RocketLobbyActionButton : public RocketSceneWidget
{
    Q_OBJECT

public:
    enum Direction
    {
        Left,
        Right
    };

    enum Mode
    {
        Unknown,
        Arrow,
        Button
    };

    RocketLobbyActionButton(Framework *framework_, Direction direction);
    ~RocketLobbyActionButton();

public slots:
    void SetArrowLook();
    void SetButtonLook(const QString &message);

    QString GetButtonText();

signals:
    void Pressed();

private:
    QPushButton *button_;
    Direction direction_;
    Mode mode_;
};

// RocketLoaderWidget

class RocketLoaderWidget : public QGraphicsObject
{
    Q_OBJECT

public:
    RocketLoaderWidget(Framework *framework);
    ~RocketLoaderWidget();

    bool isPromoted;

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

public slots:
    void SetServer(const Meshmoon::Server &serverData);
    void SetMessage(const QString &message);
    void SetGeometry(const QSizeF &size, const QPointF &pos);
    void SetButtonCount(int buttonCount);
    void SetCompletion(qreal percent);

    void StartAnimation();
    void StopAnimation();
    bool IsAnimating() { return timerId_ != 0; }

    void Show();
    void Hide(bool takeScreenShot = false);

    void ResetServer();

private slots:
    void LoadServerScreenshot();
    void StoreServerScreenshot();

protected:
    void timerEvent(QTimerEvent *event);

private:
    Framework *framework_;

    Meshmoon::Server serverData_;

    QPixmap serverImage_;
    QPixmap buffer_;
    QFont font_;
    QFont percentFont_;
    QString message_;
    QSizeF size_;

    QMovie *movie_;

    int step_;
    int opacity_;
    int timerId_;
    qreal buttonCount_;
    qreal percent_;

    bool sizeChanged_;
    QRectF slideRect_;
};

// RocketServerFilter

class RocketServerFilter : public QTextEdit
{
    Q_OBJECT

public:
    RocketServerFilter(RocketLobby *controller);
    virtual ~RocketServerFilter();

    void UnInitialize(Framework *framework);

    void Show();
    void ShowSuggestionLabel(bool updateGeometryDelayed = false);
    void ShowSuggestionList(bool updateGeometryDelayed = false);

    void Hide();
    void HideAndClearSuggestions();
    void HideSuggestionLabel(bool clearText = false);
    void HideSuggestionList(bool clearItems = false);

    QString GetTerm();
    bool IsTermValid();
    void ApplyCurrentTerm();
    
    void SetSortCriteriaInterfaceVisible(bool visible);
    void EmitCurrentSortConfigChanged();

    bool HasOpenMenus();
    bool HasFocus();
    void Unfocus();
    
    void FocusSearchBox();
    void FocusSuggestionBox();

    Meshmoon::ServerScore::SortCriteria CurrentSortCriteria() const { return currentSortCriterial_; }
    bool FavorAdministratedWorlds() const { return favorAdministratedWorlds_; }
    bool FavorMEPWorlds() const { return favorMEPWorlds_; }
    bool FavorActiveWorlds() const { return favorActiveWorlds_; }
    bool ShowPrivateWorlds() const { return showPrivateWorlds_; }
    bool ShowMaintenanceWorlds() const { return showMaintenanceWorlds_; }

    bool eventFilter(QObject *obj, QEvent *e);

protected:
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);
    void keyPressEvent(QKeyEvent *e);

public slots:
    void UpdateGeometry();

signals:
    void SortConfigChanged();

private slots:
    void ReadConfig();
    void WriteConfig();
    
    QAction *CreateSortCriterialTitle(const QString &text);
    QAction *CreateSortCriterialAction(const QString &text, const QString &propertyName, const QString &tooltip, bool checked);

    void UpdateSuggestionWidgets(Meshmoon::RelevancyFilter &filter);
    void ApplySuggestion(bool selectFirstServerOnEmpty = false);
    
    bool IsPlaceholder(const QString &term);

    void OnTextChanged();
    void OnOrderServers();
    void OnCheckFocus();
    
    void OnSuggestionListItemSelected(QListWidgetItem *item);
    
    void OnShowSortingConfigMenu();
    void OnSortConfigChanged(QAction *act);
    void OnRestoreSearchCriteriaButton();

private:
    RocketLobby *controller_;

    QString helpText_;
    
    Meshmoon::ServerScore::SortCriteria currentSortCriterial_;
    bool favorAdministratedWorlds_;
    bool favorMEPWorlds_;
    bool favorActiveWorlds_;
    bool showPrivateWorlds_;
    bool showMaintenanceWorlds_;
    
    QPushButton *sortCriteriaButton_;
    QMenu *sortCriteriaMenu_;
    QLabel *suggestLabel_;
    QGraphicsProxyWidget *suggestProxy_;
    RocketSceneWidget *suggestionListProxy_;
    
    QTimer orderDelayTimer_;
    QTimer focusDelayTimer_;
    
    bool emptyTermFired_;
};

// RocketLicensesDialog

class RocketLicensesDialog : public QDialog
{
    Q_OBJECT
    
public:
    RocketLicensesDialog(RocketPlugin *plugin);
        
private slots:
    void OnLicenseSelected(QListWidgetItem *item, QListWidgetItem *previous);

private:
    void LoadLicenses();
    
    struct License
    {
        QString name;
        QString absoluteFilePath;
        QStringList urls;
        QListWidgetItem *item;
    };

    Ui::RocketLicensesWidget ui_;
    QList<License> licenses_;
};

// RocketEulaDialog

class RocketEulaDialog : public QDialog
{
    Q_OBJECT

public:
    RocketEulaDialog(RocketPlugin *plugin);

private:
    Ui::RocketLicensesWidget ui_;
};
