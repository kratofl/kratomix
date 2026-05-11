.PHONY: package release

DIST_DIR ?= dist
ABS_DIST_DIR = $(if $(filter /%,$(DIST_DIR)),$(DIST_DIR),$(ROOT_DIR)/$(DIST_DIR))

define require_ditto
if ! command -v ditto >/dev/null 2>&1; then \
	echo "Target '$@' requires macOS ditto." >&2; \
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

release:
	@$(call require_plugin)
	@$(MAKE) build PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@$(call load_plugin_metadata); \
	if [ "$$has_au" = "1" ]; then \
		$(MAKE) validate PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'; \
	fi
	@$(MAKE) package PLUGIN='$(PLUGIN)' BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)' DIST_DIR='$(DIST_DIR)'
