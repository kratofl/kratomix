# Kratomix Release Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Makefile release packaging so every Kratomix plugin can produce a clean zip artifact for attaching to a GitHub release.

**Architecture:** Packaging belongs in the shared Make layer, not individual plugin trees. The target builds one plugin, reads metadata from the existing CMake metadata script, stages only current plugin outputs into `dist/<slug>/`, rejects stale sidechain-named Prism artifacts, and writes one zip per plugin under `dist/`. The package step should be deterministic enough for release use and should not commit generated zips.

**Tech Stack:** GNU Make, CMake metadata script, macOS `ditto`, shell checks, existing JUCE build output layout.

---

## Scope Check

This plan should run after the Prism EQ implementation plans:

1. `docs/superpowers/plans/2026-05-11-prism-eq-live-ui-truth.md`
2. `docs/superpowers/plans/2026-05-11-prism-eq-dsp-quality-engine.md`
3. This release packaging plan.

Included:

- Add `make package PLUGIN=<slug>`.
- Add `make release PLUGIN=<slug>` as an alias that builds, validates AU when available, then packages.
- Create one zip per plugin.
- Ensure Prism EQ zips do not include stale `Kratomix Prism EQ Sidechain.*` artifacts.
- Document the command.

Excluded:

- Uploading to GitHub releases.
- Code signing and notarization.
- Installer `.pkg` generation.
- Universal binary/lipo workflows.

## Current Finding

The current repository has no zip/release Make target. A local scan found stale sidechain-named Prism build outputs under `build/KratomixPrismEq_artefacts/Release/`, including:

```text
build/KratomixPrismEq_artefacts/Release/AU/Kratomix Prism EQ Sidechain.component
build/KratomixPrismEq_artefacts/Release/VST3/Kratomix Prism EQ Sidechain.vst3
build/KratomixPrismEq_artefacts/Release/Standalone/Kratomix Prism EQ Sidechain.app
```

Those stale files should not be packaged. Current Prism metadata in `plugins/prism-eq/plugin.cmake` uses product name `Kratomix Prism EQ`, so package staging must copy only exact current output names from metadata and must reject any staged path containing `Sidechain`.

## File Structure

- Create `mk/package.mk`: package and release Make targets.
- Modify `Makefile`: include `mk/package.mk`.
- Modify `mk/build.mk`: help text should list package and release commands.
- Modify `docs/monorepo.md`: document package output and stale-artifact behavior.
- Modify `plugins/prism-eq/README.md`: document Prism release zip command.
- Optional modify `README.md`: list release packaging command if the root README already has command docs.

### Task 1: Package Target Foundation

**Files:**
- Create: `mk/package.mk`
- Modify: `Makefile`
- Modify: `mk/build.mk`
- Test: shell command checks

- [ ] **Step 1: Write the failing command check**

Run:

```bash
make package PLUGIN=prism-eq
```

Expected: FAIL with `No rule to make target 'package'`.

- [ ] **Step 2: Create `mk/package.mk`**

Create `mk/package.mk`:

```make
.PHONY: package release

DIST_DIR ?= dist
ABS_DIST_DIR = $(if $(filter /%,$(DIST_DIR)),$(DIST_DIR),$(ROOT_DIR)/$(DIST_DIR))

define require_ditto
if ! command -v ditto >/dev/null 2>&1; then \
	echo "The package target requires macOS ditto." >&2; \
	exit 2; \
fi
endef

package:
	@$(call require_plugin)
	@$(call require_ditto)
	@$(MAKE) build PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@$(call load_plugin_metadata); \
	stage_dir='$(ABS_DIST_DIR)/$(PLUGIN)'; \
	zip_path='$(ABS_DIST_DIR)/$(PLUGIN)-$(CONFIG).zip'; \
	rm -rf "$$stage_dir" "$$zip_path"; \
	mkdir -p "$$stage_dir"; \
	if [ "$$has_au" = "1" ]; then \
		au_path='$(ABS_BUILD_DIR)/'"$$cmake_target"'_artefacts/$(CONFIG)/AU/'"$$product_name"'.component'; \
		if [ ! -d "$$au_path" ]; then echo "Missing AU artifact: $$au_path" >&2; exit 2; fi; \
		mkdir -p "$$stage_dir/AU"; \
		ditto "$$au_path" "$$stage_dir/AU/$${product_name}.component"; \
	fi; \
	if [ "$$has_vst3" = "1" ]; then \
		vst3_path='$(ABS_BUILD_DIR)/'"$$cmake_target"'_artefacts/$(CONFIG)/VST3/'"$$product_name"'.vst3'; \
		if [ ! -d "$$vst3_path" ]; then echo "Missing VST3 artifact: $$vst3_path" >&2; exit 2; fi; \
		mkdir -p "$$stage_dir/VST3"; \
		ditto "$$vst3_path" "$$stage_dir/VST3/$${product_name}.vst3"; \
	fi; \
	if [ "$$has_standalone" = "1" ]; then \
		standalone_path='$(ABS_BUILD_DIR)/'"$$cmake_target"'_artefacts/$(CONFIG)/Standalone/'"$$product_name"'.app'; \
		if [ ! -d "$$standalone_path" ]; then echo "Missing Standalone artifact: $$standalone_path" >&2; exit 2; fi; \
		mkdir -p "$$stage_dir/Standalone"; \
		ditto "$$standalone_path" "$$stage_dir/Standalone/$${product_name}.app"; \
	fi; \
	if find "$$stage_dir" -iname '*Sidechain*' -print -quit | grep -q .; then \
		echo "Refusing to package stale sidechain artifact for $(PLUGIN)." >&2; \
		find "$$stage_dir" -iname '*Sidechain*' -print >&2; \
		exit 2; \
	fi; \
	ditto -c -k --keepParent "$$stage_dir" "$$zip_path"; \
	echo "Created $$zip_path"

release:
	@$(call require_plugin)
	@$(MAKE) build PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@$(call load_plugin_metadata); \
	if [ "$$has_au" = "1" ]; then \
		$(MAKE) validate PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'; \
	fi
	@$(MAKE) package PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' DIST_DIR='$(DIST_DIR)'
```

- [ ] **Step 3: Include the package Makefile**

In `Makefile`, add this line after `include $(ROOT_DIR)/mk/build.mk`:

```make
include $(ROOT_DIR)/mk/package.mk
```

- [ ] **Step 4: Add help text**

In `mk/build.mk`, change the `.PHONY` line:

```make
.PHONY: help list configure build test run validate clean
```

to:

```make
.PHONY: help list configure build test run validate package release clean
```

Add these help lines after the validate line:

```make
		'  make package PLUGIN=<slug> [BUILD_DIR=build] [CONFIG=Release] [DIST_DIR=dist]' \
		'  make release PLUGIN=<slug> [BUILD_DIR=build] [CONFIG=Release] [DIST_DIR=dist]' \
```

- [ ] **Step 5: Run help command**

Run:

```bash
make help
```

Expected: output includes `make package PLUGIN=<slug>` and `make release PLUGIN=<slug>`.

### Task 2: Metadata Variables For Package Paths

**Files:**
- Modify: `core/cmake/print_plugin_metadata.cmake`
- Test: metadata print command

- [ ] **Step 1: Inspect current metadata output**

Run:

```bash
cmake -DKRATOMIX_ROOT="$PWD" -DKRATOMIX_PLUGIN_SLUG=prism-eq -DKRATOMIX_BUILD_DIR="$PWD/build" -DKRATOMIX_BUILD_CONFIG=Release -P core/cmake/print_plugin_metadata.cmake
```

Expected: output contains shell assignments such as `cmake_target=...`, `product_name=...`, `has_au=...`, `has_vst3=...`, and `has_standalone=...`. If any of these are missing, add them in Step 2.

- [ ] **Step 2: Add missing metadata assignments**

In `core/cmake/print_plugin_metadata.cmake`, ensure it prints these shell-safe variables:

```cmake
message("slug='${KRATOMIX_PLUGIN_SLUG}'")
message("cmake_target='${KRATOMIX_CMAKE_TARGET}'")
message("product_name='${KRATOMIX_PRODUCT_NAME}'")
message("plugin_code='${KRATOMIX_PLUGIN_CODE}'")
message("manufacturer_code='${KRATOMIX_PLUGIN_MANUFACTURER_CODE}'")
message("au_main_type='${KRATOMIX_AU_MAIN_TYPE}'")
message("has_au='${has_au}'")
message("has_vst3='${has_vst3}'")
message("has_standalone='${has_standalone}'")
message("au_target='${au_target}'")
message("standalone_target='${standalone_target}'")
```

Keep any existing variables already used by `make validate` and `make run`.

- [ ] **Step 3: Re-run metadata output**

Run:

```bash
cmake -DKRATOMIX_ROOT="$PWD" -DKRATOMIX_PLUGIN_SLUG=prism-eq -DKRATOMIX_BUILD_DIR="$PWD/build" -DKRATOMIX_BUILD_CONFIG=Release -P core/cmake/print_plugin_metadata.cmake
```

Expected: all variables listed in Step 2 are present.

### Task 3: Prism EQ Package Must Exclude Stale Sidechain Artifacts

**Files:**
- Modify: `mk/package.mk`
- Test: package command and zip listing

- [ ] **Step 1: Verify stale artifacts exist locally**

Run:

```bash
find build/KratomixPrismEq_artefacts/Release -iname '*Sidechain*' -print
```

Expected: this may print old `Kratomix Prism EQ Sidechain.*` artifacts. These are stale build outputs and are not part of current Prism metadata.

- [ ] **Step 2: Package Prism EQ**

Run:

```bash
make package PLUGIN=prism-eq
```

Expected: command creates `dist/prism-eq-Release.zip`.

- [ ] **Step 3: Inspect zip contents**

Run:

```bash
zipinfo -1 dist/prism-eq-Release.zip
```

Expected: zip contains current exact-name artifacts only:

```text
prism-eq/
prism-eq/AU/
prism-eq/AU/Kratomix Prism EQ.component/
prism-eq/VST3/
prism-eq/VST3/Kratomix Prism EQ.vst3/
prism-eq/Standalone/
prism-eq/Standalone/Kratomix Prism EQ.app/
```

Expected: no output path contains `Sidechain`.

- [ ] **Step 4: Run explicit sidechain rejection check**

Run:

```bash
zipinfo -1 dist/prism-eq-Release.zip | grep -i Sidechain
```

Expected: command exits with status 1 and prints nothing.

If this command prints any path, fix `mk/package.mk` so it stages only exact current metadata artifact names and fails when staged files contain `Sidechain`.

### Task 4: Package Every Plugin

**Files:**
- No source changes unless a plugin metadata issue is found.
- Test: package commands

- [ ] **Step 1: Package Velvet Channel**

Run:

```bash
make package PLUGIN=velvet-channel
```

Expected: command creates `dist/velvet-channel-Release.zip`.

- [ ] **Step 2: Inspect Velvet zip**

Run:

```bash
zipinfo -1 dist/velvet-channel-Release.zip | head -40
```

Expected: output lists current Velvet Channel AU/VST3/Standalone artifacts and no unrelated plugin artifacts.

- [ ] **Step 3: Package Prism EQ**

Run:

```bash
make package PLUGIN=prism-eq
```

Expected: command creates or replaces `dist/prism-eq-Release.zip`.

- [ ] **Step 4: Check dist directory**

Run:

```bash
find dist -maxdepth 1 -type f -name '*.zip' -print | sort
```

Expected:

```text
dist/prism-eq-Release.zip
dist/velvet-channel-Release.zip
```

### Task 5: Documentation

**Files:**
- Modify: `docs/monorepo.md`
- Modify: `plugins/prism-eq/README.md`
- Optional Modify: `README.md`

- [ ] **Step 1: Update monorepo docs**

In `docs/monorepo.md`, add a release packaging section:

```markdown
## Release Packaging

Create a zip for one plugin:

```bash
make package PLUGIN=prism-eq
```

Create a build, validate the AU when present, and write a release zip:

```bash
make release PLUGIN=prism-eq
```

Release zips are written to `dist/<plugin>-<config>.zip`. The package step stages only the current artifact names from `plugins/<slug>/plugin.cmake`, so stale build outputs from old product names are not included. Generated `dist/` artifacts are release outputs and should not be committed.
```

- [ ] **Step 2: Update Prism README**

In `plugins/prism-eq/README.md`, add:

```markdown
## Release Zip

```bash
make package PLUGIN=prism-eq
```

The zip is written to `dist/prism-eq-Release.zip` and should contain only `Kratomix Prism EQ` artifacts. Stale sidechain-named build outputs are not part of the Prism package.
```

- [ ] **Step 3: Run docs grep**

Run:

```bash
rg -n "make package|make release|dist/.+zip|Sidechain" docs plugins/prism-eq/README.md README.md
```

Expected: docs mention the package/release commands and explain that stale sidechain-named Prism outputs are excluded.

### Task 6: Full Verification

**Files:**
- No code changes.

- [ ] **Step 1: Run tests**

Run:

```bash
make test
```

Expected: all plugin tests pass.

- [ ] **Step 2: Package all current plugins**

Run:

```bash
make package PLUGIN=velvet-channel
make package PLUGIN=prism-eq
```

Expected: both commands create release zips under `dist/`.

- [ ] **Step 3: Confirm Prism zip excludes sidechain artifacts**

Run:

```bash
zipinfo -1 dist/prism-eq-Release.zip | grep -i Sidechain
```

Expected: command exits with status 1 and prints nothing.

- [ ] **Step 4: Check worktree**

Run:

```bash
git status --short
```

Expected: intentional source/docs changes are listed. Generated `dist/` zips may appear only if `.gitignore` does not ignore `dist/`; if they appear, add `dist/` to `.gitignore` or remove the generated zips before committing.

## Self-Review

- User request coverage: Adds a Makefile zip/release step after the Prism implementation plans, creates one zip per plugin, and explicitly excludes stale Prism sidechain artifacts.
- Sidechain answer: Current Prism source metadata is `Kratomix Prism EQ`; stale `Kratomix Prism EQ Sidechain.*` build outputs can still remain in `build/` and must not be copied into release packages.
- Scope control: The plan does not upload to GitHub or add signing/notarization.
- Placeholder scan: The plan uses concrete file paths, commands, code snippets, and expected outputs.
