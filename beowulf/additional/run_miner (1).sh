#!/bin/bash

# =============================================================================
# RESILIENT MPI CRYPTO MINER LAUNCHER
# =============================================================================

# --- CONFIGURATION ---
TOTAL_ITERS=10000000          # Total Hashes to mine (10 Million)
CHUNK_ITERS=2000000           # Hashes per chunk (2 Million)
EXEC=./crypto_miner           # The compiled C executable

# List your nodes here (ensure these are in /etc/hosts)
NODES=("master" "worker1" "worker2") 
SLOTS_PER_NODE=4              # CPU Cores per node

# TIMEOUT SETTINGS
CHUNK_TIMEOUT=25              # Seconds before soft kill (SIGTERM)
KILL_TIMEOUT=5s               # Seconds after soft kill to FORCE KILL (SIGKILL)

# GLOBAL ACCUMULATORS
final_gold_count=0
final_total_hashes=0
completed=0
chunk_id=1

echo "=== Resilient MPI Mining Farm ==="
echo "Target: $TOTAL_ITERS Hashes"
echo "Nodes:  ${NODES[*]}"
echo

while [ $completed -lt $TOTAL_ITERS ]; do
    echo "----------------------------------------"
    echo "Chunk $chunk_id: Progress $completed / $TOTAL_ITERS"

    remaining=$((TOTAL_ITERS - completed))
    this_chunk_iters=$(( remaining < CHUNK_ITERS ? remaining : CHUNK_ITERS ))

    # -------------------------------------------------------------------------
    # STEP 1: PROBE NODES & CLEAN ZOMBIES
    # -------------------------------------------------------------------------
    HOSTFILE=hosts.dynamic
    : > "$HOSTFILE"
    total_procs=0

    echo "Probing nodes and cleaning zombies..."

    for node in "${NODES[@]}"; do
        # 1. Ping Check (Is the wire connected?)
        if ping -c1 -W0.5 "$node" &>/dev/null; then
            
            # 2. Zombie Cleanup (Kill old processes from previous crashes)
            # using ssh with a strict 2s timeout
            # Note: We kill 'crypto_miner' now
            timeout 2s ssh "$node" "killall -q -9 crypto_miner" &>/dev/null

            echo "$node slots=$SLOTS_PER_NODE" >> "$HOSTFILE"
            total_procs=$((total_procs + SLOTS_PER_NODE))
        else
            echo "  ! Node $node is DOWN (Skipping)"
        fi
    done

    if [ $total_procs -eq 0 ]; then
        echo "CRITICAL: No nodes alive. Sleeping 5s..."
        sleep 5
        continue
    fi

    # -------------------------------------------------------------------------
    # STEP 2: RUN MPIRUN (WITH LOGGING)
    # -------------------------------------------------------------------------
    LOGFILE="chunk_${chunk_id}.log"
    
    # Run with timeout to prevent hangs
    timeout -k "$KILL_TIMEOUT" "$CHUNK_TIMEOUT" \
        mpirun \
        --mca orte_base_help_aggregate 0 \
        -np "$total_procs" \
        --hostfile "$HOSTFILE" \
        "$EXEC" "$this_chunk_iters" > "$LOGFILE" 2>&1

    rc=$?

    # -------------------------------------------------------------------------
    # STEP 3: HANDLE FAILURES
    # -------------------------------------------------------------------------
    if [ $rc -ne 0 ]; then
        if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
             echo "!!! Chunk TIMED OUT (Node execution hung). Retrying..."
        else
             echo "!!! Chunk FAILED (MPI Error code $rc). Retrying..."
        fi
        # Wait a bit before retrying to let sockets close
        sleep 2
        continue 
    fi

    # -------------------------------------------------------------------------
    # STEP 4: PARSE RESULTS & DISPLAY LOAD STATS
    # -------------------------------------------------------------------------
    chunk_line=$(grep -m1 '^CHUNK_RESULT' "$LOGFILE")

    if [ -n "$chunk_line" ]; then
        # 4a. PRINT LOAD DISTRIBUTION
        echo "  > Mining Breakdown:"
        # Grep the lines starting with [Node:, indent them, and print
        grep "\[Node:" "$LOGFILE" | sed 's/^/    /'
        echo ""

        # 4b. Parse Numbers
        # Expecting: CHUNK_RESULT: iters=X found=Y
        c_iter=$(echo "$chunk_line" | awk -F'=' '{print $2}' | awk '{print $1}')
        c_found=$(echo "$chunk_line" | awk -F'=' '{print $3}' | awk '{print $1}')
        
        echo "  > Chunk Success: Gold Found=$c_found / Hashes=$c_iter"
        
        # Accumulate
        final_gold_count=$((final_gold_count + c_found))
        final_total_hashes=$((final_total_hashes + c_iter))
        
        # Move to next chunk
        completed=$((completed + this_chunk_iters))
        chunk_id=$((chunk_id + 1))
    else
        echo "!!! Output format error (Log file empty?). Retrying..."
        sleep 1
    fi

done

# -------------------------------------------------------------------------
# STEP 5: FINAL CALCULATION
# -------------------------------------------------------------------------
echo "==============================================="
echo " MINING OPERATION FINISHED"
echo " Total Hashes:     $final_total_hashes"
echo " Gold Nuggets:     $final_gold_count"

# Calculate Yield Rate (Percentage of rare blocks found)
yield_rate=$(awk "BEGIN {printf \"%.6f\", ($final_gold_count / $final_total_hashes) * 100}")
echo " YIELD RATE:       $yield_rate %"
echo "==============================================="
