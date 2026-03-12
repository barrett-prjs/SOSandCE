import json, os, random, math

_BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "base_data")


def _load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


LEADER_NAMES = _load_json(os.path.join(_BASE, "leader_names.json"))

CULTURE_TO_REGION = {
    "Russian": "Eastern_European",
    "Ukrainian": "Eastern_European",
    "Polish": "Eastern_European",
    "Czech": "Eastern_European",
    "Serbian": "Eastern_European",
    "Romanian": "Eastern_European",
    "Hungarian": "Eastern_European",
    "Bulgarian": "Eastern_European",
    "French": "Western_European",
    "German": "Western_European",
    "British": "Western_European",
    "Dutch": "Western_European",
    "Belgian": "Western_European",
    "Austrian": "Western_European",
    "Swiss": "Western_European",
    "Italian": "Southern_European",
    "Spanish": "Southern_European",
    "Portuguese": "Southern_European",
    "Greek": "Southern_European",
    "Swedish": "Scandinavian",
    "Norwegian": "Scandinavian",
    "Danish": "Scandinavian",
    "Finnish": "Scandinavian",
    "Chinese": "East_Asian",
    "Japanese": "East_Asian",
    "Korean": "East_Asian",
    "Vietnamese": "Southeast_Asian",
    "Thai": "Southeast_Asian",
    "Indonesian": "Southeast_Asian",
    "Filipino": "Southeast_Asian",
    "Indian": "South_Asian",
    "Pakistani": "South_Asian",
    "Bengali": "South_Asian",
    "Turkish": "Middle_Eastern",
    "Persian": "Middle_Eastern",
    "Arabian": "Middle_Eastern",
    "Egyptian": "Middle_Eastern",
    "Iraqi": "Middle_Eastern",
    "Nigerian": "African",
    "Ethiopian": "African",
    "South_African": "African",
    "Congolese": "African",
    "Kenyan": "African",
    "American": "North_American",
    "Canadian": "North_American",
    "Mexican": "Latin_American",
    "Brazilian": "Latin_American",
    "Argentinian": "Latin_American",
    "Colombian": "Latin_American",
    "Chilean": "Latin_American",
    "Cuban": "Latin_American",
}

_FALLBACK_FIRST = ["Alex", "Jordan", "Morgan", "Taylor", "Casey", "Robin", "Kim", "Lee"]
_FALLBACK_LAST = ["Smith", "Jones", "Brown", "Wilson", "Clark", "Hall", "King", "Wright"]

TRAIT_POOL = [
    {"name": "Industrialist",    "economy_bonus": 0.10, "military_bonus": 0.0,  "diplomacy_bonus": 0.0,  "stability_bonus": 0.0},
    {"name": "War Hawk",         "economy_bonus": 0.0,  "military_bonus": 0.15, "diplomacy_bonus": -0.05, "stability_bonus": -0.05},
    {"name": "Diplomat",         "economy_bonus": 0.0,  "military_bonus": 0.0,  "diplomacy_bonus": 0.15, "stability_bonus": 0.05},
    {"name": "Populist",         "economy_bonus": -0.05, "military_bonus": 0.0, "diplomacy_bonus": 0.0,  "stability_bonus": 0.15},
    {"name": "Reformer",         "economy_bonus": 0.05, "military_bonus": 0.0,  "diplomacy_bonus": 0.05, "stability_bonus": 0.05},
    {"name": "Authoritarian",    "economy_bonus": 0.0,  "military_bonus": 0.10, "diplomacy_bonus": -0.10, "stability_bonus": 0.10},
    {"name": "Free Trader",      "economy_bonus": 0.15, "military_bonus": -0.05, "diplomacy_bonus": 0.05, "stability_bonus": 0.0},
    {"name": "Isolationist",     "economy_bonus": 0.05, "military_bonus": 0.05, "diplomacy_bonus": -0.15, "stability_bonus": 0.10},
    {"name": "Visionary",        "economy_bonus": 0.05, "military_bonus": 0.05, "diplomacy_bonus": 0.05, "stability_bonus": 0.0},
    {"name": "Corrupt",          "economy_bonus": -0.10, "military_bonus": 0.0, "diplomacy_bonus": 0.0,  "stability_bonus": -0.10},
]


def _get_name_pool(culture):
    region = CULTURE_TO_REGION.get(culture, None)
    if LEADER_NAMES:
        if region and region in LEADER_NAMES:
            pool = LEADER_NAMES[region]
            return pool.get("first", _FALLBACK_FIRST), pool.get("last", _FALLBACK_LAST)
        if culture in LEADER_NAMES:
            pool = LEADER_NAMES[culture]
            return pool.get("first", _FALLBACK_FIRST), pool.get("last", _FALLBACK_LAST)
    return _FALLBACK_FIRST, _FALLBACK_LAST


class Leader:
    def __init__(self, name, ideology, traits=None, age=50):
        self.name = name
        self.ideology = ideology
        self.traits = traits or {}
        self.age = age

    @property
    def economy_bonus(self):
        return self.traits.get("economy_bonus", 0.0)

    @property
    def military_bonus(self):
        return self.traits.get("military_bonus", 0.0)

    @property
    def diplomacy_bonus(self):
        return self.traits.get("diplomacy_bonus", 0.0)

    @property
    def stability_bonus(self):
        return self.traits.get("stability_bonus", 0.0)

    def get_modifier_dict(self):
        return {
            "economy_bonus": self.economy_bonus,
            "military_bonus": self.military_bonus,
            "diplomacy_bonus": self.diplomacy_bonus,
            "stability_bonus": self.stability_bonus,
        }

    def __repr__(self):
        return f"Leader({self.name!r}, ideology={self.ideology!r}, age={self.age})"


class Cabinet:
    def __init__(self, economy=None, military=None, diplomacy=None, intelligence=None):
        self.economy = economy or {"name": "Vacant", "modifier": 0.0}
        self.military = military or {"name": "Vacant", "modifier": 0.0}
        self.diplomacy = diplomacy or {"name": "Vacant", "modifier": 0.0}
        self.intelligence = intelligence or {"name": "Vacant", "modifier": 0.0}

    def get_total_modifier(self, field):
        minister = getattr(self, field, None)
        if minister:
            return minister.get("modifier", 0.0)
        return 0.0

    def get_all_modifiers(self):
        return {
            "economy": self.economy["modifier"],
            "military": self.military["modifier"],
            "diplomacy": self.diplomacy["modifier"],
            "intelligence": self.intelligence["modifier"],
        }

    @property
    def ministers(self):
        return {
            "economy": self.economy,
            "military": self.military,
            "diplomacy": self.diplomacy,
            "intelligence": self.intelligence,
        }

    def __repr__(self):
        return (f"Cabinet(economy={self.economy['name']!r}, military={self.military['name']!r}, "
                f"diplomacy={self.diplomacy['name']!r}, intelligence={self.intelligence['name']!r})")


def generate_leader(culture, ideology_name):
    firsts, lasts = _get_name_pool(culture)
    name = f"{random.choice(firsts)} {random.choice(lasts)}"
    trait = random.choice(TRAIT_POOL)
    traits = {
        "trait_name": trait["name"],
        "economy_bonus": trait["economy_bonus"],
        "military_bonus": trait["military_bonus"],
        "diplomacy_bonus": trait["diplomacy_bonus"],
        "stability_bonus": trait["stability_bonus"],
    }
    age = random.randint(35, 75)
    return Leader(name=name, ideology=ideology_name, traits=traits, age=age)


def generate_cabinet(culture):
    firsts, lasts = _get_name_pool(culture)

    def _minister():
        return {
            "name": f"{random.choice(firsts)} {random.choice(lasts)}",
            "modifier": round(random.uniform(-0.05, 0.15), 2),
        }

    return Cabinet(
        economy=_minister(),
        military=_minister(),
        diplomacy=_minister(),
        intelligence=_minister(),
    )


class ElectionSystem:
    def __init__(self, ideology_name, election_interval=1460):
        self.ideology_name = ideology_name
        self.election_interval = election_interval
        self.next_election_day = election_interval
        self.ideology_popularity = {
            "Democracy": 0.30,
            "Communism": 0.15,
            "Fascism": 0.10,
            "Monarchy": 0.15,
            "Anarchy": 0.05,
            "Theocracy": 0.05,
            "Oligarchy": 0.10,
            "Military Junta": 0.10,
        }

    def set_popularity(self, ideology, value):
        self.ideology_popularity[ideology] = max(0.0, min(1.0, value))
        self._normalize()

    def _normalize(self):
        total = sum(self.ideology_popularity.values())
        if total > 0:
            for k in self.ideology_popularity:
                self.ideology_popularity[k] /= total

    def shift_popularity(self, ideology, delta):
        if ideology not in self.ideology_popularity:
            return
        self.ideology_popularity[ideology] = max(0.0, self.ideology_popularity[ideology] + delta)
        self._normalize()

    def trigger_election(self, country_obj, current_day):
        if current_day < self.next_election_day:
            return None

        self.next_election_day = current_day + self.election_interval
        winner = max(self.ideology_popularity, key=self.ideology_popularity.get)
        old_ideology = getattr(country_obj, 'ideologyName', None)
        changed = winner != old_ideology

        culture = getattr(country_obj, 'culture', 'British')
        new_leader = generate_leader(culture, winner)

        result = {
            "type": "election",
            "title": "National Election",
            "text": f"Elections held. {'Ideology changed to ' + winner + '!' if changed else 'Ruling ideology retained.'}",
            "old_ideology": old_ideology,
            "new_ideology": winner,
            "ideology_changed": changed,
            "new_leader": new_leader,
            "popularity": dict(self.ideology_popularity),
        }
        return result


class PoliticalEventManager:
    def __init__(self):
        self.event_cooldowns = {}

    def _on_cooldown(self, event_type, current_day, cooldown=180):
        last = self.event_cooldowns.get(event_type, -999)
        return (current_day - last) < cooldown

    def _set_cooldown(self, event_type, current_day):
        self.event_cooldowns[event_type] = current_day

    def check_leader_death(self, country_obj, current_day):
        leader = getattr(country_obj, 'leader', None)
        if leader is None:
            return None
        if self._on_cooldown("leader_death", current_day, cooldown=365):
            return None
        age = getattr(leader, 'age', 50)
        chance = max(0.0, (age - 55) * 0.002)
        if random.random() < chance:
            self._set_cooldown("leader_death", current_day)
            culture = getattr(country_obj, 'culture', 'British')
            ideology = getattr(leader, 'ideology', 'Democracy')
            new_leader = generate_leader(culture, ideology)
            return {
                "type": "leader_death",
                "title": "Leader Has Died",
                "text": f"{leader.name} has passed away at age {age}. {new_leader.name} rises to power.",
                "effects": ["new_leader", "stability -10"],
                "old_leader": leader,
                "new_leader": new_leader,
            }
        return None

    def check_scandal(self, country_obj, current_day):
        if self._on_cooldown("scandal", current_day):
            return None
        stability = getattr(country_obj, 'stability', 50)
        chance = max(0.0, (50 - stability) * 0.003)
        if random.random() < chance:
            self._set_cooldown("scandal", current_day)
            severity = random.choice(["minor", "major"])
            stab_hit = -5 if severity == "minor" else -15
            pp_hit = -10 if severity == "minor" else -30
            return {
                "type": "scandal",
                "title": f"{'Minor' if severity == 'minor' else 'Major'} Political Scandal",
                "text": f"A {severity} scandal has rocked the government.",
                "effects": [f"stability {stab_hit}", f"political_power {pp_hit}"],
                "stability_change": stab_hit,
                "pp_change": pp_hit,
            }
        return None

    def check_economic_shock(self, country_obj, current_day):
        if self._on_cooldown("economic_shock", current_day, cooldown=360):
            return None
        money = getattr(country_obj, 'money', 1000)
        chance = max(0.0, 0.005 - money * 0.000002)
        if random.random() < chance:
            self._set_cooldown("economic_shock", current_day)
            loss_pct = random.randint(5, 20)
            return {
                "type": "economic_shock",
                "title": "Economic Shock",
                "text": f"An economic downturn has struck. Treasury reduced by {loss_pct}%.",
                "effects": [f"money -{loss_pct}%", "stability -5"],
                "money_loss_pct": loss_pct,
                "stability_change": -5,
            }
        return None

    def check_military_incident(self, country_obj, current_day):
        if self._on_cooldown("military_incident", current_day, cooldown=270):
            return None
        at_war = getattr(country_obj, 'atWarWith', [])
        base_chance = 0.003 if at_war else 0.001
        if random.random() < base_chance:
            self._set_cooldown("military_incident", current_day)
            if at_war:
                return {
                    "type": "military_incident",
                    "title": "Frontline Incident",
                    "text": "A costly incident on the front lines has shaken morale.",
                    "effects": ["stability -8", "manpower -500"],
                    "stability_change": -8,
                    "manpower_change": -500,
                }
            return {
                "type": "military_incident",
                "title": "Military Unrest",
                "text": "Sections of the military are voicing dissatisfaction.",
                "effects": ["stability -5", "political_power -10"],
                "stability_change": -5,
                "pp_change": -10,
            }
        return None

    def tick(self, country_obj, current_day):
        events = []
        for checker in [self.check_leader_death, self.check_scandal,
                        self.check_economic_shock, self.check_military_incident]:
            result = checker(country_obj, current_day)
            if result is not None:
                events.append(result)
        return events

    def check_events(self, country_obj, current_day):
        events = self.tick(country_obj, current_day)
        return events[0] if events else None
