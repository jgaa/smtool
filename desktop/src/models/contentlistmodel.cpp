#include "models/contentlistmodel.h"

#include <QRegularExpression>

#include <algorithm>

namespace SmTool::Models {
namespace {

QString normalizedWhitespace(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QString firstSentence(const QString &text)
{
    const QRegularExpression sentencePattern(QStringLiteral(R"(^\s*(.+?[.!?])(?:\s|$))"),
                                             QRegularExpression::DotMatchesEverythingOption);
    const auto match = sentencePattern.match(text);
    if (match.hasMatch()) {
        return normalizedWhitespace(match.captured(1));
    }

    return normalizedWhitespace(text.section(u'\n', 0, 0));
}

QString displayTags(const QString &text)
{
    const auto tags = text.split(u' ', Qt::SkipEmptyParts);
    if (tags.isEmpty()) {
        return {};
    }

    QStringList formatted;
    formatted.reserve(tags.size());
    for (const auto &tag : tags) {
        formatted.append(QStringLiteral("#") + tag);
    }
    return formatted.join(u' ');
}

bool sameContentSummary(const Domain::ContentSummary &lhs, const Domain::ContentSummary &rhs)
{
    return lhs.id == rhs.id
        && lhs.parentId == rhs.parentId
        && lhs.burstTemplateKey == rhs.burstTemplateKey
        && lhs.title == rhs.title
        && lhs.description == rhs.description
        && lhs.tags == rhs.tags
        && lhs.pillarName == rhs.pillarName
        && lhs.seriesName == rhs.seriesName
        && lhs.kindName == rhs.kindName
        && lhs.outcomeName == rhs.outcomeName
        && lhs.suggestedChannelName == rhs.suggestedChannelName
        && lhs.status == rhs.status
        && lhs.priority == rhs.priority
        && lhs.seriesPosition == rhs.seriesPosition
        && lhs.hasSeriesPosition == rhs.hasSeriesPosition
        && lhs.scheduledAt == rhs.scheduledAt
        && lhs.publishedAt == rhs.publishedAt
        && lhs.firstPublicationAt == rhs.firstPublicationAt;
}

bool sameItems(const std::vector<Domain::ContentSummary> &lhs, const std::vector<Domain::ContentSummary> &rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!sameContentSummary(lhs.at(index), rhs.at(index))) {
            return false;
        }
    }

    return true;
}

} // namespace

ContentListModel::ContentListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContentListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant ContentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case ParentIdRole:
        return item.parentId;
    case BurstTemplateKeyRole:
        return item.burstTemplateKey;
    case TitleRole:
        return item.title;
    case DescriptionRole:
        return item.description;
    case DescriptionPreviewRole:
        return descriptionPreview(item.description);
    case TagsRole:
        return item.tags;
    case DisplayTagsRole:
        return displayTags(item.tags);
    case PillarRole:
        return item.pillarName;
    case SeriesRole:
        return item.seriesName;
    case KindRole:
        return item.kindName;
    case OutcomeRole:
        return item.outcomeName;
    case SuggestedChannelRole:
        return item.suggestedChannelName;
    case StatusRole:
        return item.status;
    case PriorityRole:
        return item.priority;
    case SeriesPositionRole:
        return item.hasSeriesPosition ? QVariant{item.seriesPosition} : QVariant{};
    case ScheduledAtRole:
        return item.scheduledAt;
    case PublishedAtRole:
        return item.publishedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> ContentListModel::roleNames() const
{
    return {
        {IdRole, "itemId"},
        {ParentIdRole, "parentId"},
        {BurstTemplateKeyRole, "burstTemplateKey"},
        {TitleRole, "title"},
        {DescriptionRole, "description"},
        {DescriptionPreviewRole, "descriptionPreview"},
        {TagsRole, "tags"},
        {DisplayTagsRole, "displayTags"},
        {PillarRole, "pillar"},
        {SeriesRole, "series"},
        {KindRole, "kind"},
        {OutcomeRole, "outcome"},
        {SuggestedChannelRole, "suggestedChannel"},
        {StatusRole, "status"},
        {PriorityRole, "priority"},
        {SeriesPositionRole, "seriesPosition"},
        {ScheduledAtRole, "scheduledAt"},
        {PublishedAtRole, "publishedAt"},
    };
}

void ContentListModel::setDescriptionPreviewWordCap(int value)
{
    const auto normalized = std::clamp(value, 0, 500);
    if (descriptionPreviewWordCap_ == normalized) {
        return;
    }

    descriptionPreviewWordCap_ = normalized;
    if (rowCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {DescriptionPreviewRole});
    }
}

void ContentListModel::setItems(std::vector<Domain::ContentSummary> items)
{
    if (sameItems(items_, items)) {
        return;
    }

    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

QString ContentListModel::descriptionPreview(const QString &text) const
{
    const auto sentence = firstSentence(text);
    if (sentence.isEmpty()) {
        return {};
    }

    if (descriptionPreviewWordCap_ == 0) {
        return sentence;
    }

    const auto words = sentence.split(u' ', Qt::SkipEmptyParts);
    if (words.size() <= descriptionPreviewWordCap_) {
        return sentence;
    }

    QStringList clipped;
    clipped.reserve(descriptionPreviewWordCap_);
    for (int index = 0; index < descriptionPreviewWordCap_; ++index) {
        clipped.append(words.at(index));
    }
    return clipped.join(u' ') + QStringLiteral("...");
}

} // namespace SmTool::Models
