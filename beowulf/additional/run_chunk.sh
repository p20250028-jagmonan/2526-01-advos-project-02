#!/bin/bash
# run_chunks.sh
#
# Controller that:
#   - splits work into CHUNKS
#   - for each chunk:
#       * detects alive nodes (heartbeats + ssh)
#       * builds a hostfile
#       * sets np = total slots
#       * runs mpirun for that chunk
#       * if mpirun fails, retries the SAME chunk on remaining alive nodes
#
# Usage:
#   run_chunks.sh <num_chunks> <mpi_program> [program_args...]
#
# Example:
#   run_chunks.sh 4 /cluster/matmul_mpi 200

ALIVE_SCRIPT="/cluster/cluster_alive_nodes.sh"
MAX_RETRIES=3
MPIRUN_TIMEOUT=30

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <num_chunks> <mpi_program> [program_args...]"
    exit 1
fi

NUM_CHUNKS="$1"
shift
MPI_PROG="$1"
shift
PROG_ARGS=("$@")

if [ "$NUM_CHUNKS" -le 0 ]; then
    echo "Error: num_chunks must be > 0"
    exit 1
fi

for CHUNK_ID in $(seq 0 $((NUM_CHUNKS - 1))); do
    echo "=================================================="
    echo "=== Chunk $CHUNK_ID / $((NUM_CHUNKS - 1)) ==="
    echo "=================================================="

    RETRY=0
    while [ $RETRY -lt $MAX_RETRIES ]; do
        echo "--- Attempt $((RETRY+1)) for chunk $CHUNK_ID ---"

        # 1. Detect alive nodes
        ALIVE=$($ALIVE_SCRIPT)
	echo $ALIVE
        if [ -z "$ALIVE" ]; then
            echo "No alive nodes available. Aborting."
            exit 1
        fi

        echo "Alive nodes this attempt:"
        echo "$ALIVE"
        echo

        # 2. Build hostfile & count total slots
        HOSTFILE=$(mktemp /tmp/mpihosts.XXXXXX)
        TOTAL_SLOTS=0
        while read -r host slots; do
            [ -z "$host" ] && continue
            echo "$host slots=$slots" >> "$HOSTFILE"
            TOTAL_SLOTS=$((TOTAL_SLOTS + slots))
        done <<< "$ALIVE"

        echo "Total slots this attempt: $TOTAL_SLOTS"
        echo "Hostfile:"
        cat "$HOSTFILE"
        echo

        if [ "$TOTAL_SLOTS" -le 0 ]; then
            echo "No slots available. Aborting."
            rm -f "$HOSTFILE"
            exit 1
        fi

        # 3. Run mpirun for THIS chunk-id
        echo "Running mpirun for chunk $CHUNK_ID (attempt $((RETRY+1)))..."
        echo "Command: mpirun -np $TOTAL_SLOTS --hostfile $HOSTFILE \\"
        echo "         $MPI_PROG ${PROG_ARGS[*]} --chunk-id $CHUNK_ID --num-chunks $NUM_CHUNKS"
        echo

        timeout "$MPIRUN_TIMEOUT" mpirun -np "$TOTAL_SLOTS" --hostfile "$HOSTFILE" \
            "$MPI_PROG" "${PROG_ARGS[@]}" \
            --chunk-id "$CHUNK_ID" --num-chunks "$NUM_CHUNKS"

        STATUS=$?
        rm -f "$HOSTFILE"

        if [ $STATUS -eq 0 ]; then
            echo "Chunk $CHUNK_ID completed successfully on attempt $((RETRY+1))."
            break
        else
            echo "Chunk $CHUNK_ID FAILED (status=$STATUS) on attempt $((RETRY+1))."
            echo "Will recompute alive nodes and retry (if retries left)."
            RETRY=$((RETRY + 1))
            echo
        fi
    done

    if [ $RETRY -ge $MAX_RETRIES ]; then
        echo "Chunk $CHUNK_ID failed after $MAX_RETRIES attempts. Aborting."
        exit 1
    fi

    echo
done

echo "All chunks completed successfully."

