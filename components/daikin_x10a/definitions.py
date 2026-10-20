"""Reads the ESPAltherma model definition files (definitions/*.h).

Each line of such a file describes one value: {registry, offset, conversion, size, type, "label"}.
The files come from ESPAltherma (MIT License, Copyright (c) 2020 Raomin).
"""

from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
import re

DEFINITIONS_DIR = Path(__file__).parent / "definitions"

_LINE = re.compile(
    r"\{\s*0x([0-9A-Fa-f]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*\"(.*)\"\s*\}"
)
# Separators between data grids, not values
_SKIP_CONV = {995, 998}


@dataclass(frozen=True)
class LabelDef:
    registry: int
    offset: int
    conv: int
    size: int
    type: int
    label: str


def model_names():
    return sorted(p.stem for p in DEFINITIONS_DIR.glob("*.h"))


def _model_file(model):
    wanted = model.strip().lower()
    for name in model_names():
        inner = name[name.find("(") + 1 : name.rfind(")")] if "(" in name else name
        if wanted in (name.lower(), inner.lower()):
            return DEFINITIONS_DIR / f"{name}.h"
    raise ValueError(f"Unknown model '{model}'")


@lru_cache(maxsize=None)
def load_model(model):
    labels = []
    for line in _model_file(model).read_text(encoding="utf-8", errors="replace").splitlines():
        m = _LINE.search(line)
        if not m:
            continue
        reg, offset, conv, size, typ, label = m.groups()
        if int(conv) in _SKIP_CONV:
            continue
        labels.append(LabelDef(int(reg, 16), int(offset), int(conv), int(size), int(typ), label))
    return labels


def find_label(model, label, registry=None):
    hits = [
        d
        for d in load_model(model)
        if d.label == label and (registry is None or d.registry == registry)
    ]
    if not hits:
        raise ValueError(f"Model has no value '{label}'")
    if len(hits) > 1:
        regs = ", ".join(f"0x{d.registry:02X}" for d in hits)
        raise ValueError(f"'{label}' exists in several registers ({regs}), set 'register'")
    return hits[0]


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("\n".join(model_names()))
    else:
        for d in load_model(sys.argv[1]):
            print(f"0x{d.registry:02X}  {d.offset:3}  {d.conv:4}  {d.size}  {d.label}")
