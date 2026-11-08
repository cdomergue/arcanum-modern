#pragma once
#include "worlded/assets.hpp"
#include "../../../source/src/game/proto_defaults.h"
#include <charconv>
#include <set>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

// Counts and outcomes only: no object names, file contents or personal saves.
inline void log_prototype_catalog(const QString& outcome,QString root={}) {
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    if(root.isEmpty()) root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if(root.isEmpty() || !QDir().mkpath(root)) return;
    auto path=QDir(root).filePath("worlded-catalog.log");
    if(QFileInfo(path).size()>256*1024) {
        QFile::remove(path+".1");
        if(!QFile::rename(path,path+".1")) return;
    }
    QFile log(path);
    if(log.open(QIODevice::WriteOnly|QIODevice::Append))
        log.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+" "+outcome.left(512)+"\n").toUtf8());
}

// Original DATs contain tables and sprites, but no built-in .pro files. The
// native engine receives only resolved tables in a new disposable directory.
inline void prepare_default_prototypes(worlded::Installation& installation,QString helper={}) {
    using namespace worlded;
    namespace fs=std::filesystem;
    if(!installation.contains("rules/npc.txt") || !installation.contains("rules/monster.txt")
        || !installation.contains("rules/unique.txt")) return; // Partial resource sets.
    QElapsedTimer timer;timer.start();
    std::set<unsigned> installed;
    for(const auto& [name,sources] : installation.resources()) {
        (void)sources;
        if(name.rfind("proto/",0)!=0 || fs::path(name).extension()!=".pro") continue;
        unsigned id=0;
        if(std::from_chars(name.data()+6,name.data()+name.size(),id).ec==std::errc{}) installed.insert(id);
    }
    unsigned missing=0;
#define PROTO_RANGE_CHECK(first,last) for(unsigned id=first+20;id<last+20;++id) if(!installed.count(id)) ++missing;
    ARCANUM_DEFAULT_PROTO_RANGES(PROTO_RANGE_CHECK)
#undef PROTO_RANGE_CHECK
    if(!missing) {log_prototype_catalog("generation=skipped reason=complete-installed-prototypes");return;}
    std::size_t tables=0;
    QString phase="helper";
    try {
        if(helper.isEmpty()) helper=QDir(QCoreApplication::applicationDirPath()).filePath("worlded-native-inventory"
#ifdef Q_OS_WIN
            ".exe"
#endif
        );
        if(!QFileInfo(helper).isExecutable()) throw std::runtime_error(
            "Built-in objects need worlded-native-inventory beside WorldED. Build the matching engine helper (see docs/worlded-build.md), or use the complete WorldED package.");
        phase="staging";
        QTemporaryDir temporary(QDir::tempPath()+"/worlded-prototypes-XXXXXX");
        if(!temporary.isValid()) throw std::runtime_error("Cannot create the temporary prototype directory");
        auto root=fs::u8path(temporary.path().toUtf8().constData());
        for(const auto& [name,sources] : installation.resources()) {
            (void)sources;
            auto extension=fs::path(name).extension();
            if(extension!=".mes" && extension!=".scr" && !(name.rfind("rules/",0)==0 && extension==".txt")) continue;
            auto relative=fs::u8path(cp1252_utf8(name));
            if(relative.is_absolute()) throw std::runtime_error("Invalid prototype resource path");
            for(const auto& part : relative) if(part=="..") throw std::runtime_error("Invalid prototype resource path");
            auto path=root/relative;
            fs::create_directories(path.parent_path());
            QFile file(QString::fromUtf8(path.u8string().c_str()));
            auto data=installation.read(name);
            if(!file.open(QIODevice::WriteOnly) || file.write(reinterpret_cast<const char*>(data.data()),qint64(data.size()))!=qint64(data.size()))
                throw std::runtime_error("Cannot stage prototype resource tables");
            ++tables;
        }
        QFile marker(temporary.filePath(".worlded-prototypes"));
        if(!marker.open(QIODevice::WriteOnly) || marker.write("1\n")!=2) throw std::runtime_error("Cannot mark the prototype directory");
        marker.close();
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        QByteArray tail;
        QObject::connect(&process,&QProcess::readyRead,&process,[&]{tail=(tail+process.readAll()).right(2048);});
        phase="start";
        process.start(helper,{"--prototypes",temporary.path()});
        if(!process.waitForStarted(5000)) throw std::runtime_error("Cannot start the native prototype generator");
        phase="generation";
        if(!process.waitForFinished(60000)) {
            process.kill();process.waitForFinished(5000);
            throw std::runtime_error("Native prototype generation timed out");
        }
        if(process.exitStatus()!=QProcess::NormalExit || process.exitCode()!=0)
            throw std::runtime_error("Native prototype generation failed. Rebuild the matching worlded-native-inventory helper. "+tail.toStdString());
        phase="validation";
        auto count=installation.add_default_prototypes(root/"proto");
        log_prototype_catalog(QString("generation=ok missing=%1 tables=%2 added=%3 elapsed_ms=%4").arg(missing).arg(tables).arg(count).arg(timer.elapsed()));
    } catch(const std::exception& e) {
        installation.diagnostics.push_back(e.what());
        log_prototype_catalog(QString("generation=failed phase=%1 missing=%2 tables=%3 elapsed_ms=%4").arg(phase).arg(missing).arg(tables).arg(timer.elapsed()));
    }
}
