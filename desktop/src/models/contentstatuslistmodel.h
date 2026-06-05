#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class ContentStatusListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        DisplayNameRole,
        InfoRole,
        SortOrderRole,
        SystemRole,
    };
    Q_ENUM(Roles)

    explicit ContentStatusListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::ContentStatus> items);
    [[nodiscard]] const std::vector<Domain::ContentStatus> &items() const;

private:
    std::vector<Domain::ContentStatus> items_;
};

} // namespace SmTool::Models
