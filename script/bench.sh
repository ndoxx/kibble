#!/bin/sh

# Use this script to execute benchmarks instead of running them directly
# This disables CPU scaling, which improves the benchmarks precision
# Just pass the benchmark name (without the bench_ prefix) as argument

if [ "$#" -ne 1 ] || ! [ -n "$1" ]; then
    echo "Usage:"
    echo "bench.sh <benchmark_name>"
    exit 1
fi

cmd=../bin/test/bench_$1

if test -f "$cmd"; then
    # Disable CPU scaling for more precise benchmarking
    sudo cpupower frequency-set --governor performance
    $cmd
    sudo cpupower frequency-set --governor powersave
else
    echo "file bench_$1 does not exist"
fi
