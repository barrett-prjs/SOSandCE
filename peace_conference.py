import json, os, math, random
import game_state as gs

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")

def _load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

DIPLOMACY_CONFIG = _load_json(os.path.join(_BASE, "diplomacy.json"))
PEACE_ACTIONS = DIPLOMACY_CONFIG.get("peace_actions", {})
AI_PREFS = DIPLOMACY_CONFIG.get("ai_preferences", {})


class PeaceConference:

    def __init__(self, victors, losers, war_scores):
        self.victors = sorted(victors, key=lambda v: war_scores.get(v, 0), reverse=True)
        self.losers = list(losers)
        self.war_scores = dict(war_scores)
        self.peace_points = {v: max(10, war_scores.get(v, 0)) for v in self.victors}
        self.current_turn_index = 0
        self.selected_provinces = []
        self.actions_taken = []
        self.available_provinces = []
        self.finished = False
        self.round_number = 1
        self.passes_this_round = 0

        self._collect_available_provinces()

    def _collect_available_provinces(self):
        self.available_provinces = []
        for loser in self.losers:
            loser_obj = gs.get_country(loser)
            if loser_obj and hasattr(loser_obj, 'regions'):
                for r in loser_obj.regions:
                    self.available_provinces.append({
                        'region': r,
                        'owner': loser,
                        'claimed_by': None
                    })

    def get_current_victor(self):
        if self.current_turn_index < len(self.victors):
            return self.victors[self.current_turn_index]
        return None

    def get_remaining_points(self, victor=None):
        if victor is None:
            victor = self.get_current_victor()
        return self.peace_points.get(victor, 0)

    def get_province_cost(self, region_id, regions_module):
        base = PEACE_ACTIONS.get("annex_province", {}).get("base_cost", 5)

        for loser in self.losers:
            loser_obj = gs.get_country(loser)
            if loser_obj and hasattr(loser_obj, 'capital'):
                city = regions_module.getCity(region_id)
                if city and city == loser_obj.capital:
                    base *= PEACE_ACTIONS.get("annex_province", {}).get("capital_multiplier", 3)
                    break

        if regions_module.getCity(region_id):
            base *= PEACE_ACTIONS.get("annex_province", {}).get("factory_multiplier", 1.5)

        return max(1, int(base))

    def select_province(self, region_id):
        if region_id in self.selected_provinces:
            self.selected_provinces.remove(region_id)
        else:
            for p in self.available_provinces:
                if p['region'] == region_id and p['claimed_by'] is None:
                    self.selected_provinces.append(region_id)
                    break

    def get_selection_cost(self, regions_module):
        total = 0
        for rid in self.selected_provinces:
            total += self.get_province_cost(rid, regions_module)
        return total

    def annex_selected(self, regions_module):
        victor = self.get_current_victor()
        if not victor:
            return False

        cost = self.get_selection_cost(regions_module)
        if cost > self.peace_points.get(victor, 0):
            return False

        self.peace_points[victor] -= cost

        for rid in self.selected_provinces:
            for p in self.available_provinces:
                if p['region'] == rid:
                    p['claimed_by'] = victor
                    break
            self.actions_taken.append({
                'type': 'annex',
                'victor': victor,
                'region': rid,
                'cost': self.get_province_cost(rid, regions_module)
            })

        self.selected_provinces = []
        return True

    def puppet_country(self, loser_name):
        victor = self.get_current_victor()
        if not victor:
            return False

        puppet_cost = PEACE_ACTIONS.get("puppet", {})
        base = puppet_cost.get("base_cost", 50)
        loser_obj = gs.get_country(loser_name)
        if loser_obj:
            base += len(loser_obj.regions) * puppet_cost.get("per_province", 2)

        if base > self.peace_points.get(victor, 0):
            return False

        self.peace_points[victor] -= base

        for p in self.available_provinces:
            if p['owner'] == loser_name and p['claimed_by'] is None:
                p['claimed_by'] = f"puppet:{victor}"

        self.actions_taken.append({
            'type': 'puppet',
            'victor': victor,
            'target': loser_name,
            'cost': base
        })
        return True

    def liberate_country(self, culture, loser_name, countries_module):
        victor = self.get_current_victor()
        if not victor:
            return False

        lib_cost = PEACE_ACTIONS.get("liberate", {}).get("base_cost", 30)

        if lib_cost > self.peace_points.get(victor, 0):
            return False

        self.peace_points[victor] -= lib_cost
        self.actions_taken.append({
            'type': 'liberate',
            'victor': victor,
            'culture': culture,
            'from': loser_name,
            'cost': lib_cost
        })
        return True

    def install_government(self, loser_name):
        victor = self.get_current_victor()
        if not victor:
            return False

        cost = PEACE_ACTIONS.get("install_government", {}).get("base_cost", 40)

        if cost > self.peace_points.get(victor, 0):
            return False

        self.peace_points[victor] -= cost
        self.actions_taken.append({
            'type': 'install_government',
            'victor': victor,
            'target': loser_name,
            'cost': cost
        })
        return True

    def pass_turn(self):
        self.passes_this_round += 1
        self.advance_turn()

    def advance_turn(self):
        self.current_turn_index += 1
        if self.current_turn_index >= len(self.victors):
            self.current_turn_index = 0
            self.round_number += 1
            if self.passes_this_round >= len(self.victors):
                self.finished = True
            self.passes_this_round = 0

        unclaimed = [p for p in self.available_provinces if p['claimed_by'] is None]
        if not unclaimed:
            self.finished = True

    def ai_take_turn(self, regions_module, countries_module):
        victor = self.get_current_victor()
        if not victor:
            self.pass_turn()
            return

        victor_obj = gs.get_country(victor)
        if not victor_obj:
            self.pass_turn()
            return

        points = self.peace_points.get(victor, 0)
        if points <= 0:
            self.pass_turn()
            return

        unclaimed = [p for p in self.available_provinces
                     if p['claimed_by'] is None]
        if not unclaimed:
            self.pass_turn()
            return

        scored = []
        for p in unclaimed:
            rid = p['region']
            cost = self.get_province_cost(rid, regions_module)
            if cost > points:
                continue

            score = 0

            if hasattr(victor_obj, 'coreRegions') and rid in victor_obj.coreRegions:
                score += AI_PREFS.get("homeland_weight", 10)

            connections = regions_module.getConnections(rid)
            for conn in connections:
                if regions_module.getOwner(conn) == victor:
                    score += AI_PREFS.get("contiguous_weight", 5)
                    break

            culture = getattr(victor_obj, 'culture', None)
            if culture:
                city = regions_module.getCity(rid)
                if city:
                    city_culture = regions_module.getCityCulture(city)
                    if city_culture == culture:
                        score += AI_PREFS.get("culture_weight", 3)

            if regions_module.getCity(rid):
                score += AI_PREFS.get("factory_weight", 2)

            if score > 0:
                scored.append((rid, score, cost))

        if not scored:
            self.pass_turn()
            return

        scored.sort(key=lambda x: -x[1])

        picked = []
        remaining = points
        for rid, score, cost in scored:
            if cost <= remaining:
                picked.append(rid)
                remaining -= cost
                if len(picked) >= 5:
                    break

        if not picked:
            self.pass_turn()
            return

        self.selected_provinces = picked
        self.annex_selected(regions_module)
        self.advance_turn()

    def execute_results(self, regions_module, countries_module, country_list,
                        faction_list, puppet_list):
        from puppet import create_puppet

        for action in self.actions_taken:
            if action['type'] == 'annex':
                victor = action['victor']
                rid = action['region']
                victor_obj = gs.get_country(victor)
                if victor_obj and hasattr(victor_obj, 'addRegion'):
                    victor_obj.addRegion(rid)

            elif action['type'] == 'puppet':
                victor = action['victor']
                target = action['target']
                create_puppet(victor, target, puppet_list)
                target_obj = gs.get_country(target)
                victor_obj = gs.get_country(victor)
                if target_obj and victor_obj:
                    target_obj.puppetTo = victor
                    if hasattr(victor_obj, 'ideology'):
                        target_obj.ideology = list(victor_obj.ideology)

            elif action['type'] == 'install_government':
                victor = action['victor']
                target = action['target']
                target_obj = gs.get_country(target)
                victor_obj = gs.get_country(victor)
                if target_obj and victor_obj and hasattr(victor_obj, 'ideology'):
                    target_obj.ideology = list(victor_obj.ideology)
                    if hasattr(victor_obj, 'ideologyName'):
                        target_obj.ideologyName = victor_obj.ideologyName
