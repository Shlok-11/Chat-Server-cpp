#!/usr/bin/env python3
"""Repeated application-level availability probes with 20 active clients.

Each probe validates LIST and delivery of a unique broadcast to all peers.
Success rate = successful probes / attempted probes; NOT time-based uptime.

Run: python3 tests/tcp_availability_5min.py --minutes 5 --clients 20
"""
import argparse
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
    p.add_argument("--clients", type=int, default=20)
    p.add_argument("--minutes", type=float, default=5.0)
    p.add_argument("--interval", type=float, default=1.0)
    p.add_argument("--timeout", type=float, default=5.0)
    p.add_argument("--output", default="results/availability_results.json")
    args = p.parse_args()
    if args.clients < 2 or args.minutes <= 0 or args.interval <= 0:
        p.error("clients >= 2; minutes and interval > 0 required")

    started_utc = datetime.now(timezone.utc).isoformat()
    start = time.monotonic()
    deadline = start + args.minutes * 60
    run_id = uuid.uuid4().hex[:8]
    usernames = [f"upt_{run_id}_{i}" for i in range(args.clients)]
    sockets = []
    succeeded = 0
    attempted = 0
    failure_samples = []
    setup_error = None
    try:
        for user in usernames:
            sockets.append(connect_user(args.host, args.port, user, args.timeout))
        # Start measurement AFTER clients are connected.
        start = time.monotonic()
        deadline = start + args.minutes * 60
        while time.monotonic() < deadline:
            probe_start = time.monotonic()
            attempted += 1
            try:
                connected = online_users(sockets[0], args.timeout)
                if not set(usernames).issubset(connected):
                    raise AssertionError(f"LIST missing {len(set(usernames)-connected)} users")
                token = f"probe-{run_id}-{attempted}"
                send_frame(sockets[0], f"MSG:{token}")
                expected = f"BROADCAST:{usernames[0]}:{token}"
                # Sequential reads with individual timeouts. For slow peers a
                # single probe can take > interval; the next probe won't overlap.
                for peer in sockets[1:]:
                    recv_until(peer, lambda m, expected=expected: m == expected, args.timeout)
                succeeded += 1
            except Exception as exc:
                if len(failure_samples) < 20:
                    failure_samples.append({"probe": attempted, "reason": f"{type(exc).__name__}: {exc}"})
                # A failed connection generally prevents meaningful further
                # testing. Stop rather than pretending subsequent probes ran.
                break
            wait = args.interval - (time.monotonic() - probe_start)
            if wait > 0:
                time.sleep(min(wait, max(0, deadline - time.monotonic())))
    except Exception as exc:
        setup_error = f"{type(exc).__name__}: {exc}"
    finally:
        for sock in sockets:
            close_user(sock)

    elapsed = time.monotonic() - start
    rate = round(100.0 * succeeded / attempted, 2) if attempted else None
    complete = setup_error is None and not failure_samples and elapsed >= args.minutes * 60
    result = {
        "test": "tcp_application_probe_availability",
        "utc_timestamp": started_utc,
        "host": args.host, "port": args.port,
        "clients": args.clients, "requested_duration_seconds": args.minutes * 60,
        "measured_duration_seconds": round(elapsed, 3),
        "probe_interval_target_seconds": args.interval,
        "attempted_probes": attempted, "successful_probes": succeeded,
        "failed_probes": attempted - succeeded,
        "observed_probe_success_percent": rate,
        "completed_requested_duration": complete,
        "failure_samples": failure_samples, "setup_error": setup_error,
        "status": "PASS" if complete else "FAIL",
        "notes": "Probe-based functional availability, not continuous time-based uptime or long-term SLA. A slow probe may reduce the number of probes."
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if complete else 1


if __name__ == "__main__":
    sys.exit(main())
