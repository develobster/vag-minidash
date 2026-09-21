"""VAG knowledge provider; VAG/MQB details do not leak into registry core."""
from pathlib import Path

from .knowledge import ProviderMetadata
from .registry import load_registry


class VagKnowledgeProvider:
    def __init__(self, data_paths=None):
        root = Path(__file__).resolve().parents[1]
        self.data_paths = tuple(Path(path) for path in (data_paths or (
            root / "data/external/opendbc-vw-mqb-map.json",
            root / "data/external/community-haldex-gen5.json",
        )))

    @property
    def metadata(self):
        return ProviderMetadata("vag", manufacturer="Volkswagen Group",
            platforms=("MQB",),
            description="First MiniDash knowledge provider; MQB is its first dataset.")

    def definitions(self):
        for path in self.data_paths:
            yield from load_registry(path).definitions
