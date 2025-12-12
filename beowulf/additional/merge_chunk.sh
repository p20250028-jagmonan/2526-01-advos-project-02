#!/bin/bash
# merge_chunks.sh
#
# Merge all C_chunk_*.txt files into C_full.txt in the correct order.
# Assumes files are under /cluster/results/ and named:
#   C_chunk_0.txt, C_chunk_1.txt, ...
#
# Usage:
#   merge_chunks.sh
#
# Result:
#   /cluster/results/C_full.txt

RESULT_DIR="/cluster/results"
OUT_FILE="$RESULT_DIR/C_full.txt"

cd "$RESULT_DIR" || { echo "Cannot cd to $RESULT_DIR"; exit 1; }

# Use natural sort so that 10 comes after 9, not after 1
CHUNK_FILES=$(ls C_chunk_*.txt 2>/dev/null | sort -V)

if [ -z "$CHUNK_FILES" ]; then
    echo "No C_chunk_*.txt files found in $RESULT_DIR"
    exit 1
fi

echo "Merging the following chunk files (in order):"
echo "$CHUNK_FILES"
echo

cat $CHUNK_FILES > "$OUT_FILE"

echo "Merged into $OUT_FILE"
