CREATE TABLE pillar (
    id TEXT PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    description TEXT NULL,
    sort_order INTEGER NOT NULL,
    is_active INTEGER NOT NULL CHECK(is_active IN (0, 1))
);

CREATE TABLE content_kind (
    id TEXT PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    description TEXT NULL,
    sort_order INTEGER NOT NULL,
    is_active INTEGER NOT NULL CHECK(is_active IN (0, 1))
);

CREATE TABLE outcome (
    id TEXT PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    description TEXT NULL,
    sort_order INTEGER NOT NULL,
    is_active INTEGER NOT NULL CHECK(is_active IN (0, 1))
);

CREATE TABLE channel (
    id TEXT PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    description TEXT NULL,
    sort_order INTEGER NOT NULL,
    is_active INTEGER NOT NULL CHECK(is_active IN (0, 1))
);

CREATE TABLE burst_template (
    key TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    title_suffix TEXT NOT NULL,
    kind_id TEXT NOT NULL REFERENCES content_kind(id),
    suggested_channel_id TEXT NOT NULL REFERENCES channel(id),
    outcome_id TEXT NOT NULL REFERENCES outcome(id),
    is_active INTEGER NOT NULL CHECK(is_active IN (0, 1))
);

CREATE TABLE series (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NULL,
    pillar_id TEXT NULL REFERENCES pillar(id),
    status TEXT NOT NULL CHECK(status IN ('active', 'paused', 'completed', 'archived')),
    created_at DATETIME NOT NULL,
    updated_at DATETIME NOT NULL
);

CREATE TABLE content (
    id TEXT PRIMARY KEY,
    parent_id TEXT NULL REFERENCES content(id),
    series_id TEXT NULL REFERENCES series(id),
    burst_template_key TEXT NULL REFERENCES burst_template(key),
    title TEXT NOT NULL,
    description TEXT NULL,
    kind_id TEXT NOT NULL REFERENCES content_kind(id),
    pillar_id TEXT NOT NULL REFERENCES pillar(id),
    outcome_id TEXT NULL REFERENCES outcome(id),
    suggested_channel_id TEXT NULL REFERENCES channel(id),
    status TEXT NOT NULL CHECK(status IN ('inbox', 'clarifying', 'shaping', 'drafting', 'ready', 'scheduled', 'published', 'reviewing', 'archived')),
    priority INTEGER NOT NULL DEFAULT 0 CHECK(priority >= 0 AND priority <= 100),
    scheduled_at DATETIME NULL,
    published_at DATETIME NULL,
    published_url TEXT NULL,
    created_at DATETIME NOT NULL,
    updated_at DATETIME NOT NULL
);

CREATE UNIQUE INDEX ux_content_parent_burst_template
ON content(parent_id, burst_template_key)
WHERE parent_id IS NOT NULL
  AND burst_template_key IS NOT NULL;

CREATE TABLE note (
    id TEXT PRIMARY KEY,
    content_id TEXT NOT NULL REFERENCES content(id),
    body TEXT NOT NULL,
    created_at DATETIME NOT NULL,
    updated_at DATETIME NOT NULL
);

CREATE TABLE publication (
    id TEXT PRIMARY KEY,
    content_id TEXT NOT NULL REFERENCES content(id),
    channel_id TEXT NOT NULL REFERENCES channel(id),
    status TEXT NOT NULL CHECK(status IN ('planned', 'scheduled', 'published')),
    scheduled_at DATETIME NULL,
    published_at DATETIME NULL,
    url TEXT NULL,
    created_at DATETIME NOT NULL,
    updated_at DATETIME NOT NULL
);
