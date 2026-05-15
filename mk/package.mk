.PHONY: package installer release

DIST_DIR ?= dist
ABS_DIST_DIR = $(if $(filter /%,$(DIST_DIR)),$(DIST_DIR),$(ROOT_DIR)/$(DIST_DIR))

define require_ditto
if ! command -v ditto >/dev/null 2>&1; then \
	echo "Target '$@' requires macOS ditto." >&2; \
	exit 2; \
fi
endef

define require_pkgbuild
if ! command -v pkgbuild >/dev/null 2>&1; then \
	echo "Target '$@' requires macOS pkgbuild." >&2; \
	exit 2; \
fi
endef

package:
	@$(call require_plugin)
	@$(call require_ditto)
	@$(MAKE_BIN) build PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@$(call load_plugin_metadata); \
	stage_dir='$(ABS_DIST_DIR)/$(PLUGIN)'; \
	zip_path='$(ABS_DIST_DIR)/$(PLUGIN)-$(CONFIG).zip'; \
	rm -rf "$$stage_dir" "$$zip_path"; \
	mkdir -p "$$stage_dir"; \
	if [ "$$has_au" = "1" ]; then \
		au_path='$(ABS_BUILD_DIR)/'"$$cmake_target"'_artefacts/$(CONFIG)/AU/'"$$product_name"'.component'; \
		if [ ! -d "$$au_path" ]; then echo "Missing AU artifact: $$au_path" >&2; exit 2; fi; \
		mkdir -p "$$stage_dir/AU"; \
		COPYFILE_DISABLE=1 ditto --norsrc "$$au_path" "$$stage_dir/AU/$${product_name}.component"; \
	fi; \
	if [ "$$has_vst3" = "1" ]; then \
		vst3_path='$(ABS_BUILD_DIR)/'"$$cmake_target"'_artefacts/$(CONFIG)/VST3/'"$$product_name"'.vst3'; \
		if [ ! -d "$$vst3_path" ]; then echo "Missing VST3 artifact: $$vst3_path" >&2; exit 2; fi; \
		mkdir -p "$$stage_dir/VST3"; \
		COPYFILE_DISABLE=1 ditto --norsrc "$$vst3_path" "$$stage_dir/VST3/$${product_name}.vst3"; \
	fi; \
	if [ "$$has_standalone" = "1" ]; then \
		standalone_path='$(ABS_BUILD_DIR)/'"$$cmake_target"'_artefacts/$(CONFIG)/Standalone/'"$$product_name"'.app'; \
		if [ ! -d "$$standalone_path" ]; then echo "Missing Standalone artifact: $$standalone_path" >&2; exit 2; fi; \
		mkdir -p "$$stage_dir/Standalone"; \
		COPYFILE_DISABLE=1 ditto --norsrc "$$standalone_path" "$$stage_dir/Standalone/$${product_name}.app"; \
	fi; \
	if find "$$stage_dir" -iname '*Sidechain*' -print -quit | grep -q .; then \
		echo "Refusing to package stale sidechain artifact for $(PLUGIN)." >&2; \
		find "$$stage_dir" -iname '*Sidechain*' -print >&2; \
		exit 2; \
	fi; \
	COPYFILE_DISABLE=1 ditto -c -k --norsrc --keepParent "$$stage_dir" "$$zip_path"; \
	echo "Created $$zip_path"

installer:
	@$(call require_release_version)
	@$(call require_ditto)
	@$(call require_pkgbuild)
	@if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
		for slug in $(PLUGIN_SLUGS); do \
			$(MAKE_BIN) installer PLUGIN="$$slug" VERSION='$(VERSION)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' DIST_DIR='$(DIST_DIR)' || exit $$?; \
		done; \
	else \
		$(call require_plugin); \
		$(MAKE_BIN) build PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'; \
		$(call load_plugin_metadata); \
		if [ "$$has_au" = "1" ]; then \
			$(MAKE_BIN) validate PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'; \
		fi; \
		pkgroot='$(ABS_DIST_DIR)/pkgroot-$(PLUGIN)-$(VERSION)'; \
		rm -rf "$$pkgroot"; \
		mkdir -p "$$pkgroot/Library/Audio/Plug-Ins"; \
		if [ "$$has_au" = "1" ]; then \
			if [ ! -d "$$au_artifact" ]; then echo "Missing AU artifact: $$au_artifact" >&2; exit 2; fi; \
			mkdir -p "$$pkgroot/Library/Audio/Plug-Ins/Components"; \
			COPYFILE_DISABLE=1 ditto --norsrc "$$au_artifact" "$$pkgroot/Library/Audio/Plug-Ins/Components/$${product_name}.component"; \
		fi; \
		if [ "$$has_vst3" = "1" ]; then \
			if [ ! -d "$$vst3_artifact" ]; then echo "Missing VST3 artifact: $$vst3_artifact" >&2; exit 2; fi; \
			mkdir -p "$$pkgroot/Library/Audio/Plug-Ins/VST3"; \
			COPYFILE_DISABLE=1 ditto --norsrc "$$vst3_artifact" "$$pkgroot/Library/Audio/Plug-Ins/VST3/$${product_name}.vst3"; \
		fi; \
		if [ -e "$$installer_pkg" ]; then echo "Refusing to overwrite existing package: $$installer_pkg" >&2; exit 2; fi; \
		mkdir -p '$(ABS_DIST_DIR)'; \
		pkgbuild --root "$$pkgroot" --identifier "$$package_id" --version '$(VERSION)' --install-location / "$$installer_pkg"; \
		rm -rf "$$pkgroot"; \
		echo "Created $$installer_pkg"; \
	fi

release:
	@$(call require_release_version)
	@$(call require_release_tools)
	@$(call require_clean_worktree)
	@$(call require_gh_ready)
	@$(call compute_release_tag); \
	$(call require_release_tag_available); \
	$(MAKE_BIN) installer PLUGIN='$(PLUGIN)' VERSION='$(VERSION)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' DIST_DIR='$(DIST_DIR)'; \
	assets=""; \
	if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
		for slug in $(PLUGIN_SLUGS); do \
			metadata="$$( $(CMAKE) -DKRATOMIX_ROOT='$(ROOT_DIR)' -DKRATOMIX_PLUGIN_SLUG="$$slug" -DKRATOMIX_BUILD_DIR='$(ABS_BUILD_DIR)' -DKRATOMIX_BUILD_CONFIG='$(CONFIG)' -DKRATOMIX_RELEASE_VERSION='$(VERSION)' -P '$(METADATA_SCRIPT)' 2>&1 )" || { printf '%s\n' "$$metadata" >&2; exit 2; }; \
			eval "$$metadata"; \
			if [ ! -f "$$installer_pkg" ]; then echo "Missing package asset: $$installer_pkg" >&2; exit 2; fi; \
			assets="$$assets $$installer_pkg"; \
		done; \
	else \
		$(call load_plugin_metadata); \
		if [ ! -f "$$installer_pkg" ]; then echo "Missing package asset: $$installer_pkg" >&2; exit 2; fi; \
		assets="$$installer_pkg"; \
	fi; \
	prerelease_flag=""; \
	if [ "$(PRERELEASE)" = "1" ]; then prerelease_flag="--prerelease"; fi; \
	git tag -a "$$tag" -m "$$release_title"; \
	git push origin "$$tag"; \
	gh release create "$$tag" $$assets --title "$$release_title" --notes "$$release_notes" $$prerelease_flag
