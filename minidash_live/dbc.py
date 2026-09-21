"""Receive-only bridge from an external DBC database to SignalStore."""
from pathlib import Path


def load_cantools_database(path):
    """Load a user-supplied DBC without making cantools a core dependency."""
    try:
        import cantools
    except ImportError as exc:
        raise RuntimeError("DBC support requires: pip install -r requirements-data.txt") from exc
    return cantools.database.load_file(str(Path(path)))


class DbcAdapter:
    """Decode CAN frames using a DBC object; this class never owns or sends on a bus."""
    def __init__(self, database, database_id, registry, store, *, context):
        self.database = database
        self.database_id = database_id
        self.registry = registry
        self.store = store
        self.context = dict(context)
        self.rejected = 0

    def feed(self, can_id, data, *, extended=False, remote=False, error=False):
        if remote or error:
            self.rejected += 1
            return 0
        try:
            message = self.database.get_message_by_frame_id(can_id)
        except (KeyError, LookupError):
            return 0
        if bool(getattr(message, "is_extended_frame", False)) != bool(extended):
            return 0
        definitions = self.registry.can_for_message(
            self.database_id, message.name, self.context)
        if not definitions:
            return 0
        try:
            values = message.decode(bytes(data), decode_choices=False, scaling=True)
        except Exception:
            self.rejected += 1
            for item in definitions:
                self.store.publish(item.ecu, item.canonical_name, "can", None, item.ttl,
                                   "decode_error", item.unit)
            return len(definitions)
        count = 0
        for item in definitions:
            if item.source.signal not in values:
                self.store.publish(item.ecu, item.canonical_name, "can", None, item.ttl,
                                   "missing_signal", item.unit)
            else:
                confidence = item.provenance.confidence
                quality = "valid" if confidence == "confirmed" and item.provenance.vehicle_tested \
                    else "candidate"
                self.store.publish(item.ecu, item.canonical_name, "can",
                                   values[item.source.signal], item.ttl, quality, item.unit)
            count += 1
        return count
