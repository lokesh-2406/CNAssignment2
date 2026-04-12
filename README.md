# CNAssignment2

This repository contains an OMNeT++ simulation for distributed program execution with gossip-based score propagation over a ring topology.

## Project Layout

```text
CNAssignment2/
├── README.md
├── LICENSE
├── OmnetProject/
│   ├── client.cc
│   ├── client.ned
│   ├── network.ned
│   ├── omnetpp.ini
│   ├── ned_file_generator.py
│   └── topo.txt
└── veeraraju-e-distributed-program-execution-8a5edab282632443.txt
```

## What Each File Does

- `OmnetProject/client.cc` implements the client behavior, task forwarding, result collection, and gossip propagation.
- `OmnetProject/client.ned` declares the client module interface.
- `OmnetProject/network.ned` defines the client ring network.
- `OmnetProject/omnetpp.ini` selects the network and simulation parameters.
- `OmnetProject/ned_file_generator.py` regenerates the ring topology artifacts.
- `OmnetProject/topo.txt` stores the generated `source target` ring edges.

## Requirements

- OMNeT++ 6.x installed and configured.
- Python 3 available on the command line.
- A terminal or OMNeT++ IDE session with the OMNeT++ environment activated.

## Step-by-Step Execution

### 1. Open the project folder

From a terminal, go to the OMNeT++ project directory:

```powershell
cd "d:\MyStuff\College\.6thSemester\CN\Assignment2\CNAssignment2\OmnetProject"
```

### 2. Regenerate the topology files

This repository keeps the client network as a ring. Regenerate `topo.txt` and `network.ned` from the helper script if you change `N`:

```powershell
python ned_file_generator.py -n 5 --output-dir .
```

If you want a different number of clients, change `-n` to the desired value.

### 3. Build the OMNeT++ project

If you are using the OMNeT++ IDE, build the project from the IDE after importing it.

If you are building from the command line and the makefile has already been generated, run:

```powershell
make
```

If the makefile does not exist yet, generate it with the OMNeT++ build tool first, then run `make`:

```powershell
opp_makemake -f --deep
make
```

### 4. Run the simulation

Launch the simulation with the INI file:

```powershell
opp_run -u Qtenv -f omnetpp.ini
```

If you want a console-only run instead of the GUI, use:

```powershell
opp_run -u Cmdenv -f omnetpp.ini
```

### 5. Inspect the output

- Simulation logs are written to the OMNeT++ event log and the text output files created by the client module.
- Gossip propagation is logged with the sender node, local simulation time, and the gossip payload.
- Task results are accumulated until all subtasks complete, then gossip starts.

## Simulation Flow

1. The simulation starts from `omnetpp.ini`.
2. `network.ned` instantiates `N` clients in a ring.
3. `client.cc` sends the initial subtasks from client `0`.
4. Results are forwarded back to the requester and aggregated.
5. When all subtasks complete, client `0` starts gossip dissemination.
6. Gossip is forwarded along the ring until every client has seen it.

## Notes

- The topology generator writes both `topo.txt` and `network.ned` so the checked-in topology stays aligned with the simulation parameters.
- If you change the number of nodes, regenerate the topology before rebuilding and rerunning the simulation.
- The current configuration uses a 5-node ring by default.