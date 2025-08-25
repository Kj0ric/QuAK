#!/usr/bin/env zsh

AUTOMATA_DIR="samples/generated_nested"
EXECUTABLE="./src/quak-main"
OUTPUT_DIR=$AUTOMATA_DIR

for file in "$AUTOMATA_DIR"/*.txt; do
    base=$(basename "$file" .txt)
    out_file="$OUTPUT_DIR/${base}.out"
    echo "===================================="
    echo "Running on: $file"
    "$EXECUTABLE" "$file" > "$out_file" 2>&1
    echo "Output written to: $out_file"
done

