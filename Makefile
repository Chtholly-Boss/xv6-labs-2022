# 定义分支列表
BRANCHES := util syscall pgtbl traps cow thread net lock fs mmap

# 计算分支总数
TOTAL_BRANCHES := 9

# 默认目标
.PHONY: all
all:
	./test.sh

# 定义一个规则来测试单个分支
.PHONY: test
test:
	@if [ -n "$(br)" ]; then \
		echo "Starting Test for branch $(br)"; \
		git checkout "$(br)" > /dev/null && \
		make clean > /dev/null && \
		make grade | grep -E "== |Score" && \
		make clean > /dev/null; \
	else \
		echo "Error: Branch name not provided"; \
		exit 1; \
	fi
	@git checkout main > /dev/null

# 定义一个规则来清理所有分支
.PHONY: clean
clean:
	@for branch in $(BRANCHES); do \
		git checkout "$$branch" > /dev/null && \
		make clean > /dev/null; \
	done
	@git checkout main > /dev/null
