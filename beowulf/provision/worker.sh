#!/usr/bin/env bash
set -euo pipefail

HEAD_IP="${1:-192.168.56.10}"

# Mount /cluster from head
sudo mkdir -p /cluster
if ! grep -q "^${HEAD_IP}:/cluster " /etc/fstab; then
  echo "${HEAD_IP}:/cluster /cluster nfs defaults 0 0" | sudo tee -a /etc/fstab >/dev/null
fi
sudo mount -a || true

# Install head’s published mpi key as authorized_keys for mpi user
if [ -f /vagrant/mpi_id_rsa.pub ]; then
  sudo -u mpi bash -lc 'cat /vagrant/mpi_id_rsa.pub >> ~/.ssh/authorized_keys && sort -u ~/.ssh/authorized_keys -o ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys'
fi

# Copy the private key to workers too so mpi can ssh among workers if needed
if [ -f /vagrant/mpi_id_rsa.pub ] && [ ! -f /home/mpi/.ssh/id_rsa ]; then
  # create a minimal private key by reusing vagrant's is messy; simplest: scp from head later or accept head-driven launches
  # For convenience, allow ssh from head -> workers (the common mpirun pattern). No strict need for worker->worker.
  :
fi
