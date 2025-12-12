#!/bin/bash

# --- CONFIG ---
MPI_PROG="/cluster/area_mpi"
NODES=("master" "worker1" "worker2")
SLOTS=2
HOSTFILE="/tmp/hostfile_active"

# Clean log only on fresh start (Optional: remove this if you want persistence across runs)
rm -f /cluster/task_log.txt

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# --- SCAN FUNCTION ---
scan_nodes() {
    rm -f $HOSTFILE
    TOTAL=0
    ALIVE=""
    
    echo -e "${YELLOW}[MONITOR]${NC} Scanning Network..."

    for node in "${NODES[@]}"; do
        # Fast Timeout (2 seconds)
        ssh -o ConnectTimeout=2 -o StrictHostKeyChecking=no $node "exit" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "$node slots=$SLOTS" >> $HOSTFILE
            TOTAL=$((TOTAL + SLOTS))
            ALIVE+="$node "
        else
            echo -e "   -> $node: ${RED}[LOST CONNECTION]${NC}"
        fi
    done
}

# --- MAIN LOOP ---
while true; do
    scan_nodes
    
    if [ $TOTAL -eq 0 ]; then
        echo "No nodes left."
        exit 1
    fi

    echo -e "${YELLOW}[MANAGER]${NC} Distributing work to: $ALIVE"

    # AGGRESSIVE KEEPALIVE FLAGS (Essential for Cable Pull)
    mpirun -np $TOTAL \
        --hostfile $HOSTFILE \
        --mca plm_rsh_args "-x" \
        --mca btl_tcp_if_include 10.8.1.0/24 \
        --mca btl_tcp_keepalive 1 \
        --mca oob_tcp_keepalive 1 \
        $MPI_PROG
    
    EXIT_CODE=$?

    # If Exit Code is 0, we are done.
    if [ $EXIT_CODE -eq 0 ]; then
        echo -e "${GREEN}[SUCCESS]${NC} Job Complete."
        break
    else
        echo -e "\n${RED}[FAILURE]${NC} Hardware Failure Detected!"
        echo "Redistributing remaining tasks..."
        sleep 2
        # Loop restarts -> Scans -> Finds dead node -> Resumes Manager
    fi
done
