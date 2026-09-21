"""Deterministic offline transport. It never touches a VCI or vehicle."""
import json
from pathlib import Path


class ReplayTransport:
    def __init__(self, fixture):
        data = json.loads(Path(fixture).read_text(encoding="utf-8"))
        if data.get("kind") != "synthetic_uds_replay_v1":
            raise ValueError("Replay fixture must explicitly be synthetic_uds_replay_v1")
        self.responses = {str(k).lower(): bytes.fromhex(v) for k, v in data.get("responses", {}).items()}
        self.queue = []
        self.sent = []

    def send(self, pdu, timeout):
        self.sent.append(bytes(pdu))
        response = self.responses.get(bytes(pdu).hex())
        if response is not None:
            self.queue.append(response)

    def recv(self, timeout):
        return self.queue.pop(0) if self.queue else None

    def close(self):
        self.queue.clear()
