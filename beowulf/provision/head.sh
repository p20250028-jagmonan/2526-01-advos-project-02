#!/usr/bin/env bash
set -euo pipefail

HEAD_IP="${1:-192.168.56.10}"
NODES="${2:-3}"

# NFS server
sudo apt-get update -y
sudo apt-get install -y nfs-kernel-server

# Shared directory
sudo mkdir -p /cluster
sudo chown mpi:mpi /cluster
sudo chmod 2775 /cluster   # setgid for consistent group

# Export to the host-only subnet
EXPORT_LINE="/cluster 192.168.56.0/24(rw,sync,no_subtree_check,no_root_squash)"
if ! grep -q "^/cluster " /etc/exports; then
  echo "$EXPORT_LINE" | sudo tee -a /etc/exports >/dev/null
fi
sudo exportfs -ra
sudo systemctl enable --now nfs-server || sudo systemctl enable --now nfs-kernel-server

# Generate SSH key for mpi (only if absent), publish pub into /vagrant
sudo -u mpi bash -lc '
  if [ ! -f ~/.ssh/id_rsa ]; then
    ssh-keygen -t rsa -b 4096 -N "" -f ~/.ssh/id_rsa
  fi
  sudo cp ~/.ssh/id_rsa.pub /vagrant/mpi_id_rsa.pub
'

# Authorize self (head) and (later) workers using the shared pub
sudo -u mpi bash -lc 'cat /vagrant/mpi_id_rsa.pub >> ~/.ssh/authorized_keys && sort -u ~/.ssh/authorized_keys -o ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys'

# Convenience hostfile for OpenMPI
sudo -u mpi bash -lc "cat >/home/mpi/hostfile<<EOF
head slots=2
node1 slots=2
node2 slots=2
node3 slots=2
EOF"
sudo chown mpi:mpi /home/mpi/hostfile

