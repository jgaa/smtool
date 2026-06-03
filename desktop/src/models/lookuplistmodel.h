#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class LookupListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        KeyRole,
        DisplayNameRole,
        DescriptionRole,
        SortOrderRole,
        ActiveRole,
    };
    Q_ENUM(Roles)

    explicit LookupListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::LookupValue> items);

private:
    std::vector<Domain::LookupValue> items_;
};

} // namespace SmTool::Models
