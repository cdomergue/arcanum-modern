#pragma once
#include <QtWidgets>

namespace worlded {
// All three editors resolve resources through Installation and use sibling tools
// from the same package, with one copy of the Qt runtime.
inline void launch_editor(QWidget* parent, const QString& name, const QString& game, QStringList arguments={}) {
    auto executable=QCoreApplication::applicationDirPath()+"/"+name;
#ifdef Q_OS_WIN
    executable+=".exe";
#endif
    if (!game.isEmpty()) arguments<<"--game"<<game;
    if (!QProcess::startDetached(executable,arguments))
        QMessageBox::warning(parent,"Cannot open editor","Keep "+name+" beside WorldED with the shared runtime files.");
}
}
