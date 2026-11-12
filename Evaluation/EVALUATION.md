# TCP Chat Server — Test and Evaluation Report

## Scope
This document describes repeatable tests for the original C++ POSIX TCP chat server at https://github.com/Kuldeepcoder2006/tcp-chat-server-cpp. The Python scripts are AI-assisted testing tools. Credit the original C++ implementation and comply with its reuse permissions.

## Environment and protocol
- C++ server runs on Linux/WSL2, listening on `127.0.0.1:5555` (example).
- Python 3, standard library only. The Python clients run on the same host in these commands.
- Every TCP application frame: **4-byte big-endian unsigned length + UTF-8 payload**.
- Client handshake: `CONNECT:<username>`; expected server reply starts `OK:`.
- Test commands: `MSG:<text>`, `PMSG:<username>:<text>`, `LIST`, `QUIT`.
- Server responses checked include `BROADCAST:<username>:<text>`, `PRIVATE:<username>:<text>`, `USERLIST:<csv>`.

## How to reproduce
From project root, with C++17, CMake, and Python 3 installed:

```bash
cmake -S . -B build
cmake --build build --parallel 4
./build/chat_server 5555
```

In a second terminal (from project root):

```bash
python3 tests/tcp_stress_test.py --clients 20 --output results/stress_20.json
python3 tests/tcp_stress_test.py --clients 50 --output results/stress_50.json
python3 tests/tcp_availability_5min.py --minutes 5 --clients 20 --output results/availability_5min.json
```

The tests assume the server is already running. Each command writes its own JSON evidence. `status: PASS` and exit code 0 indicate the tests' assertions passed. Running the script does **not** compile or start the server for you.

## Integration tests (script `tcp_stress_test.py`)
1. Simulate 20 or 50 distinct TCP clients by opening sockets and sending framed registration messages concurrently.
2. Send `LIST`; verify the registered usernames are present.
3. Send one unique `MSG:` from client 0; verify that all **N − 1** other clients receive the correct `BROADCAST:` payload.
4. Send one unique `PMSG:` from client 0 to client 1; verify the expected `PRIVATE:` payload.
5. Send `QUIT` for one user; verify that username disappears from `LIST`.
6. Close the remaining test connections.

**Pass criteria:** No connection, framing, registration, message-delivery, list, or disconnect assertion fails.

## Application-level availability test (script `tcp_availability_5min.py`)
1. Connect 20 distinct test clients before starting the measurement clock.
2. Approximately once a second for five minutes, request `LIST` and confirm all 20 users remain present.
3. Broadcast a unique message from client 0 and confirm the other 19 receive it.
4. Count a probe successful **only if both the user-list and broadcast checks pass**.
5. Record elapsed time, attempted and successful probes, failure samples, and a JSON summary.

**Formula:** `observed_probe_success_percent = successful_probes / attempted_probes × 100`.

The script does not deliberately simulate a disconnected or unresponsive server recovering; on the first failed probe it records the failure and stops to avoid misleading subsequent counts. A stalled probe can take longer than one second. Therefore, five minutes does **not** automatically imply exactly 300 probes.

## Previously reported observations (historical, not new results)
In the preceding testing discussion, the following results were reported from a separate Linux test session. They have **not been re-executed or independently verified for this package**:

| Test | Previously reported outcome |
|---|---|
| 20 simultaneous connections | Passed |
| 50 simultaneous connections | Passed |
| Broadcast, private messages, user list, disconnect | Passed |
| Five-minute test, 20 active clients | Reported 300 / 300 successful probes |
| Observed probe success rate for that run | Reported 100.00% |

These observations are not proof of 99% long-term production uptime. For a defensible GitHub submission, commit the actual JSON results produced by a documented run, with the timestamp, duration, scripts, and source revision.

## Known limitations
- A five-minute localhost functional probe test measures **observed probe success**, not continuous time-based uptime or an SLA.
- One test machine cannot reproduce internet latency, packet loss, or real production traffic.
- The original server's detached worker threads and locking during socket sends merit additional concurrency and shutdown tests.
- No claim about reliability beyond the explicitly observed test window is established.
- Test script correctness must be reviewed against the precise C++ server revision used.

## Attribution
The base C++ chat implementation is from Kuldeepcoder2006/tcp-chat-server-cpp. Test scaffolding and documentation here were generated with ChatGPT assistance. Describe your personal contributions accurately, and preserve the original author’s attribution and applicable permissions.
