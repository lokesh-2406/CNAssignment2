#!/usr/bin/env python3
"""Generate the ring topology artifacts for the OMNeT++ client network."""

from __future__ import annotations

import argparse
from pathlib import Path


NETWORK_TEMPLATE = """network P2PNetwork
{{
    parameters:
        int N = default({nodes});

    submodules:
        client[N]: Client;

    connections allowunconnected:
        for i=0..N-1 {{
            client[i].out++ --> client[(i+1)%N].in++;
        }}
}}
"""


def build_ring_edges(node_count: int) -> list[tuple[int, int]]:
    return [(node, (node + 1) % node_count) for node in range(node_count)]


def write_topology(output_path: Path, edges: list[tuple[int, int]]) -> None:
    topology = "\n".join(f"{source} {target}" for source, target in edges) + "\n"
    output_path.write_text(topology, encoding="utf-8")


def write_network(output_path: Path, node_count: int) -> None:
    output_path.write_text(NETWORK_TEMPLATE.format(nodes=node_count), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the OMNeT++ ring topology files.")
    parser.add_argument("-n", "--nodes", type=int, default=5, help="Number of clients in the ring")
    parser.add_argument(
        "--output-dir",
        default=".",
        help="Directory where topo.txt and network.ned should be written",
    )
    args = parser.parse_args()

    if args.nodes < 2:
        raise ValueError("The ring topology requires at least two nodes")

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    edges = build_ring_edges(args.nodes)
    write_topology(output_dir / "topo.txt", edges)
    write_network(output_dir / "network.ned", args.nodes)

    print(f"Generated topo.txt and network.ned for N={args.nodes} in {output_dir}")


if __name__ == "__main__":
    main()
