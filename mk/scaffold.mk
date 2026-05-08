.PHONY: new-plugin

new-plugin:
	@if [ -z "$(SLUG)" ]; then \
		echo "Missing required variable SLUG=<slug>." >&2; \
		exit 2; \
	fi
	@if [ -z "$(NAME)" ]; then \
		echo "Missing required variable NAME=\"Kratomix ...\"." >&2; \
		exit 2; \
	fi
	@if [ -z "$(CODE)" ]; then \
		echo "Missing required variable CODE=<FourCC>." >&2; \
		exit 2; \
	fi
	@if [ "$$(printf '%s' '$(CODE)' | wc -c | tr -d ' ')" -ne 4 ]; then \
		echo "CODE must be exactly 4 ASCII characters." >&2; \
		exit 2; \
	fi
	@bundle='$(BUNDLE)'; \
	if [ -z "$$bundle" ]; then \
		bundle='com.kratomix.$(SLUG)'; \
	fi; \
	$(PYTHON) '$(ROOT_DIR)/tools/new-plugin.py' --root '$(ROOT_DIR)' --slug '$(SLUG)' --name '$(NAME)' --code '$(CODE)' --bundle "$$bundle"
