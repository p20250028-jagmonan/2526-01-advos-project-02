## 🔹 Script 1 — Head installation (`head-setup.md` style)

**Goal:** prepare `head` as:

* MPI node
* NFS server
* SSH master for `mpi` user

Run everything on **head** as a sudo-capable user.

---

### Step 0 — Set hostname and /etc/hosts
* Check the ip addresses of your nodes and adjust below if needed.
  + Use `ip a` or `ifconfig` to find the IP of each node.

```bash
sudo hostnamectl set-hostname head

ip a  # Check your IP address here and adjust below if needed

cat <<EOF | sudo tee -a /etc/hosts
192.168.1.10 head # Adjust this IP address
192.168.1.11 node1 # Adjust this IP address
192.168.1.12 node2 # Adjust this IP address
EOF
```

✅ **Test 0**

```bash
hostname
getent hosts head node1 node2
```

You should see `head` and correct IPs.

---

### Step 1 — Install base packages

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev \
                    nfs-kernel-server nfs-common pdsh git curl htop
```

✅ **Test 1**

```bash
mpirun --version
```

You should see OpenMPI (or MPICH) version.

---

### Step 2 — Create `mpi` user with fixed UID/GID
* Step 2.1: Check if user `mpi` already exists:

```bash
id mpi
```
* Step 2.2 If user exists, note its UID/GID and home directory, skip to Step 3. If not, create it as below:

* Step 2.3 Find out suitable GID and UID for the user `mpi`
  + with available UID and GID 999 for consistency across nodes.
  + use command below to get available UID/GID if 999 is taken: 

```bash
`awk -F: '($3>=1000)&&($3!=1001)&&($3!=1002)&&($3!=1003){print $3}' /etc/passwd | sort -n | head -1`
```

* Step 2.4 Create user `mpi` with <UID>/<GID> identified in Step 2.3:

```bash
sudo groupadd -g <UID> mpi || true
sudo useradd -m -u <UID> -g <GID> -s /bin/bash mpi || true
echo "mpi ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/99-mpi >/dev/null
sudo chmod 440 /etc/sudoers.d/99-mpi
```

✅ **Test 2**

```bash
id mpi
ls -ld /home/mpi
```

Verify UID <UID>, GID <GID>, home directory exists.

---

### Step 3 — Generate SSH key for `mpi` (local test)

```bash
sudo -iu mpi bash <<'EOF'
mkdir -p ~/.ssh
chmod 700 ~/.ssh
if [ ! -f ~/.ssh/id_rsa ]; then
  ssh-keygen -t rsa -b 4096 -N "" -f ~/.ssh/id_rsa
fi
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
sort -u ~/.ssh/authorized_keys -o ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
EOF
```

✅ **Test 3**

```bash
sudo -iu mpi ssh -o StrictHostKeyChecking=no localhost hostname
```

Should print `head` without asking password.

---

### Step 4 — Setup `/cluster` and NFS export
* Create `/cluster` directory
* Export it via NFS to all nodes in the cluster (adjust IP range as needed)

```bash
sudo mkdir -p /cluster
sudo chown mpi:mpi /cluster
sudo chmod 2775 /cluster

echo "/cluster <IP ADDRESS RANGE>/24(rw,sync,no_subtree_check,no_root_squash)" \
  | sudo tee -a /etc/exports

sudo exportfs -ra
sudo systemctl enable --now nfs-server || sudo systemctl enable --now nfs-kernel-server
```

✅ **Test 4**

```bash
sudo exportfs -v
sudo ss -tulpn | grep 2049   # NFS port
```

You should see `/cluster` exported and NFS listening.

---

### Step 5 — Prepare a default MPI hostfile

```bash
sudo -iu mpi bash <<'EOF'
cat > ~/hostfile <<HF
head slots=2
node1 slots=2
node2 slots=2
HF
EOF
```

✅ **Test 5**

```bash
sudo -iu mpi cat /home/mpi/hostfile
```

You should see `head`, `node1`, `node2` entries.

---
