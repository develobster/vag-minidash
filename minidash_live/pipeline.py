"""End-to-end orchestration using the existing registry, scheduler and store."""
from dataclasses import asdict, dataclass
from enum import Enum
import time

from .arbitration import SourceArbitrator
from .dbc import DbcAdapter
from .identity import EcuIdentity, IdentityContext
from .registry import ConflictingDefinitionError, SignalRegistry
from .runtime import PollJob, Scheduler, SignalStore


class Availability(str, Enum):
    KNOWN = "known"
    AVAILABLE = "available"
    STALE = "stale"
    UNAVAILABLE = "unavailable"


class SourceAvailability:
    """Observed runtime state, deliberately separate from knowledge definitions."""
    def __init__(self, definitions=(), clock=time.monotonic):
        self.clock = clock
        self.states = {item.id: {"state": Availability.KNOWN.value,
                                "reason": "definition_loaded", "updated": None}
                       for item in definitions}

    def set(self, definition_id, state, reason):
        self.states[definition_id] = {
            "state": Availability(state).value, "reason": reason, "updated": self.clock()}

    def get(self, definition_id):
        return dict(self.states.get(definition_id, {
            "state": Availability.KNOWN.value, "reason": "definition_loaded", "updated": None}))

    def refresh(self, rows):
        for row in rows:
            identifier = row.get("definition_id")
            if not identifier:
                continue
            if row.get("quality") == "stale":
                self.set(identifier, Availability.STALE, "ttl_expired")
            elif row.get("quality") in ("unavailable", "unsupported", "error",
                                       "identity_unverified"):
                self.set(identifier, Availability.UNAVAILABLE, row["quality"])
            else:
                self.set(identifier, Availability.AVAILABLE, "sample_published")


def _context_for(definition, vehicle, ecus):
    ecu = ecus.get(definition.ecu, EcuIdentity(logical_role=definition.ecu))
    return IdentityContext(vehicle, ecu)


def applicable_implementations(registry, canonical_name, vehicle, ecus):
    """Resolve specificity independently for every physical source route."""
    found = []
    for item in registry.definitions:
        context = _context_for(item, vehicle, ecus)
        if item.canonical_name == canonical_name and item.applies_to(context):
            found.append((item, context))
    selected = []
    routes = dict.fromkeys((item.ecu, item.source.key()) for item, _ in found)
    for route in routes:
        choices = [(item, context) for item, context in found
                   if (item.ecu, item.source.key()) == route]
        specificity = max(item.applicability.specificity() for item, _ in choices)
        best = [(item, context) for item, context in choices
                if item.applicability.specificity() == specificity]
        if len(best) != 1:
            raise ConflictingDefinitionError(
                f"Ambiguous implementations for {canonical_name} route {route}")
        selected.append(best[0])
    return tuple(selected)


def signal_catalog(registry, vehicle, ecus, availability=None):
    availability = availability or SourceAvailability(registry.definitions)
    names = sorted({item.canonical_name for item in registry.definitions})
    result = []
    for name in names:
        for item, context in applicable_implementations(registry, name, vehicle, ecus):
            state = availability.get(item.id)
            if item.source.kind == "uds" and item.ecu not in ecus:
                state = {"state": Availability.UNAVAILABLE.value,
                         "reason": "ecu_not_discovered", "updated": None}
            result.append({
                "canonical_name": name, "ecu": item.ecu, "definition_id": item.id,
                "provider_id": item.provider_id, "source": item.source.kind,
                "source_route": {"did": f"{item.source.did:04X}" if item.source.did is not None else None,
                                 "database": item.source.database,
                                 "message": item.source.message,
                                 "signal": item.source.signal},
                "provenance_origin": item.provenance.origin,
                "confidence": item.provenance.confidence,
                "applicability": item.applicability.as_dict(),
                "unit": item.unit, "ttl": item.ttl,
                "expected_period": item.expected_period,
                "validation_status": item.metadata.get("validation_status"),
                "raw_decoder_available": item.decoder is not None,
                "validated_for_identity": item.provenance.validated("raw_decode", context),
                "verification_scope": item.provenance.verification_scope,
                "validation_evidence": [asdict(evidence)
                                        for evidence in item.provenance.validation_evidence],
                "availability": state,
            })
    return result


@dataclass
class AcquisitionPlan:
    requested: tuple
    definitions: tuple
    passive: tuple
    jobs: list
    contexts: dict
    skipped: tuple


class AcquisitionPlanner:
    def __init__(self, registry, vehicle, ecus, availability, *, poll_periods=None):
        self.registry, self.vehicle, self.ecus = registry, vehicle, dict(ecus)
        self.availability = availability
        self.poll_periods = dict(poll_periods or {})

    def build(self, requested):
        requested = tuple(dict.fromkeys(requested))
        definitions, passive, jobs, skipped, contexts = [], [], [], [], {}
        for canonical_name in requested:
            implementations = applicable_implementations(
                self.registry, canonical_name, self.vehicle, self.ecus)
            if not implementations:
                skipped.append({"signal": canonical_name, "reason": "no_applicable_definition"})
                continue
            can_ready = any(item.source.kind == "can" and
                            self.availability.get(item.id)["state"] == Availability.AVAILABLE.value
                            for item, _ in implementations)
            for item, context in implementations:
                definitions.append(item)
                contexts[item.id] = context
                if item.source.kind == "can":
                    passive.append(item)
                    continue
                if item.ecu not in self.ecus:
                    skipped.append({"signal": canonical_name, "definition_id": item.id,
                                    "reason": "ecu_not_discovered"})
                    continue
                period = self.poll_periods.get(item.id,
                    self.poll_periods.get(canonical_name, item.expected_period))
                if period is None:
                    skipped.append({"signal": canonical_name, "definition_id": item.id,
                                    "reason": "poll_period_not_configured"})
                    continue
                def decoder(raw, ecu=item.ecu, did=item.source.did, ctx=context):
                    return self.registry.decode_uds(
                        ecu, did, raw, ctx, identity_verified=True)
                jobs.append(PollJob(
                    item.ecu, canonical_name, item.source.did, period, item.ttl, decoder,
                    definition_id=item.id, confidence=item.provenance.confidence,
                    active_cost=item.source.active_cost,
                    expected_period=item.expected_period or period, enabled=not can_ready,
                    require_verified=True))
        return AcquisitionPlan(requested, tuple(definitions), tuple(passive), jobs,
                               contexts, tuple(skipped))

    def reconcile(self, plan, rows):
        """Poll UDS only while no usable passive implementation is live."""
        for job in plan.jobs:
            can_live = any(row.get("signal") == job.signal and row.get("source") == "can" and
                           row.get("value") is not None and
                           row.get("quality") in ("valid", "candidate")
                           for row in rows)
            job.enabled = not can_live and not job.blocked
            if job.enabled and job.due == float("inf"):
                job.due = 0


class RuntimeEngine:
    """One observable pipeline; transport ownership remains in existing sessions/buses."""
    def __init__(self, plan, planner, sessions, databases, *, store=None,
                 arbitrator=None, cooldown=.05, clock=time.monotonic, trace=lambda event: None):
        self.plan, self.planner = plan, planner
        self.store = store or SignalStore(clock)
        self.arbitrator = arbitrator or SourceArbitrator()
        self.availability, self.clock, self.trace = planner.availability, clock, trace
        self.metrics = {
            "can_frames_received": 0, "can_frames_decoded": 0,
            "signals_published": 0, "uds_requests": 0, "uds_responses": 0,
            "uds_errors": 0, "uds_timeouts": 0, "poll_latency_total": 0.0,
            "poll_latency_max": 0.0, "source_switches": 0,
        }
        self._selected = {}
        passive_registry = SignalRegistry(plan.passive)
        def identity_for(item):
            return plan.contexts[item.id]
        self.adapters = {
            database_id: DbcAdapter(database, database_id, passive_registry, self.store,
                identity=identity_for, observed=self._can_observed)
            for database_id, database in databases.items()
            if any(item.source.database == database_id for item in plan.passive)
        }
        for item in plan.passive:
            if item.source.database not in databases:
                self.availability.set(item.id, Availability.UNAVAILABLE, "dbc_not_loaded")
        self.scheduler = Scheduler(sessions, self.store, plan.jobs, cooldown=cooldown,
                                   clock=clock, on_result=self._uds_result)

    def _can_observed(self, definition, quality, metadata):
        self.availability.set(definition.id, Availability.AVAILABLE, "can_frame_observed")
        self.trace({"kind": "source", "definition_id": definition.id,
                    "availability": "available", "quality": quality, **metadata})

    def _uds_result(self, job, status, latency, error=None):
        if status != "identity_unverified":
            self.metrics["uds_requests"] += 1
        self.metrics["poll_latency_total"] += latency
        self.metrics["poll_latency_max"] = max(self.metrics["poll_latency_max"], latency)
        if status == "success":
            self.metrics["uds_responses"] += 1
            self.availability.set(job.definition_id, Availability.AVAILABLE, "uds_response")
        else:
            self.metrics["uds_errors"] += 1
            if status == "timeout":
                self.metrics["uds_timeouts"] += 1
            self.availability.set(job.definition_id, Availability.UNAVAILABLE, status)
        self.metrics["signals_published"] += 1
        self.trace({"kind": "acquisition", "definition_id": job.definition_id,
                    "source": "uds", "status": status, "latency": latency,
                    "error": error})

    def feed_can(self, can_id, data, *, extended=False, remote=False, error=False):
        self.metrics["can_frames_received"] += 1
        decoded = 0
        for adapter in self.adapters.values():
            decoded += adapter.feed(can_id, data, extended=extended, remote=remote, error=error)
        self.metrics["can_frames_decoded"] += int(decoded > 0)
        self.metrics["signals_published"] += decoded
        return decoded

    def feed_message(self, message):
        return self.feed_can(message.arbitration_id, message.data,
            extended=message.is_extended_id, remote=message.is_remote_frame,
            error=message.is_error_frame)

    def step(self):
        rows = self.store.snapshot()
        self.availability.refresh(rows)
        self.planner.reconcile(self.plan, rows)
        result = self.scheduler.step()
        self._record_selections()
        return result

    def _record_selections(self):
        for signal in self.plan.requested:
            decision = self.arbitrator.decision(self.store.snapshot(), signal)
            selected = decision.get("selected")
            identifier = selected.get("definition_id") if selected else None
            previous = self._selected.get(signal)
            if previous is not None and identifier is not None and previous != identifier:
                self.metrics["source_switches"] += 1
            if identifier is not None:
                if previous != identifier:
                    self.trace({"kind": "selection", "signal": signal,
                                "selected": identifier,
                                "alternatives": decision["candidates"]})
                self._selected[signal] = identifier

    def snapshot(self):
        rows = self.store.snapshot()
        self.availability.refresh(rows)
        decisions = {signal: self.arbitrator.decision(rows, signal)
                     for signal in self.plan.requested}
        metrics = dict(self.metrics)
        metrics["stale_signals"] = sum(row["quality"] == "stale" for row in rows)
        metrics["scheduler"] = dict(self.scheduler.stats)
        return {"signals": rows, "decisions": decisions, "availability": {
            item.id: self.availability.get(item.id) for item in self.plan.definitions},
            "metrics": metrics, "skipped": list(self.plan.skipped)}
