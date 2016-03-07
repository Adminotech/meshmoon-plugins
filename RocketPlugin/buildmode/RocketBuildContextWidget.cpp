/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildEditor.cpp
    @brief  RocketBuildContextWidget is a context UI for the Build UI. */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketBuildContextWidget.h"
#include "RocketBuildEditor.h"
#include "RocketBuildWidget.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "utils/RocketAnimations.h"

#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageWidget.h"

#include "Framework.h"
#include "Application.h"
#include "UiAPI.h"
#include "Entity.h"
#include "EC_Name.h"

#include "MemoryLeakCheck.h"

namespace
{
    const char * const cNoNameId = "(no name)";
    const char * const cNoGroupId = "(no group)";
    const char * const cMultipleId = "(Multiple)";

    /// Returns list of unique group names for the entities, an empty group is also considered as a group.
    QStringList FindUniqueGroupNames(const EntityList &entities)
    {
        QSet<QString> groupNames;
        foreach(const EntityPtr &e, entities)
            groupNames.insert(e->Group());
        return groupNames.toList();
    }
}

RocketBuildContextWidget::RocketBuildContextWidget(RocketPlugin *plugin) :
    RocketSceneWidget(plugin->GetFramework()),
    rocket(plugin),
    isCompact_(false)
{
    mainWidget.setupUi(widget_);

    // Undo/redo
    undoButton = new QToolButton();
    undoButton->setToolTip("Undo previous action");
    undoButton->setPopupMode(QToolButton::MenuButtonPopup);
    undoButton->setDisabled(true);
    undoButton->setIcon(QIcon(QDir::fromNativeSeparators(Application::InstallationDirectory()) + "data/ui/images/icon/undo-icon.png"));

    redoButton = new QToolButton();
    redoButton->setToolTip("Redo last action");
    redoButton->setPopupMode(QToolButton::MenuButtonPopup);
    redoButton->setDisabled(true);
    redoButton->setIcon(QIcon(QDir::fromNativeSeparators(Application::InstallationDirectory()) + "data/ui/images/icon/redo-icon.png"));

    mainWidget.undoLayout->addWidget(undoButton);
    mainWidget.undoLayout->addWidget(redoButton);

    mainWidget.entityNameLineEdit->setPlaceholderText(cNoNameId);
    mainWidget.groupComboBox->lineEdit()->setPlaceholderText(cNoGroupId);

    connect(mainWidget.entityNameLineEdit, SIGNAL(editingFinished()), SLOT(SetEntityName()));
    connect(mainWidget.groupComboBox->lineEdit(), SIGNAL(editingFinished()), SLOT(SetEntityGroup()));
    connect(mainWidget.groupComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetEntityGroup()));
    connect(mainWidget.selectGroupsButton, SIGNAL(toggled(bool)), rocket->BuildEditor(), SLOT(SetSelectGroupsEnabled(bool)));
    connect(mainWidget.deleteButton, SIGNAL(clicked()), rocket->BuildEditor(), SLOT(DeleteSelection()));

    connect(rocket->BuildEditor(), SIGNAL(SelectionChanged(const EntityList &)), SLOT(Refresh(const EntityList &)));

    connect(Fw()->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), SLOT(Resize()));

    Fw()->Ui()->GraphicsScene()->addItem(this);
    assert(scene());
    hide();
}

RocketBuildContextWidget::~RocketBuildContextWidget()
{
    Fw()->Ui()->GraphicsScene()->removeItem(this);
    SAFE_DELETE(redoButton);
    SAFE_DELETE(undoButton);
}

void RocketBuildContextWidget::Show()
{
    Resize();
    setOpacity(0.0);
    Animate(size(), pos(), 1.0, Fw()->Ui()->GraphicsScene()->sceneRect(), RocketAnimations::AnimateDown);
    PopulateGroups();
    Refresh(rocket->BuildEditor()->Selection());
}

void RocketBuildContextWidget::Hide(RocketAnimations::Animation anim, const QRectF &sceneRect)
{
    RocketSceneWidget::Hide(RocketAnimations::AnimateUp, Fw()->Ui()->GraphicsScene()->sceneRect());
}

void RocketBuildContextWidget::ConnectToNameChanges()
{
    EC_Name *name = !editedEntity.expired() ? editedEntity.lock()->Component<EC_Name>().get() : 0;
    if (name)
    {
        connect(name, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
            SLOT(UpdateEntityName(IAttribute *)), Qt::UniqueConnection);
    }
}

void RocketBuildContextWidget::DisconnectFromNameChanges()
{
    EC_Name *name = !editedEntity.expired() ? editedEntity.lock()->Component<EC_Name>().get() : 0;
    if (name)
        disconnect(name);
}

void RocketBuildContextWidget::Refresh(const EntityList &selection)
{
    DisconnectFromNameChanges();

    editedEntity = selection.size() == 1 && selection.back() ? selection.back() : EntityWeakPtr();

    ConnectToNameChanges();

    // Name possible to edit only if single entity selected.
    mainWidget.entityNameLineEdit->setEnabled(!editedEntity.expired());

    if (selection.size() > 1)
        mainWidget.entityNameLineEdit->setPlaceholderText(cMultipleId);
    else
        mainWidget.entityNameLineEdit->setPlaceholderText(cNoNameId);

    mainWidget.entityNameLineEdit->setText(!editedEntity.expired() ? editedEntity.lock()->Name() : "");

    // Group possible to edit if one or more entities selected.
    // Setting a new group for a mixed group will show a confirmation dialog later on.
    mainWidget.groupComboBox->setEnabled(!selection.empty());

    const QStringList groupNames = FindUniqueGroupNames(selection);
    if (groupNames.size() == 1)
        mainWidget.groupComboBox->lineEdit()->setText(groupNames.first());
    else
        mainWidget.groupComboBox->lineEdit()->setText(!editedEntity.expired() ? editedEntity.lock()->Group() : "");

    if (groupNames.size() > 1)
        mainWidget.groupComboBox->lineEdit()->setPlaceholderText(cMultipleId);
    else
        mainWidget.groupComboBox->lineEdit()->setPlaceholderText(cNoGroupId);
}

void RocketBuildContextWidget::Resize()
{
    RocketBuildWidget *buildWidget = rocket->BuildEditor()->BuildWidget();
    if (!buildWidget)
        return; // for now do nothing, but in the future context widget might be standalone widget.

    const QRectF sceneRect = Fw()->Ui()->GraphicsScene()->sceneRect();
    QPointF bPos = buildWidget->scenePos();
    
    QRectF buildWidgetVisibleRect = buildWidget->VisibleSceneRect();
    qreal availableWidth = sceneRect.width() - buildWidgetVisibleRect.width();
    qreal x = buildWidgetVisibleRect.width();

    RocketStorageWidget *storageWidget = rocket->Storage()->StorageWidget();
    if (storageWidget)
    {
        /// @todo Remove
        connect(storageWidget, SIGNAL(VisibilityChanged(bool)), SLOT(Resize()), Qt::UniqueConnection);
        availableWidth -= (storageWidget->isVisible() ? storageWidget->VisibleSceneRect().width() : 0.0);
    }

    bool compact = (availableWidth <= 800);
    qreal height = (!compact ? 36 : 72);
    if (compact != isCompact_)
    {
        isCompact_ = compact;
        QHBoxLayout *l = (isCompact_ ? mainWidget.bottomLayout : mainWidget.topLayout);
        l->addWidget(mainWidget.gizmoContainer);
        mainWidget.frame->updateGeometry();
        mainWidget.frame->update();
    }

    if (!animations_->IsRunning())
    {
        setPos(x, 0.0);
        resize(availableWidth, height);
    }
    else
    {
        animations_->pos->setEndValue(QPointF(x, 0.0));
        animations_->size->setEndValue(QSizeF(availableWidth, height));
        
        // If the animation a hide/show animation is running in build widget sync the time.
        if (buildWidget->AnimationEngine()->IsRunning())
            animations_->SynchronizeFrom(buildWidget->AnimationEngine());
    }
}

void RocketBuildContextWidget::SetUndoButtonEnabled(bool enabled)
{
    undoButton->setEnabled(enabled);
}

void RocketBuildContextWidget::SetRedoButtonEnabled(bool enabled)
{
    redoButton->setEnabled(enabled);
}

void RocketBuildContextWidget::SetEntityName()
{
    DisconnectFromNameChanges();

    const EntityList selection = rocket->BuildEditor()->Selection();
    if (selection.size() == 1 && selection.back() && selection.back()->Name() != mainWidget.entityNameLineEdit->text())
        rocket->BuildEditor()->SetAttribute(selection.back()->GetOrCreateComponent<EC_Name>()->name, mainWidget.entityNameLineEdit->text());

    ConnectToNameChanges();
}

void RocketBuildContextWidget::SetEntityGroup()
{
    DisconnectFromNameChanges();

    const QString newGroup = mainWidget.groupComboBox->currentText();

    EntityList selection = rocket->BuildEditor()->Selection();
    if (selection.size() == 1)
    {
        if (selection.back() && selection.back()->Group() == newGroup)
            selection.clear();
    }
    else if (selection.size() > 1)
    {
        const QStringList groupNames = FindUniqueGroupNames(selection);
        if (groupNames.size() > 1)
        {
            QStringList groupNamesDecorated = groupNames;
            for(int i = 0; i < groupNamesDecorated.size(); ++i)
            {
                groupNamesDecorated[i] = "\"" + groupNamesDecorated[i] + "\"";
                if (i < groupNamesDecorated.size() - 1)
                    groupNamesDecorated[i].append(",");
                if (groupNamesDecorated.size() > 1 && i == groupNamesDecorated.size() - 2)
                    groupNamesDecorated[i].append(tr(" and"));
            }

            mainWidget.groupComboBox->lineEdit()->blockSignals(true);

            if (rocket->Notifications()->ExecSplashDialog(
                tr("Are you sure you want to assign entities belonging to groups %1 to the group \"%2\"?")
                .arg(groupNamesDecorated.join(" ")).arg(newGroup), ":/images/icon-questionmark.png",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
            {
                selection.clear();
            }
        }
    }

    foreach(const EntityPtr &e, selection)
        rocket->BuildEditor()->SetAttribute(e->GetOrCreateComponent<EC_Name>()->group, newGroup);

    ConnectToNameChanges();

    mainWidget.groupComboBox->lineEdit()->blockSignals(false);
}

void RocketBuildContextWidget::UpdateEntityName(IAttribute *attr)
{
    if (attr->Id().compare("name", Qt::CaseInsensitive) == 0)
        mainWidget.entityNameLineEdit->setText(attr->ToString());
    if (attr->Id().compare("group", Qt::CaseInsensitive) == 0)
        mainWidget.groupComboBox->lineEdit()->setText(attr->ToString());
}

void RocketBuildContextWidget::PopulateGroups()
{
    mainWidget.groupComboBox->blockSignals(true);

    mainWidget.groupComboBox->clear();

    /// @todo Make RocketBuildEditor keep its entity group info up to date at all times and use that info there.
    /*
    const EntityGroupMap &entityGroups = rocket->BuildEditor()->EntityGroups();
    for(EntityGroupMap::const_iterator it = entityGroups.begin(); it != entityGroups.end(); ++it)
    {
        bool duplicate = false;
        for(int itemIdx = 0; itemIdx < mainWidget.groupComboBox->count(); ++itemIdx)
            if (duplicate = mainWidget.groupComboBox->itemText(itemIdx).compare(it->first) == 0)
                break;

        if (!duplicate)
            mainWidget.groupComboBox->addItem(it->first);
    }
    */
    ScenePtr scene = rocket->BuildEditor()->Scene();
    if (scene)
    {
        std::vector<shared_ptr<EC_Name> > names = scene->Components<EC_Name>();
        for(size_t i = 0; i < names.size(); ++i)
        {
            const QString groupName = names[i]->group.Get();
            if (!groupName.isEmpty())
            {
                bool duplicate = false;
                for(int itemIdx = 0; itemIdx < mainWidget.groupComboBox->count(); ++itemIdx)
                {
                    duplicate = (mainWidget.groupComboBox->itemText(itemIdx).compare(groupName, Qt::CaseSensitive) == 0);
                    if (duplicate)
                        break;
                }
                if (!duplicate)
                    mainWidget.groupComboBox->addItem(names[i]->group.Get());
            }
        }
    }

    mainWidget.groupComboBox->blockSignals(false);
}
