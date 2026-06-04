#include "app/appinfo.h"

#include <QCoreApplication>
#include <QLibraryInfo>

namespace SmTool::App {

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
}

QString AppInfo::applicationName() const
{
    return QCoreApplication::applicationName();
}

QString AppInfo::applicationVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString AppInfo::qtVersion() const
{
    return QLibraryInfo::version().toString();
}

QString AppInfo::description() const
{
    return QStringLiteral(
        "SmTool is a local-first desktop app for capturing ideas, shaping them into content, "
        "and managing the workflow from inbox to publishing.");
}

QString AppInfo::blogUrl() const
{
    return {};
}

QStringList AppInfo::components() const
{
    return {
        QStringLiteral("Qt 6"),
        QStringLiteral("QML"),
        QStringLiteral("C++20"),
        QStringLiteral("SQLite"),
        QStringLiteral("QtSql"),
        QStringLiteral("logfault"),
    };
}

} // namespace SmTool::App
