.PHONY: package manifest installer release-docs release

DIST_DIR ?= dist
ABS_DIST_DIR = $(if $(filter /%,$(DIST_DIR)),$(DIST_DIR),$(ROOT_DIR)/$(DIST_DIR))
INSTALLER_DIR := $(ROOT_DIR)/tools/kratomix-installer
INSTALLER_BUILD_DIR := $(ABS_DIST_DIR)/Kratomix Installer.app
INSTALLER_ASSET := $(ABS_DIST_DIR)/Kratomix-Installer-$(VERSION)-macos.zip
MANIFEST_PATH := $(ABS_DIST_DIR)/manifest.json
INSTALLER_SCREENSHOT_SOURCE := $(ROOT_DIR)/docs/assets/installer.png
RELEASE_INSTALL_GUIDE := $(ABS_DIST_DIR)/INSTALL.md
RELEASE_NOTES_PATH := $(ABS_DIST_DIR)/RELEASE_NOTES.md
RELEASE_SCREENSHOT := $(ABS_DIST_DIR)/kratomix-installer-screenshot.png

define require_ditto
if ! command -v ditto >/dev/null 2>&1; then \
	echo "Target '$@' requires macOS ditto." >&2; \
	exit 2; \
fi
endef

define require_zip
if ! command -v zip >/dev/null 2>&1; then \
	echo "Target '$@' requires zip." >&2; \
	exit 2; \
fi
endef

define require_go
if ! command -v go >/dev/null 2>&1; then \
	echo "Target '$@' requires Go." >&2; \
	exit 2; \
fi
endef

package:
	@$(call require_release_version)
	@$(call require_ditto)
	@$(call require_zip)
	@if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
		for slug in $(PLUGIN_SLUGS); do \
			$(MAKE_BIN) package PLUGIN="$$slug" VERSION='$(VERSION)' RELEASE_TAG='$(RELEASE_TAG)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' DIST_DIR='$(DIST_DIR)' || exit $$?; \
		done; \
	else \
		$(call require_plugin); \
		$(MAKE_BIN) build PLUGIN='$(PLUGIN)' VERSION='$(VERSION)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' || exit $$?; \
		$(call load_plugin_metadata); \
		stage_dir='$(ABS_DIST_DIR)/$(PLUGIN)-$(VERSION)'; \
		rm -rf "$$stage_dir" "$$asset_zip"; \
		mkdir -p "$$stage_dir"; \
		if [ "$$has_au" = "1" ]; then \
			if [ ! -d "$$au_artifact" ]; then echo "Missing AU artifact: $$au_artifact" >&2; exit 2; fi; \
			mkdir -p "$$stage_dir/AU"; \
			COPYFILE_DISABLE=1 ditto --norsrc "$$au_artifact" "$$stage_dir/AU/$${product_name}.component"; \
		fi; \
		if [ "$$has_vst3" = "1" ]; then \
			if [ ! -d "$$vst3_artifact" ]; then echo "Missing VST3 artifact: $$vst3_artifact" >&2; exit 2; fi; \
			mkdir -p "$$stage_dir/VST3"; \
			COPYFILE_DISABLE=1 ditto --norsrc "$$vst3_artifact" "$$stage_dir/VST3/$${product_name}.vst3"; \
		fi; \
		if find "$$stage_dir" -iname '*Sidechain*' -print -quit | grep -q .; then \
			echo "Refusing to package stale sidechain artifact for $(PLUGIN)." >&2; \
			find "$$stage_dir" -iname '*Sidechain*' -print >&2; \
			exit 2; \
		fi; \
		mkdir -p '$(ABS_DIST_DIR)'; \
		(cd "$$stage_dir" && COPYFILE_DISABLE=1 zip -qry "$$asset_zip" .); \
		rm -rf "$$stage_dir"; \
		echo "Created $$asset_zip"; \
	fi

manifest:
	@$(call require_release_version)
	@mkdir -p '$(ABS_DIST_DIR)'
	@$(call compute_release_tag); \
	printf '{\n' > '$(MANIFEST_PATH)'; \
	printf '  "schema": 1,\n' >> '$(MANIFEST_PATH)'; \
	printf '  "brand": "Kratomix",\n' >> '$(MANIFEST_PATH)'; \
	printf '  "version": "$(VERSION)",\n' >> '$(MANIFEST_PATH)'; \
	printf '  "releaseTag": "%s",\n' "$$tag" >> '$(MANIFEST_PATH)'; \
	printf '  "plugins": [\n' >> '$(MANIFEST_PATH)'; \
	first=1; \
	for slug in $(PLUGIN_SLUGS); do \
		if [ -n "$(PLUGIN)" ] && [ "$(PLUGIN)" != "all" ] && [ "$(PLUGIN)" != "$$slug" ]; then continue; fi; \
		metadata="$$( $(CMAKE) -DKRATOMIX_ROOT='$(ROOT_DIR)' -DKRATOMIX_PLUGIN_SLUG="$$slug" -DKRATOMIX_BUILD_DIR='$(ABS_BUILD_DIR)' -DKRATOMIX_BUILD_CONFIG='$(CONFIG)' -DKRATOMIX_RELEASE_VERSION='$(VERSION)' -DKRATOMIX_RELEASE_TAG="$$tag" -P '$(METADATA_SCRIPT)' 2>&1 )" || { printf '%s\n' "$$metadata" >&2; exit 2; }; \
		eval "$$metadata"; \
		if [ "$$first" -eq 0 ]; then printf ',\n' >> '$(MANIFEST_PATH)'; fi; \
		first=0; \
		printf '    {\n' >> '$(MANIFEST_PATH)'; \
		printf '      "slug": "%s",\n' "$$slug" >> '$(MANIFEST_PATH)'; \
		printf '      "name": "%s",\n' "$$product_name" >> '$(MANIFEST_PATH)'; \
		printf '      "version": "$(VERSION)",\n' >> '$(MANIFEST_PATH)'; \
		printf '      "asset": "%s",\n' "$$asset_name" >> '$(MANIFEST_PATH)'; \
		printf '      "url": "%s",\n' "$$asset_url" >> '$(MANIFEST_PATH)'; \
		printf '      "formats": {' >> '$(MANIFEST_PATH)'; \
		printf '"au": %s, "vst3": %s' "$$([ "$$has_au" = "1" ] && echo true || echo false)" "$$([ "$$has_vst3" = "1" ] && echo true || echo false)" >> '$(MANIFEST_PATH)'; \
		printf '}\n' >> '$(MANIFEST_PATH)'; \
		printf '    }' >> '$(MANIFEST_PATH)'; \
	done; \
	printf '\n  ]\n' >> '$(MANIFEST_PATH)'; \
	printf '}\n' >> '$(MANIFEST_PATH)'; \
	echo "Created $(MANIFEST_PATH)"

installer:
	@$(call require_release_version)
	@$(call require_go)
	@$(call require_ditto)
	@rm -rf '$(INSTALLER_BUILD_DIR)' '$(INSTALLER_ASSET)'
	@mkdir -p '$(INSTALLER_BUILD_DIR)/Contents/MacOS' '$(INSTALLER_BUILD_DIR)/Contents/Resources' '$(ABS_DIST_DIR)'
	@cd '$(INSTALLER_DIR)' && go build -o '$(INSTALLER_BUILD_DIR)/Contents/MacOS/Kratomix Installer' .
	@printf '%s\n' \
		'<?xml version="1.0" encoding="UTF-8"?>' \
		'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
		'<plist version="1.0">' \
		'<dict>' \
		'  <key>CFBundleExecutable</key>' \
		'  <string>Kratomix Installer</string>' \
		'  <key>CFBundleIdentifier</key>' \
		'  <string>com.kratomix.installer</string>' \
		'  <key>CFBundleName</key>' \
		'  <string>Kratomix Installer</string>' \
		'  <key>CFBundleDisplayName</key>' \
		'  <string>Kratomix Installer</string>' \
		'  <key>CFBundleIconFile</key>' \
		'  <string>AppIcon</string>' \
		'  <key>CFBundlePackageType</key>' \
		'  <string>APPL</string>' \
		'  <key>CFBundleShortVersionString</key>' \
		'  <string>$(VERSION)</string>' \
		'  <key>CFBundleVersion</key>' \
		'  <string>$(VERSION)</string>' \
		'</dict>' \
		'</plist>' > '$(INSTALLER_BUILD_DIR)/Contents/Info.plist'
	@if [ -f '$(MANIFEST_PATH)' ]; then \
		COPYFILE_DISABLE=1 ditto --norsrc '$(MANIFEST_PATH)' '$(INSTALLER_BUILD_DIR)/Contents/Resources/manifest.json'; \
	fi
	@if [ -f '$(INSTALLER_DIR)/assets/icon.svg' ]; then \
		COPYFILE_DISABLE=1 ditto --norsrc '$(INSTALLER_DIR)/assets/icon.svg' '$(INSTALLER_BUILD_DIR)/Contents/Resources/icon.svg'; \
	fi
	@if [ -f '$(INSTALLER_DIR)/assets/AppIcon.icns' ]; then \
		COPYFILE_DISABLE=1 ditto --norsrc '$(INSTALLER_DIR)/assets/AppIcon.icns' '$(INSTALLER_BUILD_DIR)/Contents/Resources/AppIcon.icns'; \
	fi
	@cd '$(ABS_DIST_DIR)' && COPYFILE_DISABLE=1 ditto -c -k --norsrc --keepParent "Kratomix Installer.app" '$(INSTALLER_ASSET)'
	@echo "Created $(INSTALLER_ASSET)"

release-docs:
	@$(call require_release_version)
	@mkdir -p '$(ABS_DIST_DIR)'
	@if [ ! -f '$(INSTALLER_SCREENSHOT_SOURCE)' ]; then \
		echo "Missing installer screenshot: $(INSTALLER_SCREENSHOT_SOURCE)" >&2; \
		exit 2; \
	fi
	@COPYFILE_DISABLE=1 ditto --norsrc '$(INSTALLER_SCREENSHOT_SOURCE)' '$(RELEASE_SCREENSHOT)'
	@$(call compute_release_tag); \
	screenshot_url="https://github.com/kratofl/kratomix/releases/download/$$tag/kratomix-installer-screenshot.png"; \
	printf '%s\n' \
		'# Kratomix Installation' \
		'' \
		'![Kratomix Installer]('"$$screenshot_url"')' \
		'' \
		'1. Download `Kratomix-Installer-$(VERSION)-macos.zip` from this release.' \
		'2. Unzip it. The archive contains `Kratomix Installer.app` directly.' \
		'3. Open `Kratomix Installer.app`.' \
		'4. Keep the default manifest URL unless you need a local/offline manifest.' \
		'5. Select the plugins and formats you want to install.' \
		'6. Choose the install location in Settings: `System-wide` or `User only`.' \
		'7. Click `Install Selected`, then restart Logic Pro.' \
		'' \
		'For now, Kratomix targets Logic Pro first. The installer can place AU and VST3 bundles, but Logic Pro uses AU.' \
		'' \
		'The installer app is unsigned. If macOS blocks the first launch, right-click `Kratomix Installer.app` and choose `Open`.' \
		'' \
		'System-wide installation writes to:' \
		'' \
		'```text' \
		'/Library/Audio/Plug-Ins/Components/' \
		'/Library/Audio/Plug-Ins/VST3/' \
		'```' \
		'' \
		'User-only installation writes to:' \
		'' \
		'```text' \
		'~/Library/Audio/Plug-Ins/Components/' \
		'~/Library/Audio/Plug-Ins/VST3/' \
		'```' \
		> '$(RELEASE_INSTALL_GUIDE)'; \
	printf '%s\n' \
		'# '"$$release_title" \
		'' \
		'![Kratomix Installer]('"$$screenshot_url"')' \
		'' \
		'## Install' \
		'' \
		'1. Download `Kratomix-Installer-$(VERSION)-macos.zip`.' \
		'2. Unzip it and open `Kratomix Installer.app`.' \
		'3. Select the plugins and formats you want.' \
		'4. Click `Install Selected`, then restart Logic Pro.' \
		'' \
		'For detailed steps, download `INSTALL.md` from this release.' \
		'' \
		'The installer is unsigned. If macOS blocks it, right-click the app and choose `Open`.' \
		> '$(RELEASE_NOTES_PATH)'; \
	echo "Created $(RELEASE_INSTALL_GUIDE)"; \
	echo "Created $(RELEASE_NOTES_PATH)"; \
	echo "Created $(RELEASE_SCREENSHOT)"

release:
	@$(call require_release_version)
	@$(call require_release_tools)
	@$(call require_clean_worktree)
	@$(call require_gh_ready)
	@$(call compute_release_tag); \
	$(call require_release_tag_available); \
	$(MAKE_BIN) package PLUGIN='$(PLUGIN)' VERSION='$(VERSION)' RELEASE_TAG="$$tag" BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' DIST_DIR='$(DIST_DIR)' || exit $$?; \
	$(MAKE_BIN) manifest PLUGIN='$(PLUGIN)' VERSION='$(VERSION)' RELEASE_TAG="$$tag" BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' DIST_DIR='$(DIST_DIR)' || exit $$?; \
	$(MAKE_BIN) installer VERSION='$(VERSION)' DIST_DIR='$(DIST_DIR)' || exit $$?; \
	$(MAKE_BIN) release-docs PLUGIN='$(PLUGIN)' VERSION='$(VERSION)' RELEASE_TAG="$$tag" DIST_DIR='$(DIST_DIR)' || exit $$?; \
	assets="'$(MANIFEST_PATH)' '$(INSTALLER_ASSET)' '$(RELEASE_INSTALL_GUIDE)' '$(RELEASE_SCREENSHOT)'"; \
	if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
		for slug in $(PLUGIN_SLUGS); do \
			metadata="$$( $(CMAKE) -DKRATOMIX_ROOT='$(ROOT_DIR)' -DKRATOMIX_PLUGIN_SLUG="$$slug" -DKRATOMIX_BUILD_DIR='$(ABS_BUILD_DIR)' -DKRATOMIX_BUILD_CONFIG='$(CONFIG)' -DKRATOMIX_RELEASE_VERSION='$(VERSION)' -DKRATOMIX_RELEASE_TAG="$$tag" -P '$(METADATA_SCRIPT)' 2>&1 )" || { printf '%s\n' "$$metadata" >&2; exit 2; }; \
			eval "$$metadata"; \
			if [ ! -f "$$asset_zip" ]; then echo "Missing package asset: $$asset_zip" >&2; exit 2; fi; \
			assets="$$assets '$$asset_zip'"; \
		done; \
	else \
		$(call load_plugin_metadata); \
		if [ ! -f "$$asset_zip" ]; then echo "Missing package asset: $$asset_zip" >&2; exit 2; fi; \
		assets="$$assets '$$asset_zip'"; \
	fi; \
	prerelease_flag=""; \
	if [ "$(PRERELEASE)" = "1" ]; then prerelease_flag="--prerelease"; fi; \
	git tag -a "$$tag" -m "$$release_title"; \
	git push origin "$$tag"; \
	eval "gh release create \"$$tag\" $$assets --title \"$$release_title\" --notes-file '$(RELEASE_NOTES_PATH)' $$prerelease_flag"
