import json, os

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")


def _load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


_DIPLOMACY = _load_json(os.path.join(_BASE, "diplomacy.json"))
_PUPPET_CFG = _DIPLOMACY.get("puppet_rules", _DIPLOMACY.get("puppet", {}))

AUTONOMY_GAIN_PER_TICK = _PUPPET_CFG.get("autonomy_gain_per_tick", 0.02)
REVOLT_THRESHOLD = _PUPPET_CFG.get("revolt_threshold", 95.0)
ANNEX_THRESHOLD = _PUPPET_CFG.get("annex_threshold", 5.0)
TRIBUTE_BASE_PCT = _PUPPET_CFG.get("tribute_base_pct", 0.25)
TRIBUTE_AUTONOMY_SCALE = _PUPPET_CFG.get("tribute_autonomy_scale", 0.005)
REDUCE_AUTONOMY_PP_COST = _PUPPET_CFG.get("reduce_autonomy_pp_cost", 50)


class PuppetState:

    def __init__(self, overlord, puppet, autonomy=50.0, active=True):
        self.overlord = overlord
        self.puppet = puppet
        self.autonomy = max(0.0, min(100.0, float(autonomy)))
        self.active = active

    def tick(self, speed_val):
        if not self.active:
            return
        self.autonomy = min(100.0, self.autonomy + AUTONOMY_GAIN_PER_TICK * speed_val)

    def check_revolt(self):
        return self.autonomy >= REVOLT_THRESHOLD

    def can_annex(self):
        return self.autonomy <= ANNEX_THRESHOLD

    def get_resource_tribute(self):
        tribute = TRIBUTE_BASE_PCT - self.autonomy * TRIBUTE_AUTONOMY_SCALE
        return max(0.0, min(1.0, tribute))

    def reduce_autonomy(self, amount, overlord_country=None):
        if overlord_country is not None:
            pp = getattr(overlord_country, 'politicalPower', 0)
            cost = REDUCE_AUTONOMY_PP_COST * (amount / 10.0)
            if pp < cost:
                return False
            overlord_country.politicalPower = pp - cost
        self.autonomy = max(0.0, self.autonomy - amount)
        return True

    def __repr__(self):
        status = "active" if self.active else "inactive"
        return (f"PuppetState({self.overlord!r} -> {self.puppet!r}, "
                f"autonomy={self.autonomy:.1f}, {status})")


def get_puppet_states(country_name, puppet_list):
    return [ps for ps in puppet_list if ps.active and ps.overlord == country_name]


def get_overlord(country_name, puppet_list):
    for ps in puppet_list:
        if ps.active and ps.puppet == country_name:
            return ps
    return None


def create_puppet(overlord_name, puppet_name, puppet_list, initial_autonomy=50.0):
    if get_overlord(puppet_name, puppet_list) is not None:
        return None
    ps = PuppetState(overlord=overlord_name, puppet=puppet_name, autonomy=initial_autonomy)
    puppet_list.append(ps)
    return ps


def dissolve_puppet(puppet_state, puppet_list):
    puppet_state.active = False
    try:
        puppet_list.remove(puppet_state)
    except ValueError:
        pass
