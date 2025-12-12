#!/bin/bash
# cluster_alive_nodes.sh
# Output: lines "node slots" for nodes considered ALIVE
# Criteria:
#   - heartbeat file exists and is recent (<= 15 seconds)
#   - ssh test passes

NODE_CONF="/cluster/nodes.conf"
HB_DIR="/cluster/heartbeats"
HB_TIMEOUT=15   # seconds

if [ ! -f "$NODE_CONF" ]; then
    echo "Error: $NODE_CONF not found" >&2
    exit 1
fi

NOW=$(date +%s)

while read -r host slots; do
    # skip comments/empty
    [ -z "$host" ] && continue
    [[ "$host" =~ ^# ]] && continue

    HB_FILE="$HB_DIR/$host"
    if [ ! -f "$HB_FILE" ]; then
        # no heartbeat yet
        continue
    fi

    LAST=$(cat "$HB_FILE" 2>/dev/null || echo 0)
    AGE=$((NOW - LAST))

    if [ "$AGE" -gt "$HB_TIMEOUT" ]; then
        # heartbeat too old
        continue
    fi

    # optional: ssh check
    ssh -n -o BatchMode=yes -o ConnectTimeout=1 "$host" "echo ok" >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        continue
    fi

    echo "$host $slots"
done < "$NODE_CONF"
