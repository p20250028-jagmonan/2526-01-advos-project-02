Assumptions (you can adjust):

* `head` → `192.168.1.10`
* `node1` → `192.168.1.11`
* `node2` → `192.168.1.12`
* All running Ubuntu (22.04-ish)
* User: `mpi` with UID/GID 999 on all three
* Shared dir: `/cluster`

---

## 🔹 Script 3 — Testing, debugging, and scaling

This “script” is more like a test & experiment playbook:

1. **Basic functional tests**
2. **MPI hello-world test**
3. **Scaling by adding more nodes**
4. **Code changes & run types**

---

### Part A — Basic cluster sanity checks (on head)

#### A1) Network / name resolution

```bash
ping -c 2 node1
ping -c 2 node2

getent hosts head node1 node2
```

If pings fail, fix IP / netplan / cabling.

---

#### A2) SSH as `mpi`

```bash
sudo -iu mpi bash <<'EOF'
for h in head node1 node2; do
  echo "=== $h ==="
  ssh -o StrictHostKeyChecking=no $h "hostname && whoami"
done
EOF
```

Expected: hostname and user `mpi` on each.

If it hangs or asks for password → SSH keys not set up correctly.

---

#### A3) NFS consistency

On **head**:

```bash
sudo -iu mpi bash <<'EOF'
echo "cluster test $(date)" > /cluster/testfile.txt
EOF

sudo -iu mpi ssh node1 cat /cluster/testfile.txt
sudo -iu mpi ssh node2 cat /cluster/testfile.txt
```

All should print the same line.

---

### Part B — MPI hello world

#### B1) Create example program (on head)

```bash
sudo -iu mpi bash <<'EOF'
cat >/cluster/hello.c <<'CEND'
#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int world_size, world_rank, name_len;
  char name[MPI_MAX_PROCESSOR_NAME];

  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Get_processor_name(name, &name_len);

  printf("Hello from rank %d of %d on %s\n",
         world_rank, world_size, name);

  MPI_Finalize();
  return 0;
}
CEND

mpicc /cluster/hello.c -o /cluster/hello
EOF
```

✅ **Test B1**

```bash
sudo -iu mpi ls -l /cluster/hello
```

---

#### B2) Run on 1 node vs 3 nodes

**Single node only:**

```bash
sudo -iu mpi mpirun -np 4 -host head /cluster/hello
```

**Across head + node1 + node2:**

```bash
sudo -iu mpi mpirun -np 6 --hostfile /home/mpi/hostfile /cluster/hello
```

✅ **Test B2**

You should see ranks spread across different hostnames.

If it hangs: run once with `-mca btl_base_verbose 100` to see details (for debug only).

---

### Part C — Scaling: add another machine (node3)

**Goal:** show you understand how to scale from 3 → 4 machines.

1. Physically connect `node3` to LAN.
2. Give it IP `192.168.1.13`, hostname `node3`.
3. Repeat **Script 2** steps for `node3`:

   * hostname + `/etc/hosts`
   * install packages
   * create `mpi` user with UID 999
   * NFS mount `/cluster`

Then, on **head**:

#### C1) Update `/etc/hosts` and hostfile

```bash
echo "192.168.1.13 node3" | sudo tee -a /etc/hosts

sudo -iu mpi bash <<'EOF'
cat > ~/hostfile <<HF
head slots=2
node1 slots=2
node2 slots=2
node3 slots=2
HF
EOF
```

#### C2) SSH key to node3

On **head**:

```bash
sudo -iu mpi ssh-copy-id -o StrictHostKeyChecking=no mpi@node3
```

✅ **Test C2**

```bash
sudo -iu mpi ssh node3 'hostname; whoami'
sudo -iu mpi ssh node3 'mount | grep " /cluster "'
```

---

#### C3) MPI scaling test

Run hello with more processes:

```bash
sudo -iu mpi mpirun -np 8 --hostfile /home/mpi/hostfile /cluster/hello
```

Observe that more ranks now run on `node3` too.

---

### Part D — Code changes & run types (for viva + experimentation)

#### D1) Simple compute-bound workload (matrix-like)

Create a fake compute-heavy program (no communication):

```bash
sudo -iu mpi bash <<'EOF'
cat >/cluster/compute.c <<'CEND'
#include <mpi.h>
#include <stdio.h>

double work(long n) {
  double x = 0.0;
  for (long i = 0; i < n; ++i) {
    x += (i % 7) * 0.123456789;
  }
  return x;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  long N = 50000000L;
  double r = work(N);
  printf("Rank %d finished work: %f\n", rank, r);

  MPI_Finalize();
  return 0;
}
CEND

mpicc /cluster/compute.c -o /cluster/compute
EOF
```

Run with different `-np`:

```bash
time sudo -iu mpi mpirun -np 1 -host head /cluster/compute
time sudo -iu mpi mpirun -np 4 --hostfile /home/mpi/hostfile /cluster/compute
time sudo -iu mpi mpirun -np 8 --hostfile /home/mpi/hostfile /cluster/compute
```

You’ll see near-linear speedup because this is embarrassingly parallel.

👉 **Talking point in viva:**
“This represents workloads like video encoding, where each process works mostly independently.”

---

#### D2) Communication-heavy variation (for conceptual contrast)

Modify to add dummy allreduce:

```bash
sudo -iu mpi bash <<'EOF'
cat >/cluster/compute_comm.c <<'CEND'
#include <mpi.h>
#include <stdio.h>

double work(long n) {
  double x = 0.0;
  for (long i = 0; i < n; ++i) {
    x += (i % 7) * 0.123456789;
  }
  return x;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int size, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  long N = 50000000L;
  double local = work(N);

  double global_sum = 0.0;
  MPI_Allreduce(&local, &global_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  if (rank == 0)
    printf("Global sum: %f (size=%d)\n", global_sum, size);

  MPI_Finalize();
  return 0;
}
CEND

mpicc /cluster/compute_comm.c -o /cluster/compute_comm
EOF
```

Run same scaling tests:

```bash
time sudo -iu mpi mpirun -np 1 --hostfile /home/mpi/hostfile /cluster/compute_comm
time sudo -iu mpi mpirun -np 4 --hostfile /home/mpi/hostfile /cluster/compute_comm
time sudo -iu mpi mpirun -np 8 --hostfile /home/mpi/hostfile /cluster/compute_comm
```

Now you can **see and explain**:

* Why speedup is not perfectly linear
* How communication overhead impacts scaling

That’s exactly what he’ll love if he asks *“what changes when you scale?”* or *“what if your program has more communication?”*.

---