#!/bin/bash
# heartbeat_client.sh
# Run this on *every* node (head, node1, node2, ...).
# It periodically updates a timestamp file on the shared NFS directory.

HB_DIR="/cluster/heartbeats"
NODE=$(hostname)

mkdir -p "$HB_DIR"

while true; do
    date +%s > "$HB_DIR/$NODE"
    sleep 5
done
