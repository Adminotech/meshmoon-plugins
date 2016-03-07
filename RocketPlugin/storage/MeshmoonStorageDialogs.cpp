/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "MeshmoonStorageDialogs.h"
#include "MeshmoonStorageHelpers.h"
#include "MeshmoonStorageWidget.h"
#include "MeshmoonStorage.h"

#include "qts3/QS3Client.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include "Framework.h"
#include "IAttribute.h"
#include "AssetAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

// RocketStorageAuthDialog

RocketStorageAuthDialog::RocketStorageAuthDialog(RocketPlugin *plugin, const QString &storageAclCheckPath, const QString &storageBucket) :
    plugin_(plugin),
    storageAclCheckPath_(storageAclCheckPath),
    storageBucket_(storageBucket),
    restoreForeground_(false),
    s3_(0)
{
    ui_.setupUi(this);
    
    loaderAnimation_ = new QMovie(":/images/loader.gif", "GIF");
    if (loaderAnimation_->isValid())
    {
        ui_.labelAuthenticatingAnim->setMovie(loaderAnimation_);
        loaderAnimation_->setCacheMode(QMovie::CacheAll);
    }
    ui_.labelAuthenticatingAnim->setVisible(false);
    ui_.labelAuthenticating->setVisible(false);
    ui_.labelError->setVisible(false);
    
    connect(ui_.buttonAuthenticate, SIGNAL(clicked()), SLOT(TryAuthenticate()));
    connect(ui_.buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    connect(this, SIGNAL(rejected()), SLOT(Cancel()));
    
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::SplashScreen);
    setModal(true);
    hide();
}

RocketStorageAuthDialog::~RocketStorageAuthDialog()
{
    SAFE_DELETE(s3_);
    SAFE_DELETE(loaderAnimation_);
}

void RocketStorageAuthDialog::Show()
{
    if (isVisible())
        return;
        
    show();
    setFocus(Qt::ActiveWindowFocusReason);
    activateWindow();
    
    setWindowOpacity(0.0);

    QPropertyAnimation *showAnim = new QPropertyAnimation(this, "windowOpacity", this);
    showAnim->setStartValue(0.0);
    showAnim->setEndValue(1.0);
    showAnim->setDuration(300);
    showAnim->setEasingCurve(QEasingCurve(QEasingCurve::InOutQuad)); 
    showAnim->start();

    plugin_->Notifications()->CenterToMainWindow(this);
    if (!plugin_->Notifications()->IsForegroundDimmed())
    {
        plugin_->Notifications()->DimForeground();
        restoreForeground_ = true;
    }
}

void RocketStorageAuthDialog::TryAuthenticate(const QString &accessKey, const QString &secretKey)
{
    ui_.accessKeyLineEdit->setText(accessKey);
    ui_.secretKeyLineEdit->setText(secretKey);
    
    if (!accessKey.isEmpty() && !secretKey.isEmpty())
        TryAuthenticate();
    else
        Show();
}

void RocketStorageAuthDialog::TryAuthenticate()
{
    QString accessKey = ui_.accessKeyLineEdit->text().trimmed();
    if (accessKey.isEmpty())
    {
        ui_.labelAuthenticating->setVisible(true);
        ui_.labelAuthenticating->setText("<span style='color: red;'>Access Key cannot be empty</span>");
        return;
    }
    QString secretKey = ui_.secretKeyLineEdit->text().trimmed();
    if (secretKey.isEmpty())
    {
        ui_.labelAuthenticating->setVisible(true);
        ui_.labelAuthenticating->setText("<span style='color: red;'>Secret Key cannot be empty</span>");
        return;
    }

    ui_.accessKeyLineEdit->setEnabled(false);
    ui_.secretKeyLineEdit->setEnabled(false);
    ui_.buttonAuthenticate->setEnabled(false);
    ui_.buttonCancel->setEnabled(false);
    ui_.labelAuthenticatingAnim->setVisible(true);
    ui_.labelAuthenticating->setVisible(true);
    if (ui_.labelError->isVisible())
        ui_.labelError->setText("");

    updateGeometry();
    update();

    loaderAnimation_->start();
    ui_.labelAuthenticating->setText("<span style='color: black;'>Authenticating...</span>");

    SAFE_DELETE(s3_);
    s3_ = new QS3Client(QS3Config(accessKey, secretKey, storageBucket_, QS3Config::EU_WEST_1));
    
    QS3GetAclResponse *response = s3_->getAcl(storageAclCheckPath_);
    if (response)
        connect(response, SIGNAL(finished(QS3GetAclResponse*)), SLOT(OnAclReply(QS3GetAclResponse*)));
    else
        AuthenticationFailed("Could not determine scene file...");
}

void RocketStorageAuthDialog::OnAclReply(QS3GetAclResponse *response)
{
    if (response && response->succeeded)
        Authenticated();
    else
        AuthenticationFailed(response ? response->error.toString() : "");
}

void RocketStorageAuthDialog::AuthenticationFailed(const QString &error)
{
    loaderAnimation_->stop();
    ui_.accessKeyLineEdit->setEnabled(true);
    ui_.secretKeyLineEdit->setEnabled(true);
    ui_.buttonAuthenticate->setEnabled(true);
    ui_.buttonCancel->setEnabled(true);
    ui_.labelAuthenticatingAnim->setVisible(false);
    ui_.labelAuthenticating->setVisible(true);
    
    ui_.labelError->setVisible(!error.isEmpty());
    ui_.labelError->setText(error);

    ui_.labelAuthenticating->setText("<span style='color: red;'>Authentication failed...</span>");
    LogError("[RocketStorageAuthDialog]: " + (error.isEmpty() ? "Authentication failed..." : error));
    
    if (!isVisible())
        Show();

    updateGeometry();
    update();
}

void RocketStorageAuthDialog::Authenticated()
{
    if (restoreForeground_)
    {
        plugin_->Notifications()->ClearForeground();
        restoreForeground_ = false;
    }
    
    loaderAnimation_->stop();
    
    emit Authenticated(ui_.accessKeyLineEdit->text().trimmed(), ui_.secretKeyLineEdit->text().trimmed());
    accept();
}

void RocketStorageAuthDialog::Cancel()
{
    if (restoreForeground_)
    {
        plugin_->Notifications()->ClearForeground();
        restoreForeground_ = false;
    }
    
    loaderAnimation_->stop();
    
    emit Authenticated("", "");
}

// RocketStorageFileDialog

RocketStorageFileDialog::RocketStorageFileDialog(QWidget *parent, RocketPlugin *plugin, const QString &title, const QString &startFolder, QDir::Filters filters, bool confirmOverwrite) :
    QFileDialog(parent, title, startFolder),
    plugin_(plugin),
    confirmOverwrite_(confirmOverwrite)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
    setFilter(filters);
    setWindowIcon(QIcon(":/images/icon-update.png"));
    
    connect(this, SIGNAL(filesSelected(const QStringList&)), SLOT(OnFilesSelected(const QStringList&)));
}

RocketStorageFileDialog::~RocketStorageFileDialog()
{
}

void RocketStorageFileDialog::Open()
{
    show();
}

void RocketStorageFileDialog::Close()
{
    close();
}

void RocketStorageFileDialog::OnFilesSelected(const QStringList &filenames)
{
    if (filenames.isEmpty())
        return;

    // Pick new or existing directory
    if (fileMode() == QFileDialog::Directory)
        emit FolderSelected(filenames.first(), files);
    // Pick new or existing file
    else if (fileMode() == QFileDialog::AnyFile)
        emit FileSelected(filenames.first(), files);
    // Pick existing files
    else
        emit FilesSelected(filenames, confirmOverwrite_);
}

// RocketStorageInfoDialog

RocketStorageInfoDialog::RocketStorageInfoDialog(RocketPlugin *plugin, const QString &title, MeshmoonStorageOperationMonitor *_monitor) :
    plugin_(plugin),
    monitor(_monitor)
{
    ui.setupUi(this);

    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::SplashScreen);
    setModal(true);
    
    ui.labelTitle->setText(title);
    ui.labelInputTitle->hide();
    ui.lineEditInput->hide();
    ui.labelText->hide();
    hide();

    connect(ui.buttonOk, SIGNAL(clicked()), SIGNAL(AcceptClicked()));
    connect(ui.buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    connect(this, SIGNAL(rejected()), plugin_->Notifications(), SLOT(ClearForeground()));
    connect(this, SIGNAL(rejected()), SIGNAL(Rejected()));
}

RocketStorageInfoDialog::~RocketStorageInfoDialog()
{
}

void RocketStorageInfoDialog::EnableInputMode(const QString &labelInputTitle, const QString &text)
{
    ui.labelInputTitle->show();
    ui.labelInputTitle->setText(labelInputTitle);
    ui.lineEditInput->show();
    ui.lineEditInput->setText(text);
}

void RocketStorageInfoDialog::EnableTextMode(const QString &text)
{
    ui.labelText->show();
    ui.labelText->setText(text);
}

void RocketStorageInfoDialog::OpenDelayed(uint msec)
{
    show();
    update();
    hide();
    
    QTimer::singleShot(msec, this, SLOT(Open()));
}

void RocketStorageInfoDialog::Open()
{
    if (isVisible())
        return;
        
    show();
    setFocus(Qt::ActiveWindowFocusReason);
    activateWindow();

    setWindowOpacity(0.0);

    QPropertyAnimation *showAnim = new QPropertyAnimation(this, "windowOpacity", this);
    showAnim->setStartValue(0.0);
    showAnim->setEndValue(1.0);
    showAnim->setDuration(300);
    showAnim->setEasingCurve(QEasingCurve(QEasingCurve::InOutQuad)); 
    showAnim->start();
    
    plugin_->Notifications()->CenterToMainWindow(this);
    plugin_->Notifications()->DimForeground();
    
    // If input mode is enabled, focus the input field and 
    // select the text so user can start writing.
    if (ui.lineEditInput->isVisible())
    {
        ui.lineEditInput->setFocus(Qt::MouseFocusReason);
        ui.lineEditInput->selectAll();
    }
}

void RocketStorageInfoDialog::Close()
{
    plugin_->Notifications()->ClearForeground();

    accepted();
    close();
}

// RocketStorageSelectionDialog

RocketStorageSelectionDialog::RocketStorageSelectionDialog(RocketPlugin *plugin, MeshmoonStorage *storage, const QStringList &suffixFilters, 
                                                           bool allowChangingFolder, MeshmoonStorageItem &startDirectory, QWidget *parent) :
    QDialog(parent),
    plugin_(plugin),
    storage_(storage),
    suffixFilters_(suffixFilters),
    allowChangingFolder_(allowChangingFolder),
    startDirectory_(startDirectory),
    currentFolder_(0)
{
    // Setup UI
    ui_.setupUi(this);
    ui_.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui_.lineEditFilter->installEventFilter(this);

    ui_.buttonSelect->setAutoDefault(false);
    ui_.buttonCancel->setAutoDefault(false);

    view_ = new RocketStorageListWidget(this, plugin_);
    view_->SetPreviewFileOnMouse(true);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);

    QVBoxLayout *l = new QVBoxLayout(ui_.scrollAreaWidgetContents);
    l->setSpacing(0);
    l->setContentsMargins(0,0,0,0);
    l->addWidget(view_);
    ui_.scrollAreaWidgetContents->setLayout(l);

    // Connections
    connect(view_, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(OnItemClicked(QListWidgetItem*)));
    connect(view_, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(OnItemDoubleClicked(QListWidgetItem*)));
    connect(ui_.buttonSelect, SIGNAL(clicked()), SLOT(OnSelect()), Qt::QueuedConnection);
    connect(ui_.buttonCancel, SIGNAL(clicked()), SLOT(reject()), Qt::QueuedConnection);
    connect(this, SIGNAL(rejected()), SLOT(OnCancel()));
    connect(ui_.lineEditFilter, SIGNAL(textChanged(const QString&)), SLOT(OnFilterChanged(const QString&)));

    // Dialog setup
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowModality(parent != 0 ? Qt::WindowModal : Qt::ApplicationModal);
    setWindowFlags(parent != 0 ? Qt::Tool : Qt::SplashScreen);
    setWindowTitle(parent != 0 ? "Meshmoon Storage Picker" : "");
    setModal(true);

    // Center to main window or to parent window.
    if (!parent)
    {
        plugin_->Notifications()->DimForeground();
        plugin_->Notifications()->CenterToMainWindow(this);
    }
    else
        plugin_->Notifications()->CenterToWindow(parent, this);
    
    // Show and activate
    show();
    setFocus(Qt::ActiveWindowFocusReason);
    activateWindow();
    
    // If this is a splash dialog animate opacity
    if (!parent)
    {
        setWindowOpacity(0.0);

        QPropertyAnimation *showAnim = new QPropertyAnimation(this, "windowOpacity", this);
        showAnim->setStartValue(0.0);
        showAnim->setEndValue(1.0);
        showAnim->setDuration(300);
        showAnim->setEasingCurve(QEasingCurve(QEasingCurve::InOutQuad)); 
        showAnim->start();
    }

    if (!plugin_ || !storage_)
    {
        view_->addItem(new QListWidgetItem("Failed to list storage content"));
        return;
    }
    if (storage_->RootDirectory().IsNull())
        view_->addItem(new QListWidgetItem("Loading..."));

    MeshmoonStorageAuthenticationMonitor *auth = storage_->Authenticate();
    connect(auth, SIGNAL(Completed()), SLOT(OnStorageAuthCompleted()), Qt::QueuedConnection);
    connect(auth, SIGNAL(Canceled()), SLOT(reject()), Qt::QueuedConnection);
    connect(auth, SIGNAL(Failed(const QString&)), SLOT(reject()), Qt::QueuedConnection);
}

MeshmoonStorageItem RocketStorageSelectionDialog::Selected() const
{
    return item_;
}

MeshmoonStorageItem RocketStorageSelectionDialog::CurrentDirectory() const
{
    return MeshmoonStorageItem(currentFolder_, storage_);
}

void RocketStorageSelectionDialog::OnStorageAuthCompleted()
{
    // If the start directory is null then we know we can default it to the storage root.
    if (startDirectory_.IsNull())
        startDirectory_ = storage_->RootDirectory();

    OpenFolder(startDirectory_);
}

void RocketStorageSelectionDialog::OpenFolder(const MeshmoonStorageItem &directory)
{
    view_->clear();
    currentFolder_ = storage_->Fill(view_, directory, true, allowChangingFolder_, suffixFilters_);
    if (!currentFolder_)
    {
        view_->clear();
        view_->addItem(new QListWidgetItem("Failed to list storage content"));
        item_ = MeshmoonStorageItem();
        return;
    }

    if (allowChangingFolder_ && currentFolder_->data.key != storage_->RootDirectory().path)
        view_->insertItem(0, new MeshmoonStorageItemWidget(plugin_, ".."));
}

void RocketStorageSelectionDialog::OnItemClicked(QListWidgetItem *item)
{
    MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(item);
    if (!storageItem)
        return;

    if (!storageItem->data.isDir)
        item_ = MeshmoonStorageItem(storageItem, storage_);
    else
        item_ = MeshmoonStorageItem();
}

void RocketStorageSelectionDialog::OnItemDoubleClicked(QListWidgetItem *item)
{
    MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(item);
    if (!storageItem)
        return;

    if (storageItem->data.isDir)
    {
        if (storageItem->data.key != "..")
        {
            MeshmoonStorageItem folder(storageItem, storage_);
            OpenFolder(folder);
        }
        else if (currentFolder_ && currentFolder_->parent)
        {
            MeshmoonStorageItem folder(currentFolder_->parent, storage_);
            OpenFolder(folder);
        }
    }
    else
    {
        item_ = MeshmoonStorageItem(storageItem, storage_);
        OnSelect();
    }
}

void RocketStorageSelectionDialog::OnSelect()
{
    // No selection yet, make the user press the cancel button to close/cancel!
    if (item_.IsNull())
        return;

    plugin_->Notifications()->ClearForeground();
    emit StorageItemSelected(item_);
    accept();
}

void RocketStorageSelectionDialog::OnCancel()
{
    item_ = MeshmoonStorageItem();
    plugin_->Notifications()->ClearForeground();
    emit Canceled();
}

void RocketStorageSelectionDialog::OnFilterChanged(const QString &filter)
{
    for(int i=0; i<view_->count(); ++i)
    {
        MeshmoonStorageItemWidget *item = dynamic_cast<MeshmoonStorageItemWidget*>(view_->item(i));
        if (item)
            item->setHidden((filter.isEmpty() ? false : !item->filename.contains(filter, Qt::CaseInsensitive)));
    }
}
