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

# cleanというファイルが万が一あってもコマンドが実行されるようにするおまじない
.PHONY: clean make format asmformat
