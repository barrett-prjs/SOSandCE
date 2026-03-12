from __future__ import annotations

import json
import os
import math

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")


def _load_json(path, default=None):
    if default is None:
        default = {}
    if not os.path.exists(path):
        return default.copy()
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


BUILDING_DEFS = _load_json(os.path.join(_BASE, "buildings.json"))


class BuildingManager:

    def __init__(self):
        self.buildings: dict[int, list[dict]] = {}
        self.queue: list[dict] = []

    def get_region_buildings(self, region_id: int) -> list[dict]:
        return self.buildings.get(region_id, [])

    def get_building_count(self, building_type: str) -> int:
        total = 0
        for blist in self.buildings.values():
            for b in blist:
                if b.get('type') == building_type:
                    total += 1
        return total

    def get_total_building_count(self) -> int:
        return sum(len(bl) for bl in self.buildings.values())

    def can_build(self, region_id: int, building_type: str) -> bool:
        bdef = BUILDING_DEFS.get(building_type)
        if bdef is None:
            return False

        max_allowed = bdef.get('max_per_region', 1)
        existing = sum(1 for b in self.get_region_buildings(region_id)
                       if b.get('type') == building_type)
        return existing < max_allowed

    def start_construction(self, region_id: int, building_type: str, country_obj=None) -> bool:
        if not self.can_build(region_id, building_type):
            return False

        bdef = BUILDING_DEFS.get(building_type)
        if bdef is None:
            return False

        cost = bdef.get('cost', 0)
        if country_obj is not None and hasattr(country_obj, 'money'):
            if country_obj.money < cost:
                return False
            country_obj.money -= cost

        total_days = bdef.get('days', 120)
        self.queue.append({
            'type': building_type,
            'region': region_id,
            'days_remaining': total_days,
            'total_days': total_days,
        })
        return True

    def get_construction_speed(self, country_obj) -> float:
        civ_count = self.get_building_count('civilian_factory')
        infra_bonus = self.get_effects_summary().get('construction_speed', 0)
        base = 1.0 + civ_count * 0.1 + infra_bonus
        build_speed = getattr(country_obj, 'buildSpeed', 1)
        return base * build_speed

    def tick(self, speed_val: float, production_penalty: float = 1.0, country_obj=None):
        if not self.queue:
            return []

        speed_mult = 1.0
        if country_obj is not None:
            speed_mult = self.get_construction_speed(country_obj)
        speed_mult *= production_penalty

        rate = speed_val / 5 / 12
        completed = []

        for item in self.queue:
            item['days_remaining'] -= rate * speed_mult
            if item['days_remaining'] <= 0:
                completed.append(item)

        finished = []
        for item in completed:
            self.queue.remove(item)
            self._place_building(item['region'], item['type'])
            finished.append({'type': item['type'], 'region': item['region']})

        return finished

    def _place_building(self, region_id: int, building_type: str):
        if region_id not in self.buildings:
            self.buildings[region_id] = []
        self.buildings[region_id].append({'type': building_type})

    def destroy_building(self, region_id: int, building_type: str) -> bool:
        blist = self.buildings.get(region_id)
        if not blist:
            return False
        for i, b in enumerate(blist):
            if b.get('type') == building_type:
                blist.pop(i)
                if not blist:
                    del self.buildings[region_id]
                return True
        return False

    def destroy_random_in_region(self, region_id: int) -> str | None:
        import random
        blist = self.buildings.get(region_id)
        if not blist:
            return None
        idx = random.randrange(len(blist))
        removed = blist.pop(idx)
        if not blist:
            del self.buildings[region_id]
        return removed.get('type')

    def get_effects_summary(self) -> dict[str, float]:
        totals: dict[str, float] = {}
        for blist in self.buildings.values():
            for b in blist:
                bdef = BUILDING_DEFS.get(b.get('type', ''), {})
                for effect_name, effect_val in bdef.get('effects', {}).items():
                    totals[effect_name] = totals.get(effect_name, 0) + effect_val
        return totals

    def get_region_effects(self, region_id: int) -> dict[str, float]:
        totals: dict[str, float] = {}
        for b in self.get_region_buildings(region_id):
            bdef = BUILDING_DEFS.get(b.get('type', ''), {})
            for effect_name, effect_val in bdef.get('effects', {}).items():
                totals[effect_name] = totals.get(effect_name, 0) + effect_val
        return totals

    def set_starting_buildings(self, regions: list[int], factory_count: int = 0):
        if not regions:
            return

        civ_to_place = max(1, factory_count // 2)
        arms_to_place = factory_count - civ_to_place

        idx = 0
        for _ in range(civ_to_place):
            rid = regions[idx % len(regions)]
            if self.can_build(rid, 'civilian_factory'):
                self._place_building(rid, 'civilian_factory')
            idx += 1

        for _ in range(arms_to_place):
            rid = regions[idx % len(regions)]
            if self.can_build(rid, 'arms_factory'):
                self._place_building(rid, 'arms_factory')
            idx += 1

    def transfer_region(self, region_id: int, target_manager: BuildingManager):
        blist = self.buildings.pop(region_id, [])
        if blist:
            existing = target_manager.buildings.get(region_id, [])
            existing.extend(blist)
            target_manager.buildings[region_id] = existing

    def get_queue_info(self) -> list[dict]:
        info = []
        for item in self.queue:
            bdef = BUILDING_DEFS.get(item['type'], {})
            progress = 1.0 - (item['days_remaining'] / max(item['total_days'], 1))
            info.append({
                'type': item['type'],
                'region': item['region'],
                'progress': round(min(1.0, max(0.0, progress)), 2),
                'days_remaining': max(0, math.ceil(item['days_remaining'])),
                'category': bdef.get('category', 'unknown'),
            })
        return info
