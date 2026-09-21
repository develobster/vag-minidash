"""Small provider boundary between generic core and vehicle knowledge datasets."""
from dataclasses import dataclass
from typing import Protocol

from .registry import SignalRegistry


@dataclass(frozen=True)
class ProviderMetadata:
    provider_id: str
    manufacturer: str | None = None
    platforms: tuple = ()
    description: str = ""


class KnowledgeProvider(Protocol):
    """A provider supplies data only; transport/session ownership stays in core."""
    @property
    def metadata(self) -> ProviderMetadata: ...
    def definitions(self): ...


def registry_from_providers(providers):
    definitions = []
    provider_ids = set()
    for provider in providers:
        metadata = provider.metadata
        if metadata.provider_id in provider_ids:
            raise ValueError(f"Duplicate knowledge provider: {metadata.provider_id}")
        provider_ids.add(metadata.provider_id)
        supplied = tuple(provider.definitions())
        if any(item.provider_id != metadata.provider_id for item in supplied):
            raise ValueError(f"Provider {metadata.provider_id} returned foreign definitions")
        definitions.extend(supplied)
    return SignalRegistry(definitions)
