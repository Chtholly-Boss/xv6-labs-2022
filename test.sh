#!/bin/bash

branches=(util syscall pgtbl traps cow thread net lock fs mmap)
total_branches=${#branches[@]} # 总分支数

for index in $(seq 1 $total_branches); do
    branch=${branches[$((index - 1))]}
    echo "Starting Test ($index/$total_branches)"
    (
        git checkout "$branch" > /dev/null && \
        make clean > /dev/null && \
        make grade | grep -E "== |Score" && \
        make clean > /dev/null
    )
done
git checkout main > /dev/null
