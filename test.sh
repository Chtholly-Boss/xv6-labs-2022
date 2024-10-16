#!/bin/bash

branches=(util syscall pgtbl traps cow thread net lock fs mmap)

for branch in "${branches[@]}"; do
    (
        git checkout "$branch" > /dev/null && \
        make clean > /dev/null && \
        make grade | grep -E "== |Score" && \
        make clean > /dev/null
    )
done
git checkout main > /dev/null
