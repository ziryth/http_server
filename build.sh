#!/bin/bash

CC=clang
FLAGS="-lm"

buildpat="*${1}*_main.c"
[[ -z "$1" ]] && buildpat="*_main.c"

build_single() {
    local source_file="$1"
    local base_name=$(basename "${source_file%.c}")
    
    [[ -d build ]] || mkdir build
    cd build
    
    local all_sources=$(find ../src -name '*.c' ! -name '*_main.c' 2>/dev/null | tr '\n' ' ')
    
    echo "Building with $CC: $source_file"
    $CC -g $all_sources "../$source_file" $FLAGS -o"${base_name}_debug"
    $CC -g -Wall -Ofast -march=native -mfma -mavx2 -ffast-math -fuse-ld=lld $all_sources "../$source_file" $FLAGS -o "${base_name}_release"
    
    cd ..
}

while IFS= read -r -d '' file; do
    build_single "$file"
done < <(find src -name "$buildpat" -type f -print0)
