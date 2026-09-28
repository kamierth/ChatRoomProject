#!/usr/bin/env python3
"""Concurrent load test for a running ChatRoomProject server.

This is intentionally separate from CTest. Start the server first, then run:

    python3 scripts/load_test.py --clients 100 --messages 20
"""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import socket
import sys
import threading
import time
from dataclasses import dataclass, field


@dataclass
class Stats:
    connected: int = 0
    sent: int = 0
    received_bytes: int = 0
    received_lines: int = 0
    errors: list[str] = field(default_factory=list)
    lock: threading.Lock = field(default_factory=threading.Lock)

    def add_error(self, message: str) -> None:
        with self.lock:
            self.errors.append(message)


def receive_until(sock: socket.socket, marker: bytes, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    received = bytearray()
    while marker not in received:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"timed out waiting for {marker!r}")
        sock.settimeout(remaining)
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("server closed the connection")
        received.extend(chunk)
    return bytes(received)


def connect_client(
    host: str,
    port: int,
    index: int,
    timeout: float,
    prefix: str,
) -> tuple[int, socket.socket]:
    sock = socket.create_connection((host, port), timeout=timeout)
    receive_until(sock, b"Please input your name:", timeout)
    sock.sendall(f"{prefix}_{index}\n".encode())
    sock.settimeout(0.5)
    return index, sock


def reader(
    index: int,
    sock: socket.socket,
    stop_event: threading.Event,
    stats: Stats,
) -> None:
    try:
        while not stop_event.is_set():
            try:
                data = sock.recv(65536)
            except socket.timeout:
                continue
            if not data:
                return
            with stats.lock:
                stats.received_bytes += len(data)
                stats.received_lines += data.count(b"\n")
    except OSError as error:
        if not stop_event.is_set():
            stats.add_error(f"reader {index}: {error}")


def send_messages(
    index: int,
    sock: socket.socket,
    messages: int,
    payload: str,
    start_event: threading.Event,
    stats: Stats,
) -> None:
    try:
        if not start_event.wait(timeout=10):
            raise TimeoutError("timed out waiting for the load test to start")
        body = payload.encode()
        for message_index in range(messages):
            sock.sendall(
                b"load "
                + str(index).encode()
                + b" "
                + str(message_index).encode()
                + b" "
                + body
                + b"\n"
            )
            with stats.lock:
                stats.sent += 1
    except (OSError, TimeoutError) as error:
        stats.add_error(f"sender {index}: {error}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="ChatRoomProject load test")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--clients", type=int, default=50)
    parser.add_argument("--messages", type=int, default=20)
    parser.add_argument("--payload-size", type=int, default=32)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--settle", type=float, default=1.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.clients <= 0 or args.messages < 0 or args.payload_size < 0:
        print("clients must be positive; messages and payload-size cannot be negative", file=sys.stderr)
        return 2

    stats = Stats()
    clients: list[tuple[int, socket.socket]] = []
    readers: list[threading.Thread] = []
    stop_event = threading.Event()
    prefix = f"load_{os.getpid()}"

    started = time.monotonic()
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(args.clients, 100)) as executor:
            futures = [
                executor.submit(
                    connect_client,
                    args.host,
                    args.port,
                    index,
                    args.timeout,
                    prefix,
                )
                for index in range(args.clients)
            ]
            for future in concurrent.futures.as_completed(futures):
                try:
                    clients.append(future.result())
                    stats.connected += 1
                except OSError as error:
                    stats.add_error(f"connect: {error}")

        if len(clients) != args.clients:
            raise RuntimeError(
                f"only {len(clients)} of {args.clients} clients connected"
            )

        for index, sock in clients:
            thread = threading.Thread(
                target=reader,
                args=(index, sock, stop_event, stats),
                daemon=True,
            )
            thread.start()
            readers.append(thread)

        time.sleep(args.settle)
        start_event = threading.Event()
        payload = "x" * args.payload_size
        send_started = time.monotonic()

        with concurrent.futures.ThreadPoolExecutor(max_workers=min(args.clients, 100)) as executor:
            futures = [
                executor.submit(
                    send_messages,
                    index,
                    sock,
                    args.messages,
                    payload,
                    start_event,
                    stats,
                )
                for index, sock in clients
            ]
            start_event.set()
            for future in futures:
                future.result()

        send_elapsed = time.monotonic() - send_started
        time.sleep(args.settle)

        for _, sock in clients:
            try:
                sock.sendall(b"/quit\n")
            except OSError:
                pass

        time.sleep(0.2)
    except (OSError, RuntimeError) as error:
        stats.add_error(str(error))
    finally:
        stop_event.set()
        for _, sock in clients:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
        for thread in readers:
            thread.join(timeout=1)

    elapsed = time.monotonic() - started
    expected_sends = args.clients * args.messages
    rate = stats.sent / send_elapsed if "send_elapsed" in locals() and send_elapsed > 0 else 0.0

    print("ChatRoomProject load test")
    print(f"  connected:      {stats.connected}/{args.clients}")
    print(f"  messages sent:  {stats.sent}/{expected_sends}")
    print(f"  received lines: {stats.received_lines}")
    print(f"  received bytes: {stats.received_bytes}")
    print(f"  send rate:      {rate:.1f} messages/s")
    print(f"  total time:     {elapsed:.2f}s")
    print(f"  errors:         {len(stats.errors)}")

    for error in stats.errors[:10]:
        print(f"    - {error}")
    if len(stats.errors) > 10:
        print(f"    - ... and {len(stats.errors) - 10} more")

    return 0 if not stats.errors and stats.sent == expected_sends else 1


if __name__ == "__main__":
    raise SystemExit(main())
