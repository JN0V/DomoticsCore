<!-- workline
sources: [library.json, library.properties, tools/check_versions.py, release-please-config.json]
-->

# Versioning

- **Root library**: The top-level `library.json` defines the `DomoticsCore` framework version (`X.Y.Z`).
- **Component libraries**: Each `DomoticsCore-*` sub-library has its own `library.json` `version` and a matching `metadata.version` in its C++ component class.
- **Versions move through release-please** (`release-please-config.json`): it keeps one pull request open, "chore: release main", with the next versions computed from the conventional commits — a component's only when its folder changed, the root by the highest level of any change (`fix` a patch, `feat` a minor, `!` a major) — in each `library.json`, `library.properties`, the components' `metadata.version`, and `CHANGELOG.md`. Merging it is the decision to release: release-please tags the merged commit (`vX.Y.Z` for the root, `DomoticsCore-<Component>-vX.Y.Z` for a component), writes the GitHub releases, and `release.yml` publishes the root to PlatformIO. Versions are never edited by hand; to force one, set `release-as` on its package in `release-please-config.json`, with the reason in that commit, and remove it once released.

### Versioning tools

- **Consistency check** (used in CI):

  ```bash
  python tools/check_versions.py --verbose
  ```

  This script ensures that, for every `DomoticsCore-*` directory:

  - `library.json.version` matches all `metadata.version = "X.Y.Z"` assignments under `include/` and `src/`.
  - (Optionally) with `--check-tag`, the root `library.json.version` matches the Git tag `vX.Y.Z` when run on a tagged commit.
