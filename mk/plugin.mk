PLUGIN_FILES := $(sort $(wildcard $(ROOT_DIR)/plugins/*/plugin.cmake))
PLUGIN_SLUGS := $(sort $(notdir $(patsubst %/,%,$(dir $(PLUGIN_FILES)))))
ABS_BUILD_DIR = $(if $(filter /%,$(BUILD_DIR)),$(BUILD_DIR),$(ROOT_DIR)/$(BUILD_DIR))
METADATA_SCRIPT := $(ROOT_DIR)/core/cmake/print_plugin_metadata.cmake

define require_plugin
if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
	echo "Target '$@' requires PLUGIN=<slug>." >&2; \
	exit 2; \
fi; \
found=0; \
for slug in $(PLUGIN_SLUGS); do \
	if [ "$$slug" = "$(PLUGIN)" ]; then \
		found=1; \
		break; \
	fi; \
done; \
if [ "$$found" -ne 1 ]; then \
	echo "Unknown PLUGIN='$(PLUGIN)'. Known plugins: $(PLUGIN_SLUGS)" >&2; \
	exit 2; \
fi
endef

define load_plugin_metadata
metadata="$$( $(CMAKE) -DKRATOMIX_ROOT='$(ROOT_DIR)' -DKRATOMIX_PLUGIN_SLUG='$(PLUGIN)' -DKRATOMIX_BUILD_DIR='$(ABS_BUILD_DIR)' -DKRATOMIX_BUILD_CONFIG='$(CONFIG)' -P '$(METADATA_SCRIPT)' 2>&1 )" || { \
	printf '%s\n' "$$metadata" >&2; \
	exit 2; \
}; \
eval "$$metadata"
endef
