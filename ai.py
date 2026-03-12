import json, os, random, math
import game_state as gs

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")

def _load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

AI_PROFILES = _load_json(os.path.join(_BASE, "ai_profiles.json"))


class AIController:

    def __init__(self, country_name, profile_name=None):
        self.country_name = country_name
        self.profile_name = profile_name or self._determine_profile()
        self.profile = AI_PROFILES.get(self.profile_name, AI_PROFILES.get("defensive", {}))
        self.last_trade_check = 0
        self.last_build_check = 0
        self.last_focus_check = 0
        self.threat_assessment = {}

    def _determine_profile(self):
        return "balanced"

    def assess_threats(self, country_list):
        country = gs.get_country(self.country_name)
        if not country:
            return

        self.threat_assessment = {}
        bordering = getattr(country, 'bordering', [])

        for neighbor in bordering:
            if neighbor == self.country_name:
                continue
            n_obj = gs.get_country(neighbor)
            if not n_obj:
                continue

            threat = 0

            if self.country_name in getattr(n_obj, 'atWarWith', []):
                threat += 100

            n_divs = len(getattr(n_obj, 'divisions', []))
            my_divs = len(getattr(country, 'divisions', []))
            if my_divs > 0:
                threat += max(0, (n_divs - my_divs) / my_divs * 30)

            n_ideology = getattr(n_obj, 'ideologyName', '')
            my_ideology = getattr(country, 'ideologyName', '')
            if n_ideology != my_ideology:
                threat += 10

            self.threat_assessment[neighbor] = threat

    def decide_build(self, country, building_manager):
        priority = self.profile.get("build_priority", "balanced")

        build_order = {
            "military": ["arms_factory", "infrastructure", "mine", "civilian_factory"],
            "civilian": ["civilian_factory", "infrastructure", "mine", "arms_factory"],
            "infrastructure": ["infrastructure", "civilian_factory", "mine", "arms_factory"],
            "naval": ["dockyard", "naval_base", "arms_factory", "civilian_factory"],
            "balanced": ["civilian_factory", "arms_factory", "infrastructure", "mine"],
        }

        order = build_order.get(priority, build_order["balanced"])

        if len(building_manager.queue) >= max(1, building_manager.get_building_count("civilian_factory")):
            return None

        for building_type in order:
            for region_id in country.regions:
                if building_manager.can_build(region_id, building_type):
                    return (building_type, region_id)

        return None

    def decide_trade(self, country, trade_contracts, country_list):
        if not hasattr(country, 'resourceManager'):
            return []

        needed = []
        rm = country.resourceManager
        for resource, is_deficit in rm.deficits.items():
            if is_deficit or rm.stockpile.get(resource, 0) < 50:
                needed.append(resource)

        proposals = []
        for resource in needed:
            best_partner = None
            best_surplus = 0

            for c_name in country_list:
                if c_name == self.country_name:
                    continue
                c_obj = gs.get_country(c_name)
                if not c_obj or not hasattr(c_obj, 'resourceManager'):
                    continue
                if self.country_name in getattr(c_obj, 'atWarWith', []):
                    continue

                surplus = (c_obj.resourceManager.production.get(resource, 0)
                          - c_obj.resourceManager.consumption.get(resource, 0))

                if surplus > best_surplus:
                    ideology_match = (getattr(c_obj, 'ideologyName', '') ==
                                    getattr(country, 'ideologyName', ''))
                    if ideology_match:
                        surplus *= 1.5
                    best_partner = c_name
                    best_surplus = surplus

            if best_partner and best_surplus > 0:
                amount = min(best_surplus * 0.5, 5)
                proposals.append((best_partner, resource, amount))

        return proposals

    def decide_war(self, country, country_list):
        if getattr(country, 'atWarWith', []):
            return None

        willingness = self.profile.get("war_willingness", 0.3)

        if random.random() > willingness * 0.01:
            return None

        self.assess_threats(country_list)

        my_strength = sum(d.divisionStack for d in getattr(country, 'divisions', []))

        for neighbor, threat in sorted(self.threat_assessment.items(), key=lambda x: -x[1]):
            if threat < 20:
                continue

            n_obj = gs.get_country(neighbor)
            if not n_obj:
                continue

            n_strength = sum(d.divisionStack for d in getattr(n_obj, 'divisions', []))

            if my_strength > n_strength * 1.5:
                core = getattr(country, 'coreRegions', [])
                n_regions = getattr(n_obj, 'regions', [])
                has_claims = any(r in core for r in n_regions)

                if has_claims or willingness > 0.6:
                    return neighbor

        return None

    def decide_focus(self, country, focus_tree_engine):
        if not focus_tree_engine:
            return None

        if getattr(country, 'focus', None) is not None:
            return None

        available = focus_tree_engine.get_available_focuses(country)
        if not available:
            return None

        preferences = self.profile.get("focus_preference", ["industry", "military", "political"])

        scored = []
        for name, node in available:
            score = 0
            name_lower = name.lower()

            for i, pref in enumerate(preferences):
                if pref in name_lower:
                    score += (len(preferences) - i) * 10

            if 'industry' in name_lower or 'factory' in name_lower or 'infrastructure' in name_lower:
                if 'industry' in preferences[:2]:
                    score += 5
            elif 'military' in name_lower or 'army' in name_lower or 'defense' in name_lower:
                if 'military' in preferences[:2]:
                    score += 5
            elif 'political' in name_lower or 'ideology' in name_lower:
                if 'political' in preferences[:2]:
                    score += 5

            cost = node.get('cost', 50)
            if cost <= getattr(country, 'politicalPower', 0):
                score += 3

            scored.append((name, node, score))

        if not scored:
            return None

        scored.sort(key=lambda x: -x[2])

        top = scored[:3]
        chosen = random.choice(top)
        return chosen[0]

    def assign_divisions_to_front(self, country):
        if not getattr(country, 'atWarWith', []):
            return

        border = getattr(country, 'battleBorder', [])
        if not border:
            return

        available = [d for d in country.divisions
                     if not d.fighting and d.commands == [] and not d.locked]

        if not available:
            return

        self.assess_threats(getattr(country, 'atWarWith', []))

        divs_per_region = max(1, len(available) // max(1, len(border)))

        assigned = 0
        for region in border:
            for _ in range(divs_per_region):
                if assigned >= len(available):
                    break
                div = available[assigned]
                if div.region != region:
                    div.command(region, False, ignoreWater=False, iterations=300)
                assigned += 1
