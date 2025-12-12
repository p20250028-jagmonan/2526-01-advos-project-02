#!/bin/bash

# =============================================================================
# RESILIENT MPI CRYPTO MINER (VERBOSE DEMO VERSION)
# =============================================================================

# --- COLORS FOR DEMO ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# --- CONFIGURATION ---
TOTAL_ITERS=10000000          # Total Hashes
CHUNK_ITERS=2000000           # Hashes per chunk
EXEC=./crypto_miner           # C program

NODES=("master" "worker1" "worker2") 
SLOTS_PER_NODE=4

# TIMEOUT SETTINGS
CHUNK_TIMEOUT=25
KILL_TIMEOUT=5s

# ACCUMULATORS
final_gold_count=0
final_total_hashes=0
completed=0
chunk_id=1

clear
echo -e "${CYAN}===============================================${NC}"
echo -e "${CYAN}   RESILIENT CRYPTO MINING FARM DEMO           ${NC}"
echo -e "${CYAN}===============================================${NC}"
echo "Target: $TOTAL_ITERS Hashes"
echo "Nodes:  ${NODES[*]}"
echo

while [ $completed -lt $TOTAL_ITERS ]; do
    echo -e "${YELLOW}------------------------------------------------------------${NC}"
    echo -e "Processing Chunk $chunk_id | Progress: $completed / $TOTAL_ITERS"

    remaining=$((TOTAL_ITERS - completed))
    this_chunk_iters=$(( remaining < CHUNK_ITERS ? remaining : CHUNK_ITERS ))

    # -------------------------------------------------------------------------
    # STEP 1: NODE DISCOVERY (VERBOSE)
    # -------------------------------------------------------------------------
    HOSTFILE=hosts.dynamic
    : > "$HOSTFILE"
    total_procs=0
    alive_list=""

    echo -ne "Scanning Cluster Status... "
    
    for node in "${NODES[@]}"; do
        # 1. Ping Check
        if ping -c1 -W0.5 "$node" &>/dev/null; then
            
            # 2. Zombie Cleanup
            timeout 2s ssh "$node" "killall -q -9 crypto_miner" &>/dev/null

            echo "$node slots=$SLOTS_PER_NODE" >> "$HOSTFILE"
            total_procs=$((total_procs + SLOTS_PER_NODE))
            alive_list+="$node "
        fi
    done

    # PRINT THE ROSTER
    if [ $total_procs -gt 0 ]; then
        echo -e "${GREEN}[OK]${NC}"
        echo -e "   -> Active Nodes: ${GREEN}$alive_list${NC}"
        echo -e "   -> Available CPUs: $total_procs"
    else
        echo -e "${RED}[CRITICAL FAILURE]${NC}"
        echo -e "${RED}   !!! NO NODES FOUND ALIVE !!!${NC}"
        echo "   Retrying scan in 5 seconds..."
        sleep 5
        continue
    fi

    # -------------------------------------------------------------------------
    # STEP 2: EXECUTION
    # -------------------------------------------------------------------------
    LOGFILE="chunk_${chunk_id}.log"
    echo -e "Assigning $this_chunk_iters hashes to $total_procs processors..."
    
    # Run MPI (Note: Removed the conflicting exclude flag)
    timeout -k "$KILL_TIMEOUT" "$CHUNK_TIMEOUT" \
        mpirun \
        --mca orte_base_help_aggregate 0 \
        -np "$total_procs" \
        --hostfile "$HOSTFILE" \
        "$EXEC" "$this_chunk_iters" > "$LOGFILE" 2>&1

    rc=$?

    # -------------------------------------------------------------------------
    # STEP 3: FAILURE ANALYSIS (VERBOSE)
    # -------------------------------------------------------------------------
    if [ $rc -ne 0 ]; then
        echo -e "${RED}!!! CHUNK FAILED !!!${NC}"
        
        if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
             echo -e "   Reason: ${YELLOW}TIMEOUT / NODE HANG${NC}"
             echo "   Analysis: A node stopped responding (Cable unplugged?)."
        else
             echo -e "   Reason: ${RED}MPI CRASH (Code $rc)${NC}"
             echo "   Analysis: A process was killed or segmentation fault."
        fi
        
        echo -e "${CYAN}   Action: Discarding partial work and rescheduling...${NC}"
        sleep 2
        continue 
    fi

    # -------------------------------------------------------------------------
    # STEP 4: RESULT PARSING
    # -------------------------------------------------------------------------
    chunk_line=$(grep -m1 '^CHUNK_RESULT' "$LOGFILE")

    if [ -n "$chunk_line" ]; then
        # 4a. Visual Breakdown
        echo -e "${GREEN}>>> Chunk Complete!${NC}"
        grep "\[Node:" "$LOGFILE" | sed 's/^/    /'
        
        # 4b. Parse Numbers
        c_iter=$(echo "$chunk_line" | awk -F'=' '{print $2}' | awk '{print $1}')
        c_found=$(echo "$chunk_line" | awk -F'=' '{print $3}' | awk '{print $1}')
        
        echo -e "   -> Gold Found in Chunk: ${YELLOW}$c_found${NC}"
        
        # Accumulate
        final_gold_count=$((final_gold_count + c_found))
        final_total_hashes=$((final_total_hashes + c_iter))
        
        completed=$((completed + this_chunk_iters))
        chunk_id=$((chunk_id + 1))
    else
        echo -e "${RED}!!! DATA CORRUPTION DETECTED !!!${NC}"
        echo "   Reason: Program finished but output was empty."
        echo "   Action: Retrying..."
        sleep 1
    fi

done

# -------------------------------------------------------------------------
# STEP 5: FINAL CALCULATION
# -------------------------------------------------------------------------
echo
echo -e "${CYAN}===============================================${NC}"
echo -e "${CYAN} MINING OPERATION FINISHED                     ${NC}"
echo -e "${CYAN}===============================================${NC}"
echo " Total Hashes:     $final_total_hashes"
echo -e " Gold Nuggets:     ${YELLOW}$final_gold_count${NC}"

yield_rate=$(awk "BEGIN {printf \"%.6f\", ($final_gold_count / $final_total_hashes) * 100}")
echo " Yield Rate:       $yield_rate %"
echo "==============================================="
