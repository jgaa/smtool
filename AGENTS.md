# AGENTS.md

This file captures project-specific working rules for humans and coding agents.

Product design, workflow, and schema intent belong in [specs/start-spec.md](/home/jgaa/src/smtool/specs/start-spec.md). Do not duplicate or maintain design details here unless they affect repository workflow.

## Project Identity

* The current app/repo name is `smtool`.
* The user previously renamed the app from `MediaStrategy` to `smtool` using search-and-replace.
* Before continuing implementation, check for leftover old-name references and handle them deliberately. Do not assume the rename is fully complete everywhere.

## Scope and Delivery Expectations

* This project is a POC intended to become usable quickly.
* Optimize for a usable app early, without letting the codebase become sloppy.
* The code is open source / GPLv3, so implementation quality matters.
* Keep the architecture small, explicit, and maintainable. Do not overengineer.

## Source of Truth

* Functional requirements and UX behavior live in [specs/start-spec.md](/home/jgaa/src/smtool/specs/start-spec.md).
* If implementation questions are not resolved by the spec or existing code, stop and ask the user instead of guessing.
* Do not invent user preferences beyond what is explicitly stated in chat or docs.

## Build and Layout

* The top-level desktop build entry point must stay in [desktop/CMakeLists.txt](/home/jgaa/src/smtool/desktop/CMakeLists.txt).
* The desktop app source lives under [desktop/src](/home/jgaa/src/smtool/desktop/src).
* SQLite migrations live under [desktop/resources/migrations](/home/jgaa/src/smtool/desktop/resources/migrations).
* Tests live under [desktop/tests](/home/jgaa/src/smtool/desktop/tests).

## Current Implementation Shape

* The app currently uses Qt 6, C++20, QML, QtSql, and CMake.
* Data access is synchronous and goes through repository classes.
* QML-facing collections are exposed via `QAbstractListModel` subclasses.
* The main coordinator is [desktop/src/app/appcontroller.h](/home/jgaa/src/smtool/desktop/src/app/appcontroller.h).
* The initial schema is in [desktop/resources/migrations/001_initial.sql](/home/jgaa/src/smtool/desktop/resources/migrations/001_initial.sql).

## Coding Rules

* Prefer modern C++20 features over C-style or legacy C++ patterns.
* Prefer standard library facilities when they improve clarity.
* Use `std::array` and similar typed containers instead of ad hoc string tables or raw C arrays.
* Use views/ranges where appropriate, but not performatively.
* Keep Qt/C++ code straightforward and readable.
* Avoid unnecessary abstraction layers, generic frameworks, or speculative extension points.
* Maintain strict repository boundaries rather than pushing SQL or business rules into QML.

## Persistence and Runtime Rules

* Production/default database storage should remain based on `QStandardPaths::AppDataLocation`.
* The app currently also supports `--database-path` for explicit SQLite path override. Keep that unless there is a good reason to remove it.
* In sandboxed or CI-like environments, `--database-path /tmp/...` may be required because the normal app-data location may not be writable.

## Validation and Behavior Rules

* Preserve database constraints and application-level validation together.
* Keep burst generation idempotent.
* Preserve explicit workflow transition rules from the spec.
* Archived visibility behavior is controlled by UI toggles, not implicit query behavior alone.

## Testing Expectations

* Keep tests runnable with `ctest`.
* Add or update tests when changing migrations, repository logic, burst behavior, scheduling logic, or status transitions.
* Prefer focused repository/database tests over vague UI smoke coverage when time is limited.

## Working Style for Future Agents

* Read the spec and inspect the current code before making structural changes.
* If a change touches naming, build paths, migrations, or app startup behavior, verify the effect end-to-end.
* Build and run tests after substantive changes.
* For runtime/UI fixes, prefer an actual startup smoke check when feasible.
* If user preference is unclear, ask instead of silently choosing.

## Known Practical Notes

* A Qt Creator user file exists at [desktop/.qtcreator/CMakeLists.txt.user](/home/jgaa/src/smtool/desktop/.qtcreator/CMakeLists.txt.user). Treat it as local/editor state unless the user explicitly asks otherwise.
