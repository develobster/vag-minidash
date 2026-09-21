"""Conservative selection of the best currently usable signal source."""
import math


QUALITY_RANK = {"raw_only": 1, "candidate": 2, "valid": 3}
CONFIDENCE_RANK = {"guessed": 0, "candidate": 1, "documented": 2, "confirmed": 3}


class AmbiguousSourceError(LookupError):
    pass


class SourceArbitrator:
    """Rank live rows without hard-coding CAN > UDS.

    Invalid/stale/unavailable rows are excluded.  Quality and evidence win
    first, then freshness, then lower active cost.  Equal winners fail safely.
    """
    def score(self, row):
        quality = QUALITY_RANK.get(row.get("quality"), 0)
        confidence = CONFIDENCE_RANK.get(row.get("confidence") or "candidate", 0)
        age, ttl = row.get("age", math.inf), row.get("ttl", 0)
        expected = row.get("expected_period")
        freshness_window = ttl
        if expected is not None:
            freshness_window = min(ttl, expected * 2)
        freshness = max(0.0, 1.0 - age / freshness_window) if freshness_window > 0 else 0.0
        cost = row.get("active_cost")
        if cost is None:
            cost = 1.0 if row.get("source") == "uds" else 0.0
        return quality, confidence, freshness, -float(cost)

    def select(self, rows, canonical_name, *, ecu=None):
        candidates = [row for row in rows if row.get("signal") == canonical_name and
                      (ecu is None or row.get("ecu") == ecu) and row.get("value") is not None and
                      row.get("quality") in QUALITY_RANK]
        if not candidates:
            return None
        scores = [(self.score(row), row) for row in candidates]
        best_score = max(score for score, _ in scores)
        best = [row for score, row in scores if score == best_score]
        if len(best) != 1:
            ids = ", ".join(str(row.get("definition_id") or row.get("source")) for row in best)
            raise AmbiguousSourceError(f"Equal sources for {canonical_name}: {ids}")
        return dict(best[0])
