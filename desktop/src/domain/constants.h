#pragma once

#include <array>
#include <optional>
#include <QAnyStringView>
#include <QString>

namespace SmTool::Domain {

using namespace Qt::Literals::StringLiterals;

inline constexpr auto seriesStatuses = std::array{
    "active"_L1,
    "paused"_L1,
    "completed"_L1,
    "archived"_L1,
};

inline constexpr auto publicationStatuses = std::array{
    "planned"_L1,
    "scheduled"_L1,
    "published"_L1,
};

inline constexpr auto seededPillars = std::array{
    "product"_L1,
    "tech"_L1,
    "lifehacks"_L1,
    "life"_L1,
    "appforge"_L1,
    "thoughtsAndIdeas"_L1,
};

inline constexpr auto seededContentKinds = std::array{
    "idea"_L1,
    "blog_post"_L1,
    "video"_L1,
    "short_post"_L1,
    "clip"_L1,
    "newsletter"_L1,
    "other"_L1,
};

inline constexpr auto seededOutcomes = std::array{
    "discussion"_L1,
    "authority"_L1,
    "trust"_L1,
    "conversion"_L1,
    "learning"_L1,
    "other"_L1,
};

inline constexpr auto seededChannels = std::array{
    "blog"_L1,
    "youtube"_L1,
    "linkedin"_L1,
    "mastodon"_L1,
    "newsletter"_L1,
    "tiktok"_L1,
    "x"_L1,
    "reddit"_L1,
    "bluesky"_L1,
    "matrix"_L1,
};

inline std::optional<int> indexOf(const auto &values, const QAnyStringView needle)
{
    for (int index = 0; index < static_cast<int>(values.size()); ++index) {
        if (QAnyStringView{values.at(index)} == needle) {
            return index;
        }
    }
    return std::nullopt;
}

inline bool isValidSeriesStatus(const QAnyStringView value)
{
    return indexOf(seriesStatuses, value).has_value();
}

inline bool isValidPublicationStatus(const QAnyStringView value)
{
    return indexOf(publicationStatuses, value).has_value();
}

inline QString titleFromKey(const QAnyStringView key)
{
    QString value = key.toString();
    value.replace(u'_', u' ');
    if (!value.isEmpty()) {
        value[0] = value[0].toUpper();
    }
    return value;
}

} // namespace SmTool::Domain
