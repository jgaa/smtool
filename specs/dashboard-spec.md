# Codex Implementation Spec: Dashboard Goal Visualization

## Goal

Remove the content of the old dashboard.

Implement the first Dashboard visualization layer for the Goals feature.

The Dashboard should help the user answer, within a few seconds:

1. Am I meeting my goals?
2. What am I neglecting?
3. Is my current pipeline strong enough for the coming period?
4. What should I work on next?

The Dashboard is not just a statistics page. It is a decision-support view. Abnormalities should stand out clearly.

---

## Dashboard Sections

Split the Dashboard into two major sections:

```text
Performance
Pipeline
```

### Performance

Looks backward.

Default period:

```text
Last 90 days
```

Purpose:

Compare goals against actual published output.

### Pipeline

Looks forward.

Default period:

```text
Next 30 days
```

Purpose:

Compare current in-progress/scheduled content against what is needed to meet upcoming goals.

---

## Period Controls

Add period controls to the Dashboard.

### Performance Period Options

```text
Last 30 days
Last 90 days
This quarter
This year
Custom
```

Default:

```text
Last 90 days
```

### Pipeline Period Options

```text
Next 7 days
Next 30 days
Next 90 days
Custom
```

Default:

```text
Next 30 days
```

---

## Core Dashboard Widgets

Implement these widgets first:

1. Goal Achievement
2. Pipeline Coverage
3. Balance Goal Deviation
4. Neglected Areas
5. Recommended Focus

---

# 1. Goal Achievement Widget

## Purpose

Show how actual published output compares to enabled count/cadence goals in the selected performance period.

Example:

```text
#nextapp      120%
LinkedIn       85%
Tech           95%
#itsecurity    40%
```

## Data

For each enabled non-balance goal:

```text
target_for_period
actual_for_period
achievement_percent = actual / target * 100
```

If target is zero or invalid, exclude the goal and log a warning.

## Evaluation Rules

Use existing goal fields:

```text
goal_type
scope_type
scope_id
metric_type
target_value
period_type
period_value
```

For count goals:

```text
achievement = actual count / expected count for selected period
```

For cadence goals:

Use the same count-based achievement for this first dashboard version.

Example:

```text
#nextapp once per week
Selected period = 90 days
Expected = about 13
Actual = matching content count
```

A later version may add a more precise streak/missed-window cadence evaluator.

## Chart Type

Use a horizontal bar chart.

Sort by worst health first:

```text
Red
Orange
Yellow
Green
Blue/Over
```

The most problematic goals should appear at the top.

---

# 2. Pipeline Coverage Widget

## Purpose

Show whether the current pipeline contains enough content to satisfy goals for the coming period.

Example:

```text
Tech        required 8   pipeline 6   coverage 75%
Product     required 4   pipeline 7   coverage 175%
#nextapp    required 4   pipeline 3   coverage 75%
LinkedIn    required 8   pipeline 2   coverage 25%
```

## Default Period

```text
Next 30 days
```

## Pipeline Eligibility

Only count content that is likely to become publishable.

Include:

```text
Drafting
Ready
Scheduled
Published
```

Exclude:

```text
Inbox
Clarifying
```

Suggested treatment of `Shaping`:

Either exclude in v1 or count as partial credit if the model layer makes that easy.

Preferred v1:

```text
Shaping = 0.25
Drafting = 0.50
Ready = 1.00
Scheduled = 1.00
Published = 1.00
```

If weighted pipeline scoring is too large for v1, use simple inclusion:

```text
Drafting
Ready
Scheduled
```

and exclude everything earlier.

## Effective Pipeline Count

Preferred:

```text
effective_count = sum(status_weight)
```

Example:

```text
2 Drafting items = 1.0 effective item
1 Ready item     = 1.0 effective item
Total            = 2.0 effective items
```

## Coverage

```text
coverage_percent = effective_pipeline_count / required_for_period * 100
```

## Important Distinction

For channel goals, pipeline coverage should use Publications when available.

For content goals, use Content.

Channel goals:

```text
scope_type = channel
metric_type = publication_count
```

Content goals:

```text
scope_type = pillar/tag/series/kind
metric_type = content_count
```

---

# 3. Balance Goal Deviation Widget

## Purpose

Show whether the actual content mix matches the intended balance.

Balance goals are based on weights, not stored percentages.

Example:

```text
Goal weights:
Tech      5
Product   4
AppForge  3
Life      2

Calculated target:
Tech      36%
Product   29%
AppForge  21%
Life      14%

Actual:
Tech      52%
Product   18%
AppForge  24%
Life       6%
```

## Calculation

For each balance goal:

```text
target_percent = item.weight / total_weight * 100
actual_percent = actual_matching_count / total_matching_count * 100
deviation = actual_percent - target_percent
absolute_deviation = abs(deviation)
```

Do not store calculated percentages.

## Visualization

Use a table or horizontal bar chart showing:

```text
Scope item
Target %
Actual %
Deviation
Health color
```

Balance is symmetric:

* Too low is bad.
* Too high is also bad.
* A category can be overrepresented and should become yellow/orange/red.

---

# 4. Neglected Areas Widget

## Purpose

Show important things that have gone stale.

This should be a simple alert list, not a chart.

Examples:

```text
#itsecurity not mentioned in 68 days
Product pillar below target by 42%
LinkedIn cadence missed
#nextapp has no scheduled content in the next 30 days
```

## Include Alerts For

* Goals below red/orange threshold
* Cadence goals with no recent matching item
* Balance items with large negative deviation
* Pipeline coverage below threshold
* Important scopes with zero output in selected period

Sort by severity.

---

# 5. Recommended Focus Widget

## Purpose

Suggest what the user should work on next.

This should be derived from the same dashboard evaluation data.

Example:

```text
Recommended Focus

1. #itsecurity
   Only 25% pipeline coverage for the next 30 days.

2. LinkedIn
   Needs 6 more scheduled publications to meet the coming-period goal.

3. Product pillar
   Actual output is 18%, target is 29%.
```

## Scoring

Codex can implement a simple heuristic:

```text
severity_score =
    missing_goal_score
  + pipeline_shortage_score
  + balance_deviation_score
  + cadence_staleness_score
```

Then show the top 3–5 items.

---

# Health Colors

Colors should be based on dashboard evaluation state, not hardcoded per chart.

Create a shared helper, for example:

```cpp
DashboardHealth classifyGoalHealth(...)
DashboardHealth classifyPipelineHealth(...)
DashboardHealth classifyBalanceHealth(...)
```

Suggested enum:

```cpp
enum class DashboardHealth {
    Good,
    SlightlyLow,
    Warning,
    Bad,
    Critical,
    TooHigh,
    Unknown
};
```

Or use existing project style.

## Goal Achievement Colors

For ordinary count goals:

```text
>= 100%      Green
90–99%       Light green / Good
75–89%       Yellow
50–74%       Orange
< 50%        Red
```

Optional overachievement:

```text
100–150%     Green
150–200%     Blue or purple / over target
> 200%       Yellow/orange if overproduction may indicate imbalance
```

Do not treat overachievement as bad for simple count goals unless the UI explicitly says it is an over-target warning.

## Pipeline Coverage Colors

Pipeline surplus is usually good.

```text
100–150%     Green
75–99%       Yellow
50–74%       Orange
< 50%        Red
150%+        Blue or neutral "surplus"
```

## Balance Goal Colors

Balance scoring must be symmetric.

Example thresholds using absolute deviation:

```text
0–10 percentage points       Green
10–20 percentage points      Yellow
20–35 percentage points      Orange
>35 percentage points        Red
```

Example:

```text
Target 40%, actual 42% = Green
Target 40%, actual 55% = Yellow
Target 40%, actual 70% = Orange/Red
Target 40%, actual 10% = Red
```

## Cadence Colors

For cadence goals:

```text
On schedule                  Green
Close to missing             Yellow
Missed                       Orange
Long overdue                 Red
```

Suggested simple rule:

```text
days_since_last <= period_days           Green
<= period_days * 1.25                    Yellow
<= period_days * 1.75                    Orange
> period_days * 1.75                     Red
```

---

# UI Notes

## Visual Priorities

Abnormalities should stand out.

Use:

* colored bars
* warning icons
* colored chips/badges
* sorted lists with worst items first

Avoid making everything colorful. Good/normal state should be visually calm.

## Dashboard Header

Suggested header:

```text
Dashboard

Performance: [Last 90 days v]
Pipeline:    [Next 30 days v]
```

Optional top-level summary:

```text
Strategy Health: 87 / 100
```

This score can wait if it complicates v1.

---

# Repository / Service Layer

Prefer adding a dashboard evaluation service rather than putting calculations directly in QML.

Suggested class:

```cpp
DashboardService
```

Responsibilities:

```text
load enabled goals
evaluate performance period
evaluate pipeline period
evaluate balance goals
produce DTOs/models for QML
```

Suggested DTOs:

```cpp
GoalAchievementRow
PipelineCoverageRow
BalanceDeviationRow
DashboardAlert
RecommendedFocusItem
```

Expose through QAbstractListModel classes or an existing model pattern.

---

# Suggested Models

Create QML-facing models such as:

```text
GoalAchievementModel
PipelineCoverageModel
BalanceDeviationModel
DashboardAlertsModel
RecommendedFocusModel
```

Roles should include:

```text
displayName
scopeType
goalId
targetValue
actualValue
percent
deviation
health
healthColor
summaryText
```

Do not duplicate heavy calculation in QML.

---

# Date Semantics

Use the existing project date/time conventions.

For selected periods:

```text
Last 90 days:
start = now - 90 days
end = now

Next 30 days:
start = now
end = now + 30 days
```

For `This quarter` and `This year`, use calendar boundaries.

---

# Counting Semantics

## Published Reality

For past performance:

* `publication_count` should count matching published Publication rows.
* `content_count` should count matching Content rows in Published status.
* If the existing model allows Content to be published without Publication, preserve that distinction.

## Pipeline

For future pipeline:

* `content_count` goals count matching Content rows in eligible workflow states.
* `publication_count` goals count matching Publications scheduled inside the pipeline period.
* If a Publication has no scheduled_at, use existing scheduling fallback rules if already implemented in the project.

---

# Initial Implementation Priority

Implement in this order:

1. DashboardService with evaluation DTOs.
2. Goal Achievement model.
3. Pipeline Coverage model.
4. Health classification and color roles.
5. Basic QML widgets for achievement and pipeline.
6. Balance deviation widget.
7. Alerts widget.
8. Recommended focus widget.

The first useful milestone is:

```text
Goal Achievement + Pipeline Coverage with color-coded health.
```

That alone provides immediate value.

---

# Out of Scope for This Task

Do not implement:

* external analytics import
* social media API integration
* AI recommendations
* exact engagement metrics
* historical trend storage
* complex per-channel algorithm modeling

The goal is a local, deterministic dashboard based on existing Content, Publication, Goal, Tag, Pillar, Channel, Series, and Kind data.

---

# Acceptance Criteria

The implementation is complete when:

1. Dashboard has a past-performance section.
2. Dashboard has a future-pipeline section.
3. Default past period is last 90 days.
4. Default pipeline period is next 30 days.
5. Enabled goals are evaluated against actual data.
6. Pipeline coverage compares eligible pipeline content/publications against upcoming goal requirements.
7. Balance goals show target vs actual distribution.
8. Health colors are applied consistently.
9. Red/orange/yellow abnormalities are easy to notice.
10. The worst problems appear near the top of relevant lists.
11. QML does not contain core dashboard business logic.
12. The dashboard can be refreshed after goals or content change.
