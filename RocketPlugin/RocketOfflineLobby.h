/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#pragma once
#include "RocketFwd.h"
#include "UiFwd.h"
#include "MeshmoonData.h"

#include "ui_RocketOfflineLobby.h"
#include "ui_RocketOfflineLobbyButton.h"

class QTimer;

class RocketOfflineLobbyButton : public QWidget
{
    Q_OBJECT

public:
    RocketOfflineLobbyButton(const QString &title, const QString &desc, const QPixmap &icon, QWidget *parent = 0);
    virtual ~RocketOfflineLobbyButton();

signals:
    void Clicked();

protected:
    Ui::OfflineLobbyButton ui_;
    virtual void mousePressEvent(QMouseEvent *e);
};

class RocketOfflineLobby : public QWidget
{
    Q_OBJECT

public:
    RocketOfflineLobby(RocketPlugin *plugin, QWidget *parent = 0);
    virtual ~RocketOfflineLobby();

public slots:
    void Show(bool animate = true);
    void Hide(bool animate = true);
    void AddButton(RocketOfflineLobbyButton *button);

private slots:
    void Countdown();
    void OnRetry();
    void OnResize(int newWidth, int newHeight);
    void OnAuthenticated();
    void OnAnimationFinished();

private:
    Ui::OfflineLobbyWidget ui_;
    RocketPlugin *plugin_;
    Framework *framework_;

    QTimer *retryTimer_;

    int retrySeconds_;
    int actualSeconds_;

    bool preventShow_;

    RocketAnimationEngine *animation_;
    qreal opacity_;
};
