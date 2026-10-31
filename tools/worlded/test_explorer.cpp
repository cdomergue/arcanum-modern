#define WORLDED_EXPLORER_NO_MAIN
#include "src/explorer.cpp"
#include <QtTest>
#include "test_authoring.hpp"
#include "test_inventory_explorer.inc"

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}
int main(int argc, char** argv) {
    QApplication app(argc,argv); app.setStyle("Fusion"); applyExplorerStyle(app);
    if (argc!=7 && argc!=5 && argc!=6 && argc!=3 && !(argc==4 && (std::string(argv[3])=="--updates" || std::string(argv[3])=="--real-sector" || std::string(argv[3])=="--carpets" || std::string(argv[3])=="--facades" || std::string(argv[3])=="--lights"))) return 2;
    try {
        checkAuthoring();
        Explorer window; window.show(); window.mount(QString::fromUtf8(argv[1]));
        check(QTest::qWaitFor([&]{return window.installation != nullptr || window.failed;},20000), "Indexing timed out");
        check(!window.failed, "Installation failed");
        if(argc==4 && std::string(argv[3])=="--updates"){
            window.resize(1280,800);window.showWorld();window.openWorldSector({1346,1024});check(window.building && window.building->reference,"Update reference not captured");
            for(unsigned n=0;n<30;n++)window.building->ground.set_blocked(n,0,!window.building->ground.blocked(n,0));
            for(unsigned n=0;n<std::min<std::size_t>(12,window.building->objects.size());n++)set_number(window.building->objects[n],"CURRENT_AID",window.building->objects[n].number("CURRENT_AID")^0x40);
            QTemporaryDir project;check(window.saveActive(project.filePath("review-scene")),"Cannot save review fixture");
            bool captured=false;QTimer::singleShot(150,[&]{auto settings=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(settings && settings->windowTitle()=="Renaissance update package","Update settings missing");check(settings->grab().save(QString::fromUtf8(argv[2])+"-settings.png"),"Settings capture failed");settings->accept();
                QTimer::singleShot(200,[&]{auto review=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(review && review->windowTitle()=="Review Renaissance update","Update review missing");auto buttons=review->findChild<QDialogButtonBox*>();check(buttons && !buttons->button(QDialogButtonBox::Save)->isEnabled(),"Unsupported changes did not disable export");check(review->grab().save(QString::fromUtf8(argv[2])),"Review capture failed");auto text=review->findChild<QTextBrowser*>();check(text && text->toPlainText().contains("Unsupported field change CURRENT_AID"),"Detailed unsupported changes missing");text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());QTest::qWait(40);check(review->grab().save(QString::fromUtf8(argv[2])+"-errors.png"),"Error capture failed");captured=true;review->reject();});});
            window.exportUpdatePackage();check(captured,"Update review did not execute");std::cout<<"PASS: update settings and review rendered; unsupported export disabled; complete footer and error rows captured\n";return 0;
        }

        check(!supports_worlded_test(QString::fromUtf8(argv[1])+"/missing-engine"),"Missing engine accepted");
        {QTemporaryDir resources;auto root=fs::u8path(resources.path().toStdString());write_bytes(root/"Maps/PopulationN",{1});
            auto selected=test_resource_path(root,"maps/populationn");check(selected.filename()=="PopulationN","Existing resource spelling lost");write_bytes(selected,{2});check(read_file(root/"Maps/PopulationN")==Bytes{2},"Case-insensitive overwrite failed");
#ifndef Q_OS_WIN
            write_bytes(root/"Maps/populationn",{3});test_resource_path(root,"maps/POPULATIONN");unsigned matches=0;for(auto& e:fs::directory_iterator(root/"Maps"))if(QString::fromStdString(e.path().filename().string()).compare("populationn",Qt::CaseInsensitive)==0)matches++;check(matches==1,"Duplicate resource casing survived");
#endif
        }
        {QGraphicsScene scene;Canvas canvas(&scene);canvas.editable=true;canvas.layer=0;auto item=scene.addRect(0,0,80,80);item->setData(0,42);item->setFlag(QGraphicsItem::ItemIsSelectable);canvas.resize(300,300);canvas.show();int activated=-1;canvas.activateObject=[&](int i){activated=i;};QTest::qWait(30);QTest::mouseDClick(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,canvas.mapFromScene(QPointF(40,40)));check(activated==42,"World object double-click event was not dispatched");}
        {QGraphicsScene scene;Canvas canvas(&scene);canvas.editable=true;canvas.layer=0;canvas.resize(400,240);scene.setSceneRect(0,0,350,180);
            auto item=scene.addRect(20,30,40,40),owner=scene.addRect(220,30,40,40);item->setData(0,0);owner->setData(0,1);item->setData(1,QPoint(10,10));
            item->setFlag(QGraphicsItem::ItemIsSelectable);owner->setFlag(QGraphicsItem::ItemIsSelectable);item->setSelected(true);
            canvas.inventoryItem=[](int i){return i==0;};canvas.inventoryOwner=[](int i){return i==1;};int transfers=0,moves=0;
            canvas.dropInventory=[&](const auto& indices,int target){check(indices==std::vector<int>{0} && target==1,"Shift-drop selected wrong objects");transfers++;};canvas.moveObjects=[&](const auto&,QPoint){moves++;};
            canvas.show();QTest::qWait(30);auto start=canvas.mapFromScene(QPointF(40,50)),end=canvas.mapFromScene(QPointF(240,50));
            QTest::keyPress(&canvas,Qt::Key_Shift);QTest::mousePress(canvas.viewport(),Qt::LeftButton,Qt::ShiftModifier,start);QTest::qWait(30);
            QMouseEvent move(QEvent::MouseMove,QPointF(end),QPointF(canvas.viewport()->mapToGlobal(end)),Qt::NoButton,Qt::LeftButton,Qt::ShiftModifier);QApplication::sendEvent(canvas.viewport(),&move);QTest::qWait(30);
            QTest::mouseRelease(canvas.viewport(),Qt::LeftButton,Qt::ShiftModifier,end);QTest::keyRelease(&canvas,Qt::Key_Shift);
            check(transfers==1 && moves==0 && item->isSelected(),"Held Shift-drag did not transfer preselected item");
            QTest::mousePress(canvas.viewport(),Qt::LeftButton,Qt::ShiftModifier,start);QApplication::sendEvent(canvas.viewport(),&move);QTest::keyClick(&canvas,Qt::Key_Escape);QTest::mouseRelease(canvas.viewport(),Qt::LeftButton,Qt::ShiftModifier,end);
            check(transfers==1 && moves==0,"Escape failed to cancel inventory drag");
            canvas.inventoryOwner=[](int){return false;};QTest::mousePress(canvas.viewport(),Qt::LeftButton,Qt::ShiftModifier,start);QApplication::sendEvent(canvas.viewport(),&move);QTest::mouseRelease(canvas.viewport(),Qt::LeftButton,Qt::ShiftModifier,end);
            check(transfers==1 && moves==0,"Invalid inventory target moved the ground item");canvas.inventoryOwner=[](int i){return i==1;};
            QTest::mousePress(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,start);QApplication::sendEvent(canvas.viewport(),&move);QTest::mouseRelease(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,end);
            check(moves==1 && transfers==1,"Releasing Shift did not restore ordinary movement");
        }
        {QTemporaryDir copyFixture;auto root=fs::u8path(copyFixture.path().toStdString());
            write_bytes(root/"first/Proto/001020 - Wall.pro",{1});write_bytes(root/"first/Proto/unique.pro",{7});
            write_bytes(root/"second/proto/001020 - Wall.pro",{2});write_bytes(root/"second/proto/other.pro",{8});
            copy_test_tree(root/"first",root/"copy");copy_test_tree(root/"second",root/"copy");
            check(read_file(root/"copy/Proto/001020 - Wall.pro")==Bytes{2},"Case-colliding prototype did not use last source");
            check(read_file(root/"copy/Proto/unique.pro")==Bytes{7} && read_file(root/"copy/Proto/other.pro")==Bytes{8},"Directory merge lost unique resources");
            check(std::distance(fs::directory_iterator(root/"copy"),fs::directory_iterator{})==1,"Case aliases created duplicate test directories");
            check(read_file(root/"first/Proto/001020 - Wall.pro")==Bytes{1},"Copy modified source prototype");
        }
        {Bytes data;std::string magic="FacWalk V101  ";data.insert(data.end(),magic.begin(),magic.end());auto word=[&](unsigned v){for(int b=0;b<4;b++)data.push_back(v>>(8*b));};
            for(unsigned v:{5,1,1,3,2,2,0,0,1,2,1,0})word(v);auto f=read_facade(data,263);
            check(f.width==3 && f.height==2 && f.tiles.size()==2,"Sparse facade footprint lost");check(f.tiles.at({0,0})==(0xb0000000u|(1u<<27)|(1u<<26)|(1u<<25)|(7u<<17)|(5u<<11)|1u),"Facade AID encoding incorrect");
            check((f.tiles.at({2,1})&2047)==2,"Facade frame/walkable bits confused");data.pop_back();bool rejected=false;try{read_facade(data,263);}catch(...){rejected=true;}check(rejected,"Truncated facade accepted");}
        QTemporaryDir engineFixtures;auto fixture=engineFixtures.filePath("engine");
        {QFile file(fixture);check(file.open(QIODevice::WriteOnly),"Cannot create engine fixture");file.write("#!/bin/sh\n# --worlded-test .worlded-playtest\n");}
        check(!supports_worlded_test(fixture),"Shell launcher accepted as compatible engine");
        {QFile file(fixture);check(file.open(QIODevice::WriteOnly),"Cannot reset engine fixture");
#ifdef Q_OS_WIN
            file.write("MZ00");
#else
            file.write("\x7f" "ELF",4);
#endif
            constexpr char capability[]="--worlded-test\0.worlded-playtest";
            file.write(QByteArray(65526,'x'));file.write(capability,sizeof(capability)-1);}
        check(supports_worlded_test(fixture),"Engine capability split across chunks was missed");
        if(argc==5 && std::string(argv[3])=="--authoring") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");window.editLayer->setCurrentIndex(0);
            int chest=-1;for(int i=0;i<int(window.building->objects.size());i++)if(window.building->objects[i].type==2){chest=i;break;}check(chest>=0,"No test chest");
            window.scene->clearSelection();window.objectItems[chest]->setSelected(true);auto before=encode_instance(window.building->objects[chest]);
            QTimer::singleShot(100,[&]{auto d=dynamic_cast<ActorEditor*>(QApplication::activeModalWidget());check(d!=nullptr,"Double-click did not open object editor");check(d->windowTitle().contains("object"),"Wrong editor title");auto field=d->findChild<QLineEdit*>("CONTAINER_LOCK_DIFFICULTY");check(field!=nullptr,"Container properties missing");field->setText("77");for(int i=0;i<d->tabs->count();i++){d->tabs->setCurrentIndex(i);QTest::qWait(30);d->grab().save(QString::fromUtf8(argv[2])+QString("-object-%1.png").arg(i));}auto box=d->findChild<QDialogButtonBox*>();box->button(QDialogButtonBox::Save)->click();});window.canvas->activateObject(chest);
            check(window.building->objects[chest].number("CONTAINER_LOCK_DIFFICULTY")==77,"Object edit not applied");window.history(false);check(encode_instance(window.building->objects[chest])==before,"Object undo lost fields");window.history(true);check(window.building->objects[chest].number("CONTAINER_LOCK_DIFFICULTY")==77,"Object redo failed");
            auto press=[](QDialog* d,const QString& label){for(auto b:d->findChildren<QPushButton*>())if(b->text()==label){b->click();return;}throw std::runtime_error("Missing authoring button");};
            QTimer::singleShot(100,[&]{auto d=dynamic_cast<ScriptDialog*>(QApplication::activeModalWidget());check(d && d->editor,"Modern script editor missing");d->editor->setPlainText("let sum=0;\nfor(let i=0;i<5;i++){\n    if(i==2){continue;}\n    sum+=i;\n}\nglobal(1900)=sum;\nreturn;\n");press(d,"Validate");d->grab().save(QString::fromUtf8(argv[2])+"-script.png");press(d,"Apply to project");check(window.building->content.count("scr/49000_authoring.scr"),"Script apply failed");auto attachment=window.building->objects[chest].field("SCRIPTS_IDX");check(attachment && attachment->present,"Script attachment displayed but not serializable");d->reject();});window.editContent("scr/49000_authoring.scr",true);
            auto content=window.building->content;window.history(false);check(!window.building->content.count("scr/49000_authoring.scr"),"Script undo failed");window.history(true);check(window.building->content==content,"Script redo failed");
            QTimer::singleShot(100,[&]{auto d=dynamic_cast<ScriptDialog*>(QApplication::activeModalWidget());check(d!=nullptr,"Dialogue editor missing");DialogueEditor* editor=nullptr;for(auto widget:d->findChildren<QWidget*>())if(auto c=dynamic_cast<DialogueEditor*>(widget))editor=c;check(editor!=nullptr,"Dialogue controls missing");editor->text->setPlainText("Welcome to the workshop.");press(d,"Validate");d->grab().save(QString::fromUtf8(argv[2])+"-dialogue.png");press(d,"Apply to project");check(window.building->content.count("dlg/49001_workshop.dlg") && window.building->content.count("scr/49001_workshop.scr"),"Dialogue companion script missing");d->reject();});window.editContent("dlg/49001_workshop.dlg",true);
            auto saved=fs::u8path(std::string(argv[2])+"-project");export_building(*window.building,saved);auto reopened=load_project(saved);check(reopened.content==window.building->content,"Project content did not round trip");auto altered=reopened;altered.content.clear();check(scene_fingerprint(reopened)!=scene_fingerprint(altered),"Content excluded from dirty fingerprint");window.grab().save(QString::fromUtf8(argv[2])+"-window.png");std::cout<<"PASS: object editor, structured scripts, dialogues, undo and project persistence\n";return 0;
        }
        if(argc==5 && std::string(argv[3])=="--actor-editor") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");window.editLayer->setCurrentIndex(3);
            int index=-1;for(int i=0;i<int(window.building->objects.size());i++)if(window.building->objects[i].type==16){index=i;break;}check(index>=0,"No project actor");
            window.objectItems[index]->setSelected(true);check(window.configureActor->isVisible(),"Actor configuration button missing");
            QTimer::singleShot(150,[&]{auto dialog=dynamic_cast<ActorEditor*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Actor editor missing");
                for(int tab=0;tab<dialog->tabs->count();tab++){dialog->tabs->setCurrentIndex(tab);QTest::qWait(50);dialog->grab().save(QString::fromUtf8(argv[2])+QString("-actor-%1.png").arg(tab));}dialog->reject();});window.editActor();
            checkInventoryExplorer(window);
            std::cout<<"PASS: actor configuration pages rendered; inventory ownership, duplication, deletion, movement and undo/redo\n";return 0;
        }
        if(argc==5 && std::string(argv[3])=="--association-visual") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing() && window.changeProjectInstallation->isEnabled(),"Project association command unavailable");
            QTest::qWait(200);check(window.grab().save(QString::fromUtf8(argv[2])+"-window.png"),"Cannot capture project window");
            auto menu=window.menuBar()->actions().front()->menu();menu->popup(window.menuBar()->mapToGlobal(QPoint(0,window.menuBar()->height())));QTest::qWait(100);check(menu->grab().save(QString::fromUtf8(argv[2])+"-menu.png"),"Cannot capture file menu");menu->close();return 0;
        }
        if(argc==5 && std::string(argv[3])=="--characters") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");window.editLayer->setCurrentIndex(3);check(window.objectPalette->isEnabled(),"Character palette disabled");
            int person=-1,creature=-1;for(unsigned i=0;i<window.paletteObjects.size() && (person<0 || creature<0);i++){const auto& obj=window.paletteObjects[i];if(obj.type!=16)continue;auto art=unsigned(obj.number("CURRENT_AID"));if((art>>28)!=2 && (art>>28)!=12 && (art>>28)!=13)continue;try{window.assets->sprite(art);if((art>>28)==2 && person<0)person=i;if((art>>28)!=2 && creature<0)creature=i;}catch(const std::exception&) {}}
            check(person>=0 && creature>=0,"Real character or creature sprite unavailable");auto count=window.building->objects.size();window.canvas->resetTransform();window.canvas->scale(.6,.6);window.canvas->centerOn(Canvas::project({32,32}));
            auto place=[&](int index,QPoint tile){QMimeData mime;mime.setData(objectMime,QByteArray::number(index));auto pos=window.canvas->mapFromScene(Canvas::project(tile));QDragEnterEvent enter(pos,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(window.canvas->viewport(),&enter);check(enter.isAccepted(),"Real character drag rejected");QDragMoveEvent move(pos,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(window.canvas->viewport(),&move);QDropEvent drop(pos,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(window.canvas->viewport(),&drop);check(drop.isAccepted(),"Real character drop rejected");};
            place(person,{30,30});place(creature,{32,30});check(window.building->objects.size()==count+2,"Real characters missing from draft");
            int projectPerson=-1,projectCreature=-1;for(int i=0;i<int(window.activeContext.actors.size());i++){const auto& a=window.activeContext.actors[i];if(!a.campaign && objectKey(a.object)==objectKey(window.building->objects[count]))projectPerson=i;if(!a.campaign && objectKey(a.object)==objectKey(window.building->objects[count+1]))projectCreature=i;}
            check(projectPerson>=0 && projectCreature>=0,"Project characters missing from list");window.peopleList->setCurrentRow(projectPerson);check(window.selectedIds()==std::set<std::string>{objectKey(window.building->objects[count])},"Character list did not select the corresponding project character");window.peopleList->setCurrentRow(projectCreature);check(window.selectedIds()==std::set<std::string>{objectKey(window.building->objects[count+1])},"Character list kept a stale selection");
            auto location=window.building->objects.back().number("LOCATION");auto from=window.canvas->mapFromScene(window.objectItems.back()->sceneBoundingRect().center());auto to=from+(window.canvas->mapFromScene(Canvas::project({33,30}))-window.canvas->mapFromScene(Canvas::project({32,30})));
            QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from);QTest::qWait(100);QTest::mouseMove(window.canvas->viewport(),to,30);QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
            check(window.building->objects.back().number("LOCATION")!=location,"Dragging a project character did not move it");window.history(false);check(window.building->objects.back().number("LOCATION")==location,"Undo character movement failed");
            QTest::qWait(250);window.grab().save(QString::fromUtf8(argv[2])+"-characters.png");
            auto obj=window.building->objects.back();QTemporaryDir temp;auto path=fs::u8path(temp.path().toStdString())/"project";export_building(*window.building,path);auto reopened=load_project(path);check(std::any_of(reopened.objects.begin(),reopened.objects.end(),[&](const auto& o){return o.id==obj.id && o.type==16;}),"Real character did not survive export and reload");
            check(!window.selectedIds().empty() && window.deleteHouse->isEnabled(),"New character not selected for editing");window.deleteSelection();check(window.building->objects.size()==count+1,"Deleting placed character failed");window.history(false);check(window.building->objects.size()==count+2,"Undo character deletion failed");
            window.history(false);check(window.building->objects.size()==count+1,"Character undo failed");window.history(true);check(window.building->objects.size()==count+2,"Character redo failed");
            auto personId=window.paletteObjects[person].prototype_id,creatureId=window.paletteObjects[creature].prototype_id;window.openWorldSector({970,1025});int campaign=-1;for(int i=0;i<int(window.activeContext.actors.size());i++)if(window.activeContext.actors[i].campaign){campaign=i;break;}
            check(campaign>=0,"Campaign actor fixture missing");auto beforeCampaign=window.building->sector().encode();window.peopleList->setCurrentRow(campaign);check(window.selectedIds().empty() && !window.deleteHouse->isEnabled(),"Campaign character became editable");window.deleteSelection();check(window.building->sector().encode()==beforeCampaign,"Campaign inspection modified the sector");
            std::cout<<"PASS: real NPC #"<<personId<<" and creature #"<<creatureId<<", drag, export, undo and read-only campaign\n";return 0;
        }
        if(argc==5 && std::string(argv[3])=="--script-attachments") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");auto before=window.building->sector().encode();window.editLayer->setCurrentIndex(4);
            int found=-1;for(int i=0;i<int(window.activeContext.scripts.size());i++)if(window.activeContext.scripts[i].id==30285 && window.activeContext.scripts[i].origin==3)found=i;
            check(found>=0,"Expected sector script missing");window.scriptsList->setCurrentRow(found);check(window.scriptDetails->text().contains("Sector-wide") && !window.locateScript->isEnabled() && window.removeScript->isEnabled(),"Sector script source and controls are unclear");
            window.grab().save(QString::fromUtf8(argv[2])+"-scripts.png");QTest::mouseClick(window.removeScript,Qt::LeftButton);check(window.building->ground.scripts().empty(),"Sector attachment not removed");check(window.zoneState->text().contains("Script attachments"),"Script modification missing from dirty summary");
            QTemporaryDir output;auto path=fs::u8path(output.path().toStdString())/"project";export_building(*window.building,path);auto reopened=load_project(path);check(reopened.ground.scripts().empty(),"Saved project kept removed sector script");
            window.history(false);check(window.building->sector().encode()==before,"Undo did not restore sector script exactly");
            auto source=read_world_sector(*window.installation,window.building->map_name,1346,1024);bool sourceScript=false;for(auto s:source.sector.scripts())sourceScript|=s.number==30285;
            std::cout<<"PASS: sector script 30285, removal, save, undo; original terrain has script: "<<sourceScript<<std::endl;return 0;
        }
        if(argc==5 && std::string(argv[3])=="--catalog") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");auto count=window.paletteObjects.size();unsigned walls=0;std::cout<<"Catalogue loaded: "<<count<<" entries"<<std::endl;
            for(const auto& object:window.building->objects)if(object.type==0){window.revealInPalette(window.assets->effective(object));walls++;check(window.paletteObjects.size()==count,"Project wall was appended instead of found in global catalogue");}
            check(walls>0,"No walls in project");QTest::qWait(300);window.grab().save(QString::fromUtf8(argv[2])+"-catalog.png");std::cout<<"PASS: "<<walls<<" project walls already in "<<count<<" global entries\n";return 0;
        }
        if(argc==5 && std::string(argv[3])=="--layers") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");
            window.openSectorBeside({1230,980});window.editLayer->setCurrentIndex(2);QTest::qWait(200);window.fitReference();
            check(window.grab().save(QString::fromUtf8(argv[2])+"-roofs.png"),"Cannot capture roofs");
            std::cout<<"Reference actors "<<window.referenceContext.actors.size()<<" scripts "<<window.referenceContext.scripts.size()<<std::endl;
            auto water=*window.building;water.objects.clear();for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)water.ground.set_tile(x,y,0x4101c0);
            for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)water.ground.set_roof(x,y,0xffffffff);
            window.commitEdit("Essai de raccord terre",{}, {},&water.ground);window.editLayer->setCurrentIndex(1);window.sideAction->setChecked(false);QTest::qWait(100);
            int dirt=-1,indoor=-1;for(int i=0;i<window.surfacePalette->count();i++){auto id=window.surfacePalette->item(i)->data(Qt::UserRole).toUInt();if(TerrainBrush::material(id,true)==0 && dirt<0)dirt=i;if(TerrainBrush::material(id,true)>=200 && indoor<0)indoor=i;}
            check(dirt>=0 && indoor>=0,"Outdoor dirt or indoor floor missing from palette");
            window.surfacePalette->setCurrentRow(indoor);check(!window.terrainTransitions->isEnabled() && !window.terrainTransitions->isChecked() && window.surfaceMaterialStatus->text().contains("Indoor"),"Indoor material silently advertised automatic transitions");
            window.surfacePalette->setCurrentRow(dirt);check(window.terrainTransitions->isEnabled() && window.terrainTransitions->isChecked(),"Outdoor transition preference was not restored");
            window.paintSurface->setChecked(true);window.canvas->resetTransform();window.canvas->scale(.6,.6);window.canvas->centerOn(Canvas::project({32,32}));
            auto point=window.canvas->mapFromScene(Canvas::project({32,32}));QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(80);QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,point);
            unsigned mixed=0;for(int y=29;y<=35;y++)for(int x=29;x<=35;x++){auto c=TerrainBrush::corners(window.building->ground.tile(x,y));if(std::set<int>(c.begin(),c.end()).size()>1)mixed++;}
            check(mixed>0,"Painting outdoor dirt on grass did not create blended edges");check(window.grab().save(QString::fromUtf8(argv[2])+"-dirt.png"),"Cannot capture dirt transitions");
            window.history(false);check(window.building->ground.encode()==water.ground.encode(),"Undo did not restore the grass after painting");window.paintSurface->setChecked(false);
            TerrainBrush brush(*window.installation);std::set<std::pair<int,int>> painted;for(int y=28;y<36;y++)for(int x=28;x<36;x++)painted.insert({x,y});
            auto terrain=brush.paint(water.ground,painted,(12u<<22)|(12u<<16)|0xf1c0,[&](unsigned id){return window.assets->available(id);});
            window.commitEdit("Essai du pinceau eau",{}, {},&terrain);window.editLayer->setCurrentIndex(1);window.sideAction->setChecked(false);QTest::qWait(100);
            window.canvas->resetTransform();window.canvas->scale(.6,.6);window.canvas->centerOn(Canvas::project({32,32}));
            check(window.grab().save(QString::fromUtf8(argv[2])+"-water.png"),"Cannot capture water brush");
            std::size_t best=0;QPoint actorSector;for(const auto& [id,mobiles]:window.campaignCache.at(window.building->map_name)){std::size_t count=0;for(const auto& object:mobiles)if(object.type==16)count++;if(count>best && (id&0x3ffffff)<2000 && (id>>26)<2000){best=count;actorSector={int(id&0x3ffffff),int(id>>26)};}}
            window.openWorldSector(actorSector);window.editLayer->setCurrentIndex(3);window.fitObjects();QTest::qWait(100);
            std::cout<<"Selected sector "<<actorSector.x()<<","<<actorSector.y()<<" expected "<<best<<" actors; "<<window.contextStatus->text().toStdString()<<std::endl;
            check(!window.activeContext.actors.empty(),"Campaign characters missing");window.inspectContextObject(false,0);
            unsigned resolvedActors=0;for(const auto& actor:window.activeContext.actors)try{window.assets->sprite(unsigned(actor.object.number("CURRENT_AID",actor.object.number("AID"))));resolvedActors++;}catch(const std::exception&){}
            check(resolvedActors>0,"No campaign character sprite resolved");
            check(window.grab().save(QString::fromUtf8(argv[2])+"-actors.png"),"Cannot capture characters");
            window.editLayer->setCurrentIndex(4);QTest::qWait(100);window.grab().save(QString::fromUtf8(argv[2])+"-scripts.png");
            check(!window.activeContext.scripts.empty(),"Campaign scripts missing");auto sourceSector=window.building->sector().encode();
            QTimer::singleShot(200,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Script editor did not open");ScriptEditor* edit=nullptr;for(auto field:dialog->findChildren<QPlainTextEdit*>())if(auto candidate=dynamic_cast<ScriptEditor*>(field))edit=candidate;check(edit && edit->toPlainText().contains("THEN"),"Native instructions not readable");auto tab=dialog->findChild<QTabWidget*>();check(tab!=nullptr,"Script tabs missing");dialog->grab().save(QString::fromUtf8(argv[2])+"-script-editor.png");tab->setCurrentIndex(1);QTest::qWait(100);dialog->grab().save(QString::fromUtf8(argv[2])+"-script-reader.png");dialog->reject();});window.showScript(window.activeContext.scripts.front().id);
            check(window.building->sector().encode()==sourceSector,"Consulting a script changed the sector");
            std::cout<<"Resolved character graphics: "<<resolvedActors<<"/"<<window.activeContext.actors.size()<<std::endl;
            std::cout<<"PASS: roofs, automatic water shores, actors "<<window.activeContext.actors.size()<<", scripts "<<window.activeContext.scripts.size()<<" in sector "<<actorSector.x()<<","<<actorSector.y()<<std::endl;return 0;
        }
        if(argc==5 && std::string(argv[3])=="--test-dialog") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");
            QTimer::singleShot(250,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Test dialog missing");
                check(dialog->grab().save(QString::fromUtf8(argv[2])),"Cannot capture test dialog");dialog->reject();});
            window.configurePlaytest();std::cout<<"PASS: test configuration opens and cancels without creating a copy\n";return 0;
        }
        if(argc==7 && (std::string(argv[3])=="--project-playtest" || std::string(argv[3])=="--inventory-playtest")) {
            window.openProjectFolder(QString::fromUtf8(argv[6]));check(window.editing(),"Project not editable");
            bool inventoryCheck=std::string(argv[3])=="--inventory-playtest";
            if(inventoryCheck){int npc=-1;for(int i=0;i<int(window.paletteObjects.size());i++)if(window.paletteObjects[i].type==16 && window.assets->effective(window.paletteObjects[i]).number("CRITTER_INVENTORY_SOURCE")){npc=i;break;}
                check(npc>=0,"No inventory-bearing NPC prototype");auto object=clone_instance(window.paletteObjects[npc]);set_number(object,"LOCATION",30ull|(30ull<<32));
                for(const char* field:{"CRITTER_TELEPORT_DEST","NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"})set_number(object,field,object.number("LOCATION"));
                auto path=window.assets->prototypePath(object.prototype_id);window.building->prototypes[path]=window.installation->read(path);window.building->objects.push_back(object);
                std::cerr<<"Inventory fixture NPC "<<object.prototype_id<<std::endl;}
            check(supports_worlded_test(QString::fromUtf8(argv[4])),"Test engine lacks support");
            QTimer::singleShot(250,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Test dialog missing");check(dialog->grab().save(QString::fromUtf8(argv[2])+"-dialog.png"),"Cannot capture test dialog");dialog->reject();});
            window.configurePlaytest();
            auto result=create_playtest(*window.building,fs::u8path(argv[1]),fs::u8path(argv[5]),fs::u8path(argv[4]),fs::u8path(argv[2]),"fr");
            if(inventoryCheck){auto log=read_file(result.root/"inventory-generation.log");check(std::string(log.begin(),log.end()).find("Prepared NPC ")!=std::string::npos,"No NPC inventory was generated");}
            QProcess process;configure_test_process(process,result.root);
            check(process.arguments()==QStringList({"--worlded-test",QFileInfo(argv[5]).baseName(),QString::number(window.building->sector_id&0x3ffffff),QString::number(window.building->sector_id>>26)}),"Incorrect test launch arguments");
            check(process.workingDirectory()==utf8(result.game.u8string()),"Test cwd must be the copied game");
            auto restored=load_project(result.project);auto contents=[](const Building& b){std::map<std::string,Bytes> result;for(auto& o:b.objects)result[o.id]=encode_instance(o);return result;};check(contents(restored)==contents(*window.building) && restored.location_names==window.building->location_names,"Test differs from user project");
            for(const auto& [name,data]:window.building->content)if(name.rfind("scr/",0)==0)check(read_file(result.game/"data"/name)==data,"New script absent from initial engine index");
            check(read_file(result.game/"modules/Arcanum/Save"/fs::u8path(QFileInfo(argv[5]).baseName().toStdString()+".tfaf"))==read_file(fs::u8path(argv[5]).replace_extension(".tfaf")),"Copied save differs");
            window.grab().save(utf8((result.root/"editor.png").u8string()));
            std::cout<<"PASS: actual project preserved, compatible engine, independent Elias save, test arguments and working directory\n";return 0;
        }
        if(argc==5 && std::string(argv[3])=="--workspace") {
            window.openProjectFolder(QString::fromUtf8(argv[4]));check(window.editing(),"Project not editable");auto key=window.activeDraft;
            window.openSectorBeside({1230,980});QTest::qWait(150);check(window.activeDraft==key && window.referenceDraft!=key,"Side-by-side changed target zone");
            window.objectsList->setCurrentRow(int(window.building->objects.size())-1);window.duplicateSelection();
            check(window.zoneState->text().contains("UNSAVED CHANGES"),"Dirty state missing");
            window.canvas->scene()->clearSelection();window.showBuilding(true);window.fitReference();QTest::qWait(150);
            check(window.grab().save(QString::fromUtf8(argv[2])),"Cannot save workspace capture");
            QTimer::singleShot(100,[&]{auto menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());check(menu!=nullptr,"Native context menu did not open");
                menu->setActiveAction(window.copyAction);menu->grab().save(QString::fromUtf8(argv[2])+"-menu.png");menu->close();});
            auto point=window.canvas->viewport()->rect().center();QContextMenuEvent context(QContextMenuEvent::Mouse,point,window.canvas->viewport()->mapToGlobal(point));QApplication::sendEvent(window.canvas->viewport(),&context);
            std::cout<<"PASS: real project, separate reference sector, visible dirty state and context menu\n";return 0;
        }
        if(argc==6 && std::string(argv[3])=="--playtest") {
            window.showWorld();window.openWorldSector({1346,1024});check(window.editing(),"Target sector unavailable");
            auto original=window.building->sector().encode();BuildingPlan plan;plan.y=18;
            window.addRoom(plan,true);auto built=window.building->sector().encode();
            window.history(false);check(window.building->sector().encode()==original,"Room undo changed sector");
            window.history(true);check(window.building->sector().encode()==built,"Room redo changed instances");
            auto named=*window.building;named.location_names={{"fr","Maison des bois"},{"en","Woodland House"}};
            window.commitEdit("Name location",named.objects,{},nullptr,&named);
            check(window.missingSprites==0,"House contains unresolved sprites");
            auto result=create_playtest(*window.building,fs::u8path(argv[1]),fs::u8path(argv[5]),fs::u8path(argv[4]),fs::u8path(argv[2]),"fr");
            auto reopened=load_project(result.project);check(reopened.objects.size()==window.building->objects.size() && reopened.location_names==named.location_names,"Project reload lost objects or names");
            window.openProjectFolder(utf8(result.project.string()));QTest::qWait(200);
            check(window.grab().save(utf8((result.root/"editor.png").string())),"Cannot save house view");
            std::cout<<"PASS: insert/undo/redo, localized names, independent save copy, mobile merge, project reopen\n"<<result.game<<std::endl;return 0;
        }
        if(argc>=4 && std::string(argv[3])=="--lights") {
            window.showWorld();window.openWorldSector({1346,1024});auto before=window.building->ground;Bytes light(48,0);light[31]=0x90;light[32]=210;light[33]=30;light[34]=90;
            QTimer::singleShot(100,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Light editor missing");
                dialog->findChild<QSpinBox*>("lightX")->setValue(30);dialog->findChild<QSpinBox*>("lightY")->setValue(30);
                dialog->grab().save(QString::fromUtf8(argv[2])+"-dialog.png");dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
            check(window.editLight(light),"Light edit cancelled");check(word(light,8)==1346*64+30 && word(light,12)==1024*64+30,"Light coordinates not global");
            auto lights=before.lights();lights.push_back(light);auto ground=before;ground.set_lights(lights);window.commitEdit("Add light",window.building->objects,{},&ground);
            window.history(false);check(window.building->ground.encode()==before.encode(),"Light undo failed");window.history(true);check(window.building->ground.lights()==lights,"Light redo failed");
            window.previewLights->setChecked(true);window.canvas->resetTransform();window.canvas->scale(1,1);window.canvas->centerOn(Canvas::project({30,30}));QTest::qWait(100);window.grab().save(QString::fromUtf8(argv[2]));
            QTemporaryDir temporary;auto path=argc==5?fs::u8path(argv[4]):fs::u8path(temporary.path().toStdString())/"project";
            export_building(*window.building,path);check(load_project(path).ground.lights()==lights,"Saved light lost");
            std::cout<<"PASS: native light edit, undo/redo, preview, save/reopen"<<std::endl;return 0;
        }
        if(argc==4 && std::string(argv[3])=="--facades") {
            window.showWorld();window.openWorldSector({970,1027});window.libraryTabs->setCurrentWidget(window.facadePanel);window.populateFacades();
            check(window.facadePalette->count()>100,"Facade library incomplete");int rug=-1;for(int i=0;i<window.facadePalette->count();i++)if(window.facadePalette->item(i)->text().compare("rug4-flip",Qt::CaseInsensitive)==0)rug=i;
            check(rug>=0,"Native flipped rug missing");window.facadeSearch->setText("rug");window.facadePalette->setCurrentRow(rug);window.facadePalette->scrollToItem(window.facadePalette->item(rug));window.loadFacadeIcons();
            auto before=window.building->ground.encode();window.placeFacade();auto footprint=window.copied;check(footprint.tiles.size()==15,"Rug footprint differs from native walkmask");
            check(!window.pasteAt({63,63}),"Out-of-bounds facade partially placed");check(window.building->ground.encode()==before,"Rejected facade changed ground");
            check(window.pasteAt({20,20}),"Facade placement failed");window.canvas->cancelInteraction();
            for(auto [cell,aid]:footprint.tiles)check(window.building->ground.tile(20+cell.first,20+cell.second)==aid,"Facade frame or walkability lost");
            auto placed=window.building->ground.encode();window.history(false);check(window.building->ground.encode()==before,"Facade undo failed");window.history(true);check(window.building->ground.encode()==placed,"Facade redo failed");
            window.libraryTabs->setCurrentWidget(window.facadePanel);window.canvas->resetTransform();window.canvas->scale(1.5,1.5);window.canvas->centerOn(Canvas::project({22,22}));QTest::qWait(150);window.grab().save(QString::fromUtf8(argv[2]));
            {QTemporaryDir dir;auto path=fs::u8path(dir.path().toStdString())/"project";export_building(*window.building,path);auto reopened=load_project(path);for(auto [cell,aid]:footprint.tiles)check(reopened.ground.tile(20+cell.first,20+cell.second)==aid,"Facade lost on save");}
            std::cout<<"PASS: native facade library, 15-piece flipped rug, boundary rejection, undo/redo and save/reopen"<<std::endl;return 0;
        }
        if(argc==4 && std::string(argv[3])=="--carpets") {
            window.showWorld();window.openWorldSector({970,1027});check(window.editing(),"Reported carpet sector did not open");
            unsigned carpets=0,facades=0;auto original=window.building->ground.encode();
            for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++) {
                auto aid=window.building->ground.tile(x,y);if((aid>>28)!=11)continue;
                auto sprite=window.assets->sprite(aid);facades++;
                if(x>=3 && x<=7 && y>=34 && y<=36){check(sprite.path=="art/facade/rug4-flip.art","Reported carpet resolved to wrong asset");carpets++;}
            }
            check(carpets==15,"Reported carpet pieces missing");check(window.building->ground.encode()==original,"Preview modified source tiles");
            window.canvas->resetTransform();window.canvas->scale(1.5,1.5);window.canvas->centerOn(Canvas::project({5,35}));
            QTest::qWait(150);check(window.grab().save(QString::fromUtf8(argv[2])),"Cannot capture carpet sector");
            std::cout<<"PASS: sector 970,1027; "<<carpets<<" rug pieces, "<<facades<<" facade tiles rendered\n";return 0;
        }
        if(argc==4) {
            // Opt-in regression on the user's reported exterior sector. Reads
            // installed resources only; no game assets are included in tests.
            window.showWorld(); window.openWorldSector({1230,980});
            check(!window.failed && window.missingSprites==0,"Real sector failed to render");
            int door=-1;
            for(unsigned i=0;i<window.effectiveObjects.size();++i) {
                const auto& object=window.effectiveObjects[i];
                if(object.type!=1) continue;
                auto location=object.number("LOCATION");
                if((location&63)==24 && ((location>>32)&63)==34) door=int(i);
            }
            check(door>=0,"Reported entrance door not found");
            auto sprite=window.assets->sprite(unsigned(window.effectiveObjects[door].number("CURRENT_AID")));
            check(sprite.path=="art/portal/stve3au0.art","Real entrance door resolves to wrong ART");
            window.canvas->resetTransform(); window.canvas->scale(1,1);
            window.canvas->centerOn(window.objectItems[door]);
            window.paletteSearch->setText("filter hiding door");
            window.paletteCategory->setCurrentIndex(window.paletteCategory->findData(3));
            auto target=window.canvas->mapFromScene(window.objectItems[door]->sceneBoundingRect().center());
            QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,target); QTest::qWait(120);
            QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,target);
            check(window.objectItems[door]->isSelected(),"Click on real door selected another object");
            auto item=window.objectPalette->currentItem(); check(item && !item->isHidden(),"Real door not revealed in library");
            auto templateObject=window.paletteObjects.at(item->data(Qt::UserRole).toInt());
            check(templateObject.prototype_id==2028 && window.assets->sprite(unsigned(templateObject.number("CURRENT_AID"))).path==sprite.path,"Library selected wrong real door variant");
            QTest::qWait(120); check(window.grab().save(QString::fromUtf8(argv[2])),"Cannot save real-sector capture");
            std::cout<<"PASS: sector 1230,980, correct door ART, held world click and library reveal\n";
            return 0;
        }
        // Asymmetric pixels expose missing mirroring; noncentral anchors expose
        // wrong adjustment order. Walls use one stored direction, portals eight.
        for (unsigned type : {1u, 3u}) for (unsigned direction=0; direction<8; ++direction) {
            bool mirrored = direction/2 == 1 || direction/2 == 3;
            unsigned aid = (type<<28) | (direction<<11) | (mirrored ? 1u : 0u) | (type==3 ? 0x400u : 0u);
            auto sprite = window.assets->sprite(aid);
            if(type==3) {
                check(sprite.path=="art/portal/door.art","Door AID resolves to a window");
                check(window.assets->sprite(aid & ~0x400u).path=="art/portal/window.art","Window AID resolves to a door");
            }
            auto pixels = sprite.pixels.toImage();
            int anchorX = -2, anchorY = 7;
            if (direction<2 || direction>5) { anchorX-=40; anchorY+=20; }
            if (mirrored) anchorX=4-anchorX-2;
            check(sprite.hotspot==QPoint(anchorX,anchorY), "Wall/portal directional anchor differs from CE");
            check(pixels.pixelColor(mirrored ? 3 : 0,0)==QColor(1,2,3), "Wall/portal pixels not mirrored");
            check(pixels.pixelColor(mirrored ? 2 : 1,0).alpha()==0, "Mirrored transparency displaced");
        }
        check(window.assets->sprite(0x30080200).path=="art/portal/paned0.art","Damaged window resolves to the wrong asset");
        for(unsigned high:{0u,1u})for(unsigned walkable:{0u,1u}) {
            unsigned aid=0xb0000000u|(high<<27)|(7u<<17)|(5u<<11)|(715u<<1)|walkable;
            auto sprite=window.assets->sprite(aid);
            check(sprite.path==(high?"art/facade/rug-high-test.art":"art/facade/rug-test.art"),"Facade number decoded incorrectly");
            auto pixels=sprite.pixels.toImage();check(pixels.pixelColor(0,0)==QColor(7,14,21),"Facade frame, rotation, palette or mirror decoded incorrectly");
            check(pixels.pixelColor(1,0).alpha()==0,"Facade transparency changed");
        }
        window.search->setFocus(); QTest::keyClicks(window.search,"art/sprite.art");
        window.category->setCurrentIndex(2);
        check(window.filter.rowCount()==1,"Search and category did not combine");
        auto index=window.filter.index(0,0);
        QTest::mouseClick(window.resources->viewport(),Qt::LeftButton,Qt::NoModifier,window.resources->visualRect(index).center());
        check(window.art && window.art->rotations()==8,"Sprite selection did not load ART");
        window.rotation->setFocus(); QTest::keyClick(window.rotation,Qt::Key_Down);
        check(window.rotation->currentIndex()==1,"Direction control failed");
        window.palette->setFocus(); QTest::keyClick(window.palette,Qt::Key_Down);
        check(window.palette->currentData().toInt()==1,"Palette control failed");
        QSignalSpy changes(window.frame,&QSpinBox::valueChanged);
        QTest::mouseClick(window.play,Qt::LeftButton);
        check(QTest::qWaitFor([&]{return changes.count()>=2;},3000),"Animation did not advance");
        QTest::mouseClick(window.play,Qt::LeftButton);
        check(!window.animation.isActive(),"Animation did not pause");
        double before=window.canvas->transform().m11();
        QPointF point=window.canvas->viewport()->rect().center();
        QWheelEvent wheel(point,window.canvas->viewport()->mapToGlobal(point.toPoint()),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(window.canvas->viewport(),&wheel);
        check(window.canvas->transform().m11()>before,"Wheel zoom failed");
        window.chooseResource("maps/test/0.sec");
        check(!window.failed && window.objects.size()==1,"Sector objects did not load");
        auto originalSector=Sector(window.installation->read("maps/test/0.sec"));
        const auto stream=originalSector.encode(); const auto info=originalSector.info();
        check(window.assets->terrainBlocked((2u<<22)|256|128),"Material /b flag ignored");
        check(window.assets->terrainBlocked((3u<<22)|256|128),"Material /f flag ignored");
        check(!window.assets->terrainBlocked((1u<<22)|256|128),"Grass marked as blocked");
        check(window.assets->terrainBlocked(0xb0000000) && !window.assets->terrainBlocked(0xb0000001),"Facade walkability bit inverted");
        {
            auto changed=originalSector;bool before=changed.blocked(31,0);
            changed.set_blocked(31,0,!before);check(Sector(changed.encode()).blocked(31,0)!=before,"Blocking bit did not persist");
            changed.set_blocked(31,0,before);check(changed.encode()==stream,"Blocking round trip changed unrelated bytes");
            auto settings=changed.settings();changed.set_settings({-42,7,100,4100});auto reread=Sector(changed.encode()).settings();
            check(reread.aptitude==-42 && reread.light==7 && reread.music==100 && reread.ambient==4100,"Sector settings did not persist");
            changed.set_settings(settings);check(changed.encode()==stream,"Sector settings changed sound flags or other bytes");
            for(unsigned version=0;version<4;version++) {
                Bytes bytes(4+4096*4,0);
                auto append=[&](unsigned value){for(int i=0;i<4;i++)bytes.push_back((value>>(8*i))&255);};
                append(1);append(0xaa0000+version);
                if(version>=1)append(0);
                if(version>=2)for(int i=0;i<3;i++)append(0);
                if(version>=3)for(int i=0;i<6;i++)append(0);
                bytes.insert(bytes.end(),stream.begin()+info.object_offset,stream.end());
                Sector old(bytes);old.set_blocked(63,63,true);old.set_roof(1,1,0xa0000000);
                auto encoded=old.encode();Sector roundtrip(encoded);
                check(roundtrip.info().version==0xaa0004 && roundtrip.blocked(63,63),"Old SEC blocking upgrade failed");
                check(roundtrip.roof(1,1)==0xa0000000,"Roof insertion lost upgraded blocking");
                check(Bytes(encoded.begin()+roundtrip.info().object_offset,encoded.end())==Bytes(stream.begin()+info.object_offset,stream.end()),"SEC upgrade changed object bytes");
                roundtrip.set_blocked(63,63,false);check(!Sector(roundtrip.encode()).blocked(63,63),"Last blocking bit did not clear");
                Sector attachments(bytes);attachments.set_script({4095,0x80000001u,0xff807f01u,42});attachments.set_script({7,3,4,43});attachments.set_script({UINT32_MAX,5,6,42});
                auto attached=Sector(attachments.encode());check(attached.scripts().size()==3 && attached.scripts()[0].tile==7 && attached.scripts()[1].counters==0xff807f01u,"Script insertion/order/header lost");
                attached.set_script({7,9,10,42});check(attached.scripts().size()==3 && attached.scripts()[0].flags==9,"Script edit duplicated attachment");
                attached.set_roof(1,1,0xa0000000);attached.remove_script(0);check(Sector(attached.encode()).scripts().size()==2,"Roof insertion invalidated script offsets");
                auto encodedScripts=attached.encode();check(Bytes(encodedScripts.begin()+attached.info().object_offset,encodedScripts.end())==Bytes(stream.begin()+info.object_offset,stream.end()),"Script edits damaged objects");
                Sector illuminated(bytes);auto beforeLight=illuminated.encode();Bytes lamp(48,0);lamp[31]=0x90;lamp[32]=255;
                illuminated.set_lights({lamp});check(illuminated.lights()==std::vector<Bytes>{lamp},"Light record not preserved");
                illuminated.set_blocked(2,3,true);illuminated.set_roof(1,1,0xa0000000);check(Sector(illuminated.encode()).lights()==std::vector<Bytes>{lamp},"Sector edits lost lights");
                illuminated.set_lights({});check(illuminated.lights().empty() && illuminated.blocked(2,3),"Light removal damaged sector");
                auto lightEncoded=illuminated.encode();check(Bytes(lightEncoded.begin()+illuminated.info().object_offset,lightEncoded.end())==Bytes(stream.begin()+info.object_offset,stream.end()),"Light edit damaged objects");
                Sector oldSettings(bytes);oldSettings.set_settings({-42,7,100,4100});oldSettings.set_roof(1,1,0xa0000000);
                auto persisted=Sector(oldSettings.encode()).settings();check(persisted.aptitude==-42 && persisted.ambient==4100,"Settings lost in old SEC upgrade or roof insertion");
            }
        }
        check(encode_instance(window.objects[0])==Bytes(stream.begin()+info.object_offset,stream.begin()+info.object_offset+info.object_bytes),
            "Instance writer changed independent sparse-array fixture");
        auto invalid=window.objects[0]; invalid.fields.push_back(invalid.fields.front());
        bool refused=false; try { encode_instance(invalid); } catch (const std::exception&) { refused=true; }
        check(refused,"Instance writer accepted duplicate field");
        check(window.animationControls->isHidden(),"Animation controls leaked into sector view");
        check(window.objectItems.size()==1,"Sector selection item missing");
        window.objectsList->setCurrentRow(-1);
        window.scene->clearSelection();
        window.populatePalette(); window.paletteSearch->setText("absent-from-library");
        window.paletteCategory->setCurrentIndex(window.paletteCategory->findData(0));
        auto target=window.canvas->mapFromScene(window.objectItems[0]->sceneBoundingRect().center());
        QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,target);
        QTest::qWait(90);
        QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,target);
        check(window.objectsList->currentRow()==0,"Click on sector object did not select it");
        check(window.libraryTabs->currentIndex()==0 && window.objectPalette->currentItem()!=nullptr,"World click did not reveal library entry");
        auto revealed=window.objectPalette->currentItem()->data(Qt::UserRole).toInt();
        check(window.paletteObjects[revealed].prototype_id==123 && window.paletteObjects[revealed].number("CURRENT_AID")==window.effectiveObjects[0].number("CURRENT_AID"),"World click revealed a different object");
        check(window.paletteSearch->text().isEmpty() && window.paletteCategory->currentData().toInt()==-1 && !window.objectPalette->currentItem()->isHidden(),"World object still hidden by palette filters");
        check(window.paletteObjects[revealed].id.empty() && !window.paletteObjects[revealed].field("SCRIPTS_IDX"),"Library copied a placed object's identity or scripts");
        check(window.properties->topLevelItemCount()>5,"Object properties missing");
        window.chooseResource("mes/notes.mes");
        check(window.document->toPlainText().contains(QString::fromUtf8("Propriété à Tarante")),"Accented MES text damaged");
        check(window.preview->currentWidget()==window.document,"Text preview not visible");
        window.chooseResource("art/bad.art"); check(window.failed,"Truncated ART was accepted");
        window.chooseResource("art/sprite.art"); check(!window.failed,"Cannot recover after bad resource");
        window.startBuilding(BuildingPlan{},false);
        check(window.building && window.objects.size()==49 && window.missingSprites==0,"Building generation/preview failed");
        for(const auto& object : window.building->objects) if(object.type==1) check((object.number("CURRENT_AID")&0x400)!=0,"New scene created a window instead of a door");
        window.objectsList->setCurrentRow(48);
        const auto id=window.building->objects.back().id;
        const auto original=window.building->objects.back().number("LOCATION");
        window.placeX->setValue(39); window.placeY->setValue(38);
        QTest::mouseClick(window.placementControls->findChild<QPushButton*>(),Qt::LeftButton);
        check(window.building->objects.back().number("LOCATION")==((38ull<<32)|39),"Building object did not move");
        window.undoHouse->trigger(); check(window.building->objects.back().number("LOCATION")==original,"Building undo failed");
        window.redoHouse->trigger(); check(window.building->objects.back().id==id,"Moving object changed its persistent ID");
        auto project=fs::u8path(argv[2]).parent_path()/"exported-building";
        window.building->location_names={{"fr","Lieu témoin"},{"en","Test location"}};
        auto output=export_building(*window.building,project);
        auto resumed=load_project(project);
        check(resumed.objects.size()==49 && resumed.location_names==window.building->location_names,"Editable project lost mobiles or translations");
        auto untouched=window.building->sector().encode();BuildingPlan roomPlan;roomPlan.x=4;roomPlan.y=4;
        window.addRoom(roomPlan,true);check(window.undoObjects.back().label=="Insert room","Room insertion absent from undo history");
        window.history(false);check(window.building->sector().encode()==untouched,"Room insertion undo changed terrain or objects");
        window.chooseResource("art/sprite.art"); check(window.placementControls->isHidden(),"Placement controls leaked into resource preview");
        window.returnHouse->trigger(); check(window.objects.size()==49,"Cannot return to building draft");
        window.showLocalSector(QString::fromUtf8(output.u8string().c_str()));
        auto moved=std::find_if(window.objects.begin(),window.objects.end(),[&](const auto& object){ return object.id==id; });
        check(moved!=window.objects.end() && moved->number("LOCATION")==((38ull<<32)|39),"Export lost the edited position");
        window.returnHouse->trigger();
        // Mouse drag is one undoable gesture, with stable IDs and no camera jump.
        window.objectsList->setCurrentRow(47);
        auto item=window.objectItems[47];
        window.canvas->resetTransform(); window.canvas->scale(3,3); window.canvas->centerOn(item);
        auto from=window.canvas->mapFromScene(item->sceneBoundingRect().center());
        auto to=from+QPoint(-120,180); // Two tiles in X and one in Y.
        auto loc=window.building->objects[47].number("LOCATION"); auto historySize=window.undoObjects.size();
        auto camera=window.canvas->transform();
        QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from); QTest::qWait(100);
        QTest::mouseMove(window.canvas->viewport(),to,30); QTest::qWait(90);
        QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        check(window.building->objects[47].number("LOCATION")==loc+2+(1ull<<32),"Mouse drag did not snap to tiles");
        check(window.undoObjects.size()==historySize+1 && window.canvas->transform()==camera,"Drag changed zoom or generated multiple history entries");
        QTest::keyClick(window.canvas,Qt::Key_Z,Qt::ControlModifier);
        check(window.building->objects[47].number("LOCATION")==loc,"Keyboard undo did not restore drag");
        item=window.objectItems[47]; from=window.canvas->mapFromScene(item->sceneBoundingRect().center()); to=from+QPoint(120,60);
        QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from); QTest::qWait(90);
        QTest::mouseMove(window.canvas->viewport(),to,30); QTest::keyClick(window.canvas,Qt::Key_Escape);
        QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        check(window.building->objects[47].number("LOCATION")==loc,"Escape committed a drag");
        auto drop=[&](int index,QPoint tile) {
            QMimeData mime; mime.setData(objectMime,QByteArray::number(index));
            auto position=window.canvas->mapFromScene(Canvas::project(tile));
            QDragEnterEvent enter(position,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(window.canvas->viewport(),&enter); check(enter.isAccepted(),"Palette drag not accepted");
            QDragMoveEvent move(position,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(window.canvas->viewport(),&move);
            QDropEvent event(position,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(window.canvas->viewport(),&event);
            check(event.isAccepted(),"Palette drop not accepted");
        };
        int chest=-1,soil=-1;
        for (unsigned i=0;i<window.paletteObjects.size();++i) { if(window.paletteObjects[i].type==2)chest=i; if(window.paletteObjects[i].type==100)soil=i; }
        check(chest>=0 && soil>=0,"Object or tile palette missing");
        int globalOnly=-1,otherChest=-1;
        for(unsigned i=0;i<window.paletteObjects.size();++i) {
            if(window.paletteObjects[i].prototype_id==3045) globalOnly=i;
            if(window.paletteObjects[i].prototype_id==3044) otherChest=i;
        }
        check(globalOnly>=0 && otherChest>=0 && globalOnly!=otherChest,"Global prototypes with identical art were lost");
        chest=globalOnly;
        auto bed=std::find_if(window.paletteObjects.begin(),window.paletteObjects.end(),[](const auto& o){return o.type==3 && o.number("CURRENT_AID")==((4u<<28)|(1u<<19)|(22u<<6));});
        check(bed!=window.paletteObjects.end(),"Visual variant absent from all sectors missing from catalogue");
        check(window.paletteObjects[globalOnly].id.empty() && !window.paletteObjects[globalOnly].prototype,"Catalogue copied a world instance or kept prototype serialization");
        auto catalogSize=window.paletteObjects.size(); auto catalogItem=window.objectPalette->item(globalOnly);
        unsigned windowWallAid=0x10000000u|(17u<<20)|(9u<<14)|(1u<<8);int windowWall=-1;
        for(unsigned i=0;i<window.paletteObjects.size();i++)if(window.paletteObjects[i].prototype_id==1021 && window.paletteObjects[i].number("CURRENT_AID")==windowWallAid){windowWall=i;break;}
        check(windowWall>=0,"Wall piece, variation and prototype absent from global catalogue");
        auto worldWall=window.paletteObjects[windowWall];set_number(worldWall,"CURRENT_AID",windowWallAid|(1u<<11));window.revealInPalette(worldWall);
        check(window.paletteObjects.size()==catalogSize && window.objectPalette->currentItem()->data(Qt::UserRole).toInt()==windowWall,"Wall selection appended a duplicate or changed prototype");
        bool catalogDoor=false,catalogWindow=false;
        for(const auto& object : window.paletteObjects) if(object.type==1) {
            auto aid=unsigned(object.number("CURRENT_AID"));
            if(aid==0x30000400) catalogDoor=window.assets->sprite(aid).path=="art/portal/door.art";
            if(aid==0x30000000) catalogWindow=window.assets->sprite(aid).path=="art/portal/window.art";
        }
        check(catalogDoor && catalogWindow,"Catalogue swapped door/window subtype");
        window.canvas->centerOn(Canvas::project({30,30})); drop(chest,{30,30});
        check(window.building->objects.size()==50 && window.redoObjects.empty(),"Adding did not clear redo history");
        auto newId=window.building->objects.back().id; check(newId!=id,"Palette addition reused persistent ID");
        QTest::keyClick(window.canvas,Qt::Key_Delete); check(window.building->objects.size()==49,"Delete did not remove selection");
        QTest::keyClick(window.canvas,Qt::Key_Z,Qt::ControlModifier); check(window.building->objects.back().id==newId,"Undo delete changed identity");
        QTest::keyClick(window.canvas,Qt::Key_D,Qt::ControlModifier); check(window.building->objects.size()==51,"Duplicate shortcut failed");
        window.objectsList->setCurrentRow(49); window.objectsList->item(50)->setSelected(true);
        check(window.scene->selectedItems().size()==2,"List multi-selection did not reach scene");
        window.objectsList->setFocus(); QTest::keyClick(window.objectsList,Qt::Key_Delete);
        check(window.building->objects.size()==49,"Batch delete failed");
        QTest::keyClick(window.objectsList,Qt::Key_Z,Qt::ControlModifier); check(window.building->objects.size()==51,"Batch undo failed");
        window.paletteSearch->setFocus(); window.paletteSearch->setText("abc"); QTest::keyClick(window.paletteSearch,Qt::Key_Home); QTest::keyClick(window.paletteSearch,Qt::Key_Delete);
        check(window.building->objects.size()==51 && window.paletteSearch->text()=="bc","Delete escaped text field"); window.paletteSearch->clear();
        auto tileBefore=window.building->ground.tile(30,30); drop(soil,{30,30});
        check(window.building->ground.tile(30,30)==window.paletteObjects[soil].number("CURRENT_AID"),"Tile drop failed");
        if(window.building->ground.tile(30,30)!=tileBefore) { window.history(false); check(window.building->ground.tile(30,30)==tileBefore,"Tile undo failed"); }
        {
            window.editLayer->setCurrentIndex(5);window.canvas->centerOn(Canvas::project({30,30}));
            auto before=window.building->ground.encode();bool blocked=window.building->ground.blocked(30,30);
            auto point=window.canvas->mapFromScene(Canvas::project({30,30}));
            QTest::mouseClick(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,point);
            check(window.building->ground.blocked(30,30)!=blocked,"Blocking click not applied");
            window.history(false);check(window.building->ground.encode()==before,"Blocking undo changed sector bytes");
            window.history(true);check(window.building->ground.blocked(30,30)!=blocked,"Blocking redo failed");
            point=window.canvas->mapFromScene(Canvas::project({30,30}));
            QTest::mouseClick(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,point);
            check(window.building->ground.blocked(30,30)==blocked,"Second blocking click did not clear tile");
            window.grab().save(QString::fromUtf8(argv[2])+"-blocking.png");window.editLayer->setCurrentIndex(0);
        }
        {
            auto before=window.building->ground.encode();
            QTimer::singleShot(50,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Sector dialog missing");dialog->reject();});window.configureSector();
            check(window.building->ground.encode()==before,"Cancelling sector dialog changed data");
            QTimer::singleShot(50,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Sector dialog missing");
                for(auto pair:{std::pair<const char*,int>{"sectorMusic",100},{"sectorAmbient",4100},{"sectorLight",7}}){auto box=dialog->findChild<QComboBox*>(pair.first);check(box && box->findData(pair.second)>=0,"Scheme names missing");box->setCurrentIndex(box->findData(pair.second));}
                dialog->findChild<QSpinBox*>("sectorAptitude")->setValue(-42);dialog->grab().save(QString::fromUtf8(argv[2])+"-sector-properties.png");dialog->accept();});window.configureSector();
            auto result=window.building->ground.settings();check(result.ambient==4100 && result.music==100 && result.light==7 && result.aptitude==-42,"Sector dialog values not applied");
            {QTemporaryDir output;auto path=fs::u8path(output.path().toStdString())/"project";export_building(*window.building,path);auto reopened=load_project(path);
                auto saved=reopened.ground.settings();check(saved.aptitude==result.aptitude && saved.light==result.light && saved.music==result.music && saved.ambient==result.ambient,"Sector settings changed during project save/reopen");
                // A saved project's SEC also contains its static objects; the
                // in-memory ground template does not. Compare the edited layers.
                for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)check(reopened.ground.blocked(x,y)==window.building->ground.blocked(x,y),"Blockers changed during project save/reopen");}
            window.history(false);check(window.building->ground.encode()==before,"Sector properties undo changed data");
            window.editLayer->setCurrentIndex(1);int exact=-1;for(int i=0;i<window.surfacePalette->count();i++)if(window.surfacePalette->item(i)->text().contains("drtgrs6a"))exact=i;
            check(exact>=0,"Exact terrain transition missing from Ground / Roofs");window.surfacePalette->setCurrentRow(exact);
            check(!window.terrainTransitions->isEnabled() && !window.terrainTransitions->isChecked(),"Exact tile still uses automatic transitions");
            window.editLayer->setCurrentIndex(0);
        }
        {
            auto before=window.building->ground.encode();
            QTimer::singleShot(100,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(d!=nullptr,"Attachment dialog missing");d->reject();});window.editGroundScript(1100);
            check(window.building->ground.encode()==before,"Cancelled attachment changed project");
            auto attach=[&](unsigned tile){QTimer::singleShot(100,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());auto list=d->findChild<QComboBox*>("attachmentResource");auto index=list->findData(42u);check(index>=0,"Fixture script missing");list->setCurrentIndex(index);
                d->findChild<QLineEdit*>("attachmentFlags")->setText("0x80000001");for(int i=0;i<4;i++)d->findChild<QSpinBox*>(QString("attachmentCounter%1").arg(i))->setValue(i==3?255:i+1);
                d->grab().save(QString::fromUtf8(argv[2])+"-attachment.png");auto b=d->findChild<QDialogButtonBox*>();b->button(QDialogButtonBox::Save)->click();});window.editGroundScript(tile);};
            attach(1100);attach(UINT32_MAX);window.editLayer->setCurrentIndex(4);window.inspectorTabs->setCurrentWidget(window.scriptsPanel);window.grab().save(QString::fromUtf8(argv[2])+"-attachments-window.png");window.editLayer->setCurrentIndex(0);bool found=false;for(auto a:window.building->ground.scripts())if(a.tile==1100)found=a.number==42 && a.flags==0x80000001u && a.counters==0xff030201u;check(found,"Attachment controls lost state");
            {QTemporaryDir output;auto path=fs::u8path(output.path().toStdString())/"project";export_building(*window.building,path);auto reopened=load_project(path);check(reopened.ground.scripts().size()==window.building->ground.scripts().size(),"Attachments lost on project save");}
            window.history(false);window.history(false);check(window.building->ground.encode()==before,"Attachment undo changed unrelated bytes");window.history(true);window.history(false);
        }
        window.showWorld(); check(window.preview->currentWidget()==window.worldView,"World map not visible");
        check(window.worldView->stored({2,1}) && !window.worldView->stored({1,2}),"Provenance confused stored sector and terrain template");
        check(window.worldView->provenance.pixelColor(2,1)==QColor("#008cff") && window.worldView->provenance.pixelColor(3,2).alpha()==0,"Provenance overlay has wrong coordinates");
        window.provenanceToggle->setChecked(false); check(!window.worldView->showProvenance,"Overlay toggle failed"); window.provenanceToggle->setChecked(true);
        auto worldPoint=window.worldView->mapFromScene(QPointF(2.2,1.4));
        QTest::mouseClick(window.worldView->viewport(),Qt::LeftButton,Qt::NoModifier,worldPoint);
        check(window.building->existing_map && window.building->sector_id==67108866,"World click opened wrong sector");
        check(window.building->objects.size()==1,"Existing world sector was replaced with a blank scene");
        check(window.paletteObjects.size()==catalogSize && window.objectPalette->item(globalOnly)==catalogItem,"Sector navigation rebuilt or changed global catalogue");
        window.paletteSearch->setText("#3045"); window.filterPalette();
        check(!catalogItem->isHidden() && window.objectPalette->item(otherChest)->isHidden(),"Prototype ID search failed");
        window.openWorldSector({1,2});
        check(window.paletteSearch->text()=="#3045" && !catalogItem->isHidden(),"Sector navigation lost catalogue filter");
        window.openWorldSector({2,1}); window.paletteSearch->clear();
        auto originalWorld=window.building->objects[0].id;
        drop(chest,{32,32}); auto placedId=window.building->objects.back().id;
        auto worldProject=fs::u8path(argv[2]).parent_path()/"world-edit";
        auto worldOutput=export_building(*window.building,worldProject);
        auto saveIndexPath=worldProject/"Slot0001.tfai";auto add32=[](Bytes& bytes,unsigned n){for(int i=0;i<4;i++)bytes.push_back((n>>(8*i))&255);};
        auto entry=std::to_string(window.building->sector_id)+".dif";Bytes saveIndex;add32(saveIndex,0);add32(saveIndex,entry.size());saveIndex.insert(saveIndex.end(),entry.begin(),entry.end());add32(saveIndex,0);add32(saveIndex,3);
        write_bytes(saveIndexPath,saveIndex);write_bytes(worldProject/"Slot0001.tfaf",{});
        bool visitedRefused=false;try{check_save_sector(saveIndexPath,*window.building);}catch(const std::exception&){visitedRefused=true;}
        check(visitedRefused,"Playtest accepted an already visited sector");
        auto saved=sector_objects(Sector(read_file(worldOutput)));
        check(saved[0].id==originalWorld && saved[0].number("LOCATION")==((71ull<<32)|140),"World export lost original ID or global coordinates");
        check(!fs::exists(worldOutput.parent_path()/"map.prp"),"World edit replaced global map properties");
        check(window.building->prototypes.count("proto/003045.pro"),"Placed global prototype was not retained for standalone export");
        // Export routing follows CE: a dynamic decoration belongs in a MOB even
        // though its type is scenery, while a static decoration stays in SEC.
        auto routed=*window.building;
        auto decoration=clone_instance(*bed); set_number(decoration,"LOCATION",(32ull<<32)|32); set_number(decoration,"FLAGS",0x2000);
        routed.objects.push_back(decoration);
        auto routedOutput=export_building(routed,fs::u8path(argv[2]).parent_path()/"dynamic-export");
        check(sector_objects(Sector(read_file(routedOutput))).size()==1,"Dynamic decoration was written into SEC");
        auto mob=read_objects(read_file(routedOutput.parent_path()/("g_"+decoration.id.substr(4)+".mob")));
        check(mob.size()==1 && mob[0].id==decoration.id && mob[0].number("LOCATION")==((96ull<<32)|160),"Dynamic object MOB lost identity or global location");
        window.openWorldSector({1,2}); check(window.building->objects.size()==1,"Generated terrain template not loaded");
        window.openWorldSector({2,1}); check(window.building->objects.size()==2 && window.building->objects.back().id==placedId,"World navigation lost draft changes");
        window.history(false); check(window.building->objects.size()==1,"World navigation lost undo history");
        for(auto [button,delta] : window.neighborButtons) if(delta==QPoint(-1,0)) QTest::mouseClick(button,Qt::LeftButton);
        check(window.building->sector_id==67108865,"East neighbor button opened wrong sector");
        window.openWorldSector({0,0});
        for(auto [button,delta] : window.neighborButtons) if(delta.x()<0 || delta.y()<0) check(!button->isEnabled(),"Out-of-map neighbor enabled");
        // Static terrain often has several null persistent IDs. Each must remain
        // individually selectable across edits and undo, without rewriting IDs.
        auto staticObject=window.building->objects.front(); staticObject.id=std::string(36,'0'); staticObject.editor_id=clone_instance(staticObject).id;
        window.building->objects={staticObject,staticObject}; window.building->objects[1].editor_id=clone_instance(staticObject).id;
        window.visibleObjects=2; window.showBuilding(true); window.objectsList->setCurrentRow(0);
        check(window.selectedIds().size()==1,"Null-ID objects share a selection identity");
        window.deleteSelection(); check(window.building->objects.size()==1,"Deleting a null-ID object deleted its neighbors");
        window.history(false); check(window.building->objects.size()==2 && window.scene->selectedItems().size()==1,"Undo lost individual null-ID selection");
        // Independent zone documents, safe building copies, save/undo state and
        // native popup dispatch. All mutations stay in synthetic projects.
        window.startBuilding(BuildingPlan{},false);auto sourceKey=window.activeDraft;
        auto sourceProject=fs::u8path(argv[2]).parent_path()/"workspace-source";
        check(window.saveActive(utf8(sourceProject.string())),"Cannot save source zone");
        auto sourceBytes=window.building->sector().encode();auto sourceIds=window.building->objects;
        window.openWorldSector({1,2});auto destinationKey=window.activeDraft;auto destinationBytes=window.building->sector().encode();
        check(!window.draftDirty(*window.worldDrafts.at(destinationKey)),"Unchanged sector marked dirty");
        window.referenceDraft=sourceKey;window.sideAction->setChecked(true);window.renderReference();
        check(window.referenceItems.size()==49 && !window.referenceCanvas->editable,"Reference does not have independent selection");
        for(auto item:window.referenceItems)item->setSelected(true);
        QTest::keyClick(window.referenceCanvas,Qt::Key_C,Qt::ControlModifier);
        check(window.copied.objects.size()==49 && window.copied.tiles.size()==143,"Building copy lost objects or floor");
        for(const auto& object:window.copied.objects)check(object.id.empty() && object.field("SCRIPTS_IDX") && !object.field("SCRIPTS_IDX")->present,"Copy retained identity or scripts");
        check(!window.pasteAt({63,63}) && window.building->sector().encode()==destinationBytes,"Out-of-bounds paste changed target");
        window.beginPaste();check(window.canvas->placementGhost!=nullptr,"Paste preview missing");
        QTest::keyClick(window.canvas,Qt::Key_Escape);check(!window.canvas->placementGhost && window.building->sector().encode()==destinationBytes,"Cancel paste changed target");
        window.canvas->resetTransform();window.canvas->scale(.4,.4);window.canvas->centerOn(Canvas::project({10,10}));
        window.beginPaste();auto pastePoint=window.canvas->mapFromScene(Canvas::project({10,10}));
        QTest::mouseMove(window.canvas->viewport(),pastePoint);QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,pastePoint);QTest::qWait(100);QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,pastePoint);
        check(window.building->objects.size()==50 && window.undoObjects.back().label=="Paste building","Click did not commit one building paste");
        check(window.worldDrafts.at(sourceKey)->building.sector().encode()==sourceBytes,"Copy changed source");
        for(std::size_t i=0;i<49;i++)check(window.building->objects[i+1].id!=sourceIds[i].id,"Copy reused persistent object ID");
        check(window.zoneState->text().contains("UNSAVED CHANGES") && window.zoneTabs->tabText(window.zoneTabs->currentIndex()).startsWith("●"),"Unsaved zone not identifiable");
        window.history(false);check(window.building->sector().encode()==destinationBytes && !window.draftDirty(*window.worldDrafts.at(destinationKey)),"Undo did not restore clean zone");
        window.history(true);auto pasted=window.building->sector().encode();auto targetProject=fs::u8path(argv[2]).parent_path()/"workspace-target";
        check(window.saveActive(utf8(targetProject.string())) && !window.draftDirty(*window.worldDrafts.at(destinationKey)),"Save did not clear dirty state");
        window.activateDraft(sourceKey);window.activateDraft(destinationKey);check(window.building->sector().encode()==pasted,"Tabs lost pending scene");
        window.history(false);check(window.draftDirty(*window.worldDrafts.at(destinationKey)),"Undo after saving was marked clean");
        write_bytes(targetProject/"notes.txt",Bytes{'k','e','e','p'});
        auto originalFile=read_file(targetProject/"building.json");auto invalidProject=*window.building;invalidProject.objects.front().fields.push_back(invalidProject.objects.front().fields.front());bool invalidRefused=false;
        try{save_scene_project(invalidProject,targetProject);}catch(const std::exception&){invalidRefused=true;}
        check(invalidRefused && read_file(targetProject/"building.json")==originalFile,"Failed save damaged previous project");
        check(window.saveActive(),"Cannot update owned project");check(read_file(targetProject/"notes.txt")==Bytes({'k','e','e','p'}),"Save discarded user notes");auto reopened=load_project(targetProject);check(reopened.objects.size()==1,"Saving deletion left stale mobile files");
        {auto project=utf8(targetProject.u8string()),game=QFileInfo(QString::fromUtf8(argv[1])).canonicalFilePath();check(project_installation(project)==game,"Saved project did not remember installation");
            Explorer automatic;automatic.show();int chooserCalls=0;automatic.projectInstallationChooser=[&](const QString&){chooserCalls++;return QString();};automatic.openProjectFolder(project);
            check(QTest::qWaitFor([&]{return automatic.editing() || automatic.failed;},20000) && automatic.editing(),"Project did not automatically mount its remembered installation");check(chooserCalls==0 && QFileInfo(automatic.installationPath).canonicalFilePath()==game,"Automatic project open asked for the installation or used a different one");
            QProcess command;auto image=QDir::temp().filePath("worlded-project-cli-"+QUuid::createUuid().toString(QUuid::Id128)+".png");command.setProgram(QCoreApplication::applicationDirPath()+"/worlded-explorer");command.setArguments({"--project",project,"--snapshot",image});command.start();check(command.waitForFinished(20000) && command.exitCode()==0 && QFileInfo::exists(image),"--project without --game did not open its remembered installation");QFile::remove(image);
            auto legacy=targetProject.parent_path()/"legacy-project";export_building(*window.building,legacy);auto legacyPath=utf8(legacy.u8string());check(project_installation(legacyPath).isEmpty(),"Legacy fixture unexpectedly contains local settings");
            Explorer old;old.show();int oldChoices=0;old.projectInstallationChooser=[&](const QString&){oldChoices++;return game;};old.openProjectFolder(legacyPath);
            check(QTest::qWaitFor([&]{return old.editing() || old.failed;},20000) && old.editing(),"Legacy project did not open after selecting an installation");check(oldChoices==1 && project_installation(legacyPath)==game,"Legacy project did not remember the selected installation");
            auto moved=targetProject.parent_path()/"moved-project";export_building(*window.building,moved);auto movedPath=utf8(moved.u8string());QJsonObject missing{{"format","worlded-local"},{"version",1},{"installation",QDir::tempPath()+"/worlded-missing-installation"}};auto json=QJsonDocument(missing).toJson();write_bytes(moved/"worlded.local.json",Bytes(json.begin(),json.end()));
            Explorer relocated;relocated.show();int replacements=0;relocated.projectInstallationChooser=[&](const QString& title){check(title.contains("not found"),"Missing installation not explained");replacements++;return game;};relocated.openProjectFolder(movedPath);
            check(QTest::qWaitFor([&]{return relocated.editing() || relocated.failed;},20000) && relocated.editing(),"Project did not recover from a moved installation");check(replacements==1 && project_installation(movedPath)==game,"Replacement installation was not remembered");
            auto canceled=targetProject.parent_path()/"cancel-project";export_building(*window.building,canceled);Explorer cancel;cancel.projectInstallationChooser=[](const QString&){return QString();};cancel.openProjectFolder(utf8(canceled.u8string()));check(!cancel.installation && !cancel.building && !fs::exists(canceled/"worlded.local.json"),"Cancelling installation selection changed the project");
            auto secondGame=targetProject.parent_path()/"second-game";fs::copy(fs::u8path(argv[1]),secondGame,fs::copy_options::recursive);auto other=targetProject.parent_path()/"other-install-project";export_building(*window.building,other);auto otherPath=utf8(other.u8string());remember_project_installation(otherPath,utf8(secondGame.u8string()));
            check(relocated.changeProjectInstallation->isEnabled(),"Change-installation command unavailable for saved project");relocated.projectInstallationChooser=[&](const QString&){return utf8(secondGame.u8string());};relocated.changeProjectInstallation->trigger();
            check(QTest::qWaitFor([&]{return relocated.editing() && QFileInfo(relocated.installationPath).canonicalFilePath()==QFileInfo(utf8(secondGame.u8string())).canonicalFilePath();},20000),"Changing the associated installation did not reopen the project");check(project_installation(movedPath)==QFileInfo(utf8(secondGame.u8string())).canonicalFilePath(),"Changed installation was not remembered");
            Explorer switching;switching.show();switching.mount(game);check(QTest::qWaitFor([&]{return switching.installation!=nullptr || switching.failed;},20000) && switching.installation,"Initial installation failed");switching.openProjectFolder(otherPath);
            check(QTest::qWaitFor([&]{return switching.editing() || switching.failed;},20000) && switching.editing() && QFileInfo(switching.installationPath).canonicalFilePath()==QFileInfo(utf8(secondGame.u8string())).canonicalFilePath(),"Opening project did not switch to its associated installation");
            auto next=switching.building->objects;set_number(next[0],"OFFSET_X",1);switching.commitEdit("fixture",next,{});auto unsaved=switching.building->sector().encode();
            QTimer::singleShot(50,[&]{if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Cancel)->click();});switching.openProjectFolder(legacyPath);
            check(switching.editing() && switching.building->sector().encode()==unsaved && switching.draftDirty(*switching.worldDrafts.at(switching.activeDraft)),"Cancelling installation switch discarded unsaved changes");
            check(!fs::exists(fs::u8path(argv[1])/"worlded.local.json"),"Project association was written into the game installation");}
        window.openProjectFolder(utf8(targetProject.string()));check(window.activeDraft==destinationKey,"Reopening project duplicated existing tab");
        window.showWorld();auto tab=window.zoneTabs->tabRect(window.zoneTabs->currentIndex()).center();QTest::mouseClick(window.zoneTabs,Qt::LeftButton,Qt::NoModifier,tab);check(window.editing(),"Clicking current tab did not leave world map");
        bool popupSeen=false;QTimer::singleShot(80,[&]{auto menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());popupSeen=menu && menu->actions().contains(window.pasteAction);if(menu)menu->close();});
        auto contextPoint=window.canvas->viewport()->rect().center();QContextMenuEvent context(QContextMenuEvent::Mouse,contextPoint,window.canvas->viewport()->mapToGlobal(contextPoint));QApplication::sendEvent(window.canvas->viewport(),&context);check(popupSeen,"Viewport right-click did not open edit menu");
        // Mouse hover must produce a selected menu item and a visible highlight.
        auto fileMenu=window.menuBar()->actions().front()->menu();fileMenu->popup(window.menuBar()->mapToGlobal(QPoint(0,window.menuBar()->height())));QTest::qWait(50);
        auto hoverAction=fileMenu->actions().front();auto menuBefore=fileMenu->grab().toImage();QTest::mouseMove(fileMenu,fileMenu->actionGeometry(hoverAction).center());QTest::qWait(60);
        check(fileMenu->activeAction()==hoverAction && menuBefore!=fileMenu->grab().toImage(),"File menu hover lacks visible feedback");fileMenu->close();
        window.history(true);check(window.draftDirty(*window.worldDrafts.at(destinationKey)),"Redo after saving was marked clean");
        QTimer::singleShot(50,[&]{if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Cancel)->click();});
        check(!window.closeZone(destinationKey) && window.worldDrafts.count(destinationKey),"Closing dirty tab ignored cancellation");
        check(window.closeZone(destinationKey,true) && window.worldDrafts.count(sourceKey),"Closing one zone discarded another");
        // Roof envelopes may be created on previously roofless sectors without
        // moving or damaging their opaque metadata, scripts and object stream.
        auto base=*window.building;auto sectorBytes=base.ground.encode();auto offset=4+48*base.ground.info().lights+4096*4;
        if(base.ground.info().roofs){sectorBytes.erase(sectorBytes.begin()+offset+4,sectorBytes.begin()+offset+1028);sectorBytes[offset]=1;sectorBytes[offset+1]=sectorBytes[offset+2]=sectorBytes[offset+3]=0;}
        Sector roofless(sectorBytes);auto metadata=roofless.scripts();roofless.set_roof(3,4,0xa0000000);Sector roofReload(roofless.encode());
        check(roofReload.roof(3,4)==0xa0000000 && roofReload.info().object_offset==Sector(sectorBytes).info().object_offset+1024,"Roof table shifted the sector envelope incorrectly");
        check(roofReload.scripts().size()==metadata.size() && sector_objects(roofReload).size()==sector_objects(Sector(sectorBytes)).size(),"Roof save lost scripts or objects");
        {auto bytes=window.installation->read("maps/test/0.sec");auto original=Sector(bytes);auto roofAt=4+48*original.info().lights+4096*4;bytes.erase(bytes.begin()+roofAt+4,bytes.begin()+roofAt+1028);bytes[roofAt]=1;bytes[roofAt+1]=bytes[roofAt+2]=bytes[roofAt+3]=0;
            Sector sector(bytes);sector.set_roof(3,4,0xa0000000);auto objectsBefore=sector_objects(sector).size();check(sector.info().tile_scripts>0 && sector.scripts().size()>1,"Script fixture incomplete");sector.remove_script(0);check(sector.info().tile_scripts==0 && sector_objects(sector).size()==objectsBefore && sector.roof(3,4)==0xa0000000,"Removing tile script damaged sector");sector.remove_script(0);check(sector.scripts().empty() && sector_objects(sector).size()==objectsBefore,"Removing sector script damaged objects");}
        auto sprite=window.assets->sprite(0xa0000011);check(sprite.hotspot.x()==0 && !sprite.pixels.isNull(),"Mirrored roof rendering failed");
        check(!window.assets->sprite(0x28000000).pixels.isNull(),"Character sprite resolution failed");
        auto listing=script_listing(window.installation->read("scr/00042example.scr"));check(listing.contains("IF") && listing.contains("THEN") && listing.contains("ELSE"),"Script instructions not exposed");
        auto originalScript=window.installation->read("scr/00042example.scr");auto editableScript=script_source(originalScript);check(compile_script(originalScript,editableScript)==originalScript,"Unchanged script lost binary data");
        auto extended=compile_script(originalScript,editableScript+"NEW IF SCT_TRUE() THEN SAT_DO_NOTHING() ELSE SAT_DO_NOTHING()\n");check(inspect_script(extended).lines==inspect_script(originalScript).lines+1,"Adding a script line failed");
        bool invalidScript=false;try{compile_script(originalScript,"NEW IF SCT_TRUE() THEN SAT_GOTO() ELSE SAT_DO_NOTHING()\n");}catch(const std::exception&){invalidScript=true;}check(invalidScript,"Invalid script arguments accepted");
        {ScriptEditor editor;editor.show();editor.setFocus();QTest::keyClicks(&editor,"SCT_TR");auto completion=editor.findChild<QCompleter*>();check(completion && completion->completionCount()>0 && completion->completionPrefix()=="SCT_TR","Opcode completion did not offer matching conditions");completion->setCurrentRow(0);completion->popup()->setCurrentIndex(completion->completionModel()->index(0,0));QTest::keyClick(completion->popup(),Qt::Key_Enter);check(editor.toPlainText()=="SCT_TRUE","Opcode completion did not insert the selected condition");editor.close();}
        auto unused=window.installation->read("scr/00042example.scr");std::fill(unused.begin()+64,unused.begin()+108,255);std::fill(unused.begin()+64,unused.begin()+68,0);
        check(script_listing(unused).contains("IF SCT_TRUE ()\n"),"Unused script operand memory was displayed as arguments");
        {auto next=window.building->objects;check(!next.empty(),"Object fixture missing");ObjectField field;field.name="SCRIPTS_IDX";field.kind="SCRIPT_ARRAY";field.present=true;Bytes first(12),second(12);first[8]=42;second[8]=43;field.elements={{0,first},{9,second}};
            auto previous=std::find_if(next[0].fields.begin(),next[0].fields.end(),[](const auto& f){return f.name=="SCRIPTS_IDX";});if(previous==next[0].fields.end())next[0].fields.push_back(field);else *previous=field;
            window.commitEdit("Add script fixture",next,{});window.editLayer->setCurrentIndex(4);auto beforeScript=window.building->sector().encode();int index=-1;
            for(int i=0;i<int(window.activeContext.scripts.size());i++)if(window.activeContext.scripts[i].id==42 && window.activeContext.scripts[i].origin==0){index=i;break;}check(index>=0,"Object script not listed");
            window.scriptsList->setCurrentRow(index);check(window.removeScript->isEnabled() && window.locateScript->isEnabled(),"Object script controls disabled");QTest::mouseClick(window.removeScript,Qt::LeftButton);
            const auto* result=window.building->objects[0].field("SCRIPTS_IDX");check(result && result->elements.size()==2 && word(result->elements[0].data,8)==0 && word(result->elements[1].data,8)==43,"Removal changed unrelated object scripts");
            window.history(false);check(window.building->sector().encode()==beforeScript,"Undo did not restore object script");window.history(false);window.editLayer->setCurrentIndex(0);}
        {auto next=window.building->objects;auto candidate=std::find_if(window.paletteObjects.begin(),window.paletteObjects.end(),[](const auto& o){return o.prototype_id==4027;});check(candidate!=window.paletteObjects.end(),"Inherited-script prototype missing");auto instance=clone_instance(*candidate);set_number(instance,"LOCATION",(20ull<<32)|20);next.push_back(instance);auto objectIndex=next.size()-1;
            window.commitEdit("Add inherited script fixture",next,{});window.editLayer->setCurrentIndex(4);auto beforeScript=window.building->sector().encode();int index=-1;
            for(int i=0;i<int(window.activeContext.scripts.size());i++)if(window.activeContext.scripts[i].id==42 && window.activeContext.scripts[i].origin==0 && window.activeContext.scripts[i].inherited){index=i;break;}check(index>=0,"Inherited script not listed");
            window.scriptsList->setCurrentRow(index);QTest::mouseClick(window.removeScript,Qt::LeftButton);auto effective=window.assets->effective(window.building->objects[objectIndex]);const auto* field=effective.field("SCRIPTS_IDX");
            check(field && word(field->elements[0].data,8)==0 && word(field->elements[1].data,8)==43,"Inherited attachment removal changed other scripts");
            auto prototype=read_objects(window.installation->read("proto/004027.pro"))[0];check(word(prototype.field("SCRIPTS_IDX")->elements[0].data,8)==42,"Prototype was modified");
            window.history(false);check(window.building->sector().encode()==beforeScript,"Undo did not restore inherited script");window.history(false);window.editLayer->setCurrentIndex(0);}
        TerrainBrush brush(*window.installation);auto terrain=base.ground;for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)terrain.set_tile(x,y,0x4101c0);
        auto transitioned=brush.paint(terrain,{{32,32}},(3u<<22)|(3u<<16)|0xf1c0,[](unsigned){return true;});
        auto sharp=terrain;for(int y=29;y<=33;y++)for(int x=29;x<=33;x++)sharp.set_tile(x,y,0xf1c0);
        auto repaired=brush.paint(sharp,{{33,33}},0xf1c0,[](unsigned){return true;});
        check(repaired.encode()!=sharp.encode(),"Repainting an existing sharp boundary did not generate transitions");
        {bool mixed=false;for(int y=32;y<=34;y++)for(int x=32;x<=34;x++){auto c=TerrainBrush::corners(repaired.tile(x,y));mixed|=std::set<int>(c.begin(),c.end()).size()>1;}check(mixed,"Repainted dirt boundary remained sharp");}
        std::set<int> terrainMaterials;for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){auto c=TerrainBrush::corners(transitioned.tile(x,y));terrainMaterials.insert(c.begin(),c.end());}
        check(terrainMaterials==std::set<int>({0,1,2,3}),"Water painting skipped intermediate shore materials");
        for(int y=0;y<63;y++)for(int x=0;x<63;x++){auto a=TerrainBrush::corners(transitioned.tile(x,y)),b=TerrainBrush::corners(transitioned.tile(x+1,y)),c=TerrainBrush::corners(transitioned.tile(x,y+1));check(a[1]==b[0] && a[2]==b[3] && a[3]==c[0] && a[2]==c[1],"Terrain transition corners do not join");}
        bool borderRefused=false;try{brush.paint(terrain,{{0,0}},(3u<<22)|(3u<<16)|0xf1c0,[](unsigned){return true;});}catch(const std::exception&){borderRefused=true;}check(borderRefused,"Cross-sector transition was silently clipped");
        window.editLayer->setCurrentIndex(1);window.terrainTransitions->setChecked(false);window.canvas->cellSelection={20,20,3,2};auto initial=window.building->sector().encode();auto objectCount=window.building->objects.size();
        window.copySelection(false);check(window.copied.objects.empty() && window.copied.tiles.size()==6,"Ground-only copy includes objects");
        check(window.pasteAt({40,40}) && window.building->objects.size()==objectCount,"Ground paste added objects");window.history(false);check(window.building->sector().encode()==initial,"Ground paste undo failed");
        window.editLayer->setCurrentIndex(2);window.canvas->cellSelection={12,16,4,4};window.surfacePalette->setCurrentRow(0);window.applySurface(false);auto roofState=window.building->sector().encode();check(window.building->ground.roof(3,4)!=0xffffffff,"Roof painting failed");
        window.copySelection(false);check(window.copied.objects.empty() && window.copied.roofs.size()==1,"Roof-only copy failed");check(!window.pasteAt({13,17}),"Misaligned roof paste accepted");check(window.pasteAt({40,40}),"Aligned roof paste rejected");window.history(false);check(window.building->sector().encode()==roofState,"Roof paste undo failed");
        window.canvas->cellSelection={12,16,4,4};window.deleteSelection();check(window.building->ground.roof(3,4)==0xffffffff,"Roof delete failed");window.history(false);check(window.building->sector().encode()==roofState,"Roof delete undo failed");
        window.editLayer->setCurrentIndex(1);window.canvas->resetTransform();window.canvas->scale(.5,.5);window.canvas->centerOn(Canvas::project({30,30}));window.surfacePalette->setCurrentRow(0);window.paintSurface->setChecked(true);auto undoCount=window.undoObjects.size();
        auto brushFrom=window.canvas->mapFromScene(Canvas::project({28,28})),brushTo=window.canvas->mapFromScene(Canvas::project({32,28}));QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,brushFrom);QTest::mouseMove(window.canvas->viewport(),brushTo);QTest::qWait(100);QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,brushTo);
        check(window.undoObjects.size()==undoCount+1,"Continuous paint stroke must be one undo operation");window.history(false);
        auto beforeCancelledStroke=window.building->sector().encode();QTest::mousePress(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,brushFrom);QTest::mouseMove(window.canvas->viewport(),brushTo);QTest::keyPress(window.canvas,Qt::Key_Escape);QTest::qWait(80);QTest::mouseRelease(window.canvas->viewport(),Qt::LeftButton,Qt::NoModifier,brushTo);QTest::keyRelease(window.canvas,Qt::Key_Escape);
        check(window.building->sector().encode()==beforeCancelledStroke && window.undoObjects.size()==undoCount,"Cancelled paint stroke modified the scene");window.paintSurface->setChecked(false);
        {int human=-1,monster=-1;for(unsigned i=0;i<window.paletteObjects.size();i++){if(window.paletteObjects[i].prototype_id==17001)human=i;if(window.paletteObjects[i].prototype_id==17002)monster=i;}
            check(human>=0 && monster>=0,"NPC or creature prototype missing from catalog");check(!window.assets->sprite(window.paletteObjects[human].number("CURRENT_AID")).pixels.isNull() && !window.assets->sprite(window.paletteObjects[monster].number("CURRENT_AID")).pixels.isNull(),"Character sprites unavailable");
            window.editLayer->setCurrentIndex(3);check(window.objectPalette->isEnabled() && window.paletteCategory->currentData().toInt()==16,"Character library inaccessible in character mode");
            QMimeData nonActor;nonActor.setData(objectMime,QByteArray::number(chest));auto point=window.canvas->mapFromScene(Canvas::project({30,30}));QDragEnterEvent rejected(point,Qt::CopyAction,&nonActor,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(window.canvas->viewport(),&rejected);check(!rejected.isAccepted(),"Non-character accepted in character mode");
            auto oldCount=window.building->objects.size();drop(human,{30,30});drop(monster,{31,30});check(window.building->objects.size()==oldCount+2,"Character drops did not create project objects");
            auto& npc=window.building->objects[oldCount];auto loc=npc.number("LOCATION");check(npc.number("NPC_STANDPOINT_DAY")==loc && npc.number("NPC_STANDPOINT_NIGHT")==loc && npc.number("CRITTER_TELEPORT_DEST")==loc,"Character start positions not initialized");
            check(window.objectItems[oldCount]->flags()&QGraphicsItem::ItemIsSelectable,"Project character not selectable");window.translateSelection({int(oldCount)},{1,0});auto moved=window.building->objects[oldCount];check(moved.number("NPC_STANDPOINT_DAY")==moved.number("LOCATION"),"Moving a character left its start point behind");window.history(false);
            auto instance=window.building->objects[oldCount];auto resolved=window.assets->effective(instance);
#include "test_actor_routes.inc"
            {auto inherited=resolved;ObjectField skill;skill.name="CRITTER_BASIC_SKILL_IDX";skill.kind="INT32_ARRAY";skill.elements={{0,{0x83,0,1,0}}};ActorEditor::put(inherited,skill);ActorEditor skills(instance,inherited,"Skill fixture");auto table=skills.findChild<QTableWidget*>("CRITTER_BASIC_SKILL_IDX");check(table->item(0,1)->text()=="3" && qobject_cast<QComboBox*>(table->cellWidget(0,2))->currentIndex()==2,"Skill points and training decoded incorrectly");table->item(0,1)->setText("7");auto modified=skills.values();check(word(modified.field("CRITTER_BASIC_SKILL_IDX")->elements[0].data,0)==0x10087,"Skill edit lost training/unknown bits");}
            ActorEditor editor(instance,resolved,"Fixture");check(encode_instance(editor.values())==encode_instance(instance),"Unchanged actor editor materialized inherited fields");
            editor.findChild<QLineEdit*>("NPC_FACTION")->setText("42");editor.findChild<QLineEdit*>("HP_PTS")->setText("75");editor.x->setValue(35);editor.scriptTable->item(0,1)->setText("123");
            auto changed=editor.values();check(changed.number("NPC_FACTION")==42 && changed.number("HP_PTS")==75 && (changed.number("LOCATION")&63)==35,"Actor parameters not applied");check(changed.field("SCRIPTS_IDX") && word(changed.field("SCRIPTS_IDX")->elements[0].data,8)==123,"Actor script event not saved");
            check(changed.id==instance.id && changed.prototype_id==instance.prototype_id && !changed.prototype,"Actor configuration changed identity");editor.findChild<QLineEdit*>("HP_PTS")->setText("not a number");bool invalid=false;try{editor.values();}catch(...){invalid=true;}check(invalid,"Invalid actor number accepted");
            window.scene->clearSelection();window.objectItems[oldCount]->setSelected(true);auto beforeActor=window.building->sector().encode();auto undoSize=window.undoObjects.size();
            QTimer::singleShot(20,[&]{auto dialog=dynamic_cast<ActorEditor*>(QApplication::activeModalWidget());check(dialog!=nullptr,"Configure action did not open actor editor");dialog->findChild<QLineEdit*>("NPC_FACTION")->setText("42");dialog->result=dialog->values();dialog->accept();});window.editActor();
            check(window.building->objects[oldCount].number("NPC_FACTION")==42 && window.undoObjects.size()==undoSize+1,"Actor edit not recorded in history");window.history(false);check(window.building->sector().encode()==beforeActor,"Undo actor parameters failed");window.history(true);check(window.building->objects[oldCount].number("NPC_FACTION")==42,"Redo actor parameters failed");window.history(false);
            QTimer::singleShot(20,[&]{auto dialog=dynamic_cast<ActorEditor*>(QApplication::activeModalWidget());dialog->findChild<QLineEdit*>("HP_PTS")->setText("99");dialog->reject();});window.editActor();check(window.building->sector().encode()==beforeActor,"Cancelled actor edit changed scene");
            QTemporaryDir output;auto path=fs::u8path(output.path().toStdString())/"project";export_building(*window.building,path);auto reopened=load_project(path);unsigned found=0;for(const auto& o:reopened.objects)if(o.prototype_id==17001 || o.prototype_id==17002)found++;check(found==2,"Export lost placed characters");
            window.history(false);window.history(false);check(window.building->objects.size()==oldCount,"Character drop undo failed");}
        window.editLayer->setCurrentIndex(0);
        QTest::qWait(80);
        check(window.grab().save(QString::fromUtf8(argv[2])),"Cannot save GUI test capture");
        std::cout << "PASS: resources, rendering, drag/drop, deletion, duplication, multi-selection, undo/redo, text focus, world clicks, provenance, neighbor navigation and isolated export\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
