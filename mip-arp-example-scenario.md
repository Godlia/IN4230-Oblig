# Summary: MIP-ARP Example Scenario

## Network Topology

- Three hosts connected in a line: **Host-1** (if_12) ↔ (if_21) **Host-2** (if_23) ↔ (if_32) **Host-3**
- Host-2 is the middle node with two interfaces; Host-1 and Host-3 are not directly connected.

## Step 0: Initialization

- All ARP caches start empty.

## Step 1: Host-1 pings Host-2

- Host-1 checks its ARP cache for Host-2's MIP address, finds nothing, and broadcasts a MIP-ARP Request ("who is Host-2?") as an Ethernet frame with SDU Type = MIP-ARP.
- Host-2 receives the broadcast and replies with a unicast MIP-ARP Response containing the MAC address of the **interface that received the request** (a RAW socket descriptor identifies the interface).
- Both hosts update their caches; Host-1 then sends the ping and Host-2 responds.
- **Resulting caches:**

| Host   | MIP address | MAC address  |
|--------|-------------|--------------|
| Host-1 | 2           | if_21 MAC    |
| Host-2 | 1           | if_12 MAC    |
| Host-3 | (empty)     | (empty)      |

## Step 2: Host-3 pings Host-2

- Host-3 finds no cache entry and broadcasts a MIP-ARP Request.
- Host-2 receives it on **if_23** and replies with if_23's MAC address.
- Host-3 updates its cache, sends the ping, and Host-2 responds.
- **Resulting caches:**

| Host   | MIP address | MAC address  |
|--------|-------------|--------------|
| Host-1 | 2           | if_21 MAC    |
| Host-2 | 1           | if_12 MAC    |
| Host-2 | 3           | if_32 MAC    |
| Host-3 | 2           | if_23 MAC    |

## Step 3: Host-1 pings Host-2 again

- Host-1 uses its cached entry and sends the ping directly, with no ARP broadcast.
- Host-2 already has Host-1's entry and replies with a "PONG" immediately.
- **Host-1 cannot ping Host-3.**
