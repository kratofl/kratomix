.PHONY: help list configure build test run validate package release clean

help:
	@printf '%s\n' \
		'Kratomix commands' \
		'' \
		'  make help' \
		'  make list' \
		'  make configure [BUILD_DIR=build] [CONFIG=Release] [JOBS=4]' \
		'  make build [PLUGIN=<slug>|all] [BUILD_DIR=build] [CONFIG=Release] [JOBS=4]' \
		'  make test [PLUGIN=<slug>|all] [BUILD_DIR=build] [CONFIG=Release] [JOBS=4]' \
		'  make run PLUGIN=<slug> [BUILD_DIR=build] [CONFIG=Release] [JOBS=4]' \
		'  make validate PLUGIN=<slug> [BUILD_DIR=build] [CONFIG=Release] [JOBS=4]' \
		'  make package PLUGIN=<slug> [BUILD_DIR=build] [CONFIG=Release] [DIST_DIR=dist]' \
		'  make release PLUGIN=<slug> [BUILD_DIR=build] [CONFIG=Release] [DIST_DIR=dist]' \
		'  make clean [BUILD_DIR=build]' \
		'  make new-plugin SLUG=<slug> NAME="Kratomix ..." CODE=<FourCC> [BUNDLE=com.kratomix.<slug>]' \
		'' \
		"Available plugins: $(PLUGIN_SLUGS)"

list:
	@for slug in $(PLUGIN_SLUGS); do printf '%s\n' "$$slug"; done

configure:
	@$(CMAKE) -S '$(ROOT_DIR)' -B '$(ABS_BUILD_DIR)' -G '$(CMAKE_GENERATOR)' -DCMAKE_BUILD_TYPE='$(CONFIG)' $(if $(strip $(JUCE_DIR)),-DJUCE_DIR='$(JUCE_DIR)',)

build:
	@$(MAKE) configure BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
		$(CMAKE) --build '$(ABS_BUILD_DIR)' --target plugins-all --parallel '$(JOBS)'; \
	else \
		$(call require_plugin); \
		$(CMAKE) --build '$(ABS_BUILD_DIR)' --target "plugin-$(PLUGIN)" --parallel '$(JOBS)'; \
	fi

test:
	@$(MAKE) configure BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
		$(CMAKE) --build '$(ABS_BUILD_DIR)' --target tests-all --parallel '$(JOBS)'; \
		ctest --test-dir '$(ABS_BUILD_DIR)' --output-on-failure; \
	else \
		$(call require_plugin); \
		$(CMAKE) --build '$(ABS_BUILD_DIR)' --target "test-$(PLUGIN)" --parallel '$(JOBS)'; \
		ctest --test-dir '$(ABS_BUILD_DIR)' --output-on-failure -R '^test-$(PLUGIN)$$'; \
	fi

run:
	@$(call require_plugin)
	@$(MAKE) configure BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@$(call load_plugin_metadata); \
	if [ "$$has_standalone" != "1" ]; then \
		echo "Plugin '$(PLUGIN)' does not define a Standalone target." >&2; \
		exit 2; \
	fi; \
	$(CMAKE) --build '$(ABS_BUILD_DIR)' --target "$$standalone_target" --parallel '$(JOBS)'; \
	open "$$standalone_app"

validate:
	@$(call require_plugin)
	@$(MAKE) configure BUILD_DIR='$(BUILD_DIR)' CONFIG='$(CONFIG)' CMAKE_GENERATOR='$(CMAKE_GENERATOR)' JOBS='$(JOBS)' JUCE_DIR='$(JUCE_DIR)'
	@$(call load_plugin_metadata); \
	if [ "$$has_au" != "1" ]; then \
		echo "Plugin '$(PLUGIN)' does not define an AU target." >&2; \
		exit 2; \
	fi; \
	$(CMAKE) --build '$(ABS_BUILD_DIR)' --target "$$au_target" --parallel '$(JOBS)'; \
	auval -v "$$au_main_type" "$$plugin_code" "$$manufacturer_code"

clean:
	@rm -rf '$(ABS_BUILD_DIR)'
