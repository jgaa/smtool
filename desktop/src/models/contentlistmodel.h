#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class ContentListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ParentIdRole,
        BurstTemplateKeyRole,
        TitleRole,
        DescriptionRole,
        DescriptionPreviewRole,
        TagsRole,
        DisplayTagsRole,
        PillarRole,
        SeriesRole,
        KindRole,
        OutcomeRole,
        SuggestedChannelRole,
        StatusRole,
        PriorityRole,
        ScheduledAtRole,
        PublishedAtRole,
    };
    Q_ENUM(Roles)

    explicit ContentListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setDescriptionPreviewWordCap(int value);
    void setItems(std::vector<Domain::ContentSummary> items);

private:
    [[nodiscard]] QString descriptionPreview(const QString &text) const;

    std::vector<Domain::ContentSummary> items_;
    int descriptionPreviewWordCap_ = 10;
};

} // namespace SmTool::Models
