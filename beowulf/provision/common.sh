#!/usr/bin/env bash
set -euo pipefail # Dont continue of something fails

ROLE="${1:-node}" # Role: head or node
NODES="${2:-3}" # Number of nodes

# Basic tooling + OpenMPI + NFS + helpers
sudo apt-get update -y
sudo apt-get install -y \
  build-essential openmpi-bin libopenmpi-dev \
  nfs-common pdsh htop git curl python3

# Create mpi user (for clean SSH/mpirun). Use fixed uid/gid so NFS perms match.
if ! id -u mpi >/dev/null 2>&1; then
  sudo groupadd -g 1999 mpi || true
  sudo useradd -m -u 1999 -g 1999 -s /bin/bash mpi || true
  echo "mpi ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/99-mpi >/dev/null
  sudo chmod 440 /etc/sudoers.d/99-mpi
fi

# /etc/hosts cluster entries
HEAD_IP="192.168.56.10"
echo "Configuring /etc/hosts entries..."
sudo bash -c "cat >/etc/hosts<<'EOF'
127.0.0.1   localhost
$HEAD_IP    head
192.168.56.11 node1
192.168.56.12 node2
192.168.56.13 node3
EOF"

# Ensure pdsh uses ssh
echo "export PDSH_RCMD_TYPE=ssh" | sudo tee /etc/profile.d/pdsh.sh >/dev/null

# MPI user convenience
sudo -u mpi bash -lc 'mkdir -p ~/.ssh && chmod 700 ~/.ssh'
