# Codex Implementation Spec: Goals Feature for MediaStrategy

## Desired Outcome

Implement a first version of the Goals feature.

The purpose of goals is to let the user define simple, understandable targets for content production, topic coverage, channel distribution, cadence, and balance.

The feature should answer questions like:

* Am I posting enough about #nextapp?
* Am I publishing regularly on LinkedIn?
* Am I neglecting important tags or pillars?
* Is my content mix roughly aligned with my strategy?

Dashboard integration is out of scope for this task. The priority is to implement a robust database model, repository/model layer, and one large Goals dialog where goals can be added, deleted, enabled/disabled, and tweaked easily.

---

## Goal Types

Support these goal types in v1:

### 1. Count Goal

Example:

* Post about #nextapp at least 1 time per week
* Publish on LinkedIn at least 2 times per week
* Create 4 Product items per month

This is a target count within a period.

### 2. Cadence Goal

Example:

* Mention #itsecurity at least once every 30 days
* Publish on TikTok at least once every 7 days

This is about avoiding long gaps.

Internally this may be represented similarly to a count goal with target = 1 and period = N days, but the UI should present it as a cadence/frequency goal.

### 3. Balance Goal

Example:

* Content pillar balance:

  * Tech weight 5
  * Product weight 4
  * AppForge weight 3
  * Life weight 2

Do not ask the user to enter percentages directly. Store weights and calculate percentages from the sum of weights.

This avoids invalid percentage totals like 150%, 900%, etc.

---

## Goal Scopes

Goals should be generic and able to target existing entities.

Support these scope types:

* pillar
* tag
* channel
* series
* kind

Codex should adapt to the actual existing table names and IDs.

Expected existing concepts:

* Pillars
* Tags
* Channels
* Series
* ContentKinds / Kinds
* Content
* Publications
* Content/tag cross-reference table

Use the existing schema conventions, naming style, UUID/TEXT ID style, and migration system.

---

## Suggested Database Design

Codex should inspect the current schema and adjust names accordingly.

### Table: goals

Suggested fields:

```sql
CREATE TABLE goals (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,

    goal_type TEXT NOT NULL,
    -- count, cadence, balance

    scope_type TEXT NOT NULL,
    -- pillar, tag, channel, series, kind, balance_group

    scope_id TEXT,
    -- ID of the pillar/tag/channel/series/kind.
    -- Nullable for balance group parent goals if needed.

    metric_type TEXT NOT NULL,
    -- content_count, publication_count, mention_count, cadence, balance_weight

    target_value INTEGER,
    -- Used by count/cadence goals.

    period_type TEXT,
    -- day, week, month, quarter, year, rolling_days

    period_value INTEGER,
    -- Usually 1, but supports values like 30 rolling days or 90 rolling days.

    enabled INTEGER NOT NULL DEFAULT 1,

    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

### Table: goal_balance_items

Balance goals need multiple scoped items.

Suggested structure:

```sql
CREATE TABLE goal_balance_items (
    id TEXT PRIMARY KEY,
    goal_id TEXT NOT NULL REFERENCES goals(id) ON DELETE CASCADE,

    scope_type TEXT NOT NULL,
    -- usually pillar for v1, but should support tag/channel/kind later

    scope_id TEXT NOT NULL,

    weight INTEGER NOT NULL DEFAULT 1,

    sort_order INTEGER NOT NULL DEFAULT 0,

    UNIQUE(goal_id, scope_type, scope_id)
);
```

Example:

A balance goal called "Pillar Balance" has rows:

```text
Tech      weight 5
Product   weight 4
AppForge  weight 3
Life      weight 2
```

The app calculates:

```text
Tech      36%
Product   29%
AppForge  21%
Life      14%
```

Do not store calculated percentages. Calculate them dynamically.

---

## Optional Future-Proofing

Do not implement goal filters in v1 unless it is trivial.

Future examples:

* #nextapp on LinkedIn
* Product content on TikTok
* #itsecurity videos

Possible future table:

```sql
CREATE TABLE goal_filters (
    id TEXT PRIMARY KEY,
    goal_id TEXT NOT NULL REFERENCES goals(id) ON DELETE CASCADE,
    filter_type TEXT NOT NULL,
    filter_id TEXT NOT NULL
);
```

For now, keep the model simple.

---

## Repository Layer

Add a GoalsRepository or equivalent repository class following the existing project style.

It should support:

* listGoals()
* getGoal(id)
* createGoal(goal)
* updateGoal(goal)
* deleteGoal(id)
* setGoalEnabled(id, enabled)
* listBalanceItems(goalId)
* updateBalanceItems(goalId, items)

Use transactions when saving a balance goal and its balance items.

Deleting a goal should delete its balance items.

---

## Qt Models

Add QAbstractListModel-based models as needed.

Suggested models:

### GoalsModel

Represents all goals in the dialog.

Roles should include:

* id
* name
* goalType
* scopeType
* scopeId
* scopeDisplayName
* metricType
* targetValue
* periodType
* periodValue
* enabled
* summaryText

The `summaryText` role is important. The UI should show human-readable goal descriptions such as:

```text
#nextapp: at least 1 post per week
LinkedIn: at least 2 publications per week
Pillar Balance: Tech 36%, Product 29%, AppForge 21%, Life 14%
```

### GoalLookupModel(s)

The dialog needs dropdown values for:

* pillars
* tags
* channels
* series
* kinds

Reuse existing lookup models if they already exist.

---

## UI Requirement

Implement one large Goals dialog.

The dialog should be accessible from the main UI, probably from the toolbar/menu/sidebar near Dashboard or Settings.

Suggested title:

```text
Goals
```

The dialog should be large enough to manage goals comfortably.

---

## Goals Dialog Layout

Suggested structure:

```text
+----------------------------------------------------------+
| Goals                                             [Close] |
+----------------------------------------------------------+
| [Add Goal] [Add Balance Goal]                            |
|                                                          |
|  Enabled | Goal Summary                         | Actions |
|  ------------------------------------------------------  |
|  [x]     | #nextapp: 1 post per week             Edit Del |
|  [x]     | LinkedIn: 2 publications per week     Edit Del |
|  [x]     | Pillar Balance: Tech 36%, Product...  Edit Del |
|                                                          |
+----------------------------------------------------------+
| Right side or bottom editor panel                        |
+----------------------------------------------------------+
```

Either of these UI styles is acceptable:

1. List on the left, editor on the right
2. List on top, editor below
3. Full-row inline editing

Prefer clarity over cleverness.

---

## Add/Edit Goal UI

The goal editor should feel like building a sentence.

For normal count/cadence goals:

```text
Goal type:
[ Count / Cadence ]

Track:
[ Pillar / Tag / Channel / Series / Kind ]

Which:
[ #nextapp ]

Metric:
[ Content items / Publications ]

Target:
[ 1 ]

Period:
[ per week ]
```

The generated preview should show:

```text
Post about #nextapp at least 1 time per week
```

or:

```text
Publish on LinkedIn at least 2 times per week
```

The preview is important. It confirms that the goal means what the user thinks it means.

---

## Balance Goal UI

For balance goals, use weights, not percentages.

Example UI:

```text
Balance goal name:
[ Pillar Balance ]

Balance scope:
[ Pillars ]

Items:

Tech              [ slider ] 5   36%
Product           [ slider ] 4   29%
AppForge          [ slider ] 3   21%
Life              [ slider ] 2   14%
Thoughts          [ slider ] 0    0%
```

Rules:

* Store weights.
* Calculate percentages from all non-zero weights.
* Allow weight 0 to mean "not part of this balance goal".
* Show calculated percentages beside the sliders.
* Do not allow negative weights.
* A reasonable slider range is 0–10 or 0–20.
* Use integer weights.

This lets the user think in terms of relative priority rather than exact percentages.

---

## Suggested UX Details

The dialog should support:

* Add goal
* Add balance goal
* Edit selected goal
* Delete goal
* Enable/disable goal
* Save changes
* Cancel changes
* Human-readable preview
* Validation before saving

Validation:

* Name must not be empty.
* goal_type must be valid.
* scope_type must be valid.
* scope_id must exist for normal goals.
* target_value must be positive for count/cadence goals.
* period must be valid for count/cadence goals.
* balance goals must have at least one item with weight > 0.

---

## Metrics Semantics

For v1, implement storage and editing only. Dashboard evaluation can come later.

However, the database model should already distinguish:

### content_count

Counts content items matching the goal scope.

Examples:

* pillar = Product
* tag = #nextapp
* kind = BlogArticle
* series = MediaStrategy

### publication_count

Counts publications matching the goal scope.

Most useful for channels.

Examples:

* channel = LinkedIn
* channel = TikTok

### mention_count

Optional alias for tag-based content count.

If this complicates things, skip mention_count and use content_count for tag scopes.

### cadence

Represents "at least once every N days/weeks/months".

This will later be evaluated by looking for the most recent matching content/publication.

---

## Migration

Add a new numbered migration using the existing migration system.

The migration should:

* Create goals table
* Create goal_balance_items table
* Add indexes

Suggested indexes:

```sql
CREATE INDEX idx_goals_type ON goals(goal_type);
CREATE INDEX idx_goals_scope ON goals(scope_type, scope_id);
CREATE INDEX idx_goals_enabled ON goals(enabled);

CREATE INDEX idx_goal_balance_items_goal ON goal_balance_items(goal_id);
CREATE INDEX idx_goal_balance_items_scope ON goal_balance_items(scope_type, scope_id);
```

---

## Seed Data

Optional but useful for testing:

```text
#nextapp: 1 content item per week
LinkedIn: 2 publications per week
Pillar Balance:
  tech weight 5
  product weight 4
  appforge weight 3
  life weight 2
```

Only add seed data if the project already has a development/demo seed mechanism.

Do not force seed goals into production user databases unless the app already does that for other sample data.

---

## Acceptance Criteria

The feature is complete when:

1. The database bootstrap creates the goal tables (no need for backwars compatibility).
2. Goals can be created, edited, deleted, enabled, and disabled.
3. Count goals can target pillar, tag, channel, series, or kind.
4. Cadence goals can target pillar, tag, channel, series, or kind.
5. Balance goals can be created using weighted sliders.
6. Percentages for balance goals are calculated from weights and never manually stored.
7. The Goals dialog shows a readable summary for every goal.
8. The UI allows goal tweaking with minimal friction.
9. The implementation follows the existing repository/model/QML style.
10. Dashboard evaluation is not required yet, but the stored data is sufficient for later dashboard comparison of goal vs reality.

---


Keep v1 simple, robust, and easy to use.
