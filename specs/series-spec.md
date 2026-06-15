# Series Workflow Specification for Codex

## Purpose

Define the v1 workflow and implementation requirements for the existing **Series** feature in the smtool application.

The UI already has a pane where Series can be created, read, updated, and deleted. This spec defines what a Series means, how it relates to Content, and what database/UI changes are needed to make the feature useful for planning ordered content sequences.

## Current Schema Baseline

The current database already contains:

```sql
CREATE TABLE IF NOT EXISTS "series" (
    "id" TEXT,
    "name" TEXT NOT NULL,
    "description" TEXT,
    "pillar_id" TEXT,
    "status" TEXT NOT NULL CHECK("status" IN ('active', 'paused', 'completed', 'archived')),
    "created_at" DATETIME NOT NULL,
    "updated_at" DATETIME NOT NULL,
    PRIMARY KEY("id"),
    FOREIGN KEY("pillar_id") REFERENCES "pillar"("id")
);
```

The `content` table already has:

```sql
"series_id" TEXT,
FOREIGN KEY("series_id") REFERENCES "series"("id")
```

Therefore, Series already exists as a first-class entity, and Content already supports optional assignment to a Series.

## Definition

A **Series** is an editorial container for a sequence of related content items.

A Series is commonly used for content that is published in order, such as:

- episodes
- parts
- build-in-public logs
- tutorials
- project journals
- thematic campaigns

A Series is not itself a Kanban workflow item. It does not move through Inbox, Drafting, Published, etc. Individual Content items do that.

A Series may contain Content items in many different states at the same time.

Example:

```text
Series: Building smtool

Part 1: Published
Part 2: Scheduled
Part 3: Drafting
Part 4: Inbox
```

## Core Rules

### 1. Content May Optionally Belong to a Series

A Content item may have `series_id` set to a Series id.

A Content item may also have `series_id = NULL`.

Series membership must be editable from the Content editor and from the Series pane.

### 2. Series Status Is Independent of Content Status

Series status controls visibility and planning state only.

Allowed statuses already exist in the schema:

```text
active
paused
completed
archived
```

Meaning:

- `active`: open for new and existing content
- `paused`: temporarily inactive but still visible in planning views
- `completed`: editorially finished, but existing content remains editable
- `archived`: hidden from default active views

Content status remains controlled by the `content.status` workflow:

```text
Inbox → Clarifying → Shaping → Drafting → Ready → Scheduled → Published → Reviewing → Archived
```

### 3. Series Does Not Replace Pillar, Kind, Channel, or Tags

Series is only an editorial grouping.

A Content item still has its own:

- `pillar_id`
- `kind_id`
- `outcome_id`
- `suggested_channel_id`
- tags
- publications

### 4. Derivatives Inherit Series

When burst/derivative content is generated from a root content item, the generated content must inherit:

```text
series_id
pillar_id
```

This preserves the editorial context across fan-out content.

Example:

```text
Root content:
  Building the payments backend, part 3
  series_id = nextapp-payments

Generated derivatives:
  LinkedIn lesson
  X note
  BlueSky note
  Reddit post

All generated derivatives inherit series_id = nextapp-payments.
```

Existing burst rules remain unchanged:

- only root content can generate bursts
- derivatives cannot generate more derivatives in v1
- burst generation remains idempotent via `(parent_id, burst_template_key)`
- generated content starts in `shaping`
- generated content is unscheduled
- generated content does not automatically create Publication rows

## Required Schema Change

The current schema can associate Content with a Series, but it cannot express editorial order.

Add an optional ordering field to `content`:

```sql
ALTER TABLE content ADD COLUMN series_position INTEGER;
```

Add indexes:

```sql
CREATE INDEX IF NOT EXISTS idx_content_series
ON content(series_id);

CREATE INDEX IF NOT EXISTS idx_content_series_position
ON content(series_id, series_position);
```

`series_position` is nullable.

Meaning:

- `NULL`: no explicit position assigned yet
- `1`: first planned item in the series
- `2`: second planned item
- etc.

Do not add a uniqueness constraint in v1. Duplicate positions can be repaired by the UI/repository layer when reordering. This keeps migration and legacy data handling simple.

## Repository / Model Requirements

### Series Repository

The existing Series CRUD should support:

- create Series
- update Series
- delete Series only if unused
- archive Series
- list active Series
- list all Series including archived
- read Series by id

Deletion rule:

- If no Content references the Series, physical delete is allowed.
- If Content references the Series, prefer setting status to `archived`.
- Do not cascade-delete Content when deleting a Series.

### Content Repository Additions

Add/update repository methods for:

```text
assignContentToSeries(contentId, seriesId)
removeContentFromSeries(contentId)
setSeriesPosition(contentId, position)
moveSeriesItem(seriesId, contentId, direction)
listContentForSeries(seriesId)
createContentInSeries(seriesId, initialFields)
```

`createContentInSeries()` should:

- create a normal Content item
- set `series_id`
- set `pillar_id` from the Series if the caller did not provide one and `series.pillar_id` is not NULL
- assign the next available `series_position`
- default status should follow normal app behavior, probably `inbox`

### Ordering Rule

When listing Content in a Series, sort by:

```sql
ORDER BY
    series_position IS NULL,
    series_position ASC,
    scheduled_at IS NULL,
    scheduled_at ASC,
    created_at ASC;
```

This gives explicit editorial ordering first, then falls back to planned schedule and creation time.

## UI Requirements

The Series pane should behave as a planning/editorial view.

Recommended layout:

```text
Left pane:
  Series list

Right pane:
  Selected Series details
  Ordered Content list for selected Series
```

### Series List

Show:

- name
- status
- pillar, if set
- number of related Content items
- next scheduled item, if available

Default filter:

- show `active`, `paused`, and `completed`
- hide `archived` unless an "include archived" toggle is enabled

### Series Detail

Show/edit:

- name
- description
- pillar
- status

### Series Content List

For each Content item in the selected Series, show:

- series position
- title
- content status
- kind
- pillar
- suggested channel
- scheduled date, if any
- published date, if any
- publication count/status summary, if cheap to compute

Actions:

- create new Content in this Series
- add existing Content to this Series
- remove Content from this Series
- move item up
- move item down
- open Content editor
- archive Series

### Reordering Behavior

Move up/down updates `series_position` values.

The repository may normalize positions after a reorder so the selected Series has compact positions:

```text
1, 2, 3, 4, ...
```

Content items with `series_position IS NULL` should appear after explicitly ordered items.

When assigning an existing Content item to a Series and it has no `series_position`, set it to the next available position.

## Dashboard / Metrics Behavior

Series should be usable as a reporting scope because the existing goals schema already allows:

```text
scope_type = 'series'
```

For dashboard summaries, count Content by Series using `content.series_id`.

Useful v1 dashboard widgets:

- active Series count
- Content count per Series
- Published Content per Series in last 30/90 days
- Series with no recent output
- Series with upcoming scheduled Content

Do not treat Series as a publishing channel.

## Calendar Behavior

Calendar items remain based on Content and Publication scheduling.

Series is only metadata for grouping/filtering.

Calendar entries may optionally show Series name as secondary text.

Useful filters:

- filter calendar by Series
- include/exclude archived Series

## Validation Rules

### Series

- `name` is required
- `status` must be one of the existing allowed values
- `pillar_id` may be NULL
- `updated_at` must change on edit

### Content Series Assignment

- `series_id` may be NULL
- if set, it must reference an existing Series
- assigning Content to an archived Series is disallowed
- removing Content from a Series sets both `series_id = NULL` and `series_position = NULL`

## Migration Requirements

Create a new numbered migration that:

1. Adds `content.series_position INTEGER` if it does not already exist.
2. Creates `idx_content_series`.
3. Creates `idx_content_series_position`.
4. Records the migration in `schema_migrations` according to the existing migration system.

The migration should be safe for existing databases.

If SQLite version/features used in the migration do not support `ADD COLUMN IF NOT EXISTS`, perform the existing project pattern for safe migrations.

## Acceptance Criteria

The feature is done when:

1. A Series can be created, edited, archived, and deleted only when unused.
2. Content can be assigned to and removed from a Series.
3. New Content can be created directly inside a Series.
4. Series detail view shows all related Content items.
5. Content items in a Series are ordered by `series_position` first.
6. Move up/down changes the editorial order.
7. Generated burst derivatives inherit `series_id` from the root Content item.
8. Archived Series are hidden from normal Series lists by default.
9. Dashboard/reporting code can count Content by Series.
10. Existing Content without a Series continues to work unchanged.

## Non-Goals for v1

Do not implement these yet:

- automatic episode numbering in titles
- strict uniqueness of `series_position`
- recurring schedule generation
- target episode count
- default channel on Series
- series-level publishing
- series-level Kanban workflow
- nested Series
- dependencies between Series items
- automatic creation of Publications from Series

## Possible Later Additions

Potential future fields:

```sql
series.slug TEXT
series.start_date DATETIME
series.end_date DATETIME
series.default_channel_id TEXT
series.target_episode_count INTEGER
```

Potential future UX:

- drag-and-drop ordering
- episode number badges
- progress bar: published / planned / total
- "continue this series" quick action
- detect gaps in episode order
- suggest next installment
- series landing page/export
