#!/bin/bash

branches=(util syscall)

for branch in "${branches[@]}"; do
    (
        git checkout "$branch" > /dev/null && \
        make grade | grep -E "== |Score" \
        || echo "Failed to grade on branch $branch"
    )
done
git checkout main > /dev/null
