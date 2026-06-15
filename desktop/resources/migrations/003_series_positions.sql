ALTER TABLE content ADD COLUMN series_position INTEGER;

CREATE INDEX IF NOT EXISTS idx_content_series
ON content(series_id);

CREATE INDEX IF NOT EXISTS idx_content_series_position
ON content(series_id, series_position);
