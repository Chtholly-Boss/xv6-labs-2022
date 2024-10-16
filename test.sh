#!/bin/bash

branches=(util syscall pgtbl traps cow thread net lock fs mmap)

for branch in "${branches[@]}"; do
    (
        git checkout "$branch" > /dev/null && \
        make grade | grep -E "== |Score" \
        || echo "Failed to grade on branch $branch"
    )
done
git checkout main > /dev/null
