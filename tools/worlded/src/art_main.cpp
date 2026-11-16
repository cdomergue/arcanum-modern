#include "art_editor.hpp"
#include <iostream>

int main(int argc,char** argv) {
    for(int i=1;i<argc;++i) if(std::string(argv[i])=="--help") {
        std::cout<<"ART Editor Renaissance\n  art-editor [FILE.art] [--game DIR] [--resource art/PATH.art]\n  art-editor --smoke-test\n";return 0;
    }
    QApplication app(argc,argv);QApplication::setApplicationName("ART Editor Renaissance");
    worlded::ArtWindow window;auto args=app.arguments();
    try {
        QString game,resource,file,snapshot;bool smoke=false;
        for(int i=1;i<args.size();++i) {
            auto key=args[i];
            if(key=="--game" || key=="--resource" || key=="--snapshot") {
                if(++i>=args.size())throw std::runtime_error("Missing argument for "+key.toStdString());
                if(key=="--game")game=args[i];else if(key=="--resource")resource=args[i];else snapshot=args[i];
            } else if(key=="--smoke-test")smoke=true;
            else if(key.startsWith('-') || !file.isEmpty())throw std::runtime_error("Unexpected argument: "+key.toStdString());
            else file=key;
        }
        if(!game.isEmpty())window.setInstallation(game);
        if(!resource.isEmpty())window.openResource(resource);
        else if(!file.isEmpty())window.openFile(file);
        else if(smoke)window.loadExample();
        window.show();app.processEvents();
        if(!snapshot.isEmpty() && !window.grab().save(snapshot))throw std::runtime_error("Cannot write screenshot");
        if(smoke || !snapshot.isEmpty())return 0;
        return app.exec();
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
