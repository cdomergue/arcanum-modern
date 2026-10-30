#pragma once
#include <QtWidgets>
#include <functional>
#include <set>
class WorldView : public QGraphicsView {
public:
    std::function<void(QPoint)> openSector;
    std::function<void(const QString&)> feedback;
    unsigned worldWidth=0,worldHeight=0;
    std::set<std::uint64_t> storedSectors;
    bool showProvenance=true;
    QImage provenance;
    QPoint sectorAt(QPointF p) const {
        if (p.x()<0 || p.y()<0 || p.x()>=worldWidth || p.y()>=worldHeight) return {-1,-1};
        int x=int(worldWidth)-int(std::floor(p.x())), y=int(std::floor(p.y()));
        return x<int(worldWidth) ? QPoint(x,y) : QPoint(-1,-1);
    }
    bool stored(QPoint sector) const { return storedSectors.count(std::uint64_t(sector.x())|(std::uint64_t(sector.y())<<26)); }
    explicit WorldView(QWidget* parent=nullptr) : QGraphicsView(parent) {
        setScene(new QGraphicsScene(this)); setDragMode(ScrollHandDrag); setMouseTracking(true); setTransformationAnchor(AnchorUnderMouse);
        setBackgroundBrush(QColor("#141b23"));
    }
    void mousePressEvent(QMouseEvent* event) override { start=event->pos(); QGraphicsView::mousePressEvent(event); }
    void mouseReleaseEvent(QMouseEvent* event) override {
        QGraphicsView::mouseReleaseEvent(event);
        auto sector=sectorAt(mapToScene(event->pos()));
        if (event->button()==Qt::LeftButton && (event->pos()-start).manhattanLength()<5 && sector.x()>=0 && openSector) openSector(sector);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        QGraphicsView::mouseMoveEvent(event); auto sector=sectorAt(mapToScene(event->pos()));
        if (feedback && sector.x()>=0)
            feedback(QString("Sector %1, %2 · %3 · tile %4, %5").arg(sector.x()).arg(sector.y())
                .arg(stored(sector) ? "stored (.sec)" : "generated terrain (if defined)").arg(sector.x()*64+32).arg(sector.y()*64+32));
    }
    void drawForeground(QPainter* painter,const QRectF&) override {
        if (!showProvenance || provenance.isNull()) return;
        painter->save(); painter->setOpacity(.65); painter->drawImage(QPointF(0,0),provenance);
        // A sector is one map pixel. Keep it discoverable when the entire world
        // is zoomed out, then show its exact footprint as the user zooms in.
        if(transform().m11()<1) {
            painter->setOpacity(1); painter->setPen(QPen(QColor("#008cff"),0)); painter->setBrush(Qt::NoBrush);
            auto radius=2/transform().m11();
            for(auto id : storedSectors) painter->drawEllipse(QPointF(worldWidth-(id&0x3ffffff)+.5,(id>>26)+.5),radius,radius);
        }
        painter->restore();
    }
    void wheelEvent(QWheelEvent* event) override {
        double f=event->angleDelta().y()>0 ? 1.4 : 1/1.4;
        if (transform().m11()*f>.1 && transform().m11()*f<32) scale(f,f);
        event->accept();
    }
private:
    QPoint start;
};
