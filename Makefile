# Makefileが存在するサブディレクトリを自動で検索してリスト化
SUBDIRS := $(wildcard */Makefile)
PROJECTS := $(patsubst %/Makefile,%,$(SUBDIRS))

make:
	@echo "Making all sub projects found..."
	@for dir in $(PROJECTS); do \
		echo "==> Making in $$dir"; \
		$(MAKE) -C $$dir; \
	done
	@echo "All sub projects made."

clean:
	@echo "Cleaning all sub projects found..."
	@for dir in $(PROJECTS); do \
		echo "==> Cleaning in $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
	@echo "All sub projects cleaned."

format:
	@echo "Formatting all .c and .h files..."
	@find . -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Formatting complete."

# install asmfmt by 'cargo install asmfmt'
asmformat:
	@echo "Formatting all .s files..."
	@find . -name "*.s" | while read f; do \
	tmpfile="$${f}.tmp"; \
	asmfmt "$$f" \
	  | sed '1,/^[a-zA-Z_]/ { s/^[[:space:]]*;\(.*\)$$/;\1/; s/^[[:space:]]*\(\[.*\]\)$$/\1/; s/^[[:space:]]*%/%/; }' \
	  > "$$tmpfile" && mv "$$tmpfile" "$$f"; \
	done
	@echo "Formatting complete."

# @find . -name "*.s" | xargs asmfmt -w

# --- Markdown formatting (prettier + markdownlint + textlint) ---
# requires Node tooling: npx が各ツールを初回実行時に取得する。
# pnpm があれば優先して使う（../workshop/Makefile と同じ方針）。
PRETTIER_IGNORED := node_modules .git
RUNNER := $(shell command -v pnpm >/dev/null 2>&1 && echo "pnpm dlx" || echo "npx")
EXEC   := $(shell command -v pnpm >/dev/null 2>&1 && echo "pnpm exec" || echo "npx")
MD_IGNORE_PATHS := $(foreach dir,$(PRETTIER_IGNORED),! -path "./$(dir)/*")

# format は .c/.h 用のため、Markdown は mdformat で分離
mdformat:
	@echo "Formatting markdown tables with prettier..."
	@find . -name "*.md" ! -name "CLAUDE.md" $(MD_IGNORE_PATHS) \
	  -exec $(EXEC) prettier --write --parser markdown {} + 2>&1 \
	  | grep -v "^$$" | grep -v "(unchanged)" || true
	@echo "Linting markdown files..."
	$(RUNNER) markdownlint-cli "**/*.md" $(foreach dir,$(PRETTIER_IGNORED),--ignore "$(dir)/**") --ignore "CLAUDE.md" --fix
	$(EXEC) textlint --fix "**/*.md"
	@echo "Markdown formatting complete."

# cleanというファイルが万が一あってもコマンドが実行されるようにするおまじない
.PHONY: clean make format asmformat mdformat
