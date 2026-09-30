#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

if ! command -v sudo >/dev/null 2>&1; then
    echo "sudo is required to run Mininet" >&2
    exit 1
fi

if ! command -v mn >/dev/null 2>&1; then
    echo "Mininet is not installed or not on PATH" >&2
    exit 1
fi

make all

sudo mn --custom oblig-topology.py --topo oblig --mac --controller=none --switch ovsk --link tc <<'EOF'
A ./target/mipd /tmp/A.sock 1 &
B ./target/mipd /tmp/B.sock 2 &
C ./target/mipd /tmp/C.sock 3 &
B sleep 1
B ./target/ping_server /tmp/B.sock &
A sleep 1
A ./target/ping_client /tmp/A.sock fromAtoB 2
C ./target/ping_client /tmp/C.sock fromCtoB 2
EOF
