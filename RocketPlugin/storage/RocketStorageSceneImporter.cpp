/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketStorageSceneImporter.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include "Framework.h"
#include "IAttribute.h"
#include "AssetAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <QWidget>

// Item comparing

static bool ImportEntityItemCompare(const RocketStorageImportEntityItem &e1, const RocketStorageImportEntityItem &e2)
{
    return (!e1.existsScene && e2.existsScene);
}

static bool ImportAssetItemCompare(const RocketStorageImportAssetItem &a1, const RocketStorageImportAssetItem &a2)
{
    return (a1.existsDisk && a2.existsDisk) ? (!a1.existsStorage && a2.existsStorage) : (a1.existsDisk && !a2.existsDisk);
}

// RocketStorageSceneImporter

RocketStorageSceneImporter::RocketStorageSceneImporter(QWidget *parent, RocketPlugin *plugin, const QString &destinationPrefix,
                                                       const SceneDesc &sceneDesc_, const ImportEntityItemList &entities_, 
                                                       const ImportAssetItemList &assets_) :
    QDialog(parent, Qt::SplashScreen),
    plugin_(plugin),
    destinationPrefix_(destinationPrefix),
    menu_(0),
    sceneDesc(sceneDesc_),
    entities(entities_),
    assets(assets_)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
    
    ui.setupUi(this);
    hide();

#ifdef Q_WS_WIN
    // @bug Scrolling is broken on Mac if you have custom stylesheet to QTableWidgets;
    QString style("QTableView { border: 1px solid grey; font: 10px \"Arial\"; }");
    ui.assets->setStyleSheet(style);
    ui.entities->setStyleSheet(style);
#endif

    ui.assets->setSortingEnabled(false);
    ui.entities->setSortingEnabled(false);
    ui.assets->setVisible(false);
    ui.entities->setVisible(false);

    // Rewrite all asset refs to be relative "file.extension" from "local://file.extension" etc.
    QMutableListIterator<EntityDesc> edIt(sceneDesc.entities);
    while(edIt.hasNext())
    {
        QMutableListIterator<ComponentDesc> cdIt(edIt.next().components);
        while(cdIt.hasNext())
        {
            QMutableListIterator<AttributeDesc> adIt(cdIt.next().attributes);
            while(adIt.hasNext())
            {
                adIt.next();
                if ((adIt.value().typeName.compare(cAttributeAssetReferenceTypeName, Qt::CaseInsensitive) == 0) ||
                    (adIt.value().typeName.compare(cAttributeAssetReferenceListTypeName, Qt::CaseInsensitive) == 0))
                {
                    QString trimmed = adIt.value().value.trimmed();
                    if (trimmed.isEmpty() || trimmed == "RexSkyBox")
                        continue;

                    QStringList newValues;
                    QStringList valueParts = adIt.value().value.split(";", QString::KeepEmptyParts);
                    for(int i=0; i<valueParts.size(); ++i)
                    {
                        QString &value = valueParts[i];
                        if (value.trimmed().isEmpty())
                            newValues << "";
                        else
                        {
                            // Leave http refs alone
                            if (!value.startsWith("http://") && !value.startsWith("https://"))
                            {
                                QString filename, subasset;
                                AssetAPI::ParseAssetRef(value, 0, 0, 0, 0, 0, 0, &filename, &subasset);
                                newValues << destinationPrefix_ + filename + (!subasset.isEmpty() ? "#" + subasset : "");
                            }
                            else
                            {
                                LogDebug("Found web ref, leaving alone: " + value);
                                newValues << value;
                            }
                           
                        }
                    }

                    if (!newValues.isEmpty())
                    {
                        QString newValue = newValues.join(";");
                        if (adIt.value().value != newValue)
                        {
                            foreach(const RocketStorageImportAssetItem &item, assets)
                            {
                                if (newValue == item.pathRelative)
                                {
                                    newValue = item.desc.destinationName;
                                    break;
                                }
                            }

                            //LogInfo("[RocketSceneImporter]: Rewriting asset reference from " + adIt.value().value + " to " + newValue);
                            adIt.value().value = newValue;
                        }
                    }
                }
            }
        }
    }

    QString checkBoxStyle = "padding: 0px; padding-left: 45px; min-width: 40px;";
    QString labelStyle = "padding: 0px; padding-left: 0px; padding-right: 10px;";
    QString colorYellow = "background-color: rgb(248, 248, 0);";
    QString colorRed = "background-color: rgb(225,0,0); color: white;";

    std::sort(entities.begin(), entities.end(), ImportEntityItemCompare);
    std::sort(assets.begin(), assets.end(), ImportAssetItemCompare);

    // Entities
    int row = 0;
    bool allChecked = true;
    ui.entities->clearContents();
    while(ui.entities->rowCount() > 0)
        ui.entities->removeRow(0);

    const QString envEntityName = "RocketEnvironment";
    bool warnRocketEnvironment = false;

    for(size_t i=0; i<entities.size(); ++i)
    {
        RocketStorageImportEntityItem &e = entities[i];
        
        ui.entities->insertRow(ui.entities->rowCount());
        
        if (!warnRocketEnvironment && e.desc.name.compare(envEntityName, Qt::CaseSensitive) == 0 && e.existsScene)
            warnRocketEnvironment = true;
        
        QCheckBox *create = new QCheckBox(); 
        create->setChecked(!e.existsScene);
        create->setStyleSheet(!e.existsScene ? checkBoxStyle : checkBoxStyle + colorYellow);
        connect(create, SIGNAL(clicked()), SLOT(UpdateActionButton()), Qt::QueuedConnection);
        if (e.existsScene)
            allChecked = false;
        
        QLabel *name = new QLabel(e.desc.name.trimmed().isEmpty() ? "(no name)" : e.desc.name);
        name->setStyleSheet(!e.existsScene ? labelStyle : labelStyle + colorYellow);
        
        ui.entities->setCellWidget(row, 0, create);
        ui.entities->setCellWidget(row, 1, name);
        
        e.row = row;
        row++;
    }
    ui.entities->resizeColumnsToContents();
    if (allChecked)
        OnToggleEntSelection();
    
    // Assets
    row = 0;
    allChecked = true;
    ui.assets->clearContents();
    while(ui.assets->rowCount() > 0)
        ui.assets->removeRow(0);

    for(size_t i=0; i<assets.size(); ++i)
    {
        RocketStorageImportAssetItem &a = assets[i];
        
        ui.assets->insertRow(ui.assets->rowCount());
        
        QCheckBox *upload = new QCheckBox(); 
        upload->setChecked(a.existsDisk);
        upload->setEnabled(a.existsDisk);
        upload->setStyleSheet(a.existsDisk ? (!a.existsStorage ? checkBoxStyle : checkBoxStyle + colorYellow) : checkBoxStyle + colorRed);
        upload->setProperty("existsDisk", a.existsDisk);
        upload->setProperty("existsStorage", a.existsStorage);
        connect(upload, SIGNAL(clicked()), SLOT(UpdateActionButton()), Qt::QueuedConnection);
        if (!a.existsDisk)
            allChecked = false;
            
        QLabel *source = new QLabel(a.pathRelative);
        source->setStyleSheet(a.existsDisk ? (!a.existsStorage ? labelStyle : labelStyle + colorYellow) : labelStyle + colorRed);
        
        QLabel *dest = new QLabel(a.existsDisk ? a.desc.destinationName : "");
        dest->setStyleSheet(a.existsDisk ? (!a.existsStorage ? labelStyle : labelStyle + colorYellow) : labelStyle + colorRed);
        
        ui.assets->setCellWidget(row, 0, upload);
        ui.assets->setCellWidget(row, 1, source);
        ui.assets->setCellWidget(row, 2, dest);

        a.row = row;
        row++;
    }
    ui.assets->resizeColumnsToContents();
    if (allChecked)
        OnToggleAssetSelection();

    ui.assets->setVisible(true);
    ui.entities->setVisible(true);

    // Signaling
    connect(ui.buttonImport, SIGNAL(clicked()), SLOT(OnUploadClicked()));
    connect(ui.buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    connect(ui.buttonMaximize, SIGNAL(toggled(bool)), SLOT(OnToggleSize(bool)));
    connect(ui.buttonEntToggleSelection, SIGNAL(clicked()), SLOT(OnToggleEntSelection()));
    connect(ui.buttonAssetToggleSelection, SIGNAL(clicked()), SLOT(OnToggleAssetSelection()));
    connect(ui.assets, SIGNAL(cellClicked(int, int)), SLOT(OnAssetWidgetClicked(int, int)));

    plugin_->Notifications()->DimForeground();
    
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
    
    UpdateActionButton();
    
    if (warnRocketEnvironment)
    {
        plugin_->Notifications()->ShowSplashDialog(
            QString("%1 is already in the space. Creating multiple can break the environment. \
                     Your imports %1 has been auto deselected.<br><br>If you wish you can enable \
                     it and merge the two Entities manually after import completes.").arg(envEntityName),
                     QPixmap(":images/icon-sky.png"), QMessageBox::Ok, QMessageBox::Ok, this);    
    }
}

RocketStorageSceneImporter::~RocketStorageSceneImporter()
{
    plugin_->Notifications()->ClearForeground(); 
    SAFE_DELETE(menu_);
}

void RocketStorageSceneImporter::OnAssetWidgetClicked(int row, int column)
{
    if (column == 0)
        return;

    QCheckBox *box = qobject_cast<QCheckBox*>(ui.assets->cellWidget(row, 0));
    if (!box || !box->isEnabled())
        return;
    QLabel *label = qobject_cast<QLabel*>(ui.assets->cellWidget(row, 2));
    if (!label || label->text().isEmpty())
        return;
    QFileInfo info(label->text());

    SAFE_DELETE(menu_);
    menu_ = new QMenu();

    bool checked = box->isChecked();
    bool existsStorage = box->property("existsStorage").toBool();

    QAction *act = menu_->addAction(QString("%1 all .%2 files").arg((checked ? "Unselect" : "Select")).arg(info.suffix()));
    connect(act, SIGNAL(triggered()), SLOT(OnToggleFileTypes()));
    menu_->setProperty("suffix", info.suffix());
    menu_->setProperty("suffixCheckedNow", checked);

    act = menu_->addAction(QString("%1 all files that %2 in storage").arg((checked ? "Unselect" : "Select")).arg((existsStorage ? "exist" : "do not exist")));
    connect(act, SIGNAL(triggered()), SLOT(OnToggleOverwriteFiles()));
    menu_->setProperty("existsStorage", existsStorage);
    menu_->setProperty("existsStorageCheckedNow", checked);

    menu_->popup(QCursor::pos());
}

void RocketStorageSceneImporter::OnUploadClicked()
{
    // Entities
    foreach(const RocketStorageImportEntityItem &e, entities)
    {
        QCheckBox *createCheckBox = qobject_cast<QCheckBox*>(ui.entities->cellWidget(e.row, 0));
        bool create = createCheckBox != 0 ? createCheckBox->isChecked() : false;
        if (!create)
        {
            for(int k=0; k<sceneDesc.entities.size(); ++k)
            {
                EntityDesc &existing = sceneDesc.entities[k];
                if (e.desc == existing)
                {
                    sceneDesc.entities.removeAt(k);
                    break;
                }
            }
        }
    }
    sceneDesc.assets.clear();
    
    QStringList uploadFilenames;
    foreach(const RocketStorageImportAssetItem &a, assets)
    {
        QCheckBox *uploadCheckBox = qobject_cast<QCheckBox*>(ui.assets->cellWidget(a.row, 0));
        bool upload = uploadCheckBox != 0 ? uploadCheckBox->isChecked() : false;
        if (upload && uploadCheckBox->property("existsDisk").toBool())
            uploadFilenames << a.pathAbsolute;
    }
    
    if (uploadFilenames.isEmpty() && sceneDesc.entities.isEmpty())
        return;

    emit ImportScene(uploadFilenames, sceneDesc);
    close();
}

void RocketStorageSceneImporter::OnToggleSize(bool checked)
{
    // Maximize
    if (checked)
    {
        originalRect_ = geometry();
        setGeometry(plugin_->GetFramework()->Ui()->MainWindow()->geometry());
        ui.buttonMaximize->setIcon(QIcon(":/images/icon-minimize.png"));
        ui.buttonMaximize->setToolTip("Minimize");
    }
    // Restore normal size
    else
    {
        setGeometry(originalRect_);
        ui.buttonMaximize->setIcon(QIcon(":/images/icon-maximize.png"));
        ui.buttonMaximize->setToolTip("Maximize");
    }
}

void RocketStorageSceneImporter::OnToggleOverwriteFiles()
{
    if (!menu_)
        return;
        
    bool existsStorage = menu_->property("existsStorage").toBool();
    bool checkeNow = menu_->property("existsStorageCheckedNow").toBool();
        
    foreach(const RocketStorageImportAssetItem &a, assets)
    {
        if (a.existsStorage == existsStorage)
        {
            QCheckBox *uploadCheckBox = qobject_cast<QCheckBox*>(ui.assets->cellWidget(a.row, 0));
            if (uploadCheckBox && uploadCheckBox->property("existsDisk").toBool())
            {
                uploadCheckBox->blockSignals(true);
                uploadCheckBox->setChecked(!checkeNow);
                uploadCheckBox->blockSignals(false);
            }
        }
    }

    UpdateActionButton();
}

void RocketStorageSceneImporter::OnToggleFileTypes()
{
    if (!menu_)
        return;

    QString suffix = menu_->property("suffix").toString();
    bool checkeNow = menu_->property("suffixCheckedNow").toBool();
    if (suffix.isEmpty())
        return;

    foreach(const RocketStorageImportAssetItem &a, assets)
    {
        QFileInfo info(a.pathAbsolute);
        if (info.suffix() == suffix)
        {
            QCheckBox *uploadCheckBox = qobject_cast<QCheckBox*>(ui.assets->cellWidget(a.row, 0));
            if (uploadCheckBox && uploadCheckBox->property("existsDisk").toBool())
            {
                uploadCheckBox->blockSignals(true);
                uploadCheckBox->setChecked(!checkeNow);
                uploadCheckBox->blockSignals(false);
            }
        }
    }

    UpdateActionButton();
}

void RocketStorageSceneImporter::OnToggleEntSelection()
{
    bool checked = (ui.buttonEntToggleSelection->text() == "Select All");
    foreach(const RocketStorageImportEntityItem &e, entities)
    {
        QCheckBox *createCheckBox = qobject_cast<QCheckBox*>(ui.entities->cellWidget(e.row, 0));
        if (createCheckBox)
        {
            createCheckBox->blockSignals(true);
            createCheckBox->setChecked(checked);
            createCheckBox->blockSignals(false);
        }
    }
    if (checked)
    {
        ui.buttonEntToggleSelection->setText("Deselect All");
        ui.buttonEntToggleSelection->setIcon(QIcon(":/images/icon-checked.png"));
    }
    else
    {
        ui.buttonEntToggleSelection->setText("Select All");
        ui.buttonEntToggleSelection->setIcon(QIcon(":/images/icon-unchecked.png"));
    }
    
    UpdateActionButton();
}

void RocketStorageSceneImporter::OnToggleAssetSelection()
{
    bool checked = (ui.buttonAssetToggleSelection->text() == "Select All");
    foreach(const RocketStorageImportAssetItem &a, assets)
    {
        QCheckBox *uploadCheckBox = qobject_cast<QCheckBox*>(ui.assets->cellWidget(a.row, 0));
        if (uploadCheckBox && uploadCheckBox->property("existsDisk").toBool())
        {
            uploadCheckBox->blockSignals(true);
            uploadCheckBox->setChecked(checked);
            uploadCheckBox->blockSignals(false);
        }
    }
    if (checked)
    {
        ui.buttonAssetToggleSelection->setText("Deselect All");
        ui.buttonAssetToggleSelection->setIcon(QIcon(":/images/icon-checked.png"));
    }
    else
    {
        ui.buttonAssetToggleSelection->setText("Select All");
        ui.buttonAssetToggleSelection->setIcon(QIcon(":/images/icon-unchecked.png"));
    }
    
    UpdateActionButton();
}

void RocketStorageSceneImporter::UpdateActionButton()
{
    bool createEntities = false;
    foreach(const RocketStorageImportEntityItem &e, entities)
    {
        QCheckBox *createCheckBox = qobject_cast<QCheckBox*>(ui.entities->cellWidget(e.row, 0));
        bool create = createCheckBox != 0 ? createCheckBox->isChecked() : false;
        if (create)
        {
            createEntities = true;
            break;
        }
    }
    bool uploads = false;
    foreach(const RocketStorageImportAssetItem &a, assets)
    {
        QCheckBox *uploadCheckBox = qobject_cast<QCheckBox*>(ui.assets->cellWidget(a.row, 0));
        bool upload = uploadCheckBox != 0 ? uploadCheckBox->isChecked() : false;
        if (upload)
        {
            uploads = true;
            break;    
        }
    }
    if (createEntities || uploads)
        ui.buttonImport->setText((createEntities && uploads ? "Upload and Import" : (createEntities && !uploads ? "Import" : "Upload")));
    else
        ui.buttonImport->setText("Nothing Selected");
    ui.buttonImport->setEnabled(createEntities || uploads);
}
#include "MemoryLeakCheck.h"