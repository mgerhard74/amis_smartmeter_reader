# Maintainer Guide

This file contains maintainer-specific operational details.
Contributor workflow, branch model and PR rules are documented in `CONTRIBUTING.md`.

## Branch model (maintainer summary)
The project uses a lightweight GitFlow model:

- `main`: stable production branch, release tags are created here.
- `develop`: integration branch for completed features.
- `feature/*`: feature development branches (from `develop`).
- `release/*`: release stabilization branches (from `develop`).
- `hotfix/*`: critical production fixes (from `main`).

For exact flow and merge rules, follow `CONTRIBUTING.md`.

## GitHub workflows

### Files
- `.github/workflows/build.yml`
- `.github/workflows/cpplint.yml`

### Trigger behavior
- `push` and `pull_request` trigger build and cpplint (https://github.com/cpplint/cpplint).
- Markdown-only and `Docs/**` changes are ignored by workflow triggers.
- Release artifacts are generated only when a tag is pushed (for example `v1.8.0`).

## Required repository settings (GitHub)

Open:
- `https://github.com/<ORG_OR_USER>/<REPO>/settings/actions`

Recommended:
- Actions permissions: `Allow all actions and reusable workflows`

Also open:
- `https://github.com/<ORG_OR_USER>/<REPO>/settings/actions` (Workflow permissions section)

Recommended:
- Workflow permissions: `Read and write permissions`

The release workflow needs write permissions to create/update GitHub releases and upload artifacts.

## Release runbook (maintainers)

On release generation the automatic built files get attached to the release.
Also a summary of commit messages is set to the release informations which can be adapted after gerneration via github interface.

1. Create `release/<version>` from `develop`.
2. Stabilize only (bug fixes, docs, version updates).
3. Merge `release/<version>` into `main` via PR.
4. Create and push tag on `main`: `v<version>`.
5. Merge `release/<version>` back into `develop`.

Example:
- branch: `release/1.8.0`
- tag: `v1.8.0`

## Hotfix runbook

1. Create `hotfix/<version>` from `main`.
2. Fix critical issue and merge into `main`.
3. Tag the hotfix release on `main` (for example `v1.8.1`).
4. Merge hotfix back into `develop`.

## Branch protection (recommended)
Enable branch protection for `main` and `develop`:

- Pull request required
- At least one reviewer
- Required status checks must pass
- Direct pushes blocked