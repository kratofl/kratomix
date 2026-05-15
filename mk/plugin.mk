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
metadata="$$( $(CMAKE) -DKRATOMIX_ROOT='$(ROOT_DIR)' -DKRATOMIX_PLUGIN_SLUG='$(PLUGIN)' -DKRATOMIX_BUILD_DIR='$(ABS_BUILD_DIR)' -DKRATOMIX_BUILD_CONFIG='$(CONFIG)' -DKRATOMIX_RELEASE_VERSION='$(VERSION)' -P '$(METADATA_SCRIPT)' 2>&1 )" || { \
	printf '%s\n' "$$metadata" >&2; \
	exit 2; \
}; \
eval "$$metadata"
endef

define require_release_version
if [ -z "$(VERSION)" ]; then \
	echo "Target '$@' requires VERSION=x.x.x." >&2; \
	exit 2; \
fi; \
case "$(VERSION)" in \
	*.*.*) ;; \
	*) echo "Invalid VERSION='$(VERSION)'. Expected x.x.x." >&2; exit 2 ;; \
esac; \
if ! printf '%s\n' "$(VERSION)" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
	echo "Invalid VERSION='$(VERSION)'. Expected x.x.x." >&2; \
	exit 2; \
fi
endef

define require_release_tools
for tool in git gh pkgbuild ditto; do \
	if ! command -v "$$tool" >/dev/null 2>&1; then \
		echo "Target '$@' requires $$tool." >&2; \
		exit 2; \
	fi; \
done
endef

define require_clean_worktree
if [ -n "$$(git status --porcelain)" ]; then \
	echo "Target '$@' requires a clean git working tree." >&2; \
	git status --short >&2; \
	exit 2; \
fi
endef

define require_gh_ready
if ! gh auth status >/dev/null 2>&1; then \
	echo "Target '$@' requires GitHub CLI authentication. Run: gh auth login" >&2; \
	exit 2; \
fi
endef

define compute_release_tag
if [ -z "$(PLUGIN)" ] || [ "$(PLUGIN)" = "all" ]; then \
	tag="v$(VERSION)"; \
	release_title="Kratomix $(VERSION)"; \
	release_notes="Kratomix $(VERSION) release."; \
else \
	$(call require_plugin); \
	$(call load_plugin_metadata); \
	tag="$(PLUGIN)-v$(VERSION)"; \
	release_title="$${product_name} $(VERSION)"; \
	release_notes="$${product_name} $(VERSION) release."; \
fi
endef

define require_release_tag_available
if git rev-parse -q --verify "refs/tags/$$tag" >/dev/null; then \
	echo "Release tag already exists locally: $$tag" >&2; \
	exit 2; \
fi; \
if git ls-remote --exit-code --tags origin "refs/tags/$$tag" >/dev/null 2>&1; then \
	echo "Release tag already exists on origin: $$tag" >&2; \
	exit 2; \
fi; \
if gh release view "$$tag" >/dev/null 2>&1; then \
	echo "GitHub release already exists: $$tag" >&2; \
	exit 2; \
fi
endef
