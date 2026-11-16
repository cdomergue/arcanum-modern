#include "src/art_editor.hpp"
#include <QtTest>
#include <iostream>
#include <random>

using namespace worlded;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F fn) {bool caught=false;try{fn();}catch(const std::exception&){caught=true;}require(caught,"Malformed ART accepted");}
void put(Bytes& b,std::size_t at,std::uint32_t n) {for(unsigned i=0;i<4;++i)b.at(at+i)=std::uint8_t(n>>(8*i));}
std::uint32_t get(const Bytes& b,std::size_t at) {return b.at(at)|(b.at(at+1)<<8)|(b.at(at+2)<<16)|(std::uint32_t(b.at(at+3))<<24);}

void formats() {
    // Independent fixture, literal + repeat packets, signed metadata, nonzero
    // pointer stubs and trailing data: the editor must retain all of them.
    Bytes bytes(132+1024+28,0);put(bytes,0,0x41);put(bytes,4,12);put(bytes,8,8);
    put(bytes,12,0x12345678);put(bytes,28,0xffffffff);put(bytes,32,1);put(bytes,36,0xabcdef);
    put(bytes,68,8);put(bytes,100,0xfedcba);put(bytes,132+4,0x123456);put(bytes,132+8,0xabcdef);
    const std::size_t header=132+1024;
    put(bytes,header,4);put(bytes,header+4,2);put(bytes,header+8,6);put(bytes,header+12,0xfffffffd);
    put(bytes,header+16,2);put(bytes,header+20,0xfffffffb);put(bytes,header+24,6);
    bytes.insert(bytes.end(),{3,1,0x82,0,2,3,2,0xee}); // Seven encoded bytes, one unused trailing byte.
    put(bytes,header+8,7);
    Art art(bytes);art.validate();require(art.encode()==bytes,"Read/write changed bytes");
    require(art.indexed(0,0)==Bytes({1,1,1,0,2,2,2,2}),"RLE decoding");
    require(art.flags()==0x41 && art.action_frame()==-1,"Header metadata");
    auto rgba=art.rgba(0,0);require(rgba[0]==0x12 && rgba[1]==0x34 && rgba[2]==0x56 && rgba[15]==0,"Palette and transparency");
    art.set_anchor(0,0,-7,15);art.set_offset(0,0,-11,21);art.set_fps(17);art.set_action_frame(0);
    auto expected=bytes;put(expected,header+12,std::uint32_t(-7));put(expected,header+16,15);
    put(expected,header+20,std::uint32_t(-11));put(expected,header+24,21);put(expected,4,17);put(expected,28,0);
    require(art.encode()==expected,"Metadata edits changed unrelated bytes");
    auto p=art.palette(0);p[1]=0xdeadbeef;art.set_palette(3,p);art.set_palette(1,p);art.set_palette(2,p);
    Art reread(art.encode());reread.validate();require(reread.palette(3)[1]==0xdeadbeef,"Extra palettes lost");
    require(reread.indexed(0,0)==art.indexed(0,0) && reread.frame(0,0).hot_x==-7,"Adding palettes shifted frame data incorrectly");
    auto bad=bytes;put(bad,header+8,2);bad[header+28]=0;rejects([&]{Art(bad).validate();});
    bad=bytes;bad.resize(header+28+6);rejects([&]{Art(bad).validate();});
    bad=bytes;put(bad,header,0xffffffff);rejects([&]{(void)Art(bad);});
    bad=bytes;put(bad,32,0);rejects([&]{(void)Art(bad);});
    bad=bytes;bad[header+28]=127;rejects([&]{Art(bad).validate();});
    bad=bytes;put(bad,header+8,2);bad[header+28]=1;rejects([&]{Art(bad).validate();});
    bad=bytes;put(bad,header+8,0);require(Art(bad).indexed(0,0)==Bytes(8,0),"Empty legacy frame");
    rejects([&]{art.set_action_frame(1);});rejects([&]{art.set_fps(0);});rejects([&]{art.set_palette(4,p);});
    rejects([&]{art.indexed(8,0);});
    std::vector<ArtFrame> frames;std::vector<Bytes> pixels;std::mt19937 rng(117);
    for(unsigned i=0;i<16;++i) {
        frames.push_back({300,2,0,int(i)-8,16,-2,3});Bytes indices(600,1);
        if(i%2)for(auto& value:indices)value=std::uint8_t(rng());
        pixels.push_back(indices);
    }
    auto generated=Art::create(8,24,p,frames,pixels);auto roundtrip=Art(generated.encode());roundtrip.validate();
    require(roundtrip.rotations()==8 && roundtrip.frames()==2,"Direction/frame count");
    require(roundtrip.frame(0,0).encoded_size<600 && roundtrip.frame(0,1).encoded_size==600,"RLE/raw selection");
    for(unsigned r=0;r<8;++r)for(unsigned f=0;f<2;++f)require(roundtrip.indexed(r,f)==pixels[r*2+f],"Encoded pixels differ");
    require(get(generated.encode(),68)>0 && get(generated.encode(),96)>0,"Rotation size table missing");
    rejects([&]{Art::create(3,10,p,frames,pixels);});
    std::cout<<"ART: lossless edits, four palettes, RLE/raw, eight directions, malformed input passed\n";
}
void pngs() {
    QImage image(256,1,QImage::Format_ARGB32);
    image.setPixel(0,0,qRgba(255,123,45,0));
    for(int x=1;x<256;++x)image.setPixel(x,0,qRgb(x,255-x,42));
    auto art=art_from_pngs({image,image},1,10);auto out=art_image(art,0,0);
    require(out.pixelColor(0,0).alpha()==0,"PNG transparency");
    for(int x=1;x<256;++x)require(out.pixelColor(x,0)==image.pixelColor(x,0),"255-color PNG must be lossless");
    require(art.indexed(0,0)==art.indexed(0,1),"Animation palette differs between frames");
    QTemporaryDir temp;auto path=temp.filePath("roundtrip.png");require(out.save(path),"PNG export");
    auto imported=art_from_pngs({QImage(path)},1,10);require(art_image(imported,0,0)==out,"PNG export/import");
    QImage many(64,64,QImage::Format_ARGB32);for(int y=0;y<64;++y)for(int x=0;x<64;++x)many.setPixel(x,y,qRgb(x*4,y*4,(x+y)*2));
    auto quantized=art_from_pngs({many,many},1,10);require(quantized.indexed(0,0)==quantized.indexed(0,1),"Quantization must be deterministic");
    for(auto index:quantized.indexed(0,0))require(index!=0,"Opaque color became transparent");
    rejects([&]{art_from_pngs({image},8,10);});
    QImage empty(1,1,QImage::Format_ARGB32);empty.fill(Qt::transparent);require(art_from_pngs({empty},1,10).indexed(0,0)==Bytes{0},"All transparent image");
    std::cout<<"PNG: exact colors, alpha, shared quantization and export/import passed\n";
}
void gui() {
    QTemporaryDir temp;QImage image(32,32,QImage::Format_ARGB32);image.fill(QColor("#cda561"));
    std::vector<QImage> images(8,image);auto art=art_from_pngs(images,8,12);auto input=temp.filePath("test.art");
    QFile file(input);require(file.open(QIODevice::WriteOnly),"Fixture write");file.write(reinterpret_cast<const char*>(art.encode().data()),qint64(art.encode().size()));file.close();
    ArtWindow window;window.openFile(input);window.show();QApplication::processEvents();
    auto direction=window.findChild<QComboBox*>("direction");auto hot=window.findChild<QSpinBox*>("hotX");
    require(direction && hot && window.findChild<QTableWidget*>("paletteGrid")->rowCount()==16,"Editor controls missing");
    direction->setCurrentIndex(7);hot->setValue(-12);window.save(false);
    file.open(QIODevice::ReadOnly);auto bytes=file.readAll();file.close();Art edited(Bytes(bytes.begin(),bytes.end()));
    require(edited.frame(7,0).hot_x==-12 && edited.frame(0,0).hot_x==16,"UI edited wrong direction");
    auto mirror=window.findChildren<QCheckBox*>().front();mirror->setChecked(true);direction->setCurrentIndex(1);
    require(hot->value()==-12,"Critter mirror selects direction 7");hot->setValue(10);window.save(false);
    auto view=window.findChild<QGraphicsView*>();auto start=view->mapFromScene(QPointF(21.5,31.5));
    auto end=view->mapFromScene(QPointF(17.5,27.5));
    QTest::mousePress(view->viewport(),Qt::LeftButton,Qt::NoModifier,start);
    QTest::mouseMove(view->viewport(),end);QTest::mouseRelease(view->viewport(),Qt::LeftButton,Qt::NoModifier,end);
    require(hot->value()==14,"Drag anchor in mirrored preview");window.save(false);
    window.findChild<QSpinBox*>("zoom")->setValue(16);QApplication::processEvents();
    require(view->transform().m11()==16,"Pixel zoom");
    require(QDir(temp.path()).mkpath("data/art"),"Resource directory");
    require(QFile::copy(input,temp.filePath("data/art/épreuve.art")),"Accented resource fixture");
    window.setInstallation(temp.path());window.openResource("art/épreuve.art");
    require(window.windowTitle().startsWith("épreuve.art"),"Accented game resource path");window.close();
    std::cout<<"GUI: direction edits, mirroring, crosshair drag, zoom and atomic save passed\n";
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        formats();pngs();gui();
        auto args=app.arguments();auto game=args.indexOf("--game");
        if(game>=0 && game+1<args.size()) {
            Installation installation(std::filesystem::u8path(args[game+1].toUtf8().constData()));unsigned count=0;
            for(const auto& [name,sources]:installation.resources())if(std::filesystem::path(name).extension()==".art") {
                auto bytes=installation.read(name);Art art(bytes);art.validate();require(art.encode()==bytes,"Installed ART roundtrip differs");
                if(++count==1000)break;
            }
            require(count>0,"No installed ART found");std::cout<<count<<" installed ART files validated without modification\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
