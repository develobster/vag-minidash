"""Experimental runtime: one cooperative I/O owner, explicit rates, no VAG guesses."""
from dataclasses import dataclass
import math
import threading
import time

from .uds import ReadClient, NegativeResponse


def positive(value):
    if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value) or value <= 0:
        raise ValueError("Positive finite duration required")
    return value


class SignalStore:
    """Separate source keys; stale values are never returned as current values."""
    def __init__(self, clock=time.monotonic):
        self.clock, self.lock, self.values = clock, threading.Lock(), {}

    def publish(self, ecu, signal, source, value, ttl, quality="valid", unit=None, *,
                definition_id=None, confidence=None, active_cost=None, expected_period=None,
                source_metadata=None):
        positive(ttl)
        if isinstance(value, float) and not math.isfinite(value):
            value, quality = None, "invalid"
        with self.lock:
            implementation = definition_id or source
            self.values[(ecu, signal, source, implementation)] = dict(
                ecu=ecu, signal=signal, source=source, definition_id=definition_id,
                value=value, quality=quality, unit=unit, timestamp=self.clock(), ttl=ttl,
                confidence=confidence, active_cost=active_cost,
                expected_period=expected_period,
                source_metadata=dict(source_metadata or {}))

    def invalidate(self, ecu, source="uds", quality="unavailable"):
        with self.lock:
            for key, row in self.values.items():
                if key[0] == ecu and key[2] == source:
                    row.update(value=None, quality=quality)

    def snapshot(self):
        now = self.clock()
        with self.lock:
            rows = []
            for original in self.values.values():
                row = dict(original, age=max(0, now-original["timestamp"]))
                if row["age"] >= row["ttl"] and row["quality"] in ("valid", "candidate", "raw_only"):
                    row.update(value=None, quality="stale")
                rows.append(row)
            return rows

    def best(self, signal, arbitrator, *, ecu=None):
        return arbitrator.select(self.snapshot(), signal, ecu=ecu)


class EcuSession:
    """Owns a fresh transport after faults. Call only from the scheduler thread.

    verify(client) must perform fresh identity checking; false prevents decoding,
    not raw research. No factory calls are made during backoff.
    """
    def __init__(self, factory, *, session=None, allow_extended=False, keepalive=1,
                 s3=5, backoff=1, max_backoff=30, verify=lambda client: False,
                 clock=time.monotonic, trace=lambda event: None, client_options=None):
        if (session is not None and (type(session) is not int or session not in (1, 3))) or \
                (session == 3 and not allow_extended):
            raise ValueError("Session not allowed")
        for value in (keepalive, s3, backoff, max_backoff):
            positive(value)
        if keepalive >= s3 or backoff > max_backoff:
            raise ValueError("Invalid keepalive/backoff limits")
        self.factory, self.verify, self.clock, self.trace = factory, verify, clock, trace
        self.session, self.allow_extended = session, allow_extended
        self.keepalive, self.s3 = keepalive, s3
        self.base_backoff, self.max_backoff = backoff, max_backoff
        self.options = dict(client_options or {})
        self.client = None
        self.state, self.identity_verified = "disconnected", False
        self.next_connect, self.last_activity, self.failures = 0, 0, 0
        self.last_error = None

    def close(self):
        client, self.client = self.client, None
        self.identity_verified = False
        self.state = "disconnected"
        if client:
            try:
                client.transport.close()
            except Exception as exc:
                self.last_error = f"cleanup: {exc}"

    def fail(self, exc):
        self.close()
        self.last_error = str(exc)
        self.failures += 1
        self.next_connect = self.clock() + min(self.max_backoff, self.base_backoff * 2**min(self.failures-1, 16))
        self.state = "backoff"

    def ready(self):
        if self.client and self.clock()-self.last_activity >= self.s3:
            self.close()  # Do not use an expired session or its verified identity.
        if self.client:
            return True
        if self.clock() < self.next_connect:
            return False
        try:
            self.state = "connecting"
            transport = self.factory()
            try:
                self.client = ReadClient(transport, trace=self.trace, **self.options)
            except Exception:
                transport.close()
                raise
            self.identity_verified = bool(self.verify(self.client))
            if self.session is not None and self.identity_verified:
                self.client.session_control(self.session, allow_extended=self.allow_extended)
            self.last_activity, self.state = self.clock(), "active"
            return True
        except Exception as exc:
            self.fail(exc)
            return False

    def maintain(self):
        if not self.ready():
            return False
        if self.session == 3 and self.clock()-self.last_activity >= self.keepalive:
            try:
                self.client.tester_present()
                self.last_activity = self.clock()
            except Exception as exc:
                self.fail(exc)
                return False
        return True

    def read(self, did):
        if not self.maintain():
            raise ConnectionError(self.last_error or "ECU in backoff")
        try:
            payload = self.client.read_did(did)
            self.last_activity, self.failures = self.clock(), 0
            return payload
        except NegativeResponse as exc:
            self.last_activity = self.clock()
            if exc.code in (0x7e, 0x7f):
                self.fail(exc)
            raise
        except Exception as exc:
            self.fail(exc)
            raise


@dataclass
class PollJob:
    ecu: str
    signal: str
    did: int
    period: float | None
    ttl: float
    decoder: object
    polling_class: str = "SLOW"
    due: float = 0
    enabled: bool = True
    definition_id: str | None = None
    confidence: str | None = None
    active_cost: float = 1.0
    expected_period: float | None = None
    blocked: bool = False
    require_verified: bool = False

    def __post_init__(self):
        if self.polling_class not in ("FAST", "MEDIUM", "SLOW", "ON_DEMAND"):
            raise ValueError("Unknown polling class")
        if self.polling_class == "ON_DEMAND":
            self.due = math.inf
        else:
            positive(self.period)
        positive(self.ttl)
        if isinstance(self.active_cost, bool) or not isinstance(self.active_cost, (int, float)) \
                or not math.isfinite(self.active_cost) or self.active_cost < 0:
            raise ValueError("Invalid active cost")
        if self.expected_period is not None:
            positive(self.expected_period)
        if type(self.did) is not int or not 0 <= self.did <= 65535:
            raise ValueError("Invalid DID")


class Scheduler:
    """Cooperative, bounded single-flight polling; no catch-up bursts.

    Per-endpoint cooldown counts attempts, including NRC. One slow ECU can delay
    a tick up to client.total; parallel physical-bus workers are intentionally
    not implemented here. Jobs must be bounded by the operator's bus budget.
    """
    def __init__(self, sessions, store, jobs, *, cooldown, clock=time.monotonic,
                 on_result=lambda job, status, latency, error=None: None):
        positive(cooldown)
        if any(j.ecu not in sessions for j in jobs):
            raise ValueError("Unknown ECU in job")
        self.sessions, self.store, self.jobs = sessions, store, list(jobs)
        self.cooldown, self.clock, self.next_tx = cooldown, clock, {}
        self.on_result = on_result
        self.stats = dict(success=0, nrc=0, errors=0, timeouts=0,
                          identity_unverified=0, latency_total=0.0, latency_max=0.0)

    def request(self, signal, ecu):
        for job in self.jobs:
            if job.ecu == ecu and job.signal == signal and job.polling_class == "ON_DEMAND" \
                    and not job.blocked:
                job.enabled, job.due = True, self.clock()

    def step(self):
        now = self.clock()
        # Maintain only connected sessions. Failed endpoints reopen on their next job.
        for ecu, session in self.sessions.items():
            if session.client and not session.maintain():
                self.store.invalidate(ecu)
        candidates = [j for j in self.jobs if j.enabled and j.due <= now
                      and self.next_tx.get(j.ecu, 0) <= now
                      and self.sessions[j.ecu].next_connect <= now]
        if not candidates:
            return False
        job = min(candidates, key=lambda j: j.due)
        session = self.sessions[job.ecu]
        started = self.clock()
        status, error = "success", None
        try:
            if not session.maintain():
                raise ConnectionError(session.last_error or "ECU in backoff")
            if session.identity_verified:
                raw = session.read(job.did)
                decoded = job.decoder(raw)
            else:
                if not job.require_verified:
                    session.read(job.did)  # Legacy raw research path; decoder stays gated.
                else:
                    status = "identity_unverified"
                decoded = dict(value=None, quality="identity_unverified", unit=None)
            self.store.publish(job.ecu, job.signal, "uds", decoded.get("value"), job.ttl,
                               decoded.get("quality", "raw_only"), decoded.get("unit"),
                               definition_id=job.definition_id, confidence=job.confidence,
                               active_cost=job.active_cost,
                               expected_period=job.expected_period or job.period,
                               source_metadata={"did": f"{job.did:04X}"})
            if status == "identity_unverified":
                self.stats["identity_unverified"] += 1
            else:
                self.stats["success"] += 1
        except NegativeResponse as exc:
            status, error = "nrc", str(exc)
            self.stats["nrc"] += 1
            quality = "unsupported" if exc.code == 0x31 else "unavailable"
            self.store.publish(job.ecu, job.signal, "uds", None, job.ttl, quality,
                               definition_id=job.definition_id, confidence=job.confidence,
                               active_cost=job.active_cost,
                               expected_period=job.expected_period or job.period,
                               source_metadata={"did": f"{job.did:04X}", "nrc": exc.code})
            if exc.code == 0x31:
                job.enabled, job.blocked = False, True  # No retry storm until reconfiguration.
        except Exception as exc:
            status, error = ("timeout" if isinstance(exc, TimeoutError) else "error"), str(exc)
            self.stats["errors"] += 1
            if isinstance(exc, TimeoutError):
                self.stats["timeouts"] += 1
            self.store.publish(job.ecu, job.signal, "uds", None, job.ttl, "error",
                               definition_id=job.definition_id, confidence=job.confidence,
                               active_cost=job.active_cost,
                               expected_period=job.expected_period or job.period,
                               source_metadata={"did": f"{job.did:04X}"})
        if session.state != "active":
            self.store.invalidate(job.ecu)
        done = self.clock()
        latency = max(0, done - started)
        self.stats["latency_total"] += latency
        self.stats["latency_max"] = max(self.stats["latency_max"], latency)
        self.on_result(job, status, latency, error)
        self.next_tx[job.ecu] = done + self.cooldown
        job.due = math.inf if job.polling_class == "ON_DEMAND" else done + job.period
        return True

    def run(self, stop, duration):
        positive(duration)
        end = self.clock() + duration
        try:
            while not stop.is_set() and self.clock() < end:
                self.step()
                stop.wait(.01)
        finally:
            for ecu, session in self.sessions.items():
                session.close()
                self.store.invalidate(ecu)
