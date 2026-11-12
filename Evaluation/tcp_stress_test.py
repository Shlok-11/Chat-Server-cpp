#!/usr/bin/env python3
"""C++ TCP chat server integration test: 20 or 50 concurrent sockets.

Run server separately first: ./build/chat_server 5555
Then: python3 tests/tcp_stress_test.py --clients 50
Writes an evidence JSON file; exits 1 if any assertion fails.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time
import uuid

from tcp_test_utils import connect_user, close_user, online_users, recv_until, send_frame


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=5555)
    p.add_argument("--clients", type=int, default=50)
    p.add_argument("--timeout", type=float, default=15.0)
    p.add_argument("--output", default="results/stress_results.json")
    args = p.parse_args()
    if args.clients < 2:
        p.error("--clients must be >= 2")
    start = time.monotonic()
    run_id = uuid.uuid4().hex[:8]
    users = [f"test_{run_id}_{i:03}" for i in range(args.clients)]
    clients = [None] * args.clients
    checks = {}
    errors = []
    try:
        # All connection handshakes are initiated concurrently.
        with ThreadPoolExecutor(max_workers=min(args.clients, 100)) as pool:
            jobs = {pool.submit(connect_user, args.host, args.port, user, args.timeout): i
                    for i, user in enumerate(users)}
            for f in as_completed(jobs):
                i = jobs[f]
                clients[i] = f.result()
        checks["concurrent_connections"] = len(clients)

        actual = online_users(clients[0], args.timeout)
        if not set(users).issubset(actual):
            raise AssertionError(f"LIST missing registered users: {set(users) - actual}")
        checks["online_user_list"] = "PASS"

        token = f"stress-{run_id}"
        send_frame(clients[0], f"MSG:{token}")
        expected = f"BROADCAST:{users[0]}:{token}"
        with ThreadPoolExecutor(max_workers=min(args.clients, 100)) as pool:
            jobs = [pool.submit(recv_until, sock, lambda m, expected=expected: m == expected,
                                args.timeout) for sock in clients[1:]]
            for f in as_completed(jobs):
                f.result()
        checks["broadcast_recipients"] = args.clients - 1

        private_token = f"private-{run_id}"
        # Project README documents PMSG:<user>:<text>.
        send_frame(clients[0], f"PMSG:{users[1]}:{private_token}")
        recv_until(clients[1], lambda m: m == f"PRIVATE:{users[0]}:{private_token}", args.timeout)
        checks["private_message"] = "PASS"

        # Verify a disconnected client disappears from the registry.
        close_user(clients[-1])
        clients[-1] = None
        limit = time.monotonic() + args.timeout
        while True:
            present = online_users(clients[0], args.timeout)
            if users[-1] not in present:
                break
            if time.monotonic() > limit:
                raise AssertionError("disconnect was not reflected in LIST")
            time.sleep(0.1)
        checks["graceful_disconnect"] = "PASS"
    except Exception as exc:
        errors.append(f"{type(exc).__name__}: {exc}")
    finally:
        for sock in clients:
            if sock is not None:
                close_user(sock)
    result = {
        "test": "tcp_stress_integration",
        "utc_timestamp": datetime.now(timezone.utc).isoformat(),
        "host": args.host, "port": args.port, "requested_clients": args.clients,
        "duration_seconds": round(time.monotonic() - start, 3),
        "checks": checks, "errors": errors, "status": "PASS" if not errors else "FAIL",
        "notes": "Live result from this test run; server must already be running."
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
