#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace SmTool::App {

class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString applicationName READ applicationName CONSTANT)
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString blogUrl READ blogUrl CONSTANT)
    Q_PROPERTY(QStringList components READ components CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    [[nodiscard]] QString applicationName() const;
    [[nodiscard]] QString applicationVersion() const;
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString blogUrl() const;
    [[nodiscard]] QStringList components() const;
};

} // namespace SmTool::App
