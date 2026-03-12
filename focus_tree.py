import json
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TREE_DIR = os.path.join(BASE_DIR, "base_data", "tech_trees")
COUNTRY_DIR = os.path.join(TREE_DIR, "countries")
GLOBAL_FILE = os.path.join(TREE_DIR, "global.json")


def _normalize_node(name, raw):
    if isinstance(raw, list):
        return {
            "x": raw[0],
            "y": raw[1],
            "cost": raw[2],
            "prerequisites": raw[3],
            "condition": raw[4],
            "effects": raw[5],
            "days": raw[6],
            "description": raw[7],
            "requirement_desc": raw[8],
            "exclusive_group": None,
        }
    return raw


def _node_to_old_format(node):
    return [
        node["x"],
        node["y"],
        node["cost"],
        node["prerequisites"],
        node["condition"],
        node["effects"],
        node["days"],
        node["description"],
        node["requirement_desc"],
        False,
    ]


class FocusTreeLoader:

    def load_tree(self, country_name):
        tree = {}

        if os.path.isfile(GLOBAL_FILE):
            with open(GLOBAL_FILE, "r", encoding="utf-8") as f:
                raw = json.load(f)
            for name, data in raw.items():
                tree[name] = _normalize_node(name, data)

        country_file = os.path.join(COUNTRY_DIR, f"{country_name}.json")
        if os.path.isfile(country_file):
            with open(country_file, "r", encoding="utf-8") as f:
                raw = json.load(f)
            for name, data in raw.items():
                tree[name] = _normalize_node(name, data)

        return tree


class FocusTreeEngine:

    def __init__(self, tree_data):
        self.tree = tree_data
        self.completed_focuses = set()
        self.locked_groups = set()

    def is_available(self, focus_name, country_obj):
        node = self.tree.get(focus_name)
        if node is None:
            return False

        if focus_name in self.completed_focuses:
            return False

        group = node.get("exclusive_group")
        if group and group in self.locked_groups:
            return False

        for prereq in node.get("prerequisites", []):
            if prereq not in self.completed_focuses:
                return False

        try:
            if not eval(node["condition"], {"self": country_obj, "__builtins__": {}}):
                return False
        except Exception:
            return False

        if hasattr(country_obj, "politicalPower") and country_obj.politicalPower < node["cost"]:
            return False

        return True

    def complete_focus(self, focus_name, country_obj):
        node = self.tree.get(focus_name)
        if node is None:
            return

        self.completed_focuses.add(focus_name)

        group = node.get("exclusive_group")
        if group:
            self.locked_groups.add(group)

        for effect in node.get("effects", []):
            try:
                exec(effect, {"self": country_obj, "__builtins__": {}})
            except Exception:
                pass

    def get_available_focuses(self, country_obj):
        return [
            (name, node)
            for name, node in self.tree.items()
            if self.is_available(name, country_obj)
        ]

    def get_focus_state(self, focus_name):
        if focus_name in self.completed_focuses:
            return "completed"

        node = self.tree.get(focus_name)
        if node is None:
            return "unavailable"

        group = node.get("exclusive_group")
        if group and group in self.locked_groups:
            return "locked"

        for prereq in node.get("prerequisites", []):
            if prereq not in self.completed_focuses:
                return "unavailable"

        return "available"

    def start_focus(self, focus_name, country_obj):
        node = self.tree.get(focus_name)
        if node is None:
            return
        country_obj.focus = [focus_name, node["days"], node["effects"]]


def create_decision_tree(country_name):
    loader = FocusTreeLoader()
    tree = loader.load_tree(country_name)
    return {name: _node_to_old_format(node) for name, node in tree.items()}
