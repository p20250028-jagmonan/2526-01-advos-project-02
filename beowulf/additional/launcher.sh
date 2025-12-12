#!/bin/bash

# --- CONFIGURATION ---
EXEC="./mining_demo"       # Program to run
TOTAL_WORKLOAD=10000000   # Total items
CHUNK_SIZE=1000000          # SMALL chunk size (Fast updates)
SLOTS_PER_NODE=2           # CPUs per node
NODES=("master" "worker1" "worker2")
HOSTFILE_DYN="hosts.dynamic"
LOG_FILE="cluster_job.log"

# MCA Flags
MCA_FLAGS="--mca orte_base_help_aggregate 0 --mca oob_tcp_listen_mode listen_thread"

# Timeouts (Tuned for Fast Demo)
PING_TIMEOUT=0.5
SSH_TIMEOUT=2
MPI_SOFT_TIMEOUT=10s  # Fast detection (10 seconds)
MPI_HARD_TIMEOUT=5s

# --- INITIALIZATION ---
> $LOG_FILE
completed=0
final_result=0

echo "=================================================="
echo "   STARTING FAULT-TOLERANT JOB: $TOTAL_WORKLOAD items"
echo "=================================================="

# --- MAIN LOOP ---
while [ $completed -lt $TOTAL_WORKLOAD ]; do

    # 1. CALCULATE CHUNK
    remaining=$((TOTAL_WORKLOAD - completed))
    if [ $remaining -lt $CHUNK_SIZE ]; then
        this_chunk=$remaining
    else
        this_chunk=$CHUNK_SIZE
    fi
    
    # 2. DYNAMIC HOST DETECTION
    rm -f $HOSTFILE_DYN
    alive_count=0
    active_nodes=""
    
    for node in "${NODES[@]}"; do
        ping -c 1 -W $PING_TIMEOUT $node >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo "$node slots=$SLOTS_PER_NODE" >> $HOSTFILE_DYN
            active_nodes+="$node "
            alive_count=$((alive_count + 1))
        fi
    done

    if [ $alive_count -eq 0 ]; then
        echo "[CRITICAL] No nodes alive! Retrying..."
        sleep 5
        continue
    fi

    total_slots=$((alive_count * SLOTS_PER_NODE))

    # 3. RUN MPI JOB
    # PRINT THE "ASSIGNMENT" MESSAGE
    echo "--------------------------------------------------"
    echo "[STATUS]   Processing Chunk ($this_chunk items)"
    echo "[ASSIGNED] Workload distributed to: $active_nodes"
    
    OUTPUT=$(timeout -k $MPI_HARD_TIMEOUT $MPI_SOFT_TIMEOUT \
             mpirun -np $total_slots \
             --hostfile $HOSTFILE_DYN \
             $MCA_FLAGS \
             $EXEC $this_chunk 2>&1)
    
    EXIT_CODE=$?

    # 4. VALIDATE EXECUTION
    if [ $EXIT_CODE -eq 0 ]; then
        # SUCCESS LOGIC
        chunk_val=$(echo "$OUTPUT" | grep "CHUNK_RESULT:" | awk -F': ' '{print $2}' | tr -d '[:space:]')
        
        if [[ -n "$chunk_val" && "$chunk_val" =~ ^[0-9]+$ ]]; then
            echo "[SUCCESS]  Chunk finished. Result: $chunk_val"
            final_result=$((final_result + chunk_val))
            completed=$((completed + this_chunk))
            
            # Progress Bar effect
            percent=$((100 * completed / TOTAL_WORKLOAD))
            echo "[PROGRESS] Total: $percent%"
        else
            echo "[ERROR]    Output malformed. Retrying..."
        fi
    else
        # --- THIS IS THE PART YOU WANTED TO SHOW ---
        echo ""
        echo "!!!!!!!!!! FAILURE DETECTED (Code: $EXIT_CODE) !!!!!!!!!!"
        echo "[ALERT]    Node failure or Network Timeout detected."
        echo "[ACTION]   Identifying lost workload..."
        echo "[RECOVERY] Reassigning THIS CHUNK to remaining active nodes..."
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        sleep 2
    fi

done

# --- FINAL SUMMARY ---
echo "=================================================="
echo "   CRYPTO MINING SIMULATION COMPLETE"
echo "=================================================="
echo "Total Blocks Scanned : $completed"
echo "Rare Hashes Found    : $final_result"
PROFIT=$(echo "scale=2; $final_result * 0.05" | bc)
echo "Estimated Value      : \$$PROFIT"
echo "=================================================="
