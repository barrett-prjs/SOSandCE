from __future__ import annotations

import json
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
BASE_DATA_DIR = _SCRIPT_DIR / "base_data"


def _load_json(path: Path, default: dict):
    if not path.exists():
        return default.copy()
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def get_country_records():
    path = BASE_DATA_DIR / "countries.json"
    return _load_json(path, {})


def get_region_payload():
    return {
        "regions": _load_json(BASE_DATA_DIR / "regions.json", {}),
        "cities": _load_json(BASE_DATA_DIR / "cities.json", {}),
        "world_regions": _load_json(BASE_DATA_DIR / "world_regions.json", {}),
        "biomes": _load_json(BASE_DATA_DIR / "biomes.json", {}),
    }


def get_resources_defs():
    return _load_json(BASE_DATA_DIR / "resources.json", {})


def get_buildings_defs():
    return _load_json(BASE_DATA_DIR / "buildings.json", {})


def get_combat_config():
    return _load_json(BASE_DATA_DIR / "combat_stats.json", {})


def get_diplomacy_config():
    return _load_json(BASE_DATA_DIR / "diplomacy.json", {})


def get_ai_profiles():
    return _load_json(BASE_DATA_DIR / "ai_profiles.json", {})


def get_leader_names():
    return _load_json(BASE_DATA_DIR / "leader_names.json", {})


def get_ideologies():
    return _load_json(BASE_DATA_DIR / "ideologies.json", {})


def get_balance_config():
    return _load_json(BASE_DATA_DIR / "balance.json", {})


def get_focus_tree(country_name: str):
    path = BASE_DATA_DIR / "tech_trees" / "countries" / f"{country_name}.json"
    if not path.exists():
        return None
    return _load_json(path, {})


def get_global_focus_tree():
    return _load_json(BASE_DATA_DIR / "tech_trees" / "global.json", {})
