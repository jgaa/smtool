CREATE TABLE goals (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    goal_type TEXT NOT NULL CHECK(goal_type IN ('count', 'cadence', 'balance')),
    scope_type TEXT NOT NULL CHECK(scope_type IN ('pillar', 'tag', 'channel', 'series', 'kind')),
    scope_id TEXT NULL,
    metric_type TEXT NOT NULL CHECK(metric_type IN ('content_count', 'publication_count', 'balance_weight')),
    target_value INTEGER NULL,
    period_type TEXT NULL CHECK(period_type IS NULL OR period_type IN ('day', 'week', 'month', 'quarter', 'year', 'rolling_days')),
    period_value INTEGER NULL CHECK(period_value IS NULL OR period_value > 0),
    enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE goal_balance_items (
    id TEXT PRIMARY KEY,
    goal_id TEXT NOT NULL REFERENCES goals(id) ON DELETE CASCADE,
    scope_type TEXT NOT NULL CHECK(scope_type IN ('pillar', 'tag', 'channel', 'series', 'kind')),
    scope_id TEXT NOT NULL,
    weight INTEGER NOT NULL DEFAULT 1 CHECK(weight >= 0),
    sort_order INTEGER NOT NULL DEFAULT 0,
    UNIQUE(goal_id, scope_type, scope_id)
);

CREATE INDEX idx_goals_type ON goals(goal_type);
CREATE INDEX idx_goals_scope ON goals(scope_type, scope_id);
CREATE INDEX idx_goals_enabled ON goals(enabled);
CREATE INDEX idx_goal_balance_items_goal ON goal_balance_items(goal_id);
CREATE INDEX idx_goal_balance_items_scope ON goal_balance_items(scope_type, scope_id);
