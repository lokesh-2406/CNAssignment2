# CSL3080 Assignment-2 — P2P Task Distribution with Chord Routing

## Overview

A peer-to-peer OMNeT++ simulation where N client nodes collaborate to find
the maximum element of an array using a distributed task model:

- **Node 0** divides the array into `totalSubtasks` subtasks.
- Subtask `i` is routed to client `i % N` using **Chord-inspired O(log N) routing**.
- Each receiving node computes the local maximum and routes the result back to Node 0.
- Node 0 aggregates all partial results to produce the **global maximum**.
- Every client then participates in a **flooding-based gossip protocol**.
  Each node broadcasts a completion message; on first receipt a node forwards
  it to all neighbours except the sender. Simulation ends when every node
  has seen gossip from all N clients.

## File Structure

```
OmnetProject/
├── client.cc               # Client module logic (Chord routing + gossip)
├── client.ned              # Module declaration (gates, parameters)
├── network.ned             # Network topology (auto-generated)
├── topo.txt                # Edge list (auto-generated, editable)
├── ned_file_generator.py   # Script to regenerate topology for any N
├── omnetpp.ini             # Simulation configuration
├── outputfile.txt          # Output written during simulation (created at runtime)
└── README.md               # This file
```

## Topology: Ring + Chord Fingers

For N nodes the simulator establishes:
- **Ring edges**: each node connects to its clockwise successor.
- **Chord finger edges**: for each node `i` and each `k` in `1 … floor(log2(N-1))`,
  an extra edge to `(i + 2^k) % N`.

This gives O(log N) routing depth — messages reach any target in at most
`ceil(log2 N)` hops.

To change N, regenerate the topology files (see §Changing N below).

---

## Prerequisites

| Tool | Version |
|------|---------|
| OMNeT++ | 6.x (tested with 6.0.3) |
| Python | 3.8+ (for ned_file_generator.py) |
| OS | Windows (IDE), Linux/WSL (opp_env or native) |

---

## Running in the OMNeT++ IDE (Recommended)

### 1 — Import the project

1. Open OMNeT++ IDE.
2. **File → Import → General → Existing Projects into Workspace**.
3. Browse to the `OmnetProject/` folder and click **Finish**.

### 2 — Build

1. Right-click the project in the **Project Explorer** → **Build Project**.
2. Confirm there are no errors in the **Problems** tab.

### 3 — Run

1. Right-click `omnetpp.ini` → **Run As → OMNeT++ Simulation**.
2. In the run dialog choose **qtenv** (GUI) or **cmdenv** (console).
3. Press the **Run** (▶) button or use **Run → Run** from the menu.

### 4 — View output

- Console output appears in the **OMNeT++ Log** pane.
- `outputfile.txt` is written to the project root directory.

---

## Running from the Terminal (WSL / opp_env / native Linux)

### 1 — Set up environment

```bash
# If using opp_env:
opp_env shell omnetpp-6.0.3   # or whichever version you have

# Native install — source the environment script:
source /path/to/omnetpp-6.x/setenv
```

### 2 — Compile

```bash
cd OmnetProject/
opp_makemake -f --deep          # generate Makefile (first time only)
make -j$(nproc)
```

### 3 — Run (command-line mode)

```bash
./OmnetProject -u Cmdenv -f omnetpp.ini
```

Or with explicit network:

```bash
./OmnetProject -u Cmdenv -f omnetpp.ini -n . P2PNetwork
```

---

## Changing N (Number of Clients)

**Step 1** — Regenerate topology files:

```bash
python3 ned_file_generator.py -n 8          # change 8 to desired N
```

This overwrites `topo.txt` and `network.ned`.

**Step 2** — Update `omnetpp.ini`:

```ini
*.client[*].N = 8        # must match the value used in step 1
```

**Step 3** — Rebuild (IDE: Build Project | Terminal: `make -j$(nproc)`) and run.

> **Note**: You are allowed to edit `topo.txt` without recompiling.
> However, `network.ned` must also be regenerated (it encodes the connections),
> and `omnetpp.ini` must reflect the new N. Only `client.cc` must not be changed
> to avoid the submission penalty.

---

## Changing Number of Subtasks

Edit `omnetpp.ini`:

```ini
*.client[*].totalSubtasks = 20    # must be > N; each chunk will have ≥ 2 elements
```

---

## Output Format

`outputfile.txt` and the console will show:

```
=== Simulation Output ===
Node 0 generated array for task distribution.
Node 2 processed subtask | local max = 87
Node 0 received subtask result: 87 (1/10)
...
=== FINAL MAX (Node 0): 95 ===
Node 0 sent gossip: 4.32:0:Client0#
[GOSSIP] Node 3 | Time: 4.45 | From gate: 1 | Originator: Node 0 | Msg: 4.32:0:Client0#
...
Node 3 received gossip from all 5 clients. Terminating.
```

Gossip message format (matches assignment spec):
```
<simTime>:<nodeId>:Client<id>#
```

---

## Routing Algorithm (Chord O(log N))

Each node builds a **finger table** at startup:

```
finger[k] = (id + 2^k) % N,  for k = 0 … floor(log2(N-1))
```

When forwarding a message toward `target`, the node picks the finger that
advances it furthest clockwise toward `target` without overshooting.
This guarantees delivery in at most ⌈log₂ N⌉ hops.

---

## Gossip Protocol

1. After completing its task, a node floods its gossip to **all connected neighbours**.
2. On first receipt of a gossip ID, a node logs it and forwards to all neighbours
   **except** the gate it arrived on.
3. Duplicate gossip IDs are silently dropped.
4. When a node has seen gossip from all N originators, it calls `endSimulation()`.