import json, os, math

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")

def _load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

COMBAT_CONFIG = _load_json(os.path.join(_BASE, "combat_stats.json"))
BASE_STATS = COMBAT_CONFIG.get("base_stats", {"attack": 10, "defense": 10, "armor": 0, "piercing": 0, "speed": 5, "fuel_use": 1, "supply_use": 2})
SCALING = COMBAT_CONFIG.get("scaling", {"attack_per_stack": 2, "defense_per_stack": 2, "armor_per_factory": 0.5, "piercing_per_factory": 0.5})
MODIFIERS = COMBAT_CONFIG.get("modifiers", {"armor_piercing_bonus": 0.5, "fuel_penalty": 0.4, "supply_penalty": 0.6, "encirclement_bonus": 1.5, "speed_retreat_bonus": 0.1, "speed_flank_bonus": 0.05})


class CombatStats:
    def __init__(self, division_stack=1, arms_factory_count=0):
        self.attack = BASE_STATS["attack"] + division_stack * SCALING["attack_per_stack"]
        self.defense = BASE_STATS["defense"] + division_stack * SCALING["defense_per_stack"]
        self.armor = BASE_STATS["armor"] + arms_factory_count * SCALING["armor_per_factory"]
        self.piercing = BASE_STATS["piercing"] + arms_factory_count * SCALING["piercing_per_factory"]
        self.speed = BASE_STATS["speed"]
        self.fuel_use = BASE_STATS["fuel_use"]
        self.supply_use = BASE_STATS["supply_use"]

    def recalculate(self, division_stack, arms_factory_count):
        self.attack = BASE_STATS["attack"] + division_stack * SCALING["attack_per_stack"]
        self.defense = BASE_STATS["defense"] + division_stack * SCALING["defense_per_stack"]
        self.armor = BASE_STATS["armor"] + arms_factory_count * SCALING["armor_per_factory"]
        self.piercing = BASE_STATS["piercing"] + arms_factory_count * SCALING["piercing_per_factory"]

    def get_stat_dict(self):
        return {
            "attack": round(self.attack, 1),
            "defense": round(self.defense, 1),
            "armor": round(self.armor, 1),
            "piercing": round(self.piercing, 1),
            "speed": round(self.speed, 1),
            "fuel_use": round(self.fuel_use, 1),
            "supply_use": round(self.supply_use, 1),
        }


def resolve_combat_tick(attacker_div, defender_div, attacker_country, defender_country,
                        attacker_biome, defender_biome, speed_val):
    a_stats = getattr(attacker_div, 'combat_stats', None)
    d_stats = getattr(defender_div, 'combat_stats', None)

    if a_stats is None:
        a_attack = getattr(attacker_div, 'attack', 10)
        a_piercing = 0
        a_speed = 5
    else:
        a_attack = a_stats.attack
        a_piercing = a_stats.piercing
        a_speed = a_stats.speed

    if d_stats is None:
        d_defense = getattr(defender_div, 'defense', 10)
        d_armor = 0
        d_speed = 5
    else:
        d_defense = d_stats.defense
        d_armor = d_stats.armor
        d_speed = d_stats.speed

    if a_piercing >= d_armor:
        atk_damage_mult = 1.0
    else:
        atk_damage_mult = MODIFIERS["armor_piercing_bonus"]

    d_piercing = getattr(d_stats, 'piercing', 0) if d_stats else 0
    a_armor = getattr(a_stats, 'armor', 0) if a_stats else 0
    if d_piercing >= a_armor:
        def_damage_mult = 1.0
    else:
        def_damage_mult = MODIFIERS["armor_piercing_bonus"]

    a_combat_penalty = 1.0
    d_combat_penalty = 1.0
    if hasattr(attacker_country, 'resourceManager'):
        a_combat_penalty = attacker_country.resourceManager.get_combat_penalty()
    if hasattr(defender_country, 'resourceManager'):
        d_combat_penalty = defender_country.resourceManager.get_combat_penalty()

    atk_biome_mult = attacker_biome[1] if attacker_biome else 1.0
    def_biome_mult = defender_biome[2] if defender_biome else 1.0

    atk_mult = getattr(attacker_country, 'attackMultiplier', 1.0)
    def_mult = getattr(defender_country, 'defenseMultiplier', 1.0)

    speed_diff = a_speed - d_speed
    flank_bonus = 1.0 + max(0, speed_diff) * MODIFIERS["speed_flank_bonus"]

    attacker_resource_loss = (d_defense * def_biome_mult * speed_val / 25
                              * attacker_div.divisionStack * def_damage_mult
                              * d_combat_penalty * def_mult)

    defender_resource_loss = (a_attack * atk_biome_mult * speed_val / 25
                              * defender_div.divisionStack * atk_damage_mult
                              * a_combat_penalty * atk_mult * flank_bonus)

    return attacker_resource_loss, defender_resource_loss


def calculate_unit_losses(winner_div, loser_div, is_attacker_win):
    a_stats = getattr(winner_div, 'combat_stats', None)
    l_stats = getattr(loser_div, 'combat_stats', None)

    w_attack = a_stats.attack if a_stats else getattr(winner_div, 'attack', 10)
    l_defense = l_stats.defense if l_stats else getattr(loser_div, 'defense', 10)

    if is_attacker_win:
        winner_loss = l_defense * 5 * loser_div.divisionStack
        loser_loss = w_attack * 20 * winner_div.divisionStack
    else:
        winner_loss = w_attack * 5 * winner_div.divisionStack
        loser_loss = l_defense * 20 * loser_div.divisionStack

    return winner_loss, loser_loss


def get_retreat_chance(div, combat_stats=None):
    speed = combat_stats.speed if combat_stats else 5
    return min(0.9, 0.5 + speed * MODIFIERS["speed_retreat_bonus"])
