#!/bin/sh
# Use this script to execute benchmarks instead of running them directly
# This disables CPU scaling, which improves the benchmarks precision
# Pass the benchmark name (without the bench_ prefix) as the first argument
# Optionally pass the build type (Debug or Release) as the second argument

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ] || ! [ -n "$1" ]; then
    echo "Usage:"
    echo "bench.sh <benchmark_name> [build_type]"
    echo "build_type can be Debug (default) or Release"
    exit 1
fi

benchmark_name=$1
build_type=${2:-Debug}  # Default to Debug if not specified

cmd="bin/${build_type}/test/bench_${benchmark_name}"

if test -f "$cmd"; then
    # Disable CPU scaling for more precise benchmarking
    sudo cpupower frequency-set --governor performance
    $cmd
    sudo cpupower frequency-set --governor powersave
else
    echo "File $cmd does not exist"
    echo "Make sure you've built the benchmark for the ${build_type} configuration"
fi