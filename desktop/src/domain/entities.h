#pragma once

#include <QDate>
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

struct ContentStatus {
    QString id;
    QString info;
    int sortOrder = 0;
    bool isSystem = false;
};

struct BurstTemplate {
    QString key;
    QString displayName;
    QString titleSuffix;
    QString kindId;
    QString suggestedChannelId;
    QString suggestedChannelName;
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

struct SeriesDetail {
    QString id;
    QString name;
    QString description;
    QString pillarId;
    QString pillarName;
    QString status;
    int contentCount = 0;
    int scheduledCount = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct ContentItem {
    QString id;
    QString parentId;
    QString seriesId;
    int seriesPosition = 0;
    bool hasSeriesPosition = false;
    QString burstTemplateKey;
    QString title;
    QString description;
    QString tags;
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
    QString tags;
    QString pillarName;
    QString seriesName;
    QString kindName;
    QString outcomeName;
    QString suggestedChannelName;
    QString status;
    int priority = 0;
    int seriesPosition = 0;
    bool hasSeriesPosition = false;
    QDateTime scheduledAt;
    QDateTime publishedAt;
    QDateTime firstPublicationAt;
};

struct Publication {
    QString id;
    QString contentId;
    QString channelId;
    QString channelName;
    QString status;
    QDateTime scheduledAt;
    QDateTime publishedAt;
    QString url;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct MediaItem {
    QString id;
    QString contentId;
    QString publicationId;
    QString name;
    QString sourceType;
    QString location;
    QDateTime createdAt;
    QDateTime updatedAt;
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
    QString contentStatus;
    QString publicationStatus;
    QDateTime scheduledAt;
    bool isOverdue = false;
};

enum class DashboardHealth {
    Good,
    SlightlyLow,
    Warning,
    Bad,
    Critical,
    TooHigh,
    Unknown,
};

struct Goal {
    QString id;
    QString name;
    QString goalType;
    QString scopeType;
    QString scopeId;
    QString scopeDisplayName;
    QString metricType;
    int targetValue = 0;
    QString periodType;
    int periodValue = 0;
    bool enabled = true;
    QString summaryText;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct GoalBalanceItem {
    QString id;
    QString goalId;
    QString scopeType;
    QString scopeId;
    QString scopeDisplayName;
    int weight = 0;
    int sortOrder = 0;
};

struct DashboardPeriod {
    QString key;
    QString label;
    QDate startDate;
    QDate endDate;
};

struct DashboardRow {
    QString goalId;
    QString goalName;
    QString goalType;
    QString scopeType;
    QString metricType;
    QString displayName;
    QString summaryText;
    QString detailText;
    double targetValue = 0.0;
    double actualValue = 0.0;
    double percent = 0.0;
    double deviation = 0.0;
    double absoluteDeviation = 0.0;
    double requiredValue = 0.0;
    double pipelineValue = 0.0;
    double shortfallValue = 0.0;
    double severityScore = 0.0;
    DashboardHealth health = DashboardHealth::Unknown;
};

struct DashboardEvaluation {
    DashboardPeriod performancePeriod;
    DashboardPeriod pipelinePeriod;
    std::vector<DashboardRow> goalAchievement;
    std::vector<DashboardRow> pipelineCoverage;
    std::vector<DashboardRow> balanceDeviation;
    std::vector<DashboardRow> alerts;
    std::vector<DashboardRow> recommendedFocus;
};

} // namespace SmTool::Domain
