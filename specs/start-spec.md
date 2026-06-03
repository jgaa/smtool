# MediaStrategy - Qt Desktop POC Specification v1

## Purpose

MediaStrategy is a local-first desktop application for managing the lifecycle of content creation.

The application is intended for solo creators, software founders, consultants, and small businesses that produce long-form and short-form content.

The primary focus is:

* Idea capture
* Content planning
* Content production workflow
* Content scheduling
* Content series management
* Fan-out ("burst") generation from long-form source content

The application is not a publishing tool.

Publishing automation, social media APIs, analytics, and cloud synchronization are explicitly out of scope for v1.

---

# Technology

* Qt 6
* C++20
* QML
* SQLite
* QtSql
* CMake

---

# Architecture

## General

* Local-first application
* Single-user
* SQLite database
* Synchronous database access
* Repository layer
* QAbstractListModel-based models exposed to QML
* MVVM-inspired architecture

## Database Location

Store the SQLite database under:

`QStandardPaths::AppDataLocation`

## SQLite Rules

* UUID values are stored as `TEXT` using standard UUID string format
* Foreign keys must be enabled on every database connection using `PRAGMA foreign_keys = ON`

## Migrations

Use numbered SQL migrations.

Create table:

`schema_migrations`

Fields:

* `version INTEGER PRIMARY KEY`
* `applied_at DATETIME`

Requirements:

* Apply migrations sequentially
* Fail startup on migration error

---

# Workflow

Content progresses through the following lifecycle:

Inbox -> Clarifying -> Shaping -> Drafting -> Ready -> Scheduled -> Published -> Reviewing -> Archived

The Kanban board uses these states directly.

## Allowed Transitions

Normal flow:

* `inbox -> clarifying`
* `clarifying -> shaping`
* `shaping -> drafting`
* `drafting -> ready`
* `ready -> scheduled`
* `scheduled -> published`
* `published -> reviewing`
* `reviewing -> archived`

Fallback transitions:

Any non-archived item may be moved back to:

* `inbox`
* `clarifying`
* `shaping`
* `drafting`

Validation:

* Enforce valid status values with database-level `CHECK` constraints
* Validate transitions in application code before writing

---

# Lookup Tables

Lookup values are database-driven.

The application seeds defaults.

Users may extend them manually through SQLite.

A management UI may be added later.

For v1:

* Only active lookup values are selectable for new or edited records
* Existing records that reference inactive values must still display correctly

## Pillar

Fields:

* `id UUID`
* `key TEXT UNIQUE`
* `display_name TEXT`
* `description TEXT NULL`
* `sort_order INTEGER`
* `is_active INTEGER`

Seed values:

* `product`
* `tech`
* `lifehacks`
* `life`
* `appforge`
* `thoughtsAndIdeas`

## ContentKind

Fields:

* `id UUID`
* `key TEXT UNIQUE`
* `display_name TEXT`
* `description TEXT NULL`
* `sort_order INTEGER`
* `is_active INTEGER`

Seed values:

* `idea`
* `blog_post`
* `video`
* `short_post`
* `clip`
* `newsletter`
* `other`

## Outcome

Fields:

* `id UUID`
* `key TEXT UNIQUE`
* `display_name TEXT`
* `description TEXT NULL`
* `sort_order INTEGER`
* `is_active INTEGER`

Seed values:

* `discussion`
* `authority`
* `trust`
* `conversion`
* `learning`
* `other`

## Channel

Fields:

* `id UUID`
* `key TEXT UNIQUE`
* `display_name TEXT`
* `description TEXT NULL`
* `sort_order INTEGER`
* `is_active INTEGER`

Seed values:

* `blog`
* `youtube`
* `linkedin`
* `mastodon`
* `newsletter`

---

# Series

A Series groups related content.

Examples:

* Building MediaStrategy
* AppForge
* NextApp Payments
* OpenValify
* Content Strategy Experiments

Fields:

* `id UUID`
* `name TEXT`
* `description TEXT NULL`
* `pillar_id UUID NULL`
* `status TEXT CHECK(status IN ('active', 'paused', 'completed', 'archived'))`
* `created_at DATETIME`
* `updated_at DATETIME`

Status values:

* `active`
* `paused`
* `completed`
* `archived`

---

# Content

Represents both source content and derivative content.

Fields:

* `id UUID`
* `parent_id UUID NULL`
* `series_id UUID NULL`
* `burst_template_key TEXT NULL`
* `title TEXT`
* `description TEXT NULL`
* `kind_id UUID`
* `pillar_id UUID`
* `outcome_id UUID NULL`
* `suggested_channel_id UUID NULL`
* `status TEXT CHECK(status IN ('inbox', 'clarifying', 'shaping', 'drafting', 'ready', 'scheduled', 'published', 'reviewing', 'archived'))`
* `priority INTEGER NOT NULL DEFAULT 0 CHECK(priority >= 0 AND priority <= 100)`
* `scheduled_at DATETIME NULL`
* `published_at DATETIME NULL`
* `published_url TEXT NULL`
* `created_at DATETIME`
* `updated_at DATETIME`

Rules:

* `parent_id IS NULL` means root/source content
* Only root content may generate bursts
* Derivatives cannot generate bursts
* Derivatives reference their source using `parent_id`
* Content may optionally belong to a series
* `suggested_channel_id` is a planning hint only
* `suggested_channel_id` may be set on manually created content as well as burst-generated content
* `published_at` should normally be set automatically by application logic when status becomes `published` and `published_at` is `NULL`
* v1 does not enforce published timestamp consistency with a database constraint

Idempotent burst uniqueness:

* Manual derivatives may have `burst_template_key NULL`
* Burst-generated children must set both `parent_id` and `burst_template_key`
* There must be at most one generated child per `parent_id` and `burst_template_key`

Schema requirement:

```sql
CREATE UNIQUE INDEX ux_content_parent_burst_template
ON content(parent_id, burst_template_key)
WHERE parent_id IS NOT NULL
  AND burst_template_key IS NOT NULL;
```

---

# Note

Notes are editable in v1.

Fields:

* `id UUID`
* `content_id UUID`
* `body TEXT`
* `created_at DATETIME`
* `updated_at DATETIME`

Default ordering:

* `ORDER BY created_at DESC`

---

# Publication

Represents publication planning for a specific channel.

Fields:

* `id UUID`
* `content_id UUID`
* `channel_id UUID`
* `status TEXT CHECK(status IN ('planned', 'scheduled', 'published'))`
* `scheduled_at DATETIME NULL`
* `published_at DATETIME NULL`
* `url TEXT NULL`
* `created_at DATETIME`
* `updated_at DATETIME`

Status values:

* `planned`
* `scheduled`
* `published`

`Publication.channel_id` is authoritative once a publication exists.

---

# Scheduling

## Content Schedule

`Content.scheduled_at` is the primary planning date.

## Publication Schedule

`Publication.scheduled_at` is a channel-specific override.

If `Publication.scheduled_at` is `NULL`, use `Content.scheduled_at` as the publication's effective schedule.

## Calendar Behavior

Rules:

* If a content item has one or more `Publication` rows, show publication-based calendar entries
* Do not also show `Content.scheduled_at` as a separate calendar entry by default when publications exist
* A publication appears on the calendar only if its effective schedule is not `NULL`
* If a content item has no publications, show `Content.scheduled_at` when it is not `NULL`

Views:

* Daily
* Weekly

---

# Publishing Rules

Content is considered published when either of the following is true:

* `status = published`
* `published_at IS NOT NULL`

Publication records track channel-specific publication activity.

A content item may be considered published even if not all associated publications are published.

---

# Burst Workflow

## Purpose

Generate derivative content from long-form source content.

Example source content:

* Blog post
* Long-form video

Example generated derivatives:

* LinkedIn post
* Mastodon post
* Newsletter summary
* Video clip

## Rules

* Only root content may generate bursts
* Derivatives cannot generate bursts
* Burst generation must be idempotent
* Running burst generation multiple times must not create duplicates
* Generated content starts with `status = shaping`
* Generated content inherits `pillar_id`
* Generated content inherits `series_id`
* Generated content does not inherit `scheduled_at`
* Generated content receives `kind_id`, `outcome_id`, and `suggested_channel_id` from the template
* Existing derivatives do not auto-sync if the source later changes pillar or series
* Burst generation does not create `Publication` records

---

# Burst Templates

Store burst templates in the database.

Fields:

* `key TEXT UNIQUE`
* `display_name TEXT`
* `title_suffix TEXT`
* `kind_id UUID`
* `suggested_channel_id UUID`
* `outcome_id UUID`
* `is_active INTEGER`

Seed templates:

## `linkedin_key_lesson`

* kind: `short_post`
* channel: `linkedin`
* outcome: `authority`

## `linkedin_opinion_angle`

* kind: `short_post`
* channel: `linkedin`
* outcome: `discussion`

## `mastodon_technical_note`

* kind: `short_post`
* channel: `mastodon`
* outcome: `authority`

## `short_video_excerpt`

* kind: `clip`
* channel: `youtube`
* outcome: `trust`

## `newsletter_summary`

* kind: `newsletter`
* channel: `newsletter`
* outcome: `trust`

---

# Main Screens

## Inbox

Purpose:

Capture and triage ideas.

Features:

* Quick add with required title and optional description
* Edit
* Assign pillar
* Assign series
* Assign priority
* Move item to `clarifying`
* Move item to `shaping`

## Board

Kanban board.

Columns:

* Inbox
* Clarifying
* Shaping
* Drafting
* Ready
* Scheduled
* Published
* Reviewing

Behavior:

* Hide archived items by default
* Provide a `Show archived` toggle
* When enabled, archived items are visible on the board
* Full workflow movement is handled on the board

Cards display:

* Title
* Pillar
* Series
* Kind
* Priority
* Schedule date

## Calendar

Views:

* Daily
* Weekly

Displays:

* Content schedules when no publications exist for that content
* Publication schedules when publications exist for that content

## Source/Burst View

Displays:

* Source content
* Derivatives
* Burst relationships

Features:

* Create burst
* Show generated items
* Show suggested channels

## Series View

Displays:

* All series
* Content belonging to series
* Progress by status
* Upcoming scheduled content

## Dashboard

Displays:

* Content count by pillar
* Content count by series
* Content count by status
* Upcoming items next 14 days
* Published content items last 30 days
* Published publications last 30 days
* Pillars with zero published content in last 30 days

Rules:

* The main balance metric counts content items once
* Channel activity counts publication rows
* Published content counts use `Publication.published_at` when publication data exists, otherwise `Content.published_at`
* Archived content is excluded from dashboard metrics by default

Behavior:

* Provide an `Include archived` toggle
* `Include archived` is off by default
* When enabled, archived content and archived series are included in dashboard aggregations where relevant

Archived behavior:

* Board hides archived content by default
* Dashboard hides archived items from metrics by default

---

# Seed Data

Automatically seed when the database is empty:

* Pillars
* Content kinds
* Outcomes
* Channels
* Burst templates

Demo content is inserted only when launched with:

`--seed-demo-data`

---

# UI Rules

For v1:

* Lookup values are loaded from the database
* No management UI for lookup tables
* Keep the UI simple and functional
* No elaborate styling

---

# Delete Strategy

No hard deletes in v1.

Use:

* `archived` status for content and series workflows where applicable

Source content with derivatives must not be physically removed.

---

# Unit Tests

Add tests for:

* Database creation
* Migration execution
* Foreign key enforcement
* Lookup table seeding
* Burst template seeding
* Series creation
* Parent-child relationships
* Burst generation
* Idempotent burst generation
* Scheduling fallback logic

---

# Explicit Non-Goals

Not part of v1:

* Cloud sync
* Android app
* gRPC
* AI integration
* Publishing APIs
* Authentication
* Multi-user support
* Analytics
* Social media metrics
* Team collaboration

---

# Future Roadmap

Potential future additions:

* Android capture app
* Voice note capture
* Local network synchronization
* gRPC sync
* AI-assisted content creation
* Publishing integrations
* Content analytics
* Team support

---

# Deliverables

* Buildable Qt/CMake project
* SQLite schema
* Migration system
* Repository layer
* QML UI with Inbox, Board, Calendar, Source/Burst, Series, and Dashboard views
* Lookup table support
* Series support
* Burst template support
* Seed data
* Burst generation implementation
* Unit tests
* README
