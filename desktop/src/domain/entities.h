#pragma once

#include <QDateTime>
#include <QString>

#include <vector>

namespace SmTool::Domain {

struct LookupValue {
    QString id;
    QString key;
    QString displayName;
    QString description;
    int sortOrder = 0;
    bool isActive = true;
};

struct BurstTemplate {
    QString key;
    QString displayName;
    QString titleSuffix;
    QString kindId;
    QString suggestedChannelId;
    QString outcomeId;
    bool isActive = true;
};

struct Series {
    QString id;
    QString name;
    QString description;
    QString pillarId;
    QString status;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct ContentItem {
    QString id;
    QString parentId;
    QString seriesId;
    QString burstTemplateKey;
    QString title;
    QString description;
    QString kindId;
    QString pillarId;
    QString outcomeId;
    QString suggestedChannelId;
    QString status;
    int priority = 0;
    QDateTime scheduledAt;
    QDateTime publishedAt;
    QString publishedUrl;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct ContentSummary {
    QString id;
    QString parentId;
    QString burstTemplateKey;
    QString title;
    QString description;
    QString pillarName;
    QString seriesName;
    QString kindName;
    QString outcomeName;
    QString suggestedChannelName;
    QString status;
    int priority = 0;
    QDateTime scheduledAt;
    QDateTime publishedAt;
};

struct SeriesSummary {
    QString id;
    QString name;
    QString description;
    QString pillarName;
    QString status;
    int contentCount = 0;
    int scheduledCount = 0;
};

struct CalendarEntry {
    QString id;
    QString contentId;
    QString title;
    QString seriesName;
    QString channelName;
    QString sourceType;
    QDateTime scheduledAt;
};

struct DashboardMetric {
    QString label;
    int value = 0;
    QString secondary;
};

struct DashboardData {
    std::vector<DashboardMetric> byPillar;
    std::vector<DashboardMetric> bySeries;
    std::vector<DashboardMetric> byStatus;
    std::vector<DashboardMetric> upcoming;
    std::vector<DashboardMetric> publishedContent;
    std::vector<DashboardMetric> publishedPublications;
    std::vector<DashboardMetric> zeroPublishedPillars;
};

} // namespace SmTool::Domain
