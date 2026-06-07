# smtool

smtool is a native content strategy and publishing workflow application built with Qt.

The goal is to help creators, founders, developers, and indie hackers transform ideas into a balanced stream of long-form and short-form content.

## Why?

Most content tools focus on publishing.

smtool focuses on the workflow before publishing:

Idea → Draft → Long-form Content → Fan-out → Schedule → Publish

The project started as an experiment to improve my own content creation workflow while documenting the development of software products in public.

## Goals

- Capture and organize content ideas
- Maintain a balanced content strategy
- Plan blogs, videos, and social posts
- Create fan-out workflows from long-form content
- Visualize publishing schedules
- Track content through its lifecycle

## Philosophy

smtool is built around a simple idea: your ideas should remain yours.

Content planning does not require cloud services, user accounts, subscriptions, tracking, telemetry, or advertising.

smtool is a desktop application that stores data on your own devices and operates independently of online services.

The project aims to provide a calm, distraction-free environment for content planning and publishing workflows while respecting privacy, ownership, and user autonomy.

Any future synchronization features will be optional and designed to preserve these principles. 

## Example Workflow

A single source item:

- Blog post: "Building secure gRPC services"

can generate:

- LinkedIn post: Key lesson
- LinkedIn post: Common mistake
- Mastodon post: Technical detail
- Short video clip
- Newsletter summary

smtool helps manage this process.

## Companion Android App

A lightweight Android companion app is planned to make idea capture fast and frictionless.

The purpose of the mobile app is not to provide full content management functionality. Instead, it acts as a simple inbox for collecting ideas while away from the desktop.

Planned features:

- One-tap idea capture
- Voice dictation
- Quick notes
- Optional content pillar selection
- Local storage
- Offline operation

Example workflow:

1. An idea appears while walking, commuting, or exercising.
2. Open the mobile app.
3. Dictate the idea.
4. Save.
5. Continue what you were doing.

At a later time, the mobile app will synchronize captured ideas with the desktop application, where they enter the Inbox workflow for clarification, planning, scheduling, and publication.

The design goal is to minimize friction and context switching while maximizing idea capture. The mobile application is designed as a capture tool, not a content creation tool.

## Current Status

The desktop app is ready in it's initial version.

The mobile companion app is under development.

## Technology

- Qt 6
- C++20
- QML
- SQLite
- CMake

## Build

```bash
cmake -S desktop -B build/desktop
cmake --build build/desktop --parallel
```

## Run

```bash
./build/desktop/bin/SmTool
```

To seed demo content into an empty database:

```bash
./build/desktop/bin/SmTool --seed-demo-data
```

To override the SQLite path explicitly:

```bash
./build/desktop/bin/SmTool --database-path /tmp/smtool.sqlite
```

By default the application stores its database under `QStandardPaths::AppDataLocation`.

## Test

```bash
ctest --test-dir build/desktop --output-on-failure
```
