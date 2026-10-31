#pragma once
#include <set>
#include "scene_assets.hpp"
#include <QtWidgets>
#include <cmath>
#include <functional>

inline constexpr const char* objectMime = "application/x-worlded-palette-object";
class ObjectPalette : public QListWidget {
public:
    std::function<void()> visibleItemsChanged;
    explicit ObjectPalette(QWidget* parent=nullptr) : QListWidget(parent) {
        setViewMode(IconMode); setResizeMode(Adjust); setMovement(Static);
        setIconSize({72,64}); setGridSize({110,104}); setSpacing(4);
        setWordWrap(true); setDragEnabled(true); setSelectionMode(SingleSelection);
    }
    void startDrag(Qt::DropActions) override {
        if (!currentItem()) return;
        QDrag drag(this); auto mime=new QMimeData;
        mime->setData(objectMime,QByteArray::number(currentItem()->data(Qt::UserRole).toInt())); drag.setMimeData(mime);
        auto icon=currentItem()->icon().pixmap(iconSize()); drag.setPixmap(icon); drag.setHotSpot(icon.rect().center());
        drag.exec(Qt::CopyAction);
    }
    void resizeEvent(QResizeEvent* event) override {
        QListWidget::resizeEvent(event);
        if(visibleItemsChanged) visibleItemsChanged();
    }
};

class Canvas : public QGraphicsView {
public:
    QBrush checkerBrush;
    bool editable=false, selectionOnly=false, showGrid=false;
    int layer=0; // Objects, ground, roofs, characters, scripts, blocking.
    std::function<void(QPoint)> toggleBlock;
    QRect cellSelection;
    bool brushEnabled=false;int brushSize=1;
    std::function<void(const std::set<std::pair<int,int>>&)> strokeFinished;
    std::function<void(QRect)> cellsSelected;
    std::function<void(int)> inspectContext;
    std::function<void(int)> activateObject;
    QPoint surfaceTile(QPoint point) const {auto tile=tileAt(mapToScene(point)+(layer==2?QPointF(0,160):QPointF{}));if(layer==2)tile=QPoint(int(std::floor(tile.x()/4.))*4,int(std::floor(tile.y()/4.))*4);return tile;}
    std::function<void()> copyObjects,pasteObjects;
    std::function<void()> focused;
    std::function<void(QPoint)> contextMenu;
    std::function<bool(QPoint)> placeSelection;
    QGraphicsItemGroup* placementGhost=nullptr;
    QSize placementSize;
    QPoint placementTile;
    std::function<Sprite(int)> paletteSprite;
    std::function<bool(int)> canDropObject;
    std::function<void(int,QPoint)> addObject;
    std::function<void(const std::vector<int>&,QPoint)> moveObjects;
    std::function<bool(int)> inventoryItem,inventoryOwner;
    std::function<void(const std::vector<int>&,int)> dropInventory;
    int inventoryTarget(QPoint point) const {
        for(auto item:items(point))if(item->data(0).isValid()){
            int index=item->data(0).toInt();bool moving=false;for(const auto& entry:dragged)if(entry.index==index)moving=true;
            if(!moving && inventoryOwner && inventoryOwner(index))return index;
        }return -1;
    }
    std::function<void(const QString&)> feedback;
    std::function<void()> deleteObjects, duplicateObjects, undoEdit, redoEdit;
    explicit Canvas(QGraphicsScene* scene) : QGraphicsView(scene) {
        setDragMode(ScrollHandDrag); setAcceptDrops(true); setMouseTracking(true);
        setTransformationAnchor(AnchorUnderMouse); setFocusPolicy(Qt::StrongFocus);
        setRenderHint(QPainter::SmoothPixmapTransform,false); setViewportUpdateMode(BoundingRectViewportUpdate);
        QPixmap checker(32,32); checker.fill(QColor("#20252c"));
        QPainter painter(&checker); painter.fillRect(0,0,16,16,QColor("#252b33")); painter.fillRect(16,16,16,16,QColor("#252b33"));
        checkerBrush=QBrush(checker); setBackgroundBrush(checkerBrush);
    }
    static QPointF project(QPoint tile) { return {(tile.y()-tile.x())*40.0,(tile.x()+tile.y())*20.0+20}; }
    static QPoint tileAt(QPointF scenePoint) {
        return {int(std::floor((scenePoint.y()-20)/40-scenePoint.x()/80+.5)),
                int(std::floor((scenePoint.y()-20)/40+scenePoint.x()/80+.5))};
    }
    static bool valid(QPoint tile) { return tile.x()>=0 && tile.y()>=0 && tile.x()<64 && tile.y()<64; }
    void setEditing(bool value) { editable=value; setDragMode((value || selectionOnly) ? RubberBandDrag : ScrollHandDrag); viewport()->update(); }
    void setSelectionOnly(bool value){selectionOnly=value;setEditing(false);}
    bool placementValid(QPoint p) const {return valid(p) && p.x()+placementSize.width()<=64 && p.y()+placementSize.height()<=64;}
    void beginPlacement(QGraphicsItemGroup* group,QSize size) {placementGhost=group;placementSize=size;placementGhost->setOpacity(.6);placementGhost->setZValue(100000);setFocus();setCursor(Qt::CrossCursor);updatePlacement(viewport()->rect().center());}
    void updatePlacement(QPoint point){if(!placementGhost)return;placementTile=surfaceTile(point);placementGhost->setPos(project(placementTile)-project({0,0}));placementGhost->setOpacity(placementValid(placementTile)?.65:.2);viewport()->update();}
    void cancelPlacement(){if(placementGhost){scene()->removeItem(placementGhost);delete placementGhost;placementGhost=nullptr;}unsetCursor();viewport()->update();}
    void fit() { if (!scene()->items().empty()) fitInView(scene()->itemsBoundingRect().adjusted(-25,-25,25,25),Qt::KeepAspectRatio); }
    bool interacting() const { return !dragged.empty() || panning || ghost || painting || selectingCells; }
    void cancelInteraction() {
        for (auto& entry : dragged) { entry.item->setPos(entry.position); entry.item->setZValue(entry.z); }
        dragged.clear(); delta={}; panning=false; selectingCells=false; painting=false;stroke.clear(); space=false; unsetCursor(); clearGhost(); cancelPlacement(); viewport()->update();
    }
    void wheelEvent(QWheelEvent* event) override {
        if (!dragged.empty()) { event->accept(); return; }
        double factor=event->angleDelta().y()>0 ? 1.2 : 1/1.2;
        if (transform().m11()*factor>=.025 && transform().m11()*factor<=32) scale(factor,factor);
        event->accept();
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if(event->button()==Qt::LeftButton && editable && (layer==0 || layer==3))
            for(auto item:items(event->pos()))if(item->data(0).isValid() && (item->flags()&QGraphicsItem::ItemIsSelectable)) {
                if(activateObject){activateObject(item->data(0).toInt());event->accept();return;}
            }
        QGraphicsView::mouseDoubleClickEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override {
        setFocus();if(focused)focused();
        if(placementGhost && event->button()==Qt::LeftButton){auto tile=surfaceTile(event->pos());if(placementValid(tile)){cancelPlacement();if(placeSelection)placeSelection(tile);}else if(feedback)feedback("The selection extends beyond the sector. Choose another position.");event->accept();return;}
        if (event->button()==Qt::MiddleButton || (space && event->button()==Qt::LeftButton)) {
            panning=true; panPoint=event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return;
        }
        if(layer==5 && event->button()==Qt::LeftButton) {
            auto tile=surfaceTile(event->pos());
            if(editable && valid(tile) && toggleBlock)toggleBlock(tile);
            event->accept();return;
        }
        if((editable || selectionOnly) && event->button()==Qt::LeftButton && (layer==1 || layer==2)) {
            cellStart=surfaceTile(event->pos());if(valid(cellStart)){if(editable && brushEnabled){painting=true;stroke.clear();lastBrush=cellStart;addBrush(cellStart);}else{selectingCells=true;updateCells(event->pos());}}event->accept();return;
        }
        if(event->button()==Qt::LeftButton && (layer==0 || layer==3 || layer==4))for(auto item:items(event->pos()))if(item->data(2).isValid()) {
            if(inspectContext)inspectContext(item->data(2).toInt());
            event->accept();return;
        }
        if ((editable || selectionOnly) && event->button()==Qt::LeftButton) {
            QGraphicsItem* hit=nullptr;
            for (auto item : items(event->pos())) if (item->data(0).isValid() && (item->flags()&QGraphicsItem::ItemIsSelectable)) { hit=item; break; }
            if (hit) {
                bool inventoryGesture=editable && (event->modifiers()&Qt::ShiftModifier) && inventoryItem && inventoryItem(hit->data(0).toInt());
                bool multi=(event->modifiers()&Qt::ControlModifier) || ((event->modifiers()&Qt::ShiftModifier) && !inventoryGesture);
                if (multi && hit->isSelected()) { hit->setSelected(false); event->accept(); return; }
                if (!hit->isSelected()) { if (!multi) scene()->clearSelection(); hit->setSelected(true); }
                if(selectionOnly){event->accept();return;}
                dragStart=mapToScene(event->pos()); pressPoint=event->pos(); delta={}; dragValid=true; dragging=false;
                for (auto item : scene()->selectedItems()) if (item->data(0).isValid())
                    dragged.push_back({item,item->pos(),item->data(1).toPoint(),item->zValue(),item->data(0).toInt()});
                event->accept(); return;
            }
        }
        QGraphicsView::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(placementGhost){updatePlacement(event->pos());event->accept();return;}
        if (panning) {
            auto distance=event->pos()-panPoint; panPoint=event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value()-distance.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value()-distance.y()); event->accept(); return;
        }
        if(selectingCells){updateCells(event->pos());event->accept();return;}
        if(painting){addBrush(surfaceTile(event->pos()));event->accept();return;}
        if (!dragged.empty()) {
            if (!dragging && (event->pos()-pressPoint).manhattanLength()<QApplication::startDragDistance()) return;
            dragging=true; setCursor(Qt::ClosedHandCursor);
            auto shift=mapToScene(event->pos())-dragStart;
            delta={qRound(shift.y()/40-shift.x()/80),qRound(shift.y()/40+shift.x()/80)};
            dragValid=true; for (const auto& entry : dragged) if (!valid(entry.tile+delta)) dragValid=false;
            auto displacement=project(delta)-project({0,0});
            for (const auto& entry : dragged) {
                entry.item->setPos(entry.position+(dragValid ? displacement : QPointF{}));
                entry.item->setZValue(entry.z+(dragValid ? displacement.y() : 0));
            }
            if((event->modifiers()&Qt::ShiftModifier) && inventoryItem && std::any_of(dragged.begin(),dragged.end(),[&](const auto& entry){return inventoryItem(entry.index);})){
                bool target=inventoryTarget(event->pos())>=0;setCursor(target?Qt::DragMoveCursor:Qt::ForbiddenCursor);
                if(feedback)feedback(target?"Release Shift-drag to put items in this inventory":"Shift-drag: drop onto a project character or container · Esc to cancel");
            }else if (feedback) feedback(dragValid ? QString("Move %1 object(s) · release to place · Esc to cancel").arg(dragged.size()) : "Outside the sector · move refused");
            viewport()->update(); event->accept(); return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (panning && (event->button()==Qt::MiddleButton || event->button()==Qt::LeftButton)) { panning=false; unsetCursor(); event->accept(); return; }
        if(selectingCells && event->button()==Qt::LeftButton){updateCells(event->pos());selectingCells=false;if(cellsSelected)cellsSelected(cellSelection);event->accept();return;}
        if(painting && event->button()==Qt::LeftButton){addBrush(surfaceTile(event->pos()));painting=false;auto cells=std::move(stroke);stroke.clear();if(strokeFinished)strokeFinished(cells);viewport()->update();event->accept();return;}
        if (!dragged.empty() && event->button()==Qt::LeftButton) {
            bool transfer=dragging && (event->modifiers()&Qt::ShiftModifier) && inventoryItem && std::any_of(dragged.begin(),dragged.end(),[&](const auto& entry){return inventoryItem(entry.index);});
            int owner=transfer?inventoryTarget(event->pos()):-1;
            std::vector<int> indices; for (const auto& entry : dragged) { indices.push_back(entry.index); entry.item->setPos(entry.position); entry.item->setZValue(entry.z); }
            auto move=delta; bool commit=dragging && dragValid && !move.isNull();
            dragged.clear(); delta={}; unsetCursor(); viewport()->update();
            if(transfer){if(owner>=0 && dropInventory)dropInventory(indices,owner);else if(feedback)feedback("No project character or container under the pointer. Items have not been moved.");}
            else if (commit && moveObjects) moveObjects(indices,move);
            event->accept(); return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent* event) override {if(contextMenu){contextMenu(event->pos());event->accept();}else QGraphicsView::contextMenuEvent(event);}
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key()==Qt::Key_Escape) { cancelInteraction();cellSelection={};viewport()->update(); event->accept(); return; }
        if (event->key()==Qt::Key_Space) { space=true; setCursor(Qt::OpenHandCursor); event->accept(); return; }
        if((editable || selectionOnly) && !interacting() && event->matches(QKeySequence::Copy) && copyObjects){copyObjects();event->accept();return;}
        if(editable && !interacting() && event->matches(QKeySequence::Paste) && pasteObjects){pasteObjects();event->accept();return;}
        if (editable && !interacting()) {
            if (event->key()==Qt::Key_Delete && deleteObjects) { deleteObjects(); event->accept(); return; }
            if (event->matches(QKeySequence::Undo) && undoEdit) { undoEdit(); event->accept(); return; }
            if (event->matches(QKeySequence::Redo) && redoEdit) { redoEdit(); event->accept(); return; }
            if (event->key()==Qt::Key_D && event->modifiers().testFlag(Qt::ControlModifier) && duplicateObjects) { duplicateObjects(); event->accept(); return; }
        }
        QGraphicsView::keyPressEvent(event);
    }
    void keyReleaseEvent(QKeyEvent* event) override {
        if (event->key()==Qt::Key_Space) { space=false; if (!panning) unsetCursor(); event->accept(); return; }
        QGraphicsView::keyReleaseEvent(event);
    }
    void focusOutEvent(QFocusEvent* event) override { cancelInteraction(); QGraphicsView::focusOutEvent(event); }
    void dragEnterEvent(QDragEnterEvent* event) override {
        bool ok=false;auto index=event->mimeData()->data(objectMime).toInt(&ok);
        if (editable && (layer==0 || layer==3) && event->mimeData()->hasFormat(objectMime) && ok && canDropObject && canDropObject(index)) { event->setDropAction(Qt::CopyAction); event->accept(); }
        else event->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* event) override {
        if (!editable || (layer!=0 && layer!=3) || !event->mimeData()->hasFormat(objectMime) || !paletteSprite) { event->ignore(); return; }
        bool ok=false; int index=event->mimeData()->data(objectMime).toInt(&ok);
        if (!ok || !canDropObject || !canDropObject(index)) { event->ignore(); return; }
        try {
            if (!ghost || ghostIndex!=index) { clearGhost(); auto sprite=paletteSprite(index); ghost=scene()->addPixmap(sprite.pixels); ghostHotspot=sprite.hotspot; ghostIndex=index; ghost->setOpacity(.65); ghost->setZValue(100000); }
            ghostTile=tileAt(mapToScene(event->position().toPoint())); ghost->setPos(project(ghostTile)-ghostHotspot);
            ghostValid=valid(ghostTile); viewport()->update();
            if (ghostValid) { event->setDropAction(Qt::CopyAction); event->accept(); }
            else event->ignore();
        } catch (const std::exception&) { clearGhost(); event->ignore(); }
    }
    void dragLeaveEvent(QDragLeaveEvent* event) override { clearGhost(); event->accept(); }
    void dropEvent(QDropEvent* event) override {
        bool ok=false; int index=event->mimeData()->data(objectMime).toInt(&ok);
        auto tile=tileAt(mapToScene(event->position().toPoint())); clearGhost();
        if (editable && (layer==0 || layer==3) && ok && valid(tile) && event->mimeData()->hasFormat(objectMime) && addObject && paletteSprite && canDropObject && canDropObject(index)) {
            try {
                paletteSprite(index);
                addObject(index,tile); event->setDropAction(Qt::CopyAction); event->accept(); setFocus();
            } catch (const std::exception& e) { if(feedback)feedback(QString("Cannot place: ")+QString::fromUtf8(e.what()));event->ignore(); }
        } else event->ignore();
    }
    void drawForeground(QPainter* painter,const QRectF& rect) override {
        QGraphicsView::drawForeground(painter,rect);
        if (!editable && !selectionOnly) return;
        if(layer==1 || layer==2) {
            painter->save();if(layer==2)painter->translate(0,-160);
            auto outline=[&](QRect r){QPolygonF p;p<<project(r.topLeft())+QPointF(0,-20)<<project({r.right(),r.top()})+QPointF(-40,0)<<project(r.bottomRight())+QPointF(0,20)<<project({r.left(),r.bottom()})+QPointF(40,0);return p;};
            painter->setPen(QPen(QColor(150,200,225,90),0));painter->setBrush(Qt::NoBrush);int step=layer==2?4:1;
            if(showGrid || layer==2)for(int y=0;y<64;y+=step)for(int x=0;x<64;x+=step)painter->drawPolygon(outline({x,y,step,step}));
            if(!cellSelection.isEmpty()){painter->setPen(QPen(QColor("#7ed5ff"),0));painter->setBrush(QColor(70,170,255,75));painter->drawPolygon(outline(cellSelection));}
            painter->setPen(QPen(QColor("#ffe59a"),0));painter->setBrush(QColor(255,190,70,85));for(auto [x,y]:stroke)painter->drawPolygon(outline({x,y,step,step}));
            painter->restore();
        }else if (showGrid) {
            painter->setPen(QPen(QColor(150,200,225,65),0));
            auto corner=[](double x,double y){ return QPointF((y-x)*40,(x+y)*20+20); };
            for (int i=0;i<=64;++i) { double n=i-.5; painter->drawLine(corner(n,-.5),corner(n,63.5)); painter->drawLine(corner(-.5,n),corner(63.5,n)); }
        }
        painter->setBrush(Qt::NoBrush); painter->setPen(QPen(QColor("#7ed5ff"),0));
        for (auto item : scene()->selectedItems()) if (item->data(0).isValid()) painter->drawRect(item->sceneBoundingRect().adjusted(-2,-2,2,2));
        if(placementGhost){auto p=placementTile;QPolygonF outline;outline<<project(p)+QPointF(0,-20)<<project(p+QPoint(placementSize.width()-1,0))+QPointF(-40,0)<<project(p+QPoint(placementSize.width()-1,placementSize.height()-1))+QPointF(0,20)<<project(p+QPoint(0,placementSize.height()-1))+QPointF(40,0);painter->setPen(QPen(placementValid(p)?QColor("#7ed5ff"):QColor("#ff7b7b"),0));painter->drawPolygon(outline);}
        if (ghost) {
            auto p=project(ghostTile); QPolygonF diamond; diamond<<p+QPointF(0,-20)<<p+QPointF(40,0)<<p+QPointF(0,20)<<p+QPointF(-40,0);
            painter->setPen(QPen(ghostValid ? QColor("#7ed5ff") : QColor("#ff7b7b"),0)); painter->drawPolygon(diamond);
        }
    }
private:
    bool selectingCells=false;QPoint cellStart;
    bool painting=false;QPoint lastBrush;std::set<std::pair<int,int>> stroke;
    void addBrush(QPoint tile){int step=layer==2?4:1;int steps=std::max(std::abs(tile.x()-lastBrush.x()),std::abs(tile.y()-lastBrush.y()))/step;
        for(int n=0;n<=steps;n++){auto p=steps?lastBrush+(tile-lastBrush)*n/steps:tile;p=QPoint(p.x()/step*step,p.y()/step*step);
            for(int y=0;y<brushSize;y++)for(int x=0;x<brushSize;x++){auto cell=p+QPoint(x*step,y*step);if(valid(cell))stroke.insert({cell.x(),cell.y()});}}
        lastBrush=tile;viewport()->update();}
    void updateCells(QPoint point){auto end=surfaceTile(point);int step=layer==2?4:1;end.setX(std::clamp(end.x(),0,64-step));end.setY(std::clamp(end.y(),0,64-step));cellSelection=QRect(cellStart,end).normalized();cellSelection.setRight(cellSelection.right()+step-1);cellSelection.setBottom(cellSelection.bottom()+step-1);viewport()->update();}
    struct Dragged { QGraphicsItem* item; QPointF position; QPoint tile; qreal z; int index; };
    std::vector<Dragged> dragged;
    QPointF dragStart; QPoint pressPoint,panPoint,delta,ghostTile;
    bool dragging=false,dragValid=true,panning=false,space=false,ghostValid=false;
    QGraphicsPixmapItem* ghost=nullptr;
    QPoint ghostHotspot; int ghostIndex=-1;
    void clearGhost() { if (ghost) { scene()->removeItem(ghost); delete ghost; ghost=nullptr; } ghostIndex=-1; viewport()->update(); }
};
