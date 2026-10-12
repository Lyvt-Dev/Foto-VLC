#pragma once
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QByteArray>

namespace paf::linux_install {
struct Layout {
    QString executable, desktop, uninstallDesktop, icon;
    explicit Layout(const QString& home) {
        executable=home+"/.local/opt/paf-photos/PAFPhotos.AppImage";
        desktop=home+"/.local/share/applications/paf-photos.desktop";
        uninstallDesktop=home+"/.local/share/applications/paf-photos-uninstall.desktop";
        icon=home+"/.local/share/icons/hicolor/256x256/apps/paf-photos.png";
    }
};
inline QString quotedExec(QString value) {
    value.replace("\\","\\\\\\\\");
    value.replace("\"","\\\\\\\"");
    value.replace("`","\\\\`");
    value.replace("$","\\\\$");
    value.replace("%","%%");
    return "\""+value+"\"";
}
inline bool write(const QString& path,const QByteArray& data) {
    if(!QDir().mkpath(QFileInfo(path).absolutePath()))return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)&&file.write(data)==data.size()&&file.commit();
}
inline bool install(const QString& source,const QString& home,const QByteArray& logo,QString* error=nullptr) {
    Layout location(home);
    auto fail=[&](const QString& reason){if(error)*error=reason;return false;};
    if(!QDir().mkpath(QFileInfo(location.executable).absolutePath()))return fail("Cannot create installation folder");
    if(QFileInfo(source).canonicalFilePath()!=QFileInfo(location.executable).canonicalFilePath()) {
        QFile input(source);QSaveFile output(location.executable);
        if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly))return fail("Cannot open application file");
        while(!input.atEnd()) {
            auto chunk=input.read(1024*1024);
            if(chunk.isEmpty()||output.write(chunk)!=chunk.size())return fail("Cannot copy application file");
        }
        if(!output.commit())return fail("Cannot finish application installation");
    }
    if(!QFile::setPermissions(location.executable,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner|QFile::ReadGroup|QFile::ExeGroup|QFile::ReadOther|QFile::ExeOther))return fail("Cannot make application executable");
    const auto command="env APPIMAGE_EXTRACT_AND_RUN=1 "+quotedExec(location.executable);
    const auto icon=location.icon;
    const auto desktop=QString("[Desktop Entry]\nType=Application\nName=PAF-Photos\nComment=View, edit and convert images\nExec=%1 %f\nIcon=%2\nTerminal=false\nCategories=Graphics;Photography;Viewer;\nMimeType=image/png;image/jpeg;image/bmp;image/gif;image/webp;image/avif;image/heic;image/tiff;image/svg+xml;application/pdf;\nActions=Uninstall;\n\n[Desktop Action Uninstall]\nName=Uninstall PAF-Photos\nName[de]=PAF-Photos deinstallieren\nExec=%1 --uninstall\n").arg(command,icon);
    const auto uninstaller=QString("[Desktop Entry]\nType=Application\nName=Uninstall PAF-Photos\nName[de]=PAF-Photos deinstallieren\nExec=%1 --uninstall\nIcon=%2\nTerminal=false\nNoDisplay=true\n").arg(command,icon);
    if(!write(location.icon,logo)||!write(location.desktop,desktop.toUtf8())||!write(location.uninstallDesktop,uninstaller.toUtf8()))return fail("Cannot create application-menu entries");
    return true;
}
inline bool uninstall(const QString& home) {
    Layout location(home);bool ok=true;
    for(const auto& path:{location.desktop,location.uninstallDesktop,location.icon,location.executable})
        if(QFile::exists(path)&&!QFile::remove(path))ok=false;
    QDir().rmdir(QFileInfo(location.executable).absolutePath());
    return ok;
}
}
