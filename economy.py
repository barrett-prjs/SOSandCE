from __future__ import annotations

import json
import os
import math
import game_state as gs

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")


def _load_json(path, default=None):
    if default is None:
        default = {}
    if not os.path.exists(path):
        return default.copy()
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


RESOURCE_DEFS = _load_json(os.path.join(_BASE, "resources.json"))
RESOURCE_NAMES = list(RESOURCE_DEFS.keys())


class ResourceManager:

    def __init__(self):
        self.production = {r: 0 for r in RESOURCE_NAMES}
        self.stockpile = {r: 0 for r in RESOURCE_NAMES}
        self.consumption = {r: 0 for r in RESOURCE_NAMES}
        self.trade_imports = {r: 0 for r in RESOURCE_NAMES}
        self.trade_exports = {r: 0 for r in RESOURCE_NAMES}
        self.deficits = {r: False for r in RESOURCE_NAMES}
        self.max_stockpile = 1000

    def calculate_production(self, country_obj, regions_module):
        for r in RESOURCE_NAMES:
            self.production[r] = 0

        building_mgr = getattr(country_obj, 'buildingManager', None)

        for region_id in country_obj.regions:
            res_data = None
            if hasattr(regions_module, 'getResources'):
                res_data = regions_module.getResources(region_id)
            if not res_data:
                continue

            region_buildings = []
            if building_mgr is not None:
                region_buildings = building_mgr.get_region_buildings(region_id)

            for r_name, amount in res_data.items():
                if r_name not in self.production:
                    continue
                multiplier = 1.0
                for b in region_buildings:
                    b_type = b.get('type', '')
                    if b_type == 'mine':
                        multiplier *= 1.5
                    elif b_type == 'oil_well' and r_name == 'oil':
                        self.production[r_name] += 2
                    elif b_type == 'refinery' and r_name == 'oil':
                        multiplier *= 1.3
                self.production[r_name] += amount * multiplier

    def calculate_consumption(self, country_obj):
        for r in RESOURCE_NAMES:
            self.consumption[r] = 0

        div_count = len(country_obj.divisions) if hasattr(country_obj, 'divisions') else 0
        factory_count = getattr(country_obj, 'factories', 0)

        self.consumption['oil'] = div_count * 0.5
        self.consumption['steel'] = div_count * 0.3
        self.consumption['rubber'] = div_count * 0.1

        self.consumption['steel'] += factory_count * 0.2
        self.consumption['aluminum'] += factory_count * 0.15

        building_mgr = getattr(country_obj, 'buildingManager', None)
        arms_count = 0
        if building_mgr is not None:
            arms_count = building_mgr.get_building_count('arms_factory')
        self.consumption['tungsten'] += arms_count * 0.3
        self.consumption['chromium'] += arms_count * 0.2

    def tick(self, country_obj, regions_module, speed_val):
        rate = speed_val / 5 / 12

        self.calculate_production(country_obj, regions_module)
        self.calculate_consumption(country_obj)

        for r in RESOURCE_NAMES:
            net = (self.production[r] + self.trade_imports[r]
                   - self.consumption[r] - self.trade_exports[r])
            self.stockpile[r] += net * rate
            self.stockpile[r] = max(0, min(self.max_stockpile, self.stockpile[r]))
            self.deficits[r] = self.stockpile[r] <= 0 and net < 0

    def get_production_penalty(self):
        penalty = 1.0
        if self.deficits.get('steel', False):
            penalty *= 0.5
        if self.deficits.get('aluminum', False):
            penalty *= 0.7
        return penalty

    def get_combat_penalty(self):
        penalty = 1.0
        if self.deficits.get('oil', False):
            penalty *= 0.6
        if self.deficits.get('rubber', False):
            penalty *= 0.8
        if self.deficits.get('tungsten', False):
            penalty *= 0.85
        if self.deficits.get('chromium', False):
            penalty *= 0.85
        return penalty

    def get_supply_status(self):
        status = {}
        for r in RESOURCE_NAMES:
            net = (self.production[r] + self.trade_imports[r]
                   - self.consumption[r] - self.trade_exports[r])
            status[r] = {
                'production': round(self.production[r], 1),
                'consumption': round(self.consumption[r], 1),
                'stockpile': round(self.stockpile[r], 1),
                'net': round(net, 1),
                'deficit': self.deficits[r],
                'imports': round(self.trade_imports[r], 1),
                'exports': round(self.trade_exports[r], 1),
            }
        return status

    def set_starting_stockpile(self, region_count):
        base = max(50, region_count * 2)
        for r in RESOURCE_NAMES:
            self.stockpile[r] = min(base, self.max_stockpile)


class TradeContract:

    def __init__(self, exporter, importer, resource, amount, price_per_unit=100):
        self.exporter = exporter
        self.importer = importer
        self.resource = resource
        self.amount = amount
        self.price_per_unit = price_per_unit
        self.active = True

    def tick(self, speed_val):
        pass

    def tick_with_objects(self, exp_obj, imp_obj, speed_val):
        if not self.active:
            return

        rate = speed_val / 5 / 12

        if hasattr(exp_obj, 'atWarWith') and self.importer in exp_obj.atWarWith:
            self.active = False
            return

        exp_rm = getattr(exp_obj, 'resourceManager', None)
        imp_rm = getattr(imp_obj, 'resourceManager', None)
        if exp_rm is None or imp_rm is None:
            self.active = False
            return

        actual = min(self.amount * rate,
                     exp_rm.stockpile.get(self.resource, 0))

        if actual > 0:
            exp_rm.stockpile[self.resource] -= actual
            exp_rm.trade_exports[self.resource] = self.amount
            imp_rm.stockpile[self.resource] += actual
            imp_rm.trade_imports[self.resource] = self.amount

            payment = actual * self.price_per_unit
            imp_obj.money -= payment
            exp_obj.money += payment

    def cancel(self):
        self.active = False
        exp_obj = gs.get_country(self.exporter)
        imp_obj = gs.get_country(self.importer)
        if exp_obj and hasattr(exp_obj, 'resourceManager'):
            exp_obj.resourceManager.trade_exports[self.resource] = 0
        if imp_obj and hasattr(imp_obj, 'resourceManager'):
            imp_obj.resourceManager.trade_imports[self.resource] = 0


def get_available_trade_partners(country_name, country_list):
    partners = []
    country_obj = gs.get_country(country_name)
    if country_obj is None:
        return partners

    for c_name in country_list:
        if c_name == country_name:
            continue
        c_obj = gs.get_country(c_name)
        if c_obj is None:
            continue
        if country_name in getattr(c_obj, 'atWarWith', []):
            continue
        partners.append(c_name)

    return partners


def get_resource_rich_regions(regions_module, resource_name):
    rich = []
    if not hasattr(regions_module, 'getResources'):
        return rich
    for rid in range(1, 3717):
        res = regions_module.getResources(rid)
        if res and res.get(resource_name, 0) > 0:
            rich.append((rid, res[resource_name]))
    return sorted(rich, key=lambda x: -x[1])
