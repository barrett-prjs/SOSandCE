#include "game/effect_system.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/faction.h"
#include "game/puppet.h"
#include "game/helpers.h"

#include <regex>


EffectSystem::EffectSystem() = default;


void EffectSystem::execute(const Effect& effect, GameState& gs) {
    const std::string& key = effect.key;
    const std::string& target = effect.targetName;


    if (key == "add_money" || key == "money") {
        float amount = 0.0f;
        if (!effect.args.empty()) {
            if (auto* f = std::get_if<float>(&effect.args[0])) amount = *f;
            else if (auto* i = std::get_if<int>(&effect.args[0])) amount = static_cast<float>(*i);
        }
        addMoney(target, amount, gs);
    }
    else if (key == "add_political_power" || key == "politicalPower") {
        float amount = 0.0f;
        if (!effect.args.empty()) {
            if (auto* f = std::get_if<float>(&effect.args[0])) amount = *f;
            else if (auto* i = std::get_if<int>(&effect.args[0])) amount = static_cast<float>(*i);
        }
        addPoliticalPower(target, amount, gs);
    }
    else if (key == "add_stability" || key == "stability") {
        float amount = 0.0f;
        if (!effect.args.empty()) {
            if (auto* f = std::get_if<float>(&effect.args[0])) amount = *f;
            else if (auto* i = std::get_if<int>(&effect.args[0])) amount = static_cast<float>(*i);
        }
        addStability(target, amount, gs);
    }
    else if (key == "add_manpower" || key == "manpower") {
        float amount = 0.0f;
        if (!effect.args.empty()) {
            if (auto* f = std::get_if<float>(&effect.args[0])) amount = *f;
            else if (auto* i = std::get_if<int>(&effect.args[0])) amount = static_cast<float>(*i);
        }
        addManpower(target, amount, gs);
    }
    else if (key == "set_ideology" || key == "ideology") {
        float eco = 0.0f, soc = 0.0f;
        if (effect.args.size() >= 2) {
            if (auto* f = std::get_if<float>(&effect.args[0])) eco = *f;
            if (auto* f = std::get_if<float>(&effect.args[1])) soc = *f;
        }
        setIdeology(target, eco, soc, gs);
    }
    else if (key == "declare_war") {
        std::string defender;
        if (!effect.args.empty()) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) defender = *s;
        }
        declareWar(target, defender, gs);
    }
    else if (key == "make_peace") {
        std::string other;
        if (!effect.args.empty()) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) other = *s;
        }
        makePeace(target, other, gs);
    }
    else if (key == "create_faction") {
        std::string factionName;
        if (!effect.args.empty()) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) factionName = *s;
        }
        createFaction(factionName, target, gs);
    }
    else if (key == "add_to_faction") {
        std::string factionName;
        if (!effect.args.empty()) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) factionName = *s;
        }
        addToFaction(factionName, target, gs);
    }
    else if (key == "puppet") {
        std::string puppetName;
        if (!effect.args.empty()) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) puppetName = *s;
        }
        puppet(target, puppetName, gs);
    }
    else if (key == "annex") {
        std::string annexTarget;
        if (!effect.args.empty()) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) annexTarget = *s;
        }
        annex(target, annexTarget, gs);
    }
    else if (key == "add_factory" || key == "addFactory") {
        int region = -1;
        if (!effect.args.empty()) {
            if (auto* i = std::get_if<int>(&effect.args[0])) region = *i;
        }
        addFactory(target, region, gs);
    }
    else if (key == "add_modifier") {
        std::string modifier;
        float value = 0.0f;
        if (effect.args.size() >= 2) {
            if (auto* s = std::get_if<std::string>(&effect.args[0])) modifier = *s;
            if (auto* f = std::get_if<float>(&effect.args[1])) value = *f;
        }
        addModifier(target, modifier, value, gs);
    }
}

void EffectSystem::executeBatch(const std::vector<Effect>& effects, GameState& gs) {
    for (auto& e : effects) {
        execute(e, gs);
    }
}


std::vector<Effect> EffectSystem::parse(const std::vector<std::string>& effectStrings,
                                         const std::string& defaultTarget) {
    std::vector<Effect> effects;

    for (auto& str : effectStrings) {
        Effect e;
        e.target = EffectTarget::Country;
        e.targetName = defaultTarget;


        std::regex assignRegex(R"(self\.(\w+)\s*([+\-*]?=)\s*(.+))");
        std::smatch match;

        if (std::regex_match(str, match, assignRegex)) {
            std::string field = match[1].str();
            std::string op = match[2].str();
            std::string valueStr = match[3].str();


            if (field == "politicalPower") {
                e.key = "add_political_power";
            } else if (field == "money") {
                e.key = "add_money";
            } else if (field == "baseStability" || field == "stability") {
                e.key = "add_stability";
            } else if (field == "manPower" || field == "manpower") {
                e.key = "add_manpower";
            } else if (field == "politicalMultiplier") {
                e.key = "add_modifier";
                e.args.push_back(std::string("political_multiplier"));
            } else if (field == "moneyMultiplier") {
                e.key = "add_modifier";
                e.args.push_back(std::string("money_multiplier"));
            } else if (field == "buildSpeed") {
                e.key = "add_modifier";
                e.args.push_back(std::string("build_speed"));
            } else if (field == "defenseMultiplier") {
                e.key = "add_modifier";
                e.args.push_back(std::string("defense_multiplier"));
            } else if (field == "attackMultiplier") {
                e.key = "add_modifier";
                e.args.push_back(std::string("attack_multiplier"));
            } else if (field == "transportMultiplier") {
                e.key = "add_modifier";
                e.args.push_back(std::string("transport_multiplier"));
            } else if (field == "canMakeFaction") {
                e.key = "add_modifier";
                e.args.push_back(std::string("can_make_faction"));
            } else if (field == "expandedInvitations") {
                e.key = "add_modifier";
                e.args.push_back(std::string("expanded_invitations"));
            } else if (field == "militarySize") {
                e.key = "add_modifier";
                e.args.push_back(std::string("military_size"));
            } else if (field == "hasChangedCountry") {
                e.key = "add_modifier";
                e.args.push_back(std::string("has_changed_country"));
            } else if (field == "puppetTo") {
                e.key = "add_modifier";
                e.args.push_back(std::string("puppet_to"));
            } else {
                e.key = "add_modifier";
                e.args.push_back(field);
            }


            try {

                std::string cleanValue = replaceAll(valueStr, "_", "");

                while (!cleanValue.empty() && (cleanValue.front() == ' ' || cleanValue.front() == '\''))
                    cleanValue.erase(cleanValue.begin());
                while (!cleanValue.empty() && (cleanValue.back() == ' ' || cleanValue.back() == '\''))
                    cleanValue.pop_back();


                if (cleanValue == "True" || cleanValue == "true") {
                    e.args.push_back(1.0f);
                } else if (cleanValue == "False" || cleanValue == "false") {
                    e.args.push_back(0.0f);
                } else if (cleanValue == "None" || cleanValue == "null") {
                    e.args.push_back(std::string(""));
                } else {
                    float val = std::stof(cleanValue);

                    if (op == "-=") val = -val;
                    e.args.push_back(val);
                }
            } catch (...) {

                e.args.push_back(valueStr);
            }

            effects.push_back(e);
            continue;
        }


        std::regex ideologyRegex(R"(self\.ideology\[(\d+)\]\s*([+\-]?=)\s*(.+))");
        if (std::regex_match(str, match, ideologyRegex)) {
            int index = std::stoi(match[1].str());
            std::string op = match[2].str();
            float val = std::stof(replaceAll(match[3].str(), "_", ""));
            if (op == "-=") val = -val;

            e.key = "set_ideology";
            e.args.push_back(index == 0 ? val : 0.0f);
            e.args.push_back(index == 1 ? val : 0.0f);
            effects.push_back(e);
            continue;
        }


        std::regex methodRegex(R"(self\.(\w+)\(([^)]*)\))");
        if (std::regex_match(str, match, methodRegex)) {
            std::string method = match[1].str();
            std::string argsStr = match[2].str();

            if (method == "addFactory") {
                e.key = "add_factory";
                if (!argsStr.empty()) {
                    try { e.args.push_back(std::stoi(argsStr)); } catch (...) {}
                }
            } else if (method == "addPort") {
                e.key = "add_factory";
            } else if (method == "revolution") {
                e.key = "set_ideology";

                std::string ideologyName = replaceAll(argsStr, "'", "");
                ideologyName = replaceAll(ideologyName, "\"", "");
                e.args.push_back(ideologyName);
            } else if (method == "training" || method == "trainDivision") {
                e.key = "add_modifier";
                e.args.push_back(std::string("train_division"));
                if (!argsStr.empty()) {
                    try { e.args.push_back(static_cast<float>(std::stoi(argsStr))); } catch (...) { e.args.push_back(1.0f); }
                } else {
                    e.args.push_back(1.0f);
                }
            } else {
                e.key = method;
                if (!argsStr.empty()) {
                    e.args.push_back(argsStr);
                }
            }

            effects.push_back(e);
            continue;
        }


        std::regex trainingRegex(R"(self\.training\.append\(\[(\d+),\s*(\d+)\]\))");
        if (std::regex_match(str, match, trainingRegex)) {
            int count = std::stoi(match[1].str());
            e.key = "add_modifier";
            e.args.push_back(std::string("train_division"));
            e.args.push_back(static_cast<float>(count));
            effects.push_back(e);
            continue;
        }


        e.key = "raw";
        e.args.push_back(str);
        effects.push_back(e);
    }

    return effects;
}


void EffectSystem::addMoney(const std::string& country, float amount, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) c->money += amount;
}

void EffectSystem::addPoliticalPower(const std::string& country, float amount, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) c->politicalPower += amount;
}

void EffectSystem::addStability(const std::string& country, float amount, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) {
        c->baseStability += amount;
        c->stability = c->baseStability;
    }
}

void EffectSystem::addManpower(const std::string& country, float amount, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) c->manPower += amount;
}

void EffectSystem::setIdeology(const std::string& country, float economic, float social,
                                GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) {
        c->ideology[0] += economic;
        c->ideology[1] += social;

        c->ideology[0] = std::max(-1.0f, std::min(1.0f, c->ideology[0]));
        c->ideology[1] = std::max(-1.0f, std::min(1.0f, c->ideology[1]));
        c->ideologyName = getIdeologyName(c->ideology[0], c->ideology[1]);
    }
}

void EffectSystem::addRegion(const std::string& country, int regionId, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) c->addRegion(regionId, gs);
}

void EffectSystem::removeRegion(const std::string& country, int regionId, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) c->removeRegion(regionId);
}

void EffectSystem::declareWar(const std::string& attacker, const std::string& defender,
                               GameState& gs) {
    Country* c = gs.getCountry(attacker);
    if (c) c->declareWar(defender, gs);
}

void EffectSystem::makePeace(const std::string& a, const std::string& b, GameState& gs) {
    Country* c = gs.getCountry(a);
    if (c) c->makePeace(b, gs);
}

void EffectSystem::createFaction(const std::string& name, const std::string& leader,
                                  GameState& gs) {
    auto faction = std::make_unique<Faction>(name, std::vector<std::string>{leader}, gs);
    gs.registerFaction(name, std::move(faction));

    Country* c = gs.getCountry(leader);
    if (c) {
        c->faction = name;
        c->factionLeader = true;
    }
}

void EffectSystem::addToFaction(const std::string& faction, const std::string& country,
                                 GameState& gs) {
    Faction* f = gs.getFaction(faction);
    if (f) f->addCountry(country, gs, false, false);
}

void EffectSystem::puppet(const std::string& overlord, const std::string& puppetName,
                           GameState& gs) {
    PuppetState ps;
    ps.overlord = overlord;
    ps.puppet = puppetName;
    ps.autonomy = 50.0f;
    ps.active = true;
    gs.puppetStates.push_back(ps);

    Country* puppetCountry = gs.getCountry(puppetName);
    Country* overlordCountry = gs.getCountry(overlord);
    if (puppetCountry) {
        puppetCountry->puppetTo = overlord;
    }
    if (overlordCountry && puppetCountry) {
        if (std::find(overlordCountry->militaryAccess.begin(), overlordCountry->militaryAccess.end(), puppetName) == overlordCountry->militaryAccess.end()) {
            overlordCountry->militaryAccess.push_back(puppetName);
        }
        if (std::find(puppetCountry->militaryAccess.begin(), puppetCountry->militaryAccess.end(), overlord) == puppetCountry->militaryAccess.end()) {
            puppetCountry->militaryAccess.push_back(overlord);
        }
    }
}

void EffectSystem::annex(const std::string& annexer, const std::string& target, GameState& gs) {
    Country* a = gs.getCountry(annexer);
    Country* t = gs.getCountry(target);
    if (a && t) {
        std::vector<int> regions = t->regions;
        for (int rid : regions) {
            a->addRegion(rid, gs);
        }
        if (t->regions.empty()) {
            gs.removeCountry(target);
        }
    }
}

void EffectSystem::addFactory(const std::string& country, int region, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) c->addFactory(gs, region);
}

void EffectSystem::addResource(const std::string& country, int resource, float amount,
                                GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c && resource >= 0 && resource < RESOURCE_COUNT) {
        c->resourceManager.stockpile[resource] += amount;
    }
}

void EffectSystem::setLeader(const std::string& country, const std::string& leaderName,
                              GameState& gs) {
    Country* c = gs.getCountry(country);
    if (c) {
        c->leader.name = leaderName;
    }
}

void EffectSystem::addModifier(const std::string& country, const std::string& modifier,
                                float value, GameState& gs) {
    Country* c = gs.getCountry(country);
    if (!c) return;

    if (modifier == "political_multiplier") {
        c->politicalMultiplier += value;
    } else if (modifier == "money_multiplier") {
        c->moneyMultiplier += value;
    } else if (modifier == "build_speed") {
        c->buildSpeed += value;
    } else if (modifier == "defense_multiplier") {
        c->defenseMultiplier += value;
    } else if (modifier == "attack_multiplier") {
        c->attackMultiplier += value;
    } else if (modifier == "transport_multiplier") {
        c->transportMultiplier += value;
    } else if (modifier == "military_size") {
        c->militarySize += static_cast<int>(value);
    } else if (modifier == "can_make_faction") {
        c->canMakeFaction = (value != 0.0f);
    } else if (modifier == "expanded_invitations") {
        c->expandedInvitations = (value != 0.0f);
    } else if (modifier == "has_changed_country") {
        c->hasChangedCountry = (value != 0.0f);
    } else if (modifier == "train_division") {
        int count = static_cast<int>(value);
        c->training.push_back({count, 0});
    } else if (modifier == "puppet_to") {

        c->puppetTo.clear();
    }
}
