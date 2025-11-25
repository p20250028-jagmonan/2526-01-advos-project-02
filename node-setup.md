## 🔹 Script 2 — Node installation (`node-setup.md` style)

**Goal:** configure `node1`, `node2` to:

* Have matching `mpi` user
* Mount `/cluster` from head
* Accept SSH from `mpi@head`

Run these instructions on **each node** (`node1`, `node2`). Where I say “on nodeX”, you repeat for each node with its own hostname/IP.

---

### Step 0 — Set hostname and /etc/hosts (on each node)

On `node1`:

```bash
sudo hostnamectl set-hostname node1

cat <<EOF | sudo tee -a /etc/hosts
192.168.1.10 head
192.168.1.11 node1
192.168.1.12 node2
EOF
```

On `node2` (change hostname):

```bash
sudo hostnamectl set-hostname node2

cat <<EOF | sudo tee -a /etc/hosts
192.168.1.10 head
192.168.1.11 node1
192.168.1.12 node2
EOF
```

✅ **Test 0 (run on each node)**

```bash
hostname
getent hosts head node1 node2
ping -c 1 head
```

---

### Step 1 — Install base packages (each node)

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev \
                    nfs-common pdsh git curl htop
```

✅ **Test 1**

```bash
mpirun --version
```

---

### Step 2 — Create `mpi` user with same UID/GID (each node)

```bash
sudo groupadd -g 999 mpi || true
sudo useradd -m -u 999 -g 999 -s /bin/bash mpi || true
echo "mpi ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/99-mpi >/dev/null
sudo chmod 440 /etc/sudoers.d/99-mpi
```

✅ **Test 2**

```bash
id mpi
```

Check UID 999, GID 999.

---

### Step 3 — Mount `/cluster` from head (each node)

```bash
sudo mkdir -p /cluster

echo "head:/cluster /cluster nfs defaults 0 0" \
  | sudo tee -a /etc/fstab

sudo mount -a
```

✅ **Test 3**

```bash
mount | grep ' /cluster '
```

You should see `head:/cluster` mounted.

---

### Step 4 — Allow SSH from `mpi@head` (run from head)

Now go back to **head** and push the SSH key to each node.

On **head**:

```bash
sudo -iu mpi ssh-copy-id -o StrictHostKeyChecking=no mpi@node1
sudo -iu mpi ssh-copy-id -o StrictHostKeyChecking=no mpi@node2
```

You will type the password for the `mpi` user on each node once.

✅ **Test 4**

On **head**:

```bash
sudo -iu mpi ssh -o StrictHostKeyChecking=no node1 hostname
sudo -iu mpi ssh -o StrictHostKeyChecking=no node2 hostname
sudo -iu mpi pdsh -w node1,node2 'uname -n; mount | grep " /cluster "'
```

You should see node hostnames and `/cluster` mounted.

---
