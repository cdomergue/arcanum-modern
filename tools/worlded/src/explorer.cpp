#include "editor_canvas.hpp"
#include "worlded/world.hpp"
#include "world_view.hpp"
#include "workspace.hpp"
#include "script_editor.hpp"
#include "modern_script.hpp"
#include "dialogue_editor.hpp"
#include "terrain_brush.hpp"
#include "facade.hpp"
#include "actor_editor.hpp"
#include "inventory_editor.hpp"
#include "campaign_editor.hpp"
#include <QtWidgets>
#include <QtConcurrent>
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

using namespace worlded;
namespace fs = std::filesystem;
namespace {
QString text(const std::string& value) { return QString::fromUtf8(cp1252_utf8(value).c_str()); }
QString utf8(const std::string& value) { return QString::fromUtf8(value.c_str()); }

class ResourceModel : public QAbstractTableModel {
public:
    std::shared_ptr<Installation> installation;
    std::vector<std::string> paths;
    void setInstallation(std::shared_ptr<Installation> value) {
        beginResetModel(); installation = std::move(value); paths.clear();
        for (const auto& [name, sources] : installation->resources()) {
            (void)sources;
            auto ext = fs::path(name).extension();
            if (ext == ".art" || ext == ".sec" || ext == ".mob" || ext == ".pro" || ext == ".mes" || ext == ".scr" || ext == ".dlg")
                paths.push_back(name);
        }
        endResetModel();
    }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : int(paths.size()); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 2; }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= int(paths.size())) return {};
        const auto& path = paths[index.row()];
        const auto& source = installation->resources().at(path).back();
        if (role == Qt::DisplayRole) return index.column() == 0 ? QVariant(text(path)) : QVariant(QLocale().formattedDataSize(source.size));
        if (role == Qt::ToolTipRole) return text(path) + "\n" + utf8(source.file.u8string());
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return section == 0 ? "Resource" : "Size";
        return {};
    }
};
class ResourceFilter : public QSortFilterProxyModel {
public:
    QString search, extension;
    void refresh() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange(); endFilterChange(Direction::Rows);
#else
        invalidateFilter();
#endif
    }
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        auto name = sourceModel()->index(row, 0, parent).data().toString();
        return name.contains(search, Qt::CaseInsensitive) && (extension.isEmpty() || name.endsWith(extension));
    }
};

std::string objectKey(const GameObject& object) { return object.editor_id.empty() ? object.id : object.editor_id; }

class Explorer : public QMainWindow {
public:
    QString installationPath, initialProject, initialReference;
    bool rememberInstallation=false;
    std::function<QString(const QString&)> projectInstallationChooser;
    std::vector<QGraphicsItem*> locationDots;
    ResourceModel model;
    ResourceFilter filter;
    QTableView* resources = new QTableView;
    QLineEdit* search = new QLineEdit;
    QComboBox* category = new QComboBox;
    QGraphicsScene* scene = new QGraphicsScene(this);
    Canvas* canvas = new Canvas(scene);
    QTreeWidget* properties = new QTreeWidget;
    QPushButton* configureActor=new QPushButton("Configure character…");
    QListWidget* objectsList = new QListWidget;
    QPlainTextEdit* document = new QPlainTextEdit;
    QStackedWidget* preview = new QStackedWidget;
    QLabel* title = new QLabel("Open an Arcanum installation");
    QLabel* origin = new QLabel;
    QLabel* countLabel = new QLabel;
    QLabel* previewStatus = new QLabel;
    QComboBox* rotation = new QComboBox;
    QComboBox* palette = new QComboBox;
    QSpinBox* frame = new QSpinBox;
    QPushButton* play = new QPushButton("Animate");
    QWidget* animationControls = new QWidget;
    QTimer animation;
    std::shared_ptr<Installation> installation;
    std::unique_ptr<SceneAssets> assets;
    std::unique_ptr<Art> art;
    std::vector<GameObject> objects, effectiveObjects;
    std::vector<QGraphicsItem*> objectItems;
    QString initialResource, initialSector, snapshot;
    bool demoHouse = false, initialWorld=false;
    QString initialWorldSector;
    std::unique_ptr<Building> building;
    bool showingBuilding = false;
    struct EditState { std::vector<GameObject> objects; std::set<std::string> selected; Sector ground; BuildingPlan plan; std::map<std::string,std::string> names; std::map<std::string,Bytes> content; };
    struct EditRecord { EditState before, after; QString label; };
    std::vector<EditRecord> undoObjects, redoObjects;
    struct CachedDraft { Building building; std::vector<EditRecord> undo,redo;
        std::shared_ptr<Building> saved;QString path,name;QTransform view;QPointF center;std::set<std::string> selection;bool hasView=false;
    };
    std::uint64_t activeDraft=UINT64_MAX,referenceDraft=UINT64_MAX,nextDraft=1ull<<63;
    std::vector<std::uint64_t> draftOrder;
    std::shared_ptr<Building> savedScene;
    QString savedPath,draftName;
    QTabBar* zoneTabs=new QTabBar;
    QLabel* zoneState=new QLabel;
    QLabel* workspaceState=new QLabel;
    QListWidget* changesList=new QListWidget;
    QSplitter* zoneViews=new QSplitter;
    QWidget* referencePanel=new QWidget;
    QComboBox* referenceSelector=new QComboBox;
    QLabel* referenceState=new QLabel;
    QGraphicsScene* referenceScene=new QGraphicsScene(this);
    Canvas* referenceCanvas=new Canvas(referenceScene);
    std::vector<QGraphicsItem*> referenceItems;
    QAction* sideAction=new QAction("Side by side",this);
    QAction* saveAction=new QAction("Save zone",this);
    QAction* changeProjectInstallation=new QAction("Change project installation…",this);
    QAction* copyAction=new QAction("Copy",this);
    QAction* pasteAction=new QAction("Paste…",this);
    QCheckBox* copyFloor=new QCheckBox("Include ground");
    QComboBox* editLayer=new QComboBox;
    QCheckBox* roofVisible=new QCheckBox("Show roofs");
    QWidget* surfaceTools=new QWidget;
    QCheckBox* paintSurface=new QCheckBox("Paint");
    QCheckBox* terrainTransitions=new QCheckBox("Automatic transitions");
    QLabel* surfaceMaterialStatus=new QLabel("Choose a material from the library.");
    bool terrainTransitionsPreference=true;
    QSpinBox* brushSize=new QSpinBox;
    QWidget* facadePanel=new QWidget;
    QListWidget* facadePalette=new QListWidget;
    QLineEdit* facadeSearch=new QLineEdit;
    QWidget* surfacePanel=new QWidget;
    QListWidget* surfacePalette=new QListWidget;
    QLineEdit* surfaceSearch=new QLineEdit;
    QTabWidget* inspectorTabs=new QTabWidget;
    QListWidget* peopleList=new QListWidget;
    QListWidget* scriptsList=new QListWidget;
    QWidget* scriptsPanel=new QWidget;
    QLabel* scriptDetails=new QLabel("Select a script to inspect its attachment.");
    QPushButton* locateScript=new QPushButton("Locate");
    QPushButton* openScript=new QPushButton("Open");
    QPushButton* editScriptAttachment=new QPushButton("Edit attachment…");
    QPushButton* removeScript=new QPushButton("Remove attachment");
    QLabel* contextStatus=new QLabel;
    struct ContextActor {GameObject object;bool campaign;};
    struct ContextScript {unsigned id;QString label;QPoint tile;int origin;std::string objectKey;unsigned slot;std::size_t sectorIndex;bool inherited;};
    struct ContextData {std::vector<ContextActor> actors;std::vector<ContextScript> scripts;QStringList warnings;};
    ContextData activeContext,referenceContext;
    std::map<std::string,std::map<std::uint64_t,std::vector<GameObject>>> campaignCache;
    SceneFragment copied;
    bool referenceFocused=false,workspaceRefreshing=false;
    std::map<std::uint64_t,std::unique_ptr<CachedDraft>> worldDrafts;
    WorldView* worldView = new WorldView;
    std::string worldMapName;
    QAction* worldAction = new QAction("World",this);
    QWidget* worldControls = new QWidget;
    QComboBox* worldPlaces = new QComboBox;
    QCheckBox* provenanceToggle = new QCheckBox("Stored sectors");
    QWidget* neighborPanel = new QWidget;
    std::vector<std::pair<QPushButton*,QPoint>> neighborButtons;
    QTabWidget* libraryTabs = new QTabWidget;
    ObjectPalette* objectPalette = new ObjectPalette;
    QLineEdit* paletteSearch = new QLineEdit;
    QComboBox* paletteCategory = new QComboBox;
    std::vector<GameObject> paletteObjects;
    bool paletteReady=false;
    QLabel* paletteCount=new QLabel;
    QTimer paletteIcons;
    QLabel* selectionLabel = new QLabel("No selection");
    QAction* deleteHouse = new QAction("Delete",this);
    QAction* duplicateHouse = new QAction("Duplicate",this);
    QAction* previewLights=new QAction("Light preview",this);
    QAction* gridAction = new QAction("Grid",this);
    bool refreshing = false;
    QTimer construction;
    std::size_t visibleObjects = 0;
    QWidget* placementControls = new QWidget;
    QSpinBox* placeX = new QSpinBox;
    QSpinBox* placeY = new QSpinBox;
    QAction* exportHouse = new QAction("Export scene…", this);
    QAction* returnHouse = new QAction("Return to scene", this);
    QAction* undoHouse = new QAction("Undo", this);
    QAction* redoHouse = new QAction("Redo", this);
    bool failed = false;
    int missingSprites = 0;
    unsigned mountGeneration = 0;

    ~Explorer() override {
        scene->disconnect(this);referenceScene->disconnect(this);objectsList->disconnect(this);
        scene->clear();referenceScene->clear();
    }
    Explorer() {
        setWindowTitle("WorldED Renaissance — Zones"); resize(1680, 1000);
        canvas->setObjectName("activeZoneView");referenceCanvas->setObjectName("referenceZoneView");
        setMinimumSize(1000, 650);
        setStyleSheet("QMenu {background:#222d39;color:#e7edf5;border:1px solid #617990;padding:5px;}"
            "QMenu::item {padding:7px 28px 7px 12px;border:1px solid transparent;}"
            "QMenu::item:selected {background:#3d6585;color:#ffffff;border:1px solid #83b9df;}"
            "QMenu::item:disabled {color:#85909b;} QMenu::separator {height:1px;background:#536476;margin:4px 6px;}"
            "QMenuBar::item {padding:6px 10px;background:transparent;}"
            "QMenuBar::item:selected,QMenuBar::item:pressed {background:#3d6585;color:#ffffff;}"
            "QTabBar::tab {padding:8px 12px;border-bottom:3px solid transparent;}"
            "QTabBar::tab:selected {background:#344e64;border-bottom:3px solid #83c8f5;}"
            "QTabBar::tab:hover {background:#3a5267;}"
            "QGraphicsView#activeZoneView {border:2px solid #669fc9;} QGraphicsView#referenceZoneView {border:1px solid #58687a;}");
        auto openGame = new QAction("Open installation…", this);
        openGame->setShortcut(QKeySequence::Open);
        auto openArchive = new QAction("Open archive…", this);
        auto openProject = new QAction("Open project…",this);
        auto locationAction = new QAction("Map location…",this);
        auto resumeTest = new QAction("Resume test…",this);
        auto testAction = new QAction("Test in a copy…",this);
        auto newHouse = new QAction("New zone…", this);
        auto openSector = new QAction("Open local sector…", this);
        auto fileMenu = menuBar()->addMenu("File");
        auto editMenu = menuBar()->addMenu("Edit");
        auto zoneMenu = menuBar()->addMenu("Zone");
        auto resourcesMenu = menuBar()->addMenu("Resources");
        auto viewMenu = menuBar()->addMenu("View");
        auto testMenu = menuBar()->addMenu("Test");
        changeProjectInstallation->setEnabled(false);
        fileMenu->addAction(newHouse);fileMenu->addAction(openProject);fileMenu->addSeparator();
        fileMenu->addAction(saveAction);
        auto saveAs=fileMenu->addAction("Save zone as…");connect(saveAs,&QAction::triggered,this,[this]{saveActive({},true);});
        fileMenu->addAction(exportHouse);fileMenu->addSeparator();
        auto installationMenu=fileMenu->addMenu("Installation");installationMenu->addAction(openGame);installationMenu->addAction(changeProjectInstallation);
        fileMenu->addSeparator();
        undoHouse->setToolTip("Undo · Ctrl+Z / ⌘Z");redoHouse->setToolTip("Redo · Ctrl+Shift+Z / ⌘Shift+Z");
        deleteHouse->setToolTip("Delete selection · Del");duplicateHouse->setToolTip("Duplicate selection · Ctrl+D");
        editMenu->addAction(undoHouse);editMenu->addAction(redoHouse);editMenu->addSeparator();
        editMenu->addAction("Inventory…",this,[this]{editInventory();});
        editMenu->addAction(copyAction);editMenu->addAction(pasteAction);editMenu->addAction(duplicateHouse);editMenu->addAction(deleteHouse);
        zoneMenu->addAction(openSector);zoneMenu->addSeparator();
        zoneMenu->addAction("Sector properties…",this,[this]{configureSector();});zoneMenu->addAction("Lights…",this,[this]{configureLights();});zoneMenu->addAction(locationAction);
        zoneMenu->addAction("Sector script…",this,[this]{editGroundScript(UINT32_MAX);});
        resourcesMenu->addAction("Campaign…",this,[this]{if(installation){CampaignEditor dialog(*installation,this,installationPath);dialog.exec();}});
        resourcesMenu->addAction("Scripts & dialogues…",this,[this]{contentLibrary();});resourcesMenu->addSeparator();
        resourcesMenu->addAction("New script…",this,[this]{createContent(false);});resourcesMenu->addAction("New dialogue…",this,[this]{createContent(true);});
        resourcesMenu->addSeparator();resourcesMenu->addAction(openArchive);
        viewMenu->addAction(worldAction);viewMenu->addAction(returnHouse);viewMenu->addAction(sideAction);viewMenu->addSeparator();
        testMenu->addAction(testAction);testMenu->addAction(resumeTest);
        saveAction->setShortcut(QKeySequence::Save);connect(saveAction,&QAction::triggered,this,[this]{saveActive();});
        exportHouse->setText("Export a copy of the zone…");
        connect(copyAction,&QAction::triggered,this,[this]{copySelection(referenceFocused);});
        connect(pasteAction,&QAction::triggered,this,[this]{beginPaste();});
        sideAction->setCheckable(true);connect(sideAction,&QAction::toggled,this,[this](bool on){if(on){cacheDraft();if(referenceDraft==UINT64_MAX){for(auto key:draftOrder)if(key!=activeDraft){referenceDraft=key;break;}if(referenceDraft==UINT64_MAX){openBeside();return;}}renderReference();}referencePanel->setVisible(on);});
        exportHouse->setEnabled(false); returnHouse->setEnabled(false); undoHouse->setEnabled(false); redoHouse->setEnabled(false);
        fileMenu->addAction("Quit", QKeySequence::Quit, this, &QWidget::close);
        auto toolbar = addToolBar("Project and view"); toolbar->setMovable(false);
        toolbar->addAction(newHouse); toolbar->addAction(openProject); toolbar->addSeparator();
        toolbar->addAction(saveAction);toolbar->addSeparator();
        toolbar->addAction(worldAction);toolbar->addAction(sideAction);connect(worldAction,&QAction::triggered,this,[this]{showWorld();});
        worldView->feedback=[this](const QString& value){statusBar()->showMessage(value);};
        worldView->openSector=[this](QPoint sector){openWorldSector(sector);};
        auto fitView=toolbar->addAction("Fit view",this,[this]{if (preview->currentWidget()==worldView) worldView->fitInView(worldView->sceneRect(),Qt::KeepAspectRatio); else canvas->fit();});
        auto fitObjectsAction=toolbar->addAction("Fit objects", this, [this]{ fitObjects(); });
        auto actualSize=toolbar->addAction("100 %", this, [this]{ if(preview->currentWidget()==worldView) worldView->resetTransform(); else canvas->resetTransform(); });
        viewMenu->addAction(fitView);viewMenu->addAction(fitObjectsAction);viewMenu->addAction(actualSize);viewMenu->addSeparator();
        viewMenu->addAction(gridAction);previewLights->setCheckable(true);viewMenu->addAction(previewLights);connect(previewLights,&QAction::toggled,this,[this]{if(editing())showBuilding(false);if(sideAction->isChecked())renderReference();});auto roofsAction=viewMenu->addAction("Show roofs");roofsAction->setCheckable(true);roofsAction->setChecked(roofVisible->isChecked());
        connect(roofsAction,&QAction::toggled,roofVisible,&QCheckBox::setChecked);connect(roofVisible,&QCheckBox::toggled,roofsAction,&QAction::setChecked);
        toolbar->addSeparator();toolbar->addAction(testAction);
        addToolBarBreak(); auto editBar=addToolBar("Edit"); editBar->setMovable(false);
        editLayer->addItems({"Objects","Ground","Roofs","Characters","Scripts","Blocking"});editBar->addWidget(new QLabel("Mode "));editBar->addWidget(editLayer);editBar->addWidget(roofVisible);editBar->addSeparator();
        connect(editLayer,&QComboBox::currentIndexChanged,this,[this](int index){setLayer(index);});
        connect(roofVisible,&QCheckBox::toggled,this,[this]{if(editing())showBuilding(false);if(sideAction->isChecked())renderReference();});
        editBar->addAction(undoHouse); editBar->addAction(redoHouse); editBar->addSeparator();
        editBar->addAction(copyAction);editBar->addAction(pasteAction);editBar->addAction(duplicateHouse); editBar->addAction(deleteHouse); editBar->addSeparator();
        gridAction->setCheckable(true); editBar->addAction(gridAction); editBar->addSeparator(); editBar->addWidget(selectionLabel);
        connect(gridAction,&QAction::toggled,this,[this](bool checked){canvas->showGrid=checked;canvas->viewport()->update();});
        connect(deleteHouse,&QAction::triggered,this,[this]{deleteSelection();});
        connect(duplicateHouse,&QAction::triggered,this,[this]{duplicateSelection();});
        canvas->moveObjects=[this](const auto& indices,QPoint delta){translateSelection(indices,delta);};
        canvas->addObject=[this](int index,QPoint tile){dropObject(index,tile);};
        canvas->paletteSprite=[this](int index){return paletteSpriteAt(index);};
        canvas->canDropObject=[this](int index){return index>=0 && index<int(paletteObjects.size()) && (canvas->layer==0 || (canvas->layer==3 && (paletteObjects[index].type==15 || paletteObjects[index].type==16)));};
        canvas->feedback=[this](const QString& message){statusBar()->showMessage(message);};
        canvas->deleteObjects=[this]{deleteSelection();}; canvas->duplicateObjects=[this]{duplicateSelection();};
        canvas->undoEdit=[this]{history(false);}; canvas->redoEdit=[this]{history(true);};
        canvas->copyObjects=[this]{copySelection(false);};canvas->pasteObjects=[this]{beginPaste();};canvas->placeSelection=[this](QPoint tile){return pasteAt(tile);};canvas->focused=[this]{referenceFocused=false;};
        referenceCanvas->setSelectionOnly(true);referenceCanvas->copyObjects=[this]{copySelection(true);};referenceCanvas->focused=[this]{referenceFocused=true;updateCopyActions();};
        connect(referenceScene,&QGraphicsScene::selectionChanged,this,[this]{referenceState->setText(QString("Reference · %1 objects selected · no changes here").arg(referenceScene->selectedItems().size()));updateCopyActions();referenceCanvas->viewport()->update();});
        objectsList->installEventFilter(this);
        canvas->contextMenu=[this](QPoint point){showCanvasMenu(canvas,point,false);};
        canvas->toggleBlock=[this](QPoint tile){if(!editing())return;auto ground=building->ground;bool blocked=ground.blocked(tile.x(),tile.y());ground.set_blocked(tile.x(),tile.y(),!blocked);commitEdit(blocked?"Clear manual blocking":"Block tile",building->objects,{},&ground);};
        referenceCanvas->contextMenu=[this](QPoint point){showCanvasMenu(referenceCanvas,point,true);};
        objectsList->setContextMenuPolicy(Qt::CustomContextMenu);connect(objectsList,&QWidget::customContextMenuRequested,this,[this](QPoint point){
            referenceFocused=false;if(auto item=objectsList->itemAt(point);item && !item->isSelected())objectsList->setCurrentItem(item);updateCopyActions();
            QMenu menu(this);menu.addAction(copyAction);menu.addAction(pasteAction);menu.addSeparator();menu.addAction(duplicateHouse);menu.addAction(deleteHouse);menu.exec(objectsList->viewport()->mapToGlobal(point));});
        worldView->setContextMenuPolicy(Qt::CustomContextMenu);connect(worldView,&QWidget::customContextMenuRequested,this,[this](QPoint point){auto sector=worldView->sectorAt(worldView->mapToScene(point));if(sector.x()<0)return;QMenu menu(this);
            menu.addAction(QString("Edit sector %1, %2").arg(sector.x()).arg(sector.y()),this,[this,sector]{openWorldSector(sector);});
            menu.addAction("Open beside as reference",this,[this,sector]{openSectorBeside(sector);});menu.exec(worldView->viewport()->mapToGlobal(point));});
        connect(openGame, &QAction::triggered, this, [this]{ auto path = QFileDialog::getExistingDirectory(this, "Arcanum installation"); if (!path.isEmpty()) mount(path); });
        connect(openArchive, &QAction::triggered, this, [this]{ auto path = QFileDialog::getOpenFileName(this, "Arcanum archive", {}, "Archives (*.dat *.DAT *.patch* *.PATCH*)"); if (!path.isEmpty()) mount(path); });

        connect(openProject,&QAction::triggered,this,[this]{auto path=QFileDialog::getExistingDirectory(this,"WorldED project");if(!path.isEmpty()) openProjectFolder(path);});
        connect(changeProjectInstallation,&QAction::triggered,this,[this]{if(savedPath.isEmpty())return;auto prompt="Choose the new project installation";auto path=projectInstallationChooser?projectInstallationChooser(prompt):QFileDialog::getExistingDirectory(this,prompt,installationPath);
            if(path.isEmpty())return;
            auto project=savedPath;if(QFileInfo(path).canonicalFilePath()==QFileInfo(installationPath).canonicalFilePath()){try{remember_project_installation(project,path);statusBar()->showMessage("Installation linked to the project.");}catch(const std::exception& e){statusBar()->showMessage("Cannot link installation: "+utf8(e.what()));}}else mount(path,project);});
        connect(locationAction,&QAction::triggered,this,[this]{configureLocation();});
        connect(resumeTest,&QAction::triggered,this,[this]{auto path=QFileDialog::getOpenFileName(this,"WorldED playable copy",{},"Test copy (playtest.json)");if(path.isEmpty())return;
            try {launchPlaytest(fs::u8path(QFileInfo(path).absolutePath().toStdString()));}
            catch(const std::exception& e){QMessageBox::warning(this,"Cannot start test",utf8(e.what()));}});
        connect(testAction,&QAction::triggered,this,[this]{configurePlaytest();});
        connect(newHouse,&QAction::triggered,this,[this]{ configureBuilding(); });
        connect(openSector,&QAction::triggered,this,[this]{
            if (!assets) { error("Open an installation first to resolve sprites and prototypes."); return; }
            auto path=QFileDialog::getOpenFileName(this,"Local sector",{},"Sector (*.sec)");
            if (!path.isEmpty()) showLocalSector(path);
        });
        connect(returnHouse,&QAction::triggered,this,[this]{ if (building) { construction.stop(); visibleObjects=building->objects.size(); showBuilding(true); } });
        connect(exportHouse,&QAction::triggered,this,[this]{
            if (!building) return;
            auto path=QFileDialog::getSaveFileName(this,"New project folder — separate export", "scene-arcanum", "Project folder (*)", nullptr, QFileDialog::DontConfirmOverwrite);
            if (path.isEmpty()) return;
            try { auto output=export_building(*building,fs::u8path(path.toUtf8().constData()));
                statusBar()->showMessage("Export created: "+utf8(output.u8string())); }
            catch (const std::exception& e) { QMessageBox::warning(this,"Cannot export",utf8(e.what())); }
        });
        connect(undoHouse,&QAction::triggered,this,[this]{ history(false); });
        connect(redoHouse,&QAction::triggered,this,[this]{ history(true); });
        connect(&construction,&QTimer::timeout,this,[this]{
            if (!building) { construction.stop(); return; }
            visibleObjects=std::min(visibleObjects+1,building->objects.size());
            if (visibleObjects==building->objects.size()) construction.stop();
            showBuilding(); if (!construction.isActive()) capture();
        });
        auto splitter = new QSplitter;
        auto browser = new QWidget; auto browserLayout = new QVBoxLayout(browser);
        auto browserTitle = new QLabel("RESOURCES"); browserTitle->setObjectName("sectionTitle");
        browserLayout->addWidget(browserTitle);
        search->setPlaceholderText("Search by name or path…"); search->setClearButtonEnabled(true);
        browserLayout->addWidget(search);
        for (const auto& pair : {std::pair{"All", ""}, {"Sectors", ".sec"}, {"Sprites", ".art"}, {"Mobile objects", ".mob"},
                {"Prototypes", ".pro"}, {"Texts", ".mes"}, {"Scripts", ".scr"}, {"Dialogues", ".dlg"}})
            category->addItem(pair.first, pair.second);
        browserLayout->addWidget(category);
        filter.setSourceModel(&model); filter.setFilterCaseSensitivity(Qt::CaseInsensitive);
        resources->setModel(&filter); resources->setSelectionBehavior(QAbstractItemView::SelectRows);
        resources->setSelectionMode(QAbstractItemView::SingleSelection); resources->setEditTriggers(QAbstractItemView::NoEditTriggers);
        resources->verticalHeader()->hide(); resources->horizontalHeader()->setStretchLastSection(false);
        resources->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        resources->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        resources->setWordWrap(false); resources->setAlternatingRowColors(true);
        browserLayout->addWidget(resources); browserLayout->addWidget(countLabel);
        auto palettePanel=new QWidget; auto paletteLayout=new QVBoxLayout(palettePanel);
        auto paletteTitle=new QLabel("OBJECT LIBRARY"); paletteTitle->setObjectName("sectionTitle"); paletteLayout->addWidget(paletteTitle);
        paletteSearch->setPlaceholderText("Search objects…"); paletteSearch->setClearButtonEnabled(true); paletteLayout->addWidget(paletteSearch);
        paletteCategory->addItem("All categories",-1); paletteCategory->addItem("Walls",0); paletteCategory->addItem("Doors",1); paletteCategory->addItem("Containers",2); paletteCategory->addItem("Scenery",3); paletteCategory->addItem("Ground",100);
        for(const auto& entry : {std::pair{"Projectiles",4},{"Weapons",5},{"Ammunition",6},{"Armor",7},{"Currency",8},{"Food",9},{"Scrolls",10},{"Keys",11},{"Key rings",12},{"Written items",13},{"Miscellaneous items",14},{"Player characters",15},{"Characters and creatures",16},{"Traps",17}}) paletteCategory->addItem(entry.first,entry.second);
        paletteLayout->addWidget(paletteCategory); paletteLayout->addWidget(objectPalette,1); paletteLayout->addWidget(paletteCount);
        paletteIcons.setSingleShot(true); paletteIcons.setInterval(30);
        objectPalette->visibleItemsChanged=[this]{paletteIcons.start();};
        connect(&paletteIcons,&QTimer::timeout,this,[this]{loadPaletteIcons();});
        connect(objectPalette->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{paletteIcons.start();});
        connect(libraryTabs,&QTabWidget::currentChanged,this,[this]{paletteIcons.start();});
        auto tip=new QLabel("Shared catalogue for all sectors.\nDrag an object into the scene."); tip->setWordWrap(true); paletteLayout->addWidget(tip);
        connect(paletteSearch,&QLineEdit::textChanged,this,[this]{filterPalette();});
        connect(paletteCategory,&QComboBox::currentIndexChanged,this,[this]{filterPalette();});
        libraryTabs->addTab(palettePanel,"Objects"); libraryTabs->addTab(browser,"Resources"); libraryTabs->setCurrentIndex(1);
        auto surfaceLayout=new QVBoxLayout(surfacePanel);surfaceSearch->setPlaceholderText("Search materials…");surfaceLayout->addWidget(surfaceSearch);
        surfacePalette->setViewMode(QListView::IconMode);surfacePalette->setResizeMode(QListView::Adjust);surfacePalette->setMovement(QListView::Static);surfacePalette->setIconSize({90,70});surfacePalette->setGridSize({110,112});surfacePalette->setSpacing(4);surfacePalette->setWordWrap(true);surfaceLayout->addWidget(surfacePalette);
        surfaceMaterialStatus->setWordWrap(true);surfaceLayout->addWidget(surfaceMaterialStatus);
        auto surfaceHint=new QLabel("Select: drag across cells.\nRight-click: sample a material.\nCtrl+C / Ctrl+V: copy only the active layer.");surfaceHint->setWordWrap(true);surfaceLayout->addWidget(surfaceHint);libraryTabs->addTab(surfacePanel,"Ground / Roofs");
        connect(surfaceSearch,&QLineEdit::textChanged,this,[this]{filterSurfaces();});connect(surfacePalette->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{loadSurfaceIcons();});
        connect(libraryTabs,&QTabWidget::currentChanged,this,[this]{QTimer::singleShot(0,this,[this]{loadSurfaceIcons();});});
        auto facadeLayout=new QVBoxLayout(facadePanel);facadeSearch->setPlaceholderText("Search facades and rugs…");facadeLayout->addWidget(facadeSearch);
        facadePalette->setViewMode(QListView::ListMode);facadePalette->setResizeMode(QListView::Adjust);facadePalette->setMovement(QListView::Static);
        facadePalette->setIconSize({128,96});facadePalette->setUniformItemSizes(true);facadePalette->setSpacing(2);facadePalette->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);facadePalette->setWordWrap(true);facadeLayout->addWidget(facadePalette);
        auto facadePlace=new QPushButton("Place facade");facadeLayout->addWidget(facadePlace);auto facadeHint=new QLabel("Choose the native orientation from the library (including flipped variants). Click Place, then click in the zone. Esc cancels.");facadeHint->setWordWrap(true);facadeLayout->addWidget(facadeHint);
        libraryTabs->addTab(facadePanel,"Facades");connect(facadeSearch,&QLineEdit::textChanged,this,[this]{filterFacades();});
        connect(facadePalette->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{loadFacadeIcons();});connect(facadePlace,&QPushButton::clicked,this,[this]{placeFacade();});
        connect(facadePalette,&QListWidget::itemDoubleClicked,this,[this]{placeFacade();});
        connect(libraryTabs,&QTabWidget::currentChanged,this,[this]{if(libraryTabs->currentWidget()==facadePanel){populateFacades();QTimer::singleShot(0,this,[this]{loadFacadeIcons();});}});
        splitter->addWidget(libraryTabs);

        auto center = new QWidget; auto centerLayout = new QVBoxLayout(center);
        auto surfaceCommands=new QHBoxLayout(surfaceTools);surfaceCommands->setContentsMargins(0,0,0,0);surfaceCommands->addWidget(paintSurface);brushSize->setRange(1,16);brushSize->setSuffix(" cells");surfaceCommands->addWidget(brushSize);terrainTransitions->setChecked(true);surfaceCommands->addWidget(terrainTransitions);
        connect(paintSurface,&QCheckBox::toggled,this,[this](bool enabled){canvas->brushEnabled=enabled;});connect(brushSize,&QSpinBox::valueChanged,this,[this](int size){canvas->brushSize=size;});canvas->strokeFinished=[this](const auto& cells){paintCells(cells);};auto apply=new QPushButton("Apply to selection");surfaceCommands->addWidget(apply);surfaceCommands->addStretch();centerLayout->addWidget(surfaceTools);surfaceTools->hide();connect(apply,&QPushButton::clicked,this,[this]{applySurface(false);});
        connect(terrainTransitions,&QCheckBox::toggled,this,[this](bool enabled){if(terrainTransitions->isEnabled())terrainTransitionsPreference=enabled;});
        connect(surfacePalette,&QListWidget::currentItemChanged,this,[this]{updateSurfaceMaterial();});
        canvas->activateObject=[this](int index){if(!editing() || index<0 || index>=int(building->objects.size()))return;scene->clearSelection();objectItems[index]->setSelected(true);editActor();};
        canvas->cellsSelected=[this](QRect){surfaceSelected(false);};referenceCanvas->cellsSelected=[this](QRect){surfaceSelected(true);};canvas->inspectContext=[this](int index){inspectContextObject(false,index);};referenceCanvas->inspectContext=[this](int index){inspectContextObject(true,index);};
        zoneTabs->setDocumentMode(true);zoneTabs->setExpanding(false);zoneTabs->setUsesScrollButtons(true);zoneTabs->setTabsClosable(true);centerLayout->addWidget(zoneTabs);
        connect(zoneTabs,&QTabBar::currentChanged,this,[this](int index){if(!workspaceRefreshing && index>=0)activateDraft(zoneTabs->tabData(index).toULongLong());});
        connect(zoneTabs,&QTabBar::tabBarClicked,this,[this](int index){if(index>=0 && !showingBuilding && zoneTabs->tabData(index).toULongLong()==activeDraft)activateDraft(activeDraft);});
        connect(zoneTabs,&QTabBar::tabCloseRequested,this,[this](int index){closeZone(zoneTabs->tabData(index).toULongLong());});
        zoneState->setWordWrap(true);zoneState->setMinimumHeight(48);centerLayout->addWidget(zoneState);
        title->setWordWrap(true); title->setObjectName("previewTitle"); centerLayout->addWidget(title);
        origin->setWordWrap(true); origin->setTextInteractionFlags(Qt::TextSelectableByMouse);
        origin->setObjectName("origin"); centerLayout->addWidget(origin);
        auto worldLayout=new QHBoxLayout(worldControls); worldLayout->setContentsMargins(0,0,0,0);
        worldPlaces->setEditable(true); worldPlaces->setInsertPolicy(QComboBox::NoInsert); worldPlaces->setMinimumWidth(170);
        worldPlaces->completer()->setFilterMode(Qt::MatchContains); worldPlaces->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        worldLayout->addWidget(worldPlaces,1); worldLayout->addWidget(provenanceToggle); provenanceToggle->setChecked(true);
        provenanceToggle->setToolTip("Blue: a .sec file exists. Elsewhere: terrain templates, if defined. A file does not guarantee a hand-built sector.");
        connect(provenanceToggle,&QCheckBox::toggled,this,[this](bool checked){worldView->showProvenance=checked;worldView->viewport()->update();});
        connect(worldPlaces,&QComboBox::activated,this,[this](int index){
            if(index<=0) return;
            auto point=worldPlaces->itemData(index).toPointF();
            worldView->resetTransform(); worldView->scale(3,3); worldView->centerOn(point);
        });
        centerLayout->addWidget(worldControls); worldControls->hide();
        preview->addWidget(worldView); preview->addWidget(canvas); document->setReadOnly(true); document->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        preview->addWidget(document);zoneViews->addWidget(preview);
        auto referenceLayout=new QVBoxLayout(referencePanel);referenceLayout->setContentsMargins(5,0,0,0);
        auto referenceTitle=new QLabel("REFERENCE ZONE · select to copy");referenceTitle->setWordWrap(true);referenceTitle->setObjectName("sectionTitle");referenceLayout->addWidget(referenceTitle);referenceLayout->addWidget(referenceSelector);
        auto referenceButtons=new QHBoxLayout;auto openReference=new QPushButton("Open zone…");auto editReference=new QPushButton("Edit this zone");referenceButtons->addWidget(openReference);referenceButtons->addWidget(editReference);referenceLayout->addLayout(referenceButtons);
        connect(openReference,&QPushButton::clicked,this,[this]{openBeside();});connect(editReference,&QPushButton::clicked,this,[this]{if(referenceDraft!=UINT64_MAX)activateDraft(referenceDraft);});
        connect(referenceSelector,&QComboBox::activated,this,[this](int index){referenceDraft=referenceSelector->itemData(index).toULongLong();renderReference();});
        referenceLayout->addWidget(referenceCanvas,1);copyFloor->setChecked(true);copyFloor->setToolTip("Copy the ground rectangle bounded by the selected objects.");
        auto copyRow=new QHBoxLayout;auto copyReference=new QPushButton("Copy selection");copyRow->addWidget(copyReference);copyRow->addWidget(copyFloor);referenceLayout->addLayout(copyRow);
        connect(copyReference,&QPushButton::clicked,this,[this]{copySelection(true);});referenceState->setWordWrap(true);referenceLayout->addWidget(referenceState);
        zoneViews->addWidget(referencePanel);zoneViews->setStretchFactor(0,1);zoneViews->setStretchFactor(1,1);referencePanel->hide();centerLayout->addWidget(zoneViews,1);
        connect(preview,&QStackedWidget::currentChanged,this,[this]{worldControls->setVisible(preview->currentWidget()==worldView);});
        auto controls = new QHBoxLayout(animationControls); controls->setContentsMargins(0,0,0,0);
        controls->addWidget(play); controls->addWidget(new QLabel("Frame")); controls->addWidget(frame);
        controls->addWidget(new QLabel("Direction")); controls->addWidget(rotation);
        controls->addWidget(new QLabel("Palette")); controls->addWidget(palette); controls->addStretch();
        centerLayout->addWidget(animationControls); animationControls->hide();
        previewStatus->setWordWrap(true); centerLayout->addWidget(previewStatus);
        splitter->addWidget(center);

        auto inspector = new QSplitter(Qt::Vertical);
        auto objectPanel = new QWidget; auto objectLayout = new QVBoxLayout(objectPanel);
        auto neighborLayout=new QGridLayout(neighborPanel); neighborLayout->setContentsMargins(0,0,0,6);
        auto neighborTitle=new QLabel("NEIGHBORING SECTORS · world directions"); neighborTitle->setWordWrap(true); neighborTitle->setObjectName("sectionTitle"); neighborLayout->addWidget(neighborTitle,0,0,1,3);
        const char* directions[]={"↖ NW","↑ North","↗ NE","← West","Map","East →","↙ SW","↓ South","↘ SE"};
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
            auto button=new QPushButton(QString::fromUtf8(directions[(y+1)*3+x+1])); neighborLayout->addWidget(button,y+2,x+1);
            if(x==0 && y==0) connect(button,&QPushButton::clicked,this,[this]{showWorld();});
            else { QPoint delta(-x,y); neighborButtons.push_back({button,delta});
                connect(button,&QPushButton::clicked,this,[this,delta]{if(building && building->existing_map) openWorldSector(QPoint(int(building->sector_id&0x3ffffff),int(building->sector_id>>26))+delta);}); }
        }
        objectLayout->addWidget(neighborPanel); neighborPanel->hide();
        objectsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        auto objectTitle = new QLabel("SCENE OBJECTS"); objectTitle->setObjectName("sectionTitle");
        auto objectTab=new QWidget;auto objectTabLayout=new QVBoxLayout(objectTab);objectTabLayout->setContentsMargins(0,0,0,0);objectTabLayout->addWidget(objectTitle);objectTabLayout->addWidget(objectsList);
        inspectorTabs->addTab(objectTab,"Objects");inspectorTabs->addTab(changesList,"Changes");inspectorTabs->addTab(peopleList,"NPCs");
        inspectorTabs->setTabToolTip(1,"Pending changes");inspectorTabs->setTabToolTip(2,"Zone characters");
        auto scriptsLayout=new QVBoxLayout(scriptsPanel);contextStatus->setWordWrap(true);scriptsLayout->addWidget(contextStatus);scriptsLayout->addWidget(scriptsList);
        scriptDetails->setWordWrap(true);scriptsLayout->addWidget(scriptDetails);auto scriptActions=new QHBoxLayout;scriptActions->addWidget(locateScript);scriptActions->addWidget(openScript);scriptActions->addWidget(editScriptAttachment);scriptActions->addWidget(removeScript);editScriptAttachment->setEnabled(false);scriptsLayout->addLayout(scriptActions);locateScript->setEnabled(false);openScript->setEnabled(false);removeScript->setEnabled(false);inspectorTabs->addTab(scriptsPanel,"Scripts");
        connect(peopleList,&QListWidget::currentRowChanged,this,[this](int index){if(index>=0)inspectContextObject(false,index);});
        connect(peopleList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){int index=peopleList->row(item);if(index>=0 && index<int(activeContext.actors.size())){if(!activeContext.actors[index].campaign){editActor();return;}auto loc=activeContext.actors[index].object.number("LOCATION");canvas->centerOn(Canvas::project({int(loc&63),int((loc>>32)&63)}));}});
        connect(scriptsList,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){showScript(item->data(Qt::UserRole).toUInt());});
        connect(scriptsList,&QListWidget::currentRowChanged,this,[this](int index){selectScript(index,false);});
        connect(locateScript,&QPushButton::clicked,this,[this]{selectScript(scriptsList->currentRow(),true);});
        connect(openScript,&QPushButton::clicked,this,[this]{int i=scriptsList->currentRow();if(i>=0 && i<int(activeContext.scripts.size()))showScript(activeContext.scripts[i].id);});
        connect(editScriptAttachment,&QPushButton::clicked,this,[this]{int i=scriptsList->currentRow();if(i>=0 && i<int(activeContext.scripts.size())){auto s=activeContext.scripts[i];if(s.origin>=2)editGroundScript(s.origin==3?UINT32_MAX:std::uint32_t(s.tile.y()*64+s.tile.x()));}});
        connect(removeScript,&QPushButton::clicked,this,[this]{removeSelectedScript();});
        auto removeScriptShortcut=new QShortcut(QKeySequence::Delete,scriptsList);connect(removeScriptShortcut,&QShortcut::activated,this,[this]{removeSelectedScript();});
        connect(properties,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){if(item->data(0,Qt::UserRole).isValid())showScript(item->data(0,Qt::UserRole).toUInt());});objectLayout->addWidget(inspectorTabs);inspector->addWidget(objectPanel);
        auto propertyPanel = new QWidget; auto propertyLayout = new QVBoxLayout(propertyPanel);
        auto propertyTitle = new QLabel("PROPERTIES"); propertyTitle->setObjectName("sectionTitle"); propertyLayout->addWidget(propertyTitle);
        properties->setHeaderLabels({"Field", "Value"}); properties->setColumnCount(2);
        properties->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        properties->header()->setStretchLastSection(true); properties->setWordWrap(true);
        propertyLayout->addWidget(properties);
        propertyLayout->addWidget(configureActor);configureActor->hide();connect(configureActor,&QPushButton::clicked,this,[this]{editActor();});
        auto placement=new QHBoxLayout(placementControls); placement->setContentsMargins(0,0,0,0);
        placeX->setRange(0,63); placeY->setRange(0,63);
        placement->addWidget(new QLabel("X")); placement->addWidget(placeX); placement->addWidget(new QLabel("Y")); placement->addWidget(placeY);
        auto move=new QPushButton("Move"); placement->addWidget(move);
        connect(move,&QPushButton::clicked,this,[this]{ moveObject(placeX->value(),placeY->value()); });
        propertyLayout->addWidget(placementControls); placementControls->hide();
        inspector->addWidget(propertyPanel); inspector->setSizes({380,440});
        splitter->addWidget(inspector); splitter->setSizes({270,1110,300}); splitter->setStretchFactor(1,1);
        setCentralWidget(splitter);
        statusBar()->showMessage("Read only · Choose the game folder or a DAT archive");
        statusBar()->addPermanentWidget(workspaceState);workspaceState->setText("No zone open");
        connect(search, &QLineEdit::textChanged, this, [this](const QString& value){ filter.search = value; filter.refresh(); updateCount(); });
        connect(category, &QComboBox::currentIndexChanged, this, [this]{ filter.extension = category->currentData().toString(); filter.refresh(); updateCount(); });
        connect(resources->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& index){
            if (index.isValid()) showResource(model.paths[filter.mapToSource(index).row()]);
        });
        connect(resources,&QAbstractItemView::doubleClicked,this,[this](const QModelIndex& index){if(!index.isValid())return;auto path=QString::fromStdString(model.paths[filter.mapToSource(index).row()]);auto match=QRegularExpression("^scr/([0-9]+).*\\.scr$",QRegularExpression::CaseInsensitiveOption).match(path);if(match.hasMatch())showScript(match.captured(1).toUInt());else if(path.endsWith(".dlg"))editContent(path);});
        connect(objectsList,&QListWidget::itemSelectionChanged,this,[this]{syncListSelection();});
        connect(scene,&QGraphicsScene::selectionChanged,this,[this]{syncSceneSelection();});
        connect(frame, &QSpinBox::valueChanged, this, [this]{ updateFrame(); });
        connect(rotation, &QComboBox::currentIndexChanged, this, [this]{ updateFrame(); });
        connect(palette, &QComboBox::currentIndexChanged, this, [this]{ updateFrame(); });
        connect(play, &QPushButton::clicked, this, [this]{
            if (animation.isActive()) { animation.stop(); play->setText("Animate"); }
            else if (art) { animation.start(1000 / std::clamp(art->fps(), 1u, 120u)); play->setText("Pause"); }
        });
        connect(&animation, &QTimer::timeout, this, [this]{ if (art) frame->setValue((frame->value()+1) % int(art->frames())); });
    }
    void updateCount() { countLabel->setText(QString("%1 / %2 resources").arg(filter.rowCount()).arg(model.rowCount())); }
    void addProperty(const QString& key, const QString& value) {
        auto item = new QTreeWidgetItem(properties, {key,value}); item->setToolTip(1,value);
    }
    bool mount(const QString& path,const QString& projectAfter={}) {
        if(!confirmLeave())return false;
        if(!projectAfter.isEmpty())initialProject.clear();
        const auto generation = ++mountGeneration;
        resources->setEnabled(false); animation.stop(); art.reset(); construction.stop(); canvas->cancelInteraction();
        paletteIcons.stop(); paletteReady=false; objectPalette->clear(); paletteObjects.clear(); paletteCount->clear();
        copied={};assets.reset(); installation.reset(); worldMapName.clear(); worldView->scene()->clear(); locationDots.clear(); worldDrafts.clear();draftOrder.clear();savedScene.reset();savedPath.clear();draftName.clear();activeDraft=referenceDraft=UINT64_MAX;referenceCanvas->cancelInteraction();referenceScene->clear();referenceItems.clear();sideAction->setChecked(false);{QSignalBlocker block(zoneTabs);while(zoneTabs->count())zoneTabs->removeTab(0);}zoneState->clear();changesList->clear();worldPlaces->clear(); worldView->storedSectors.clear(); worldView->provenance=QImage();
        campaignCache.clear();activeContext={};referenceContext={};peopleList->clear();scriptsList->clear();surfacePalette->clear();facadePalette->clear();
        building.reset(); showingBuilding=false; undoObjects.clear(); redoObjects.clear(); updateBuildingActions();
        animationControls->hide(); objectsList->clear(); objects.clear(); effectiveObjects.clear(); objectItems.clear();
        scene->clear(); properties->clear(); origin->clear(); previewStatus->clear();
        document->setPlainText("Reading DAT indexes and loose files…"); preview->setCurrentWidget(document);
        title->setText("Indexing resources…"); statusBar()->showMessage(path);
        auto watcher = new QFutureWatcher<std::shared_ptr<Installation>>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this,watcher,path,generation,projectAfter]{
            if (generation != mountGeneration) { watcher->deleteLater(); return; }
            try {
                installationPath=path;
                installation = watcher->result(); assets = std::make_unique<SceneAssets>(installation);
                model.setInstallation(installation); updateCount(); resources->setEnabled(true);
                if(rememberInstallation && QFileInfo(path).isDir() && !installation->resources().empty()) {
                    QSettings settings;settings.setValue("installation/lastPath",QFileInfo(path).canonicalFilePath());
                }
                title->setText("Select a resource");
                document->setPlainText("Choose a sector, sprite, object or text from the list.\n\nUse search and categories to filter resources.");
                origin->setText("Archives, loose files and Renaissance patches are active.");
                statusBar()->showMessage("Read only · " + path);
                if (!projectAfter.isEmpty()) openProjectFolder(projectAfter,true);
                else if (!initialProject.isEmpty()) {auto project=initialProject;initialProject.clear();openProjectFolder(project,true);}
                else if (initialWorld) { showWorld(); if (!initialWorldSector.isEmpty()) {auto parts=initialWorldSector.split(','); if(parts.size()!=2) throw std::runtime_error("Expected sector x,y");openWorldSector({parts[0].toInt(),parts[1].toInt()});} }
                else if (demoHouse) startBuilding(BuildingPlan{},true);
                else if (!initialSector.isEmpty()) showLocalSector(initialSector);
                else if (!initialResource.isEmpty()) chooseResource(initialResource);
                if(!initialReference.isEmpty()){auto parts=initialReference.split(',');if(parts.size()!=2)throw std::runtime_error("Expected reference sector x,y");openSectorBeside({parts[0].toInt(),parts[1].toInt()});}
                if (!demoHouse) capture();
            } catch (const std::exception& e) { error(utf8(e.what())); capture(); }
            catch (...) { error("Failed to index resources."); capture(); }
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([path]{ return std::make_shared<Installation>(fs::u8path(path.toUtf8().constData())); }));
        return true;
    }
    void chooseResource(const QString& path) {
        auto name = resource_name(utf8_cp1252(path.toUtf8().constData()));
        auto it = std::find(model.paths.begin(), model.paths.end(), name);
        if (it == model.paths.end()) throw std::runtime_error("Requested resource was not found");
        category->setCurrentIndex(0); search->clear();
        auto index = filter.mapFromSource(model.index(int(it-model.paths.begin()),0));
        { QSignalBlocker blocker(resources->selectionModel()); resources->setCurrentIndex(index); }
        resources->scrollTo(index, QAbstractItemView::PositionAtCenter); showResource(name);
    }
    void error(const QString& value) {
        failed = true; title->setText("Resource not loaded");
        document->setPlainText(value); preview->setCurrentWidget(document);
        previewStatus->setText("The source file has not been modified.");
        std::cerr << value.toUtf8().constData() << '\n';
    }
    void capture() {
        if (snapshot.isEmpty()) return;
        QTimer::singleShot(350, this, [this]{
            if (!grab().save(snapshot)) failed = true;
            std::cout << "preview_objects=" << objects.size() << " unresolved_sprites=" << missingSprites << "\n";
            QApplication::exit(failed ? 1 : 0);
        });
    }
    void showResource(const std::string& path) {
        cacheDraft();
        QScopedValueRollback<bool> guard(refreshing,true); canvas->cancelInteraction();
        construction.stop(); showingBuilding=false; updateBuildingActions();
        try {
            failed = false; missingSprites = 0; animation.stop(); play->setText("Animate"); art.reset();
            animationControls->hide(); objectsList->clear(); objects.clear(); effectiveObjects.clear(); objectItems.clear();
            scene->clear(); properties->clear(); document->clear(); previewStatus->clear(); preview->setCurrentWidget(canvas);
            canvas->setBackgroundBrush(canvas->checkerBrush);
            title->setText(text(path));
            const auto& sources = installation->resources().at(path);
            origin->setText(utf8(sources.back().file.u8string()) + (sources.size()>1 ? QString(" · %1 sources, last takes priority").arg(sources.size()) : QString()));
            QString provenance;
            for (const auto& source : sources) provenance += utf8(source.file.u8string()) + '\n';
            origin->setToolTip(provenance);
            auto data = installation->read(path);
            auto ext = fs::path(path).extension();
            if (ext == ".art") {
                art = std::make_unique<Art>(std::move(data));
                { QSignalBlocker a(frame), b(rotation), c(palette);
                    frame->setRange(0, int(art->frames())-1); frame->setValue(0);
                    rotation->clear(); for (unsigned r=0;r<art->rotations();++r) rotation->addItem(QString::number(r));
                    palette->clear(); for (unsigned p=0;p<4;++p) if (art->palette_available(p)) palette->addItem(QString::number(p), p);
                }
                animationControls->show(); updateFrame(); canvas->fit();
            } else if (ext == ".sec") {
                canvas->setBackgroundBrush(Qt::black);
                Sector sector(std::move(data)); objects = sector_objects(sector);
                for (const auto& object : objects) effectiveObjects.push_back(assets->effective(object));
                drawSector(sector);
                for (const auto& object : effectiveObjects) {auto row=new QListWidgetItem(assets->description(object),objectsList);if(object.number("FLAGS")&0x1000u){row->setFlags(row->flags()&~Qt::ItemIsSelectable);row->setToolTip("Edit this item through its owner’s Inventory dialog.");}}
                previewStatus->setText(QString("64 × 64 tiles · %1 static objects · %2 unresolved sprites\nPreview without roofs, lighting, animations or mobile objects; orange markers indicate unavailable sprites.").arg(objects.size()).arg(missingSprites));
                canvas->fit(); fitObjects();
                if (!objects.empty()) objectsList->setCurrentRow(0);
            } else if (ext == ".mob" || ext == ".pro") {
                objects = read_objects(data);
                for (const auto& object : objects) {
                    effectiveObjects.push_back(assets->effective(object));
                    objectsList->addItem(assets->description(effectiveObjects.back()));if(effectiveObjects.back().number("FLAGS")&0x1000u)objectsList->item(objectsList->count()-1)->setFlags(Qt::ItemIsEnabled);
                }
                if (!objects.empty()) {
                    objectsList->setCurrentRow(0);
                    try { auto sprite = assets->sprite(unsigned(effectiveObjects[0].number("CURRENT_AID", effectiveObjects[0].number("AID")))); scene->addPixmap(sprite.pixels); }
                    catch (const std::exception& e) { previewStatus->setText(utf8(e.what())); }
                }
                canvas->fit();
            } else if (ext == ".mes") {
                QString content;
                for (const auto& message : read_messages(data)) content += QString::number(message.id) + "  " + text(message.text) + "\n\n";
                document->setPlainText(content); preview->setCurrentWidget(document);
                previewStatus->setText("Installation texts · CP1252 → UTF-8 · Read only");
            } else if (ext == ".scr") {
                document->setPlainText(script_listing(data));
                preview->setCurrentWidget(document); previewStatus->setText("Script structure · double-click the resource to open the editor");
            } else {
                document->setPlainText(text(std::string(data.begin(),data.end()))); preview->setCurrentWidget(document);
                previewStatus->setText("Source text · Read only");
            }
        } catch (const std::exception& e) { error(utf8(e.what())); }
        refreshWorkspace();
    }
    bool editing() const { return showingBuilding && building && !construction.isActive(); }
    std::set<std::string> selectedIds() const {
        std::set<std::string> result;
        for (auto item : scene->selectedItems()) {
            auto index=item->data(0).toInt();
            if (item->data(0).isValid() && index>=0 && index<int(objects.size())) result.insert(objectKey(objects[index]));
        }
        return result;
    }
    void updateBuildingActions() {
        returnHouse->setEnabled(building!=nullptr); exportHouse->setEnabled(building && !construction.isActive());
        undoHouse->setEnabled(editing() && !undoObjects.empty()); redoHouse->setEnabled(editing() && !redoObjects.empty());
        undoHouse->setText(undoObjects.empty() ? "Undo" : "Undo: "+undoObjects.back().label);
        redoHouse->setText(redoObjects.empty() ? "Redo" : "Redo: "+redoObjects.back().label);
        auto count=scene->selectedItems().size();
        bool actor=false;if(editing() && count==1){auto ids=selectedIds();for(auto& o:building->objects)if(ids.count(objectKey(o)) && (o.type==15 || o.type==16))actor=true;}configureActor->setVisible(editing() && count==1);configureActor->setText(actor?"Configure character…":"Configure object…");
        deleteHouse->setEnabled(editing() && count); duplicateHouse->setEnabled(editing() && canvas->layer==0 && count);
        if(canvas->layer==2)deleteHouse->setEnabled(editing() && !canvas->cellSelection.isEmpty());else if(canvas->layer==4)deleteHouse->setEnabled(removeScript->isEnabled());else if(canvas->layer!=0 && canvas->layer!=3)deleteHouse->setEnabled(false);
        objectPalette->setEnabled(editing() && (canvas->layer==0 || canvas->layer==3)); canvas->setEditing(editing()); gridAction->setEnabled(editing());
        placementControls->setVisible(showingBuilding); placementControls->setEnabled(editing() && count==1);
        neighborPanel->setVisible(editing() && building->existing_map);
        if(building && building->existing_map) for(auto [button,delta] : neighborButtons) {
            auto destination=QPoint(int(building->sector_id&0x3ffffff),int(building->sector_id>>26))+delta;
            button->setEnabled(destination.x()>=0 && destination.y()>=0 && destination.x()<int(worldView->worldWidth) && destination.y()<int(worldView->worldHeight));
            button->setToolTip(QString("Open sector %1, %2 · current draft preserved").arg(destination.x()).arg(destination.y()));
        }
        updateCopyActions();saveAction->setEnabled(building!=nullptr);
        selectionLabel->setText(count ? QString("%1 object(s) selected").arg(count) : "No selection");
    }
    void syncSceneSelection() {
        if (refreshing) return;
        QScopedValueRollback<bool> guard(refreshing,true); QSignalBlocker block(objectsList);
        objectsList->clearSelection(); int first=-1;
        for (auto item : scene->selectedItems()) if (item->data(0).isValid()) {
            int index=item->data(0).toInt(); if (index>=0 && index<objectsList->count()) { objectsList->item(index)->setSelected(true); if (first<0) first=index; }
        }
        if (first>=0) { objectsList->setCurrentRow(first,QItemSelectionModel::NoUpdate); selectObject(first); }
        else properties->clear();
        updateBuildingActions(); canvas->viewport()->update();
    }
    void syncListSelection() {
        if (refreshing) return;
        referenceFocused=false;
        QScopedValueRollback<bool> guard(refreshing,true); QSignalBlocker block(scene);
        scene->clearSelection(); int first=-1;
        for (auto item : objectsList->selectedItems()) { int index=objectsList->row(item); if (index<int(objectItems.size())) objectItems[index]->setSelected(true); if (first<0) first=index; }
        if (first>=0) selectObject(first); else properties->clear();
        updateBuildingActions(); canvas->viewport()->update();
    }
    bool eventFilter(QObject* watched,QEvent* event) override {
        if (watched==objectsList && event->type()==QEvent::KeyPress && editing()) {
            auto key=static_cast<QKeyEvent*>(event);
            if(key->matches(QKeySequence::Copy)){copySelection(false);return true;}
            if(key->matches(QKeySequence::Paste)){beginPaste();return true;}
            if (key->key()==Qt::Key_Delete) { deleteSelection(); return true; }
            if (key->matches(QKeySequence::Undo)) { history(false); return true; }
            if (key->matches(QKeySequence::Redo)) { history(true); return true; }
            if (key->key()==Qt::Key_D && key->modifiers().testFlag(Qt::ControlModifier)) { duplicateSelection(); return true; }
        }
        return QMainWindow::eventFilter(watched,event);
    }
    void filterPalette() {
        unsigned visible=0;
        for (int i=0;i<objectPalette->count();++i) {
            auto item=objectPalette->item(i); const auto& object=paletteObjects.at(item->data(Qt::UserRole).toInt());
            item->setHidden(!(item->text()+" "+item->data(Qt::UserRole+2).toString()).contains(paletteSearch->text(),Qt::CaseInsensitive) || (paletteCategory->currentData().toInt()!=-1 && int(object.type)!=paletteCategory->currentData().toInt()));
            if(!item->isHidden()) ++visible;
        }
        paletteCount->setText(QString("%1 / %2 entries").arg(visible).arg(paletteObjects.size())+(assets && !assets->catalogErrors.empty() ? QString(" · %1 errors").arg(assets->catalogErrors.size()) : QString{}));
        paletteIcons.start();
    }
    Sprite paletteSpriteAt(int index) {
        const auto& object=paletteObjects.at(index);
        try { return assets->sprite(unsigned(object.number("CURRENT_AID"))); }
        catch(const std::exception&) {
            QPixmap marker(48,48); marker.fill(Qt::transparent); QPainter painter(&marker);
            painter.setPen(QPen(QColor("#ffc36a"),2)); painter.drawRect(4,4,39,39);
            painter.drawText(marker.rect(),Qt::AlignCenter,"?");
            return {marker,QPoint(24,24),{}};
        }
    }
    void loadPaletteIcons() {
        if(!assets || !objectPalette->isVisible()) return;
        const auto viewport=objectPalette->viewport()->rect();
        for(int i=0;i<objectPalette->count();++i) {
            auto item=objectPalette->item(i);
            if(item->isHidden() || item->data(Qt::UserRole+1).toBool() || !objectPalette->visualItemRect(item).intersects(viewport)) continue;
            try { item->setIcon(QIcon(assets->sprite(unsigned(paletteObjects[i].number("CURRENT_AID"))).pixels)); }
            catch(const std::exception& e) {
                item->setIcon(QIcon(paletteSpriteAt(i).pixels));
                item->setToolTip(item->toolTip()+"\nPreview unavailable: orange marker in the scene.\n"+utf8(e.what()));
            }
            item->setData(Qt::UserRole+1,true);
        }
    }
    void populatePalette() {
        if(paletteReady) return;
        auto catalog=assets->catalog();
        objectPalette->setUpdatesEnabled(false);
        for(auto& entry : catalog) {
            auto item=new QListWidgetItem(entry.label,objectPalette);
            item->setData(Qt::UserRole,int(paletteObjects.size()));
            item->setData(Qt::UserRole+2,entry.source);
            item->setToolTip(entry.label+"\n"+entry.source+"\n"+(entry.object.type==100 ? "Drag to replace the ground" : "Drag to create a prototype instance"));
            paletteObjects.push_back(std::move(entry.object));
        }
        paletteReady=true; objectPalette->setUpdatesEnabled(true);
        paletteCount->setToolTip(assets->catalogErrors.join("\n"));
        filterPalette();
    }
    void revealInPalette(const GameObject& object) {
        if(!assets) return;
        populatePalette();
        const auto aid=unsigned(object.number("CURRENT_AID",object.number("AID")));
        // Match animation and mirror variants; retain wall face, piece, variation,
        // prototype and damage, as well as the door/window distinction.
        auto appearance=[](unsigned value) {
            unsigned type=value>>28;
            if(type==1) return value & ~0x0831u;
            if(type==3 || type==4 || type==7) return value & ~0x7f831u;
            return value;
        };
        int match=-1,best=0;bool refilter=false;
        for(unsigned i=0;i<paletteObjects.size();++i) {
            const auto& candidate=paletteObjects[i];
            if(candidate.type!=object.type || candidate.prototype_id!=object.prototype_id) continue;
            unsigned candidateAid=unsigned(candidate.number("CURRENT_AID"));
            int score=candidateAid==aid ? 2 : appearance(candidateAid)==appearance(aid) ? 1 : 0;
            if(score>best) {best=score;match=int(i);if(score==2)break;}
        }
        if(match<0) {
            // Existing maps may use an art/prototype combination absent from the
            // standard palette. Add a reusable template, never the placed object's
            // GUID, inventory or instance scripts. It remains available thereafter.
            GameObject entry; entry.type=object.type; entry.prototype_id=object.prototype_id;
            set_number(entry,"AID",aid); set_number(entry,"CURRENT_AID",aid);
            auto prototype=assets->effective(entry); set_number(entry,"FLAGS",prototype.number("FLAGS"));
            if(entry.type==16) set_number(entry,"NPC_FLAGS",prototype.number("NPC_FLAGS")|2);
            auto name=assets->description(object)+QString(" · #%1 · %2").arg(object.prototype_id).arg(aid,8,16,QChar('0'));
            auto item=new QListWidgetItem(name,objectPalette); match=int(paletteObjects.size());
            item->setData(Qt::UserRole,match); item->setData(Qt::UserRole+2,name);
            item->setToolTip(name+"\nDrag to create a new object with the same appearance");
            paletteObjects.push_back(std::move(entry));
            refilter=true;
        }
        auto item=objectPalette->item(match);
        const auto query=paletteSearch->text();
        if(!(item->text()+" "+item->data(Qt::UserRole+2).toString()).contains(query,Qt::CaseInsensitive)) {
            QSignalBlocker block(paletteSearch); paletteSearch->clear();
            refilter=true;
        }
        if(paletteCategory->currentData().toInt()!=-1 && paletteCategory->currentData().toInt()!=int(object.type)) {
            QSignalBlocker block(paletteCategory); paletteCategory->setCurrentIndex(0);
            refilter=true;
        }
        if(refilter)filterPalette();
        libraryTabs->setCurrentIndex(0); objectPalette->setCurrentItem(item);
        if(refilter)objectPalette->doItemsLayout();
        objectPalette->scrollToItem(item,QAbstractItemView::PositionAtCenter);
        paletteIcons.start();
    }
    #include "workspace_ui.inc"
    #include "layers_ui.inc"
    #include "context_ui.inc"
    #include "lights_ui.inc"
    #include "facade_ui.inc"
    #include "content_ui.inc"
    #include "inventory_ui.inc"
    void showWorld() {
        if (!installation) return;
        try {
            if (worldView->scene()->items().empty()) {
                for (const auto& entry : read_messages(installation->read("rules/maplist.mes")))
                    if (entry.text.find("START_MAP")!=std::string::npos) { worldMapName=resource_name(entry.text.substr(0,entry.text.find(','))); break; }
                if (worldMapName.empty()) throw std::runtime_error("No exterior starting map in this installation");
                auto properties=installation->read("maps/"+worldMapName+"/map.prp");
                if (properties.size()!=24) throw std::runtime_error("Invalid map properties");
                auto u64=[&](unsigned start){std::uint64_t n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(properties[start+i])<<(i*8);return n;};
                auto width=u64(8)/64,height=u64(16)/64;
                if (!width || !height || width>4096 || height>4096) throw std::runtime_error("World map image dimensions unsupported");
                worldView->worldWidth=unsigned(width); worldView->worldHeight=unsigned(height);
                std::string config;
                for (const auto& entry : read_messages(installation->read("worldmap/worldmap.mes"))) if(entry.id==50) config=entry.text;
                std::replace(config.begin(),config.end(),',',' '); std::istringstream values(config); unsigned cols=0,rows=0;std::string prefix; values>>cols>>rows>>prefix;
                if (!cols || !rows || cols*rows>256) throw std::runtime_error("Invalid world map image grid");
                QImage map(int(width),int(height),QImage::Format_RGB32); map.fill(Qt::black); QPainter painter(&map);
                for (unsigned i=0;i<cols*rows;++i) {
                    auto path=QString("worldmap/%1%2.bmp").arg(utf8(prefix)).arg(i+1,3,10,QChar('0')).toStdString();
                    auto bytes=installation->read(path); auto chunk=QImage::fromData(bytes.data(),int(bytes.size()),"BMP");
                    if (chunk.isNull() || chunk.width()*cols!=width || chunk.height()*rows!=height) throw std::runtime_error("World map image dimensions disagree");
                    painter.drawImage(QPoint((i%cols)*chunk.width(),(i/cols)*chunk.height()),chunk);
                }
                painter.end(); worldView->scene()->addPixmap(QPixmap::fromImage(map)); worldView->setSceneRect(map.rect());
                worldView->provenance=QImage(map.size(),QImage::Format_ARGB32); worldView->provenance.fill(Qt::transparent);
                auto sectorPrefix="maps/"+worldMapName+"/";
                for(const auto& entry : installation->resources()) {
                    const auto& name=entry.first;
                    if(name.rfind(sectorPrefix,0)!=0 || !QString::fromStdString(name).endsWith(".sec")) continue;
                    bool valid=false; auto id=QString::fromStdString(name.substr(sectorPrefix.size(),name.size()-sectorPrefix.size()-4)).toULongLong(&valid);
                    auto x=id&0x3ffffff,y=id>>26;
                    if(!valid || x>=width || y>=height) continue;
                    worldView->storedSectors.insert(id);
                    if(x>0) worldView->provenance.setPixelColor(int(width-x),int(y),QColor("#008cff"));
                }
                worldPlaces->addItem("Search locations…");
                if(installation->contains("mes/gamearea.mes")) for(const auto& entry : read_messages(installation->read("mes/gamearea.mes"))) {
                    auto parts=text(entry.text).split('/'); if(parts.size()<2) continue;
                    auto coords=parts[0].split(','); if(coords.size()<2) continue;
                    bool okX=false,okY=false; auto x=coords[0].trimmed().toUInt(&okX),y=coords[1].trimmed().toUInt(&okY);
                    if(!okX || !okY || !x || !y || x/64>=width || y/64>=height) continue;
                    QPointF point(width-x/64+.5,y/64+.5);
                    auto dot=worldView->scene()->addEllipse(-3,-3,6,6,QPen(QColor("#fff0c0"),0),QBrush(QColor("#8b291b")));
                    dot->setFlag(QGraphicsItem::ItemIgnoresTransformations); dot->setPos(point); dot->setToolTip(parts[1].trimmed()); dot->setZValue(1);
                    worldPlaces->addItem(parts[1].trimmed(),point);
                }
            }
            cacheDraft();
            for(auto dot:locationDots){worldView->scene()->removeItem(dot);delete dot;}locationDots.clear();
            while(worldPlaces->count()>0 && worldPlaces->itemData(worldPlaces->count()-1,Qt::UserRole+1).toBool())worldPlaces->removeItem(worldPlaces->count()-1);
            std::map<std::uint64_t,QString> labels;
            if(installation->contains("rules/worlded_locations.mes") && installation->contains("mes/worlded_location_names.mes")) {
                std::map<unsigned,QString> names;for(const auto& m:read_messages(installation->read("mes/worlded_location_names.mes")))names[m.id]=text(m.text);
                for(const auto& m:read_messages(installation->read("rules/worlded_locations.mes"))){auto parts=QString::fromStdString(m.text).split(',');if(parts.size()!=3)continue;
                    bool okX=false,okY=false,okId=false;auto x=parts[0].toUInt(&okX),y=parts[1].toUInt(&okY),id=parts[2].toUInt(&okId);
                    if(okX && okY && okId && x<worldView->worldWidth && y<worldView->worldHeight && names.count(id))labels[std::uint64_t(x)|(std::uint64_t(y)<<26)]=names[id];}
            }
            for(const auto& [key,draft]:worldDrafts)if(draft->building.existing_map && !draft->building.location_names.empty()) {
                const auto& names=draft->building.location_names;auto it=names.find(QLocale().name().left(2).toStdString());if(it==names.end())it=names.find("en");if(it==names.end())it=names.begin();labels[draft->building.sector_id]=utf8(it->second);
            }
            for(const auto& [id,label]:labels){QPointF point(worldView->worldWidth-(id&0x3ffffff)+.5,(id>>26)+.5);
                auto dot=worldView->scene()->addEllipse(-4,-4,8,8,QPen(QColor("#fff0c0"),0),QBrush(QColor("#247f52")));dot->setFlag(QGraphicsItem::ItemIgnoresTransformations);dot->setPos(point);dot->setToolTip(label);dot->setZValue(2);locationDots.push_back(dot);
                worldPlaces->addItem(label,point);worldPlaces->setItemData(worldPlaces->count()-1,true,Qt::UserRole+1);}
            canvas->cancelInteraction(); construction.stop(); animation.stop(); showingBuilding=false; updateBuildingActions();
            animationControls->hide(); preview->setCurrentWidget(worldView); title->setText("World map"); origin->setText(text(worldMapName));
            previewStatus->setText(QString("Click to edit a 64 × 64 sector. Wheel: zoom · drag: pan.\nBlue: %1 stored sectors (.sec) · elsewhere: generated terrain, if defined. Drafts remain in memory.").arg(worldView->storedSectors.size()));
            worldView->fitInView(worldView->sceneRect(),Qt::KeepAspectRatio);refreshWorkspace();
        } catch (const std::exception& e) { error(utf8(e.what())); }
    }
    void openWorldSector(QPoint point) {
        if(installation && worldMapName.empty())showWorld();
        if (!installation || worldMapName.empty() || point.x()<0 || point.y()<0 || point.x()>=int(worldView->worldWidth) || point.y()>=int(worldView->worldHeight)) return;
        try {
            cacheDraft(); auto id=std::uint64_t(point.x())|(std::uint64_t(point.y())<<26);
            if(worldDrafts.count(id)){activateDraft(id);return;}
            auto source=read_world_sector(*installation,worldMapName,point.x(),point.y());
            auto objects=sector_objects(source.sector);
            for(auto& object:objects){object.editor_id=clone_instance(object).id;auto loc=object.number("LOCATION");set_number(object,"LOCATION",(loc&63)|(((loc>>32)&63)<<32));transform_npc_routes(object,id,false);}
            building=std::make_unique<Building>(Building{BuildingPlan{},std::move(source.sector),std::move(objects),{}});
            building->map_name=worldMapName;building->sector_id=id;building->existing_map=true;
            activeDraft=id;savedScene=std::make_shared<Building>(*building);savedPath.clear();draftName=QString("Sector %1, %2").arg(point.x()).arg(point.y());undoObjects.clear();redoObjects.clear();
            construction.stop();canvas->cellSelection={};visibleObjects=building->objects.size();populatePalette();libraryTabs->setCurrentIndex(0);showBuilding(true);
        }catch(const std::exception& e){error(utf8(e.what()));}
    }
    void openProjectFolder(const QString& path,bool useCurrentInstallation=false) {
        auto canonical=QFileInfo(path).canonicalFilePath();
        if(canonical.isEmpty() || !QFileInfo::exists(QDir(canonical).filePath("building.json"))){error("WorldED project not found.");return;}
        QString remembered;
        try{remembered=project_installation(canonical);}catch(const std::exception& e){statusBar()->showMessage("Cannot read local settings: "+utf8(e.what()));}
        if(!useCurrentInstallation && !remembered.isEmpty()) {
            auto target=QFileInfo(remembered).canonicalFilePath();auto current=QFileInfo(installationPath).canonicalFilePath();
            if(target.isEmpty()) {
                auto prompt="Saved installation not found — choose the game folder";
                target=projectInstallationChooser?projectInstallationChooser(prompt):QFileDialog::getExistingDirectory(this,prompt,QFileInfo(remembered).absolutePath());
                if(target.isEmpty())return;
                target=QFileInfo(target).canonicalFilePath();if(target.isEmpty()){statusBar()->showMessage("Installation not found.");return;}
            }
            if(!installation || target!=current){mount(target,canonical);return;}
        }
        if(!installation) {
            auto prompt="Choose the Arcanum installation for this project";
            auto target=projectInstallationChooser?projectInstallationChooser(prompt):QFileDialog::getExistingDirectory(this,prompt,QDir::homePath());
            if(!target.isEmpty())mount(target,canonical);
            return;
        }
        try {
            cacheDraft();
            for(const auto& [key,draft]:worldDrafts)if(!draft->path.isEmpty() && draft->path==canonical){activateDraft(key);return;}
            auto next=std::make_unique<Building>(load_project(fs::u8path(path.toStdString())));construction.stop();building=std::move(next);
            activeDraft=nextDraft++;savedScene=std::make_shared<Building>(*building);savedPath=canonical;draftName=QFileInfo(canonical).fileName();
            if(draftName=="project")draftName=QFileInfo(QFileInfo(canonical).absolutePath()).fileName();
            if(building->existing_map)draftName+=QString(" · %1, %2").arg(building->sector_id&0x3ffffff).arg(building->sector_id>>26);
            undoObjects.clear();redoObjects.clear();visibleObjects=building->objects.size();populatePalette();libraryTabs->setCurrentIndex(0);showBuilding(true);
            try{if(project_installation(canonical)!=QFileInfo(installationPath).canonicalFilePath())remember_project_installation(canonical,installationPath);}
            catch(const std::exception& e){statusBar()->showMessage("Project opened, installation link not saved: "+utf8(e.what()));}
        }catch(const std::exception& e){error(utf8(e.what()));}
    }
    void addRoom(const BuildingPlan& plan,bool clear) {
        if(!editing()) return;
        auto next=insert_room(*installation,*building,plan,clear); std::set<std::string> added;
        for(const auto& object:next.objects) if(std::none_of(building->objects.begin(),building->objects.end(),[&](const auto& old){return objectKey(old)==objectKey(object);})) added.insert(objectKey(object));
        commitEdit("Insert room",next.objects,added,&next.ground,&next);showBuilding(true);
    }
    void configureLocation() {
        if(!editing() || !building->existing_map) {error("Open a sector from the world map.");return;}
        QDialog dialog(this);dialog.setWindowTitle("Map location");QVBoxLayout layout(&dialog);
        QLabel explanation("One name per language (code en, fr, de…). Leave the list empty to omit the marker.");explanation.setWordWrap(true);layout.addWidget(&explanation);
        QTableWidget table(0,2);table.setHorizontalHeaderLabels({"Language","Location name"});table.horizontalHeader()->setStretchLastSection(true);layout.addWidget(&table);
        auto add=[&](const QString& lang,const QString& name){int row=table.rowCount();table.insertRow(row);table.setItem(row,0,new QTableWidgetItem(lang));table.setItem(row,1,new QTableWidgetItem(name));};
        for(const auto& [lang,name]:building->location_names)add(utf8(lang),utf8(name));
        if(!table.rowCount()){add("fr","");add("en","");}
        QPushButton extra("Add language");layout.addWidget(&extra);connect(&extra,&QPushButton::clicked,&dialog,[&]{add("","");});
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        dialog.resize(550,350);if(dialog.exec()!=QDialog::Accepted)return;
        auto next=*building;next.location_names.clear();
        for(int row=0;row<table.rowCount();row++){auto lang=table.item(row,0)->text().trimmed().toLower();auto name=table.item(row,1)->text().trimmed();if(!lang.isEmpty() && !name.isEmpty()) next.location_names[lang.toStdString()]=name.toStdString();}
        commitEdit("Name location",next.objects,selectedIds(),nullptr,&next);
    }
    QString compatibleTestEngine() const {
        auto app=QCoreApplication::applicationDirPath();
        QStringList folders={app+"/../../game",app+"/../game",app,installationPath};
        for(const auto& folder:folders)for(const auto& name:QStringList{"arcanum-modern","arcanum-ce","arcanum-modern.exe","arcanum-ce.exe"}) {
            auto path=QFileInfo(folder+"/"+name).absoluteFilePath();if(supports_worlded_test(path))return path;
        }
        return {};
    }
    void launchPlaytest(const fs::path& root) {
        auto process=new QProcess;
        try{configure_test_process(*process,root);}catch(...){delete process;throw;}
        auto log=utf8((root/"test-launch.log").u8string());auto timer=std::make_shared<QElapsedTimer>();timer->start();
        connect(process,&QProcess::finished,this,[this,log,timer](int code,QProcess::ExitStatus status){
            if(code!=0 || status==QProcess::CrashExit || timer->elapsed()<5000) {
                QFile file(log);QString detail;if(file.open(QIODevice::ReadOnly)){if(file.size()>6000)file.seek(file.size()-6000);detail=QString::fromUtf8(file.readAll());}
                QMessageBox box(QMessageBox::Warning,"The test stopped","The game could not continue the test.\nLog: "+log,QMessageBox::Ok,this);box.setDetailedText(detail);box.exec();
            }
        });
        connect(process,&QProcess::finished,process,&QObject::deleteLater);
        process->start();if(!process->waitForStarted(3000)){auto error=process->errorString();delete process;throw std::runtime_error(("Cannot launch the game: "+error+"\nLog: "+log).toStdString());}
        statusBar()->showMessage("Test launched in a copy · log: "+log);
    }
    void configurePlaytest() {
        if(!editing() || !building->existing_map){error("Open a sector from the world map.");return;}
        QDialog dialog(this);dialog.setWindowTitle("Test the zone in a game copy");dialog.resize(820,420);QFormLayout layout(&dialog);
        QLabel summary(QString("Sector %1, %2 · %3 objects. The test uses the zone as shown in the editor.\nThe character will be placed at the sector center in an independent copy of the save.").arg(building->sector_id&0x3ffffff).arg(building->sector_id>>26).arg(building->objects.size()));summary.setWordWrap(true);layout.addRow(&summary);
        QLabel source(installationPath);source.setWordWrap(true);layout.addRow("Source installation",&source);
        auto pathRow=[&](QLineEdit* field,QPushButton* browse){auto row=new QWidget(&dialog);auto h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);h->addWidget(field);h->addWidget(browse);return row;};
        QComboBox saves;saves.setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);saves.setMinimumContentsLength(30);
        auto saveFolder=QDir(installationPath+"/modules/Arcanum/Save");
        for(const auto& entry:saveFolder.entryInfoList({"Slot*.tfai"},QDir::Files,QDir::Name)) {
            auto slot=entry.completeBaseName();auto companions=saveFolder.entryList({slot+"*.gsi"},QDir::Files);QString label=slot;
            if(!companions.isEmpty())label=QFileInfo(companions.front()).completeBaseName().mid(slot.size())+" — "+slot;
            saves.addItem(label,entry.absoluteFilePath());
        }
        QPushButton browseSave("Browse…");auto saveRow=new QWidget(&dialog);auto saveLayout=new QHBoxLayout(saveRow);saveLayout->setContentsMargins(0,0,0,0);saveLayout->addWidget(&saves);saveLayout->addWidget(&browseSave);layout.addRow("Save to copy",saveRow);
        connect(&browseSave,&QPushButton::clicked,&dialog,[&]{auto path=QFileDialog::getOpenFileName(&dialog,"Save to copy",saveFolder.absolutePath(),"Save index (*.tfai)");if(!path.isEmpty()){saves.addItem(QFileInfo(path).fileName(),path);saves.setCurrentIndex(saves.count()-1);}});
        QLineEdit executable(compatibleTestEngine());QPushButton browseEngine("Browse…");layout.addRow("Compatible engine",pathRow(&executable,&browseEngine));
        connect(&browseEngine,&QPushButton::clicked,&dialog,[&]{auto path=QFileDialog::getOpenFileName(&dialog,"WorldED-compatible Renaissance executable",executable.text().isEmpty()?installationPath:executable.text());if(!path.isEmpty())executable.setText(path);});
        QLabel engineStatus;engineStatus.setWordWrap(true);layout.addRow(&engineStatus);
        auto checkEngine=[&]{bool supported=supports_worlded_test(executable.text());engineStatus.setText(supported?"WorldED support detected.":"Choose the WorldED-compatible engine. The .sh launcher and older versions cannot run this test.");engineStatus.setStyleSheet(supported?"color:#9fd6b0":"color:#f0bd79");};connect(&executable,&QLineEdit::textChanged,&dialog,checkEngine);checkEngine();
        auto base=savedPath.isEmpty()?QDir::homePath()+"/WorldED/test-zone":QFileInfo(savedPath).absolutePath()+"/test-"+QFileInfo(savedPath).fileName();auto candidate=base;int suffix=2;while(QFileInfo::exists(candidate))candidate=base+"-"+QString::number(suffix++);
        QLineEdit output(candidate);QPushButton browseOutput("Browse…");layout.addRow("New test folder",pathRow(&output,&browseOutput));
        connect(&browseOutput,&QPushButton::clicked,&dialog,[&]{auto path=QFileDialog::getSaveFileName(&dialog,"New test folder",output.text(),"Folder (*)",nullptr,QFileDialog::DontConfirmOverwrite);if(!path.isEmpty())output.setText(path);});
        QString detectedLanguage="en";
        if(installation->contains("mes/language.mes"))for(const auto& message:read_messages(installation->read("mes/language.mes")))
            if(message.id==1 && QStringList{"fr","en","de","es","it","sv"}.contains(text(message.text)))detectedLanguage=text(message.text);
        for(const auto& lang:QStringList{"fr","en"}) {QFile ready(installationPath+"/modern-ui/uap-"+lang+"/active/ready");if(ready.open(QIODevice::ReadOnly) && ready.readAll()==("2.0.2-"+lang+"-1\n").toUtf8()){detectedLanguage=lang;break;}}
        QLineEdit language(detectedLanguage);layout.addRow("Installation language",&language);
        QStringList names;for(const auto& [lang,name]:building->location_names)names<<utf8(lang)+" : "+utf8(name);
        QLabel marker(names.isEmpty()?"No named marker: add a name using “Map location…”.":"In-game map marker: "+names.join(" · "));marker.setWordWrap(true);layout.addRow(&marker);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Ok)->setText("Create and launch test");layout.addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(saves.currentData().toString().isEmpty() || !supports_worlded_test(executable.text()) || output.text().trimmed().isEmpty()){QMessageBox::warning(&dialog,"Incomplete test","Choose a save, a WorldED-compatible engine and a new test folder.");return;}dialog.accept();});
        connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return;
        auto save=saves.currentData().toString(),engine=executable.text(),destination=output.text(),lang=language.text().trimmed().toLower();
        QProgressDialog progress("Preparing the playable copy…",QString(),0,0,this);progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
        QFutureWatcher<QPair<PlaytestResult,QString>> watcher;QEventLoop loop;connect(&watcher,&QFutureWatcherBase::finished,&loop,&QEventLoop::quit);
        auto sceneCopy=*building;auto sourcePath=installationPath;
        watcher.setFuture(QtConcurrent::run([=]() -> QPair<PlaytestResult,QString> {
            try {return {create_playtest(sceneCopy,fs::u8path(sourcePath.toStdString()),fs::u8path(save.toStdString()),fs::u8path(engine.toStdString()),fs::u8path(destination.toStdString()),lang.toStdString()),{}};}
            catch(const std::exception& e){return {{},QString::fromUtf8(e.what())};}
        }));
        loop.exec();progress.close();
        try {auto outcome=watcher.result();if(!outcome.second.isEmpty())throw std::runtime_error(outcome.second.toStdString());launchPlaytest(outcome.first.root);}
        catch(const std::exception& e){QMessageBox::warning(this,"Cannot start test",utf8(e.what()));}
    }
    void configureBuilding() {
        if (!assets) { error("Open an Arcanum installation first."); return; }
        QDialog dialog(this); dialog.setWindowTitle("New scene"); QFormLayout layout(&dialog);
        QComboBox preset; preset.addItems({"Empty terrain","Rectangular room"}); layout.addRow("Starting point",&preset);
        QSpinBox width,depth; width.setRange(8,24); width.setValue(12); depth.setRange(8,24); depth.setValue(10);
        layout.addRow("Width (tiles)",&width); layout.addRow("Depth (tiles)",&depth);
        width.setEnabled(false); depth.setEnabled(false);
        connect(&preset,&QComboBox::currentIndexChanged,&dialog,[&]{width.setEnabled(preset.currentIndex()==1);depth.setEnabled(preset.currentIndex()==1);});
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); layout.addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if (dialog.exec()!=QDialog::Accepted) return;
        BuildingPlan plan; plan.width=width.value(); plan.depth=depth.value(); startBuilding(plan,false);
        if (!building) return;
        if (preset.currentIndex()==0) { building->objects.clear(); for (unsigned y=0;y<64;++y) for (unsigned x=0;x<64;++x) building->ground.set_tile(x,y,plan.ground); }
        else building->objects.erase(std::remove_if(building->objects.begin(),building->objects.end(),[](const auto& obj){return obj.type==2;}),building->objects.end());
        visibleObjects=building->objects.size(); showBuilding(true);
    }
    void startBuilding(const BuildingPlan& plan,bool animate) {
        try {
            auto next=std::make_unique<Building>(build_house(*installation,plan));cacheDraft();activeDraft=nextDraft++;savedScene.reset();savedPath.clear();draftName=QString("New zone %1").arg(activeDraft-(1ull<<63)+1);
            construction.stop(); building=std::move(next); undoObjects.clear(); redoObjects.clear();
            populatePalette(); libraryTabs->setCurrentIndex(0);
            visibleObjects=animate ? 0 : building->objects.size();
            if (animate) construction.start(110);
            showBuilding(true);
        } catch (const std::exception& e) { construction.stop(); updateBuildingActions(); error(utf8(e.what())); capture(); }
    }
    void showSectorData(Sector sector, const std::vector<GameObject>& mobiles = {}) {
        QScopedValueRollback<bool> guard(refreshing,true); canvas->cancelInteraction();
        { QSignalBlocker blocker(resources->selectionModel()); resources->setCurrentIndex({}); }
        animation.stop(); art.reset(); animationControls->hide(); scene->clear(); objectItems.clear();
        objectsList->clear(); properties->clear(); effectiveObjects.clear();
        preview->setCurrentWidget(canvas); canvas->setBackgroundBrush(Qt::black); missingSprites=0; failed=false;
        objects=sector_objects(sector); objects.insert(objects.end(),mobiles.begin(),mobiles.end());
        for (const auto& object : objects) effectiveObjects.push_back(assets->effective(object));
        drawSector(sector);
        for (const auto& object : effectiveObjects) {auto row=new QListWidgetItem(assets->description(object),objectsList);if(object.number("FLAGS")&0x1000u){row->setFlags(row->flags()&~Qt::ItemIsSelectable);row->setToolTip("Edit this item through its owner’s Inventory dialog.");}}
        if (!objects.empty()) objectsList->setCurrentRow(0);
    }
    void showBuilding(bool frameScene=false, const std::set<std::string>* selection=nullptr) {
        if (!building) return;
        auto selected=selection ? *selection : selectedIds();
        auto transform=canvas->transform(); auto center=canvas->mapToScene(canvas->viewport()->rect().center());
        showingBuilding=true; showSectorData(building->sector(visibleObjects));
        // Session identities are intentionally absent from the SEC serialization.
        // Keep distinct selections for static objects with a null persistent ID.
        for (std::size_t i=0;i<objects.size();++i) {
            objects[i].editor_id=building->objects[i].editor_id;
            effectiveObjects[i].editor_id=building->objects[i].editor_id;
        }
        const auto& p=building->plan;
        if (frameScene) {
            QPolygonF bounds; bounds<<Canvas::project({int(p.x),int(p.y)})<<Canvas::project({int(p.x+p.width),int(p.y)})<<Canvas::project({int(p.x),int(p.y+p.depth)})<<Canvas::project({int(p.x+p.width),int(p.y+p.depth)});
            canvas->fitInView(bounds.boundingRect().adjusted(-150,-220,150,140),Qt::KeepAspectRatio);
        } else { canvas->setTransform(transform); canvas->centerOn(center); }
        { QSignalBlocker block(scene); scene->clearSelection();
          for (std::size_t i=0;i<objects.size();++i) objectItems[i]->setSelected(selected.count(objectKey(objects[i]))); }
        syncSceneSelection();
        title->setText(building->existing_map ? QString("Sector %1, %2").arg(building->sector_id&0x3ffffff).arg(building->sector_id>>26) : "Untitled scene");
        origin->setText(building->existing_map ? text(building->map_name)+" · "+(worldView->stored({int(building->sector_id&0x3ffffff),int(building->sector_id>>26)}) ? "stored sector" : "generated terrain template")+" · draft\nCampaign characters and scripts can be inspected in the tabs on the right." : "64 × 64 cells · draft · read-only game sources");
        previewStatus->setText(QString("%1 objects · %2 unresolved sprites\nDrag: move · Ctrl/box: multi-select · Del: delete · wheel: zoom · middle button: pan")
            .arg(objects.size()).arg(missingSprites));
        statusBar()->showMessage(construction.isActive() ? "Creating scene…" : canvas->layer==0?"Drag objects from the palette into the scene":canvas->layer<3?"Choose a material · select cells or enable Paint · Ctrl+Z to undo":canvas->layer==5?"Left-click to toggle manual blocking. Right-click for sector properties.":canvas->layer==4?"Select an attachment to inspect or remove it; double-click to open its script.":"Click a character to inspect it");
        activeContext={};try{activeContext=buildContext(*building);drawContext(scene,activeContext,false);}catch(const std::exception& e){peopleList->clear();scriptsList->clear();contextStatus->setText("Reading campaign: "+utf8(e.what()));}
        if(canvas->layer==1 || canvas->layer==2) {libraryTabs->setCurrentWidget(surfacePanel);if(!surfacePalette->count())populateSurfaces();}
        previewStatus->setText(canvas->layer==1?"GROUND · select ground cells · Ctrl+C / Ctrl+V · Paint to apply material":canvas->layer==2?"ROOFS · 4 × 4 pieces · select at roof height · Del to remove · Ctrl+Z to undo":previewStatus->text());
        if(canvas->layer==3)previewStatus->setText("CHARACTERS · drag from the library to add · move or delete project characters · campaign is read only");
        if(canvas->layer==4)previewStatus->setText("SCRIPTS · click a purple marker or double-click a list entry to open its instructions");
        if(canvas->layer==5)previewStatus->setText("BLOCKING · left-click toggles manual blocking (bright red) · dark red: terrain or object blocking · Ctrl+Z to undo");
        updateBuildingActions();refreshWorkspace();
    }
    void showLocalSector(const QString& path) {
        cacheDraft();
        try {
            construction.stop(); showingBuilding=false; updateBuildingActions();
            auto local=fs::u8path(path.toUtf8().constData());
            bool validSector=false; auto id=QFileInfo(path).baseName().toULongLong(&validSector);
            std::vector<GameObject> mobiles;
            if (validSector) for (const auto& entry : fs::directory_iterator(local.parent_path())) {
                if (entry.path().extension()!=".mob" || entry.is_symlink()) continue;
                for (auto& object : read_objects(read_file(entry.path()))) {
                    auto loc=object.number("LOCATION");
                    if ((std::uint32_t(loc)>>6)==(id&0x3ffffff) && (loc>>38)==(id>>26)) mobiles.push_back(std::move(object));
                }
            }
            showSectorData(Sector(read_file(local)),mobiles);
            title->setText(QFileInfo(path).fileName()); origin->setText(path); fitObjects();
            previewStatus->setText(QString("Local sector · %1 objects · %2 unresolved sprites · read only").arg(objects.size()).arg(missingSprites));
        } catch (const std::exception& e) { error(utf8(e.what())); }
        refreshWorkspace();
    }
    void commitEdit(const QString& label,std::vector<GameObject> next,std::set<std::string> selected, const Sector* ground=nullptr,const Building* metadata=nullptr) {
        if (!editing()) return;
        EditRecord record{{building->objects,selectedIds(),building->ground,building->plan,building->location_names,building->content},{std::move(next),std::move(selected),ground ? *ground : building->ground,metadata ? metadata->plan : building->plan,metadata ? metadata->location_names : building->location_names,metadata ? metadata->content : building->content},label};
        undoObjects.push_back(record); redoObjects.clear();
        building->content=record.after.content; building->plan=record.after.plan; building->location_names=record.after.names; building->ground=record.after.ground; building->objects=record.after.objects; visibleObjects=building->objects.size(); showBuilding(false,&record.after.selected);
    }
    void dropObject(int index,QPoint tile) {
        if (!editing() || index<0 || index>=int(paletteObjects.size()) || !Canvas::valid(tile)) return;
        if (paletteObjects[index].type==100) {
            auto ground=building->ground; auto aid=unsigned(paletteObjects[index].number("CURRENT_AID"));
            if (ground.tile(tile.x(),tile.y())==aid) return;
            ground.set_tile(tile.x(),tile.y(),aid); commitEdit("Paint ground",building->objects,selectedIds(),&ground); return;
        }
        auto object=clone_instance(paletteObjects[index]); set_number(object,"LOCATION",std::uint64_t(tile.x())|(std::uint64_t(tile.y())<<32));
        // Standalone exports must carry every prototype actually placed, including
        // prototypes absent from the original scene's starter set.
        const auto& path=assets->prototypePath(object.prototype_id);
        if(!building->prototypes.count(path)) building->prototypes[path]=installation->read(path);
        if(object.type==16) for(const char* name : {"CRITTER_TELEPORT_DEST","NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"}) set_number(object,name,object.number("LOCATION"));
        auto next=building->objects; next.push_back(object); commitEdit("Add",std::move(next),{object.id});
    }
    void translateSelection(const std::vector<int>& indices,QPoint delta) {
        if (!editing() || delta.isNull() || indices.empty()) return;
        auto next=building->objects; std::set<std::string> selected;
        for (int index : indices) {
            if (index<0 || index>=int(next.size())) return;
            if(next[index].number("FLAGS")&0x1000u){statusBar()->showMessage("Edit this item through its owner’s Inventory dialog.");return;}
            auto loc=next[index].number("LOCATION"); QPoint tile(int(std::uint32_t(loc)),int(loc>>32)); tile+=delta;
            if (!Canvas::valid(tile)) { statusBar()->showMessage("Outside the sector · move refused"); return; }
            auto destination=std::uint64_t(tile.x())|(std::uint64_t(tile.y())<<32);
            set_number(next[index],"LOCATION",destination);
            if(next[index].type==16)for(const char* field:{"CRITTER_TELEPORT_DEST","NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"})if(next[index].field(field) && next[index].number(field)==loc)set_number(next[index],field,destination);
            for(auto& item:next)if(inventory_parent(item)==next[index].id)set_number(item,"LOCATION",destination);
            selected.insert(objectKey(next[index]));
        }
        commitEdit("Move",std::move(next),std::move(selected));
    }
    void deleteSelection() {
        if(canvas->layer==4){removeSelectedScript();return;}
        if(canvas->layer==1 || canvas->layer==2){applySurface(true);return;}if(canvas->layer!=0 && canvas->layer!=3)return;
        if (!editing()) return;
        auto selected=selectedIds(); if (selected.empty()) return;
        for(const auto& object:building->objects)if(selected.count(objectKey(object)) && (object.number("FLAGS")&0x1000u)){statusBar()->showMessage("Edit this item through its owner’s Inventory dialog.");return;}
        std::set<std::string> persistent;for(const auto& object:building->objects)if(selected.count(objectKey(object)))persistent.insert(object.id);
        auto removed=inventory_descendants(building->objects,persistent);
        auto next=building->objects; next.erase(std::remove_if(next.begin(),next.end(),[&](const auto& object){return selected.count(objectKey(object)) || (object.id.substr(0,4)=="0200" && removed.count(object.id));}),next.end());
        commitEdit("Delete",std::move(next),{});
    }
    void duplicateSelection() {
        if(canvas->layer!=0)return;
        if (!editing()) return;
        auto selected=selectedIds(); if (selected.empty()) return;
        for(const auto& object:building->objects)if(selected.count(objectKey(object)) && (object.number("FLAGS")&0x1000u)){statusBar()->showMessage("Edit this item through its owner’s Inventory dialog.");return;}
        std::set<std::string> persistent;for(const auto& o:building->objects)if(selected.count(objectKey(o)))persistent.insert(o.id);
        auto expanded=inventory_descendants(building->objects,persistent);
        auto next=building->objects;std::set<std::string> copies;std::map<std::string,std::string> remap;std::vector<GameObject> additions;
        for(const auto& object:building->objects)if(selected.count(objectKey(object)) || (object.id.substr(0,4)=="0200" && expanded.count(object.id))){
            auto copy=clone_instance(object);auto loc=object.number("LOCATION");QPoint tile(int(std::uint32_t(loc)),int(loc>>32));tile+=QPoint(1,1);
            if(!Canvas::valid(tile)){statusBar()->showMessage("No room to duplicate the selection here");return;}
            auto destination=std::uint64_t(tile.x())|(std::uint64_t(tile.y())<<32);set_number(copy,"LOCATION",destination);
            if(copy.type==16)for(auto name:{"CRITTER_TELEPORT_DEST","NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"})if(copy.field(name) && copy.number(name)==loc)set_number(copy,name,destination);
            remap[object.id]=copy.id;if(!(copy.number("FLAGS")&0x1000))copies.insert(copy.id);additions.push_back(copy);
        }
        for(auto& copy:additions){inventory_remap(copy,remap);next.push_back(std::move(copy));}
        commitEdit("Duplicate",std::move(next),std::move(copies));
    }
    void moveObject(unsigned x,unsigned y) {
        int index=objectsList->currentRow(); if (!editing() || index<0 || index>=int(building->objects.size())) return;
        auto loc=building->objects[index].number("LOCATION"); translateSelection({index},{int(x)-int(std::uint32_t(loc)),int(y)-int(loc>>32)});
    }
    void history(bool redo) {
        if (!editing()) return;
        canvas->cancelInteraction();
        auto& from=redo ? redoObjects : undoObjects; auto& to=redo ? undoObjects : redoObjects; if (from.empty()) return;
        auto record=from.back(); from.pop_back(); const auto& state=redo ? record.after : record.before;
        building->content=state.content; building->plan=state.plan; building->location_names=state.names; building->ground=state.ground; building->objects=state.objects; visibleObjects=building->objects.size(); to.push_back(record); showBuilding(false,&state.selected);
    }
    void updateFrame() {
        if (!art || rotation->currentIndex()<0 || palette->currentIndex()<0) return;
        try {
            const auto& info = art->frame(rotation->currentIndex(), frame->value());
            auto bytes = art->rgba(rotation->currentIndex(),frame->value(),palette->currentData().toUInt());
            QImage image(bytes.data(), info.width,info.height,info.width*4,QImage::Format_RGBA8888);
            scene->clear(); scene->addPixmap(QPixmap::fromImage(image.copy()));
            scene->setSceneRect(scene->itemsBoundingRect().adjusted(-30,-30,30,30));
            properties->clear(); addProperty("Dimensions",QString("%1 × %2").arg(info.width).arg(info.height));
            addProperty("Anchor point",QString("%1, %2").arg(info.hot_x).arg(info.hot_y));
            addProperty("Offset",QString("%1, %2").arg(info.offset_x).arg(info.offset_y));
            addProperty("Frame rate",QString::number(art->fps()) + " frames/s");
            previewStatus->setText(QString("Frame %1 / %2 · %3 direction(s) · Index 0 transparency\nWheel: zoom · Drag: pan").arg(frame->value()+1).arg(art->frames()).arg(art->rotations()));
        } catch (const std::exception& e) { animation.stop(); error(utf8(e.what())); }
    }
    void drawSector(const Sector& sector) {drawScene(scene,sector,effectiveObjects,objectItems,missingSprites);}
    void drawScene(QGraphicsScene* target,const Sector& sector,const std::vector<GameObject>& rendered,std::vector<QGraphicsItem*>& items,int& missing) {
        std::map<unsigned, Sprite> sprites;
        std::set<unsigned> unresolved;
        auto fetch = [&](unsigned aid) -> const Sprite* {
            if (unresolved.count(aid)) return nullptr;
            auto found = sprites.find(aid);
            if (found != sprites.end()) return &found->second;
            try { return &sprites.emplace(aid, assets->sprite(aid)).first->second; }
            catch (const std::exception&) { unresolved.insert(aid); return nullptr; }
        };
        for (unsigned y=0;y<64;++y) for (unsigned x=0;x<64;++x) {
            QPointF top((int(y)-int(x))*40-40,(x+y)*20);
            auto sprite = fetch(sector.tile(x,y));
            if (sprite) { auto item=target->addPixmap(sprite->pixels); item->setPos(top+QPointF((sector.tile(x,y)>>28)==11?1:0,0)); item->setZValue(-10000); }
            else { QPolygonF diamond; diamond << top+QPointF(40,0) << top+QPointF(80,20) << top+QPointF(40,40) << top+QPointF(0,20);
                auto item=target->addPolygon(diamond,QPen(QColor("#46505d")),QBrush(QColor("#303945"))); item->setZValue(-10000); ++missing; }
        }
        for (std::size_t i=0;i<rendered.size();++i) {
            const auto& object=rendered[i]; auto location=object.number("LOCATION");
            if(object.number("FLAGS")&0x1000u){auto hidden=target->addRect(QRectF());hidden->hide();items.push_back(hidden);continue;}
            int x=location&63, y=(location>>32)&63;
            int ox=static_cast<std::int32_t>(object.number("OFFSET_X")), oy=static_cast<std::int32_t>(object.number("OFFSET_Y"));
            QPointF position((y-x)*40+ox,(x+y)*20+20+oy);
            auto sprite=fetch(unsigned(object.number("CURRENT_AID",object.number("AID"))));
            QGraphicsItem* item;
            if (sprite) { auto image=target->addPixmap(sprite->pixels); image->setPos(position-sprite->hotspot);if(canvas->layer==3 && (object.type==15 || object.type==16))image->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);item=image; }
            else { item=target->addEllipse(-5,-5,10,10,QPen(QColor("#ffd6a0")),QBrush(QColor("#e9a25a"))); item->setPos(position); ++missing; }
            item->setZValue(position.y()); item->setFlag(QGraphicsItem::ItemIsSelectable);
            item->setData(0,int(i)); item->setData(1,QPoint(x,y)); item->setToolTip(assets->description(object)); items.push_back(item);
        }
        drawRoofs(target,sector);
        if(previewLights->isChecked())drawLights(target,sector);
        if(canvas->layer==5) {
            std::array<bool,4096> generated{};
            for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)generated[y*64+x]=assets->terrainBlocked(sector.tile(x,y));
            for(const auto& object:rendered)if(object.type!=0 && object.type!=1 && !(object.number("FLAGS")&(0x400u|2u|0x100u|0x1000u))) {
                auto loc=object.number("LOCATION");generated[((loc>>32)&63)*64+(loc&63)]=true;
            }
            for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)if(sector.blocked(x,y) || generated[y*64+x]) {
                bool manual=sector.blocked(x,y);auto center=Canvas::project({int(x),int(y)});
                QPolygonF diamond;diamond<<center+QPointF(0,-20)<<center+QPointF(40,0)<<center+QPointF(0,20)<<center+QPointF(-40,0);
                QColor color=manual?QColor("#ff5050"):QColor("#941c28");
                auto marker=target->addPolygon(diamond,QPen(color,0),QBrush(color,Qt::BDiagPattern));marker->setZValue(30000);
                marker->setToolTip(manual?"Manual blocking: click to clear. Terrain or objects may still block this cell.":"Generated blocking: change the terrain or blocking object to clear it.");
            }
        }
        for(std::size_t i=0;i<items.size();i++){auto item=items[i];item->setFlag(QGraphicsItem::ItemIsSelectable,canvas->layer==0 || (canvas->layer==3 && (rendered[i].type==15 || rendered[i].type==16)));if(canvas->layer==1 || canvas->layer==2)item->setOpacity(.3);}
        target->setSceneRect(target->itemsBoundingRect().adjusted(-50,-50,50,50));
    }
    void fitObjects() {
        if (objectItems.empty()) return;
        QRectF bounds;
        for (auto item : objectItems) bounds = bounds.united(item->sceneBoundingRect());
        canvas->fitInView(bounds.adjusted(-120,-100,120,100), Qt::KeepAspectRatio);
    }
    void editActor() {
        if(!editing())return;
        auto ids=selectedIds();if(ids.size()!=1)return;
        for(std::size_t i=0;i<building->objects.size();i++)if(ids.count(objectKey(building->objects[i]))){
            auto instance=building->objects[i];
            auto resolved=assets->effective(instance);ActorEditor dialog(instance,resolved,assets->description(resolved),this,installation.get(),building->sector_id);
            dialog.openScript=[this](unsigned id){showScript(id);};
            dialog.scriptDefaults=[this](unsigned id){for(auto path:contentPaths(".scr"))if(contentNumber(path)==id)return contentRead(path);throw std::runtime_error("Script resource not found");};
            if(dialog.exec()==QDialog::Accepted && encode_instance(dialog.result)!=encode_instance(instance)){
                auto next=building->objects;next[i]=dialog.result;commitEdit("Configure object",std::move(next),ids);
            }
            return;
        }
    }
    void selectObject(int index) {
        if (index<0 || index>=int(effectiveObjects.size())) return;
        const auto& object=effectiveObjects[index]; properties->clear();
        if(scene->selectedItems().size()==1 && (canvas->layer==0 || canvas->layer==3)) revealInPalette(object);
        placeX->setValue(unsigned(object.number("LOCATION"))&63); placeY->setValue(unsigned(object.number("LOCATION")>>32)&63);
        addProperty("Type",object_type_name(object.type)); addProperty("Prototype",QString::number(object.prototype_id));
        addProperty("ID",utf8(object.id));
        if (object.field("LOCATION")) addProperty("Position (x, y)", QString("%1, %2").arg(quint32(object.number("LOCATION"))).arg(quint32(object.number("LOCATION")>>32)));
        for (const auto& field : object.fields) {
            auto label=utf8(field.name);
            bool inherited=objects[index].field(field.name)==nullptr;
            auto node=new QTreeWidgetItem(properties,{label,utf8(object_field_text(field))});
            node->setToolTip(0,inherited ? "Value inherited from prototype" : "Value stored in this object");
            node->setToolTip(1,node->text(1));
            if (inherited) node->setForeground(0,QColor("#8d99a8"));
            for (const auto& element : field.elements) {
                QString values;
                for (std::size_t b=0;b<element.data.size();b+=4) {
                    std::uint32_t n=0; for (unsigned k=0;k<4 && b+k<element.data.size();++k) n|=std::uint32_t(element.data[b+k])<<(8*k);
                    if (!values.isEmpty()) values += " · ";
                    values += QString::number(n);
                }
                auto child=new QTreeWidgetItem(node,{QString("[%1]").arg(element.index),values});
                if(field.name=="SCRIPTS_IDX" && element.data.size()==12){child->setText(0,element.index<std::size(attachmentNames)?QString(attachmentNames[element.index]):QString::number(element.index));child->setData(0,Qt::UserRole,word(element.data,8));child->setText(1,QString("Script %1 · double-click to open").arg(word(element.data,8)));node->setExpanded(true);}
            }
        }

    }
};
}

inline void applyExplorerStyle(QApplication& app) {
    app.setStyleSheet("QWidget { background:#191f27; color:#dce3eb; font-size:12px; }"
        "QToolBar,QMenuBar,QStatusBar { background:#222b36; padding:5px; }"
        "QLineEdit,QComboBox,QSpinBox { background:#293441; border:1px solid #445164; border-radius:4px; padding:6px; }"
        "QPushButton { background:#344b60; border:1px solid #52718c; border-radius:4px; padding:6px 12px; }"
        "QPushButton:hover { background:#42617b; }"
        "QTableView,QTreeWidget,QListWidget,QPlainTextEdit { background:#202832; alternate-background-color:#242e39; border:1px solid #344252; selection-background-color:#365d7e; }"
        "QHeaderView::section { background:#293441; color:#acbacb; border:0; padding:6px; }"
        "QLabel#sectionTitle { color:#85b6d6; font-weight:bold; padding:4px 0; }"
        "QLabel#previewTitle { font-size:16px; font-weight:bold; } QLabel#origin { color:#9aaabb; font-size:11px; }"
        "QSplitter::handle { background:#2a3542; } QToolTip { background:#293441; color:#eef3f8; border:1px solid #66849d; }");
}

#ifndef WORLDED_EXPLORER_NO_MAIN
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    app.setOrganizationName("Arcanum Renaissance");app.setApplicationName("WorldED Renaissance"); app.setStyle("Fusion");
    applyExplorerStyle(app);
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOption({"game","Installation or archive to open","path"});
    parser.addOption({"reference-sector","Open sector x,y beside the active zone as a reference","coordinates"});
    parser.addOption({"project","WorldED project to resume","path"});
    parser.addOption({"resume-test","Relaunch a WorldED test copy","playtest.json"});
    parser.addOption({"resource","Initial resource","path"});
    parser.addOption({"world","Open the world map"});
    parser.addOption({"world-sector","Open outdoor sector x,y","coordinates"});
    parser.addOption({QStringList{"demo-scene","demo-house"},"Create a demo scene in memory"});
    parser.addOption({"sector","Local sector to display using installation resources","path"});
    parser.addOption({"snapshot","Capture the full window and exit (validation)","png"});
    parser.process(app);
    Explorer window; window.rememberInstallation=true; window.initialReference=parser.value("reference-sector"); window.initialProject=parser.value("project"); window.initialResource=parser.value("resource"); window.snapshot=parser.value("snapshot"); window.initialSector=parser.value("sector"); window.demoHouse=parser.isSet("demo-house"); window.initialWorld=parser.isSet("world") || parser.isSet("world-sector"); window.initialWorldSector=parser.value("world-sector"); window.show();
    if (!parser.value("game").isEmpty()) window.mount(parser.value("game"));
    else if (!window.initialProject.isEmpty()) {auto project=window.initialProject;window.initialProject.clear();window.openProjectFolder(project);}
    else if (!parser.isSet("resume-test")) {
        QSettings settings;auto last=settings.value("installation/lastPath").toString();
        if(!last.isEmpty() && QFileInfo(last).isDir())window.mount(last);
        else {if(!last.isEmpty())window.statusBar()->showMessage("Last installation is unavailable. Choose File > Installation > Open installation…");if(!window.snapshot.isEmpty())window.capture();}
    }
    else if (!window.snapshot.isEmpty())window.capture();
    if(parser.isSet("resume-test"))try{window.launchPlaytest(fs::u8path(QFileInfo(parser.value("resume-test")).absolutePath().toStdString()));}
        catch(const std::exception& e){QMessageBox::warning(&window,"Cannot start test",utf8(e.what()));}
    return app.exec();
}
#endif
