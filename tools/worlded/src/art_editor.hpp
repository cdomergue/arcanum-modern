#pragma once
#include "art_png.hpp"
#include "editor_tools.hpp"
#include <QSaveFile>
#include <QImageReader>
#include <cmath>

namespace worlded {
class ArtCanvas : public QGraphicsView {
    QGraphicsScene scene_;
    QGraphicsPixmapItem* sprite_;
    bool dragging_=false;
public:
    QPoint anchor;
    std::function<void(QPoint)> moved;
    ArtCanvas() : QGraphicsView(), scene_(this) {
        setScene(&scene_); sprite_=scene_.addPixmap(QPixmap());
        setRenderHint(QPainter::SmoothPixmapTransform,false);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setMinimumSize(320,260);
        setToolTip("Drag the cyan crosshair to move the anchor. Drag elsewhere to pan.");
        QPixmap tile(16,16);tile.fill(QColor(64,68,74));QPainter p(&tile);
        p.fillRect(0,0,8,8,QColor(84,88,94));p.fillRect(8,8,8,8,QColor(84,88,94));
        setBackgroundBrush(QBrush(tile));
    }
    void display(const QImage& image, QPoint hot, int zoom) {
        anchor=hot;sprite_->setPixmap(QPixmap::fromImage(image));
        auto bounds=QRectF(image.rect()).united(QRectF(hot-QPoint(12,12),QSize(24,24))).adjusted(-32,-32,32,32);
        setSceneRect(bounds);setTransform(QTransform::fromScale(zoom,zoom));viewport()->update();
    }
protected:
    void drawForeground(QPainter* painter,const QRectF&) override {
        painter->save();QPen pen(QColor(70,230,255));pen.setCosmetic(true);painter->setPen(pen);
        const qreal reach=10/transform().m11();auto center=QPointF(anchor)+QPointF(.5,.5);
        painter->drawLine(center-QPointF(reach,0),center+QPointF(reach,0));
        painter->drawLine(center-QPointF(0,reach),center+QPointF(0,reach));painter->restore();
    }
    void mousePressEvent(QMouseEvent* e) override {
        auto center=mapFromScene(QPointF(anchor)+QPointF(.5,.5));
        if (e->button()==Qt::LeftButton && (e->position().toPoint()-center).manhattanLength()<=20) {dragging_=true;e->accept();}
        else QGraphicsView::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_) {auto point=mapToScene(e->position().toPoint());if(moved)moved(QPoint(int(std::floor(point.x())),int(std::floor(point.y()))));e->accept();}
        else QGraphicsView::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (dragging_ && e->button()==Qt::LeftButton) {dragging_=false;e->accept();}
        else QGraphicsView::mouseReleaseEvent(e);
    }
};

class ArtWindow : public QMainWindow {
    std::unique_ptr<Art> art_;
    std::shared_ptr<Installation> installation_;
    QString path_,name_,game_;
    bool dirty_=false,refreshing_=false;
    QTimer timer_;
    ArtCanvas* canvas_=new ArtCanvas;
    QListWidget* library_=new QListWidget;
    QLineEdit* filter_=new QLineEdit;
    QComboBox* rotation_=new QComboBox;
    QComboBox* palette_=new QComboBox;
    QSpinBox* frame_=new QSpinBox;
    QSpinBox* fps_=new QSpinBox;
    QSpinBox* action_=new QSpinBox;
    QSpinBox* hotX_=new QSpinBox;
    QSpinBox* hotY_=new QSpinBox;
    QSpinBox* offsetX_=new QSpinBox;
    QSpinBox* offsetY_=new QSpinBox;
    QSpinBox* zoom_=new QSpinBox;
    QSlider* timeline_=new QSlider(Qt::Horizontal);
    QCheckBox* mirror_=new QCheckBox("Critter mirroring (directions 1–3)");
    QPushButton* play_=new QPushButton("Play");
    QTableWidget* colors_=new QTableWidget(16,16);
    QLabel* info_=new QLabel;
    QWidget* controls_=new QWidget;

    void guard(const std::function<void()>& fn) {
        try {fn();} catch(const std::exception& e) {timer_.stop();play_->setText("Play");QMessageBox::warning(this,"ART editor",QString::fromUtf8(e.what()));}
    }
    bool mirrored() const {return art_ && art_->rotations()==8 && mirror_->isChecked() && rotation_->currentIndex()>0 && rotation_->currentIndex()<4;}
    unsigned sourceRotation() const {return mirrored()?8-rotation_->currentIndex():rotation_->currentIndex();}
    unsigned selectedPalette() const {return palette_->currentData().toUInt();}
    void changed() {dirty_=true;setWindowTitle(name_+" * — ART Editor Renaissance");}
    bool leave() {
        if (!dirty_) return true;
        auto choice=QMessageBox::question(this,"Unsaved ART","Save changes to "+name_+"?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
        if (choice==QMessageBox::Cancel) return false;
        if (choice==QMessageBox::Save) {save(false);return !dirty_;}
        return true;
    }
    void refreshPalette() {
        const auto& p=art_->palette(selectedPalette());
        for (int i=0;i<256;++i) {
            auto item=colors_->item(i/16,i%16);
            item->setBackground(QColor::fromRgb(p[i]));
            item->setText(i==0?"×":"");item->setForeground(Qt::white);
            item->setToolTip(QString("Index %1 · %2%3").arg(i).arg(QColor::fromRgb(p[i]).name()).arg(i==0?" · always transparent":""));
        }
    }
    void refresh() {
        if (!art_) return;
        refreshing_=true;
        auto r=sourceRotation();auto f=unsigned(frame_->value());const auto& meta=art_->frame(r,f);
        auto image=art_image(*art_,r,f,selectedPalette());
        int hx=meta.hot_x;
        if (mirrored()) {image=image.mirrored(true,false);hx=int(meta.width)-1-hx;}
        // Fields always edit the stored source direction, not transformed preview coordinates.
        hotX_->setValue(meta.hot_x);hotY_->setValue(meta.hot_y);
        offsetX_->setValue(meta.offset_x);offsetY_->setValue(meta.offset_y);
        timeline_->setValue(frame_->value());
        canvas_->display(image,QPoint(hx,meta.hot_y),zoom_->value());
        info_->setText(QString("%1 × %2 px · frame %3 / %4 · source direction %5%6")
            .arg(meta.width).arg(meta.height).arg(f).arg(art_->frames()-1).arg(r)
            .arg(mirrored()?" (mirrored preview)":""));
        statusBar()->showMessage(mirrored()?"Mirrored preview: anchor and offset fields edit the stored source direction.":"Palette index 0 is transparent. Drag the cyan anchor to reposition it.");
        refreshing_=false;
    }
    void refreshLibrary() {
        library_->clear();if (!installation_) return;
        for (const auto& [name,sources]:installation_->resources()) {
            auto text=QString::fromStdString(cp1252_utf8(name));
            if (text.endsWith(".art") && text.contains(filter_->text(),Qt::CaseInsensitive)) {
                auto item=new QListWidgetItem(text,library_);
                item->setToolTip(QString::fromStdString(sources.back().file.u8string()));
            }
        }
    }
    void installDocument(Art next, const QString& name, const QString& path, bool dirty) {
        next.validate();timer_.stop();play_->setText("Play");art_=std::make_unique<Art>(std::move(next));
        name_=name;path_=path;dirty_=dirty;refreshing_=true;
        rotation_->clear();for (unsigned r=0;r<art_->rotations();++r) rotation_->addItem(QString::number(r));
        palette_->clear();for (unsigned p=0;p<4;++p) if (art_->palette_available(p)) palette_->addItem(QString("Palette %1").arg(p),p);
        frame_->setRange(0,int(art_->frames())-1);frame_->setValue(0);timeline_->setRange(0,int(art_->frames())-1);
        fps_->setValue(int(std::min(art_->fps(),1000u)));action_->setRange(-1,int(art_->frames())-1);action_->setValue(art_->action_frame());
        mirror_->setEnabled(art_->rotations()==8);mirror_->setChecked(false);controls_->setEnabled(true);
        refreshing_=false;refreshPalette();refresh();canvas_->centerOn(canvas_->sceneRect().center());
        setWindowTitle(name_+(dirty_?" *":"")+" — ART Editor Renaissance");
    }
    void importPng() {
        auto paths=QFileDialog::getOpenFileNames(this,"Import PNG animation",{},"PNG images (*.png)");
        if (paths.isEmpty()) return;
        paths.sort(Qt::CaseSensitive);
        bool ok=false;auto layout=QInputDialog::getItem(this,"Animation directions",
            "Files are sorted by name. For 8 directions, use equal groups ordered 0–7.\nAlpha below 128 becomes transparent; other colors share a 255-color palette.",
            {"1 direction","8 directions"},0,false,&ok);
        if (!ok || !leave()) return;
        std::vector<QImage> images;std::uint64_t total=0;
        for (const auto& path:paths) {
            QImageReader reader(path,"PNG");auto size=reader.size();
            if (size.width()<=0 || size.height()<=0 || size.width()>16384 || size.height()>16384)
                throw std::runtime_error("Invalid PNG dimensions: "+path.toStdString());
            total+=std::uint64_t(size.width())*size.height();
            if (total>resource_limit/4) throw std::runtime_error("PNG animation exceeds the resource limit");
            auto image=reader.read();if (image.isNull()) throw std::runtime_error(reader.errorString().toStdString());
            images.push_back(image.convertToFormat(QImage::Format_ARGB32));
        }
        installDocument(art_from_pngs(images,layout.startsWith('8')?8:1,10),"untitled.art",{},true);
    }
    void exportPng(bool all) {
        if (!art_) return;
        if (!all) {
            auto path=QFileDialog::getSaveFileName(this,"Export stored frame",QFileInfo(name_).completeBaseName()+".png","PNG (*.png)");
            if (!path.isEmpty()) writePng(path,art_image(*art_,sourceRotation(),frame_->value(),selectedPalette()));
            return;
        }
        auto dir=QFileDialog::getExistingDirectory(this,"Choose an empty directory for PNG frames");
        if (dir.isEmpty()) return;
        if (!QDir(dir).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty())
            throw std::runtime_error("Choose an empty folder to avoid replacing existing frames");
        for (unsigned r=0;r<art_->rotations();++r) for (unsigned f=0;f<art_->frames();++f)
            writePng(QDir(dir).filePath(QString("r%1_f%2.png").arg(r).arg(f,4,10,QChar('0'))),art_image(*art_,r,f,selectedPalette()));
        statusBar()->showMessage("Exported all stored directions using the selected palette to "+dir);
    }
    static void writePng(const QString& path,const QImage& image) {
        QSaveFile file(path);if (!file.open(QIODevice::WriteOnly) || !image.save(&file,"PNG") || !file.commit())
            throw std::runtime_error("Cannot write PNG: "+path.toStdString());
    }
public:
    ArtWindow() {
        setWindowTitle("ART Editor Renaissance");resize(1220,820);
        auto main=new QWidget;auto layout=new QVBoxLayout(main);setCentralWidget(main);
        layout->addWidget(canvas_,1);layout->addWidget(info_);
        auto transport=new QHBoxLayout;transport->addWidget(play_);transport->addWidget(timeline_,1);transport->addWidget(new QLabel("Frame"));transport->addWidget(frame_);
        layout->addLayout(transport);
        auto dock=new QDockWidget("Animation and palette",this);auto panel=new QVBoxLayout(controls_);auto form=new QFormLayout;
        rotation_->setObjectName("direction");frame_->setObjectName("frame");hotX_->setObjectName("hotX");hotY_->setObjectName("hotY");
        colors_->setObjectName("paletteGrid");zoom_->setObjectName("zoom");
        zoom_->setRange(1,16);zoom_->setValue(2);zoom_->setSuffix("×");fps_->setRange(1,1000);
        for (auto spin:{hotX_,hotY_,offsetX_,offsetY_}) spin->setRange(INT_MIN,INT_MAX);
        form->addRow("Zoom",zoom_);form->addRow("Direction",rotation_);form->addRow("Frames / second",fps_);form->addRow("Action frame (−1: none)",action_);
        form->addRow("Anchor X",hotX_);form->addRow("Anchor Y",hotY_);form->addRow("Offset X",offsetX_);form->addRow("Offset Y",offsetY_);
        panel->addLayout(form);panel->addWidget(mirror_);panel->addWidget(palette_);
        colors_->horizontalHeader()->hide();colors_->verticalHeader()->hide();colors_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        colors_->setSelectionMode(QAbstractItemView::SingleSelection);colors_->setFixedSize(16*18+2,16*18+2);
        colors_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);colors_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        colors_->setFixedSize(16*18+2*colors_->frameWidth(),16*18+2*colors_->frameWidth());
        colors_->horizontalHeader()->setMinimumSectionSize(18);colors_->verticalHeader()->setMinimumSectionSize(18);
        for (int i=0;i<16;++i) {colors_->setColumnWidth(i,18);colors_->setRowHeight(i,18);}
        for (int i=0;i<256;++i) colors_->setItem(i/16,i%16,new QTableWidgetItem);
        panel->addWidget(colors_);auto hint=new QLabel("Double-click a color to edit it. × is transparent.");hint->setWordWrap(true);panel->addWidget(hint);
        auto copyPalette=new QPushButton("Copy palette to another slot…");panel->addWidget(copyPalette);panel->addStretch();
        auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(controls_);dock->setWidget(scroll);addDockWidget(Qt::RightDockWidgetArea,dock);
        auto browserDock=new QDockWidget("Game ART resources",this);auto browser=new QWidget;auto browserLayout=new QVBoxLayout(browser);
        filter_->setPlaceholderText("Filter resource path");browserLayout->addWidget(filter_);browserLayout->addWidget(library_);browserDock->setWidget(browser);addDockWidget(Qt::LeftDockWidgetArea,browserDock);
        auto add=[this](QMenu* menu,const QString& label,const QKeySequence& key,const std::function<void()>& fn) {
            auto a=menu->addAction(label);a->setShortcut(key);connect(a,&QAction::triggered,this,[this,fn]{guard(fn);});
        };
        auto file=menuBar()->addMenu("File");
        add(file,"Open ART…",QKeySequence::Open,[this]{auto p=QFileDialog::getOpenFileName(this,"Open ART",{},"Arcanum art (*.art)");if(!p.isEmpty())openFile(p);});
        add(file,"Import PNG animation…",QKeySequence::New,[this]{importPng();});
        add(file,"Choose Arcanum installation…",{},[this]{auto p=QFileDialog::getExistingDirectory(this,"Arcanum installation",game_);if(!p.isEmpty())setInstallation(p);});
        file->addSeparator();add(file,"Save",QKeySequence::Save,[this]{save(false);});add(file,"Save as…",QKeySequence::SaveAs,[this]{save(true);});
        add(file,"Export frame as PNG…",{},[this]{exportPng(false);});add(file,"Export all frames as PNG…",{},[this]{exportPng(true);});
        file->addSeparator();add(file,"Quit",QKeySequence::Quit,[this]{close();});
        auto tools=menuBar()->addMenu("Tools");add(tools,"WorldED…",{},[this]{launch_editor(this,"worlded-explorer",game_);});add(tools,"Script Editor…",{},[this]{launch_editor(this,"script-editor",game_);});
        auto help=menuBar()->addMenu("Help");add(help,"ART editing guide",{},[this]{
            auto p=QCoreApplication::applicationDirPath()+"/docs/art-editor.md";
            if(QFileInfo::exists(p))QDesktopServices::openUrl(QUrl::fromLocalFile(p));
            else QMessageBox::information(this,"ART editing","Open an ART or import a PNG animation. Drag the cyan crosshair to set its anchor. Use Play, direction and palette to preview. See docs/art-editor.md in the source repository.");
        });
        connect(filter_,&QLineEdit::textChanged,this,[this]{refreshLibrary();});
        connect(library_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){auto path=item->text();guard([&]{openResource(path);});});
        for (auto spin:{frame_,zoom_}) connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,[this]{if(!refreshing_)guard([this]{refresh();});});
        connect(rotation_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{if(!refreshing_)guard([this]{refresh();});});
        connect(palette_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{if(!refreshing_)guard([this]{refreshPalette();refresh();});});
        connect(mirror_,&QCheckBox::toggled,this,[this]{if(!refreshing_)guard([this]{refresh();});});
        connect(timeline_,&QSlider::valueChanged,frame_,&QSpinBox::setValue);
        connect(play_,&QPushButton::clicked,this,[this]{if(!art_)return;if(timer_.isActive()){timer_.stop();play_->setText("Play");}else{timer_.start(std::max(1,1000/fps_->value()));play_->setText("Pause");}});
        connect(&timer_,&QTimer::timeout,this,[this]{frame_->setValue((frame_->value()+1)%art_->frames());});
        connect(fps_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int value){if(refreshing_||!art_)return;art_->set_fps(value);if(timer_.isActive())timer_.setInterval(std::max(1,1000/value));changed();});
        connect(action_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int value){if(refreshing_||!art_)return;art_->set_action_frame(value);changed();});
        for (auto spin:{hotX_,hotY_,offsetX_,offsetY_}) connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,[this]{
            if(refreshing_||!art_)return;
            art_->set_anchor(sourceRotation(),frame_->value(),hotX_->value(),hotY_->value());
            art_->set_offset(sourceRotation(),frame_->value(),offsetX_->value(),offsetY_->value());changed();refresh();
        });
        canvas_->moved=[this](QPoint point){if(!art_)return;timer_.stop();play_->setText("Play");
            auto r=sourceRotation();auto f=unsigned(frame_->value());int x=point.x();if(mirrored())x=int(art_->frame(r,f).width)-1-x;
            art_->set_anchor(r,f,x,point.y());changed();refresh();
        };
        connect(colors_,&QTableWidget::cellDoubleClicked,this,[this](int row,int col){guard([&]{
            if(!art_)return;auto p=art_->palette(selectedPalette());int i=row*16+col;
            auto color=QColorDialog::getColor(QColor::fromRgb(p[i]),this,QString("Palette index %1").arg(i));
            if(color.isValid()){p[i]=color.rgb()&0xffffff;art_->set_palette(selectedPalette(),p);changed();refreshPalette();refresh();}
        });});
        connect(copyPalette,&QPushButton::clicked,this,[this]{guard([this]{
            bool ok=false;int slot=QInputDialog::getInt(this,"Copy palette","Destination slot (0–3)",0,0,3,1,&ok);if(!ok)return;
            if(art_->palette_available(slot) && QMessageBox::question(this,"Replace palette","Replace the colors in this palette slot?")!=QMessageBox::Yes)return;
            auto colors=art_->palette(selectedPalette());art_->set_palette(slot,colors);
            refreshing_=true;palette_->clear();for(unsigned p=0;p<4;++p)if(art_->palette_available(p))palette_->addItem(QString("Palette %1").arg(p),p);
            palette_->setCurrentIndex(palette_->findData(slot));refreshing_=false;changed();refreshPalette();refresh();
        });});
        controls_->setEnabled(false);statusBar()->showMessage("Open an ART file, import PNG frames, or choose your Arcanum installation.");
    }
    void setInstallation(const QString& path) {
        auto next=std::make_shared<Installation>(std::filesystem::u8path(path.toUtf8().constData()));
        installation_=std::move(next);game_=QFileInfo(path).absoluteFilePath();refreshLibrary();
    }
    void openResource(const QString& resource) {
        if (!installation_) throw std::runtime_error("Choose an Arcanum installation first");
        Art next(installation_->read(utf8_cp1252(resource.toStdString())));next.validate();if(!leave())return;
        installDocument(std::move(next),QFileInfo(resource).fileName(),{},false);
    }
    void openFile(const QString& path) {
        QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>qint64(resource_limit))throw std::runtime_error("Cannot read ART file");
        auto bytes=file.readAll();Art next(Bytes(bytes.begin(),bytes.end()));next.validate();if(!leave())return;
        installDocument(std::move(next),QFileInfo(path).fileName(),QFileInfo(path).absoluteFilePath(),false);
    }
    void save(bool choose) {
        if(!art_)return;auto path=path_;
        if(choose || path.isEmpty())path=QFileDialog::getSaveFileName(this,"Save ART",path.isEmpty()?name_:path,"Arcanum art (*.art)");
        if(path.isEmpty())return;
        if(!path.endsWith(".art",Qt::CaseInsensitive))path+=".art";
        art_->validate();const auto& bytes=art_->encode();QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(reinterpret_cast<const char*>(bytes.data()),qint64(bytes.size()))!=qint64(bytes.size()) || !file.commit())
            throw std::runtime_error("Cannot save ART: "+file.errorString().toStdString());
        path_=QFileInfo(path).absoluteFilePath();name_=QFileInfo(path).fileName();dirty_=false;setWindowTitle(name_+" — ART Editor Renaissance");
    }
    void loadExample() {
        QImage a(64,64,QImage::Format_ARGB32);a.fill(Qt::transparent);QPainter p(&a);p.fillRect(12,14,40,40,QColor("#d5a64c"));p.fillRect(25,6,14,52,QColor("#65bbd4"));p.end();
        installDocument(art_from_pngs({a,a},1,10),"example.art",{},false);
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        try {if(leave())event->accept();else event->ignore();}catch(const std::exception& e){event->ignore();QMessageBox::warning(this,"Cannot save",e.what());}
    }
};
}
