#include "game/political_options.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/helpers.h"


std::unordered_map<std::string, std::vector<OptionValue>> createDecisionTree(
    const std::string& countryName, GameState& gs)
{
    std::unordered_map<std::string, std::vector<OptionValue>> tree;


    int x = 1600;
    int y = 0;


    tree["Political Effort"]      = {x + 625, 0, 50, std::string(""), std::string("True"), std::string("self.politicalPower += 50"), 30, std::string("Gain +50 political power."), std::string("No additional requirements."), false};
    tree["Ideology Focus"]        = {x + 325, 1, 50, std::string("Political Effort"), std::string("True"), std::string("self.politicalMultiplier += 0.1"), 30, std::string("Gain 10% political power gain."), std::string("No additional requirements."), false};

    tree["Workers Rights"]        = {x, 2, 50, std::string("Ideology Focus"), std::string("True"), std::string("self.ideology[1] -= 0.25"), 30, std::string("Gain 25 authoritarian."), std::string("No additional requirements."), false};
    tree["Class Struggle"]        = {x, 3, 50, std::string("Workers Rights"), std::string("True"), std::string("self.ideology[0] -= 0.25"), 30, std::string("Gain 25 left."), std::string("No additional requirements."), false};
    tree["Socialism"]             = {x, 4, 50, std::string("Class Struggle"), std::string("True"), std::string("self.ideology[1] -= 0.25"), 30, std::string("Gain 25 communist."), std::string("No additional requirements."), false};
    tree["Proletariat Dictatorship"] = {x, 5, 50, std::string("Socialism"), std::string("True"), std::string("self.ideology[1] -= 0.25"), 30, std::string("Gain 25 communist."), std::string("No additional requirements."), false};

    tree["Progressivism"]         = {x + 200, 2, 50, std::string("Ideology Focus"), std::string("True"), std::string("self.ideology[0] -= 0.25"), 30, std::string("Gain 25 left."), std::string("No additional requirements."), false};
    tree["Life, Liberty, Property"] = {x + 200, 3, 50, std::string("Progressivism"), std::string("True"), std::string("self.ideology[1] += 0.25"), 30, std::string("Gain 25 libertarian."), std::string("No additional requirements."), false};
    tree["Suffrage For All"]      = {x + 200, 4, 50, std::string("Life, Liberty, Property"), std::string("True"), std::string("self.ideology[0] -= 0.25"), 30, std::string("Gain 25 liberal."), std::string("No additional requirements."), false};
    tree["Enshrine Democracy"]    = {x + 200, 5, 50, std::string("Suffrage For All"), std::string("True"), std::string("self.ideology[0] -= 0.25"), 30, std::string("Gain 25 liberal."), std::string("No additional requirements."), false};

    tree["Traditionalism"]        = {x + 450, 2, 50, std::string("Ideology Focus"), std::string("True"), std::string("self.ideology[0] += 0.25"), 30, std::string("Gain 25 right."), std::string("No additional requirements."), false};
    tree["Under God"]             = {x + 450, 3, 50, std::string("Traditionalism"), std::string("True"), std::string("self.ideology[1] += 0.25"), 30, std::string("Gain 25 libertarian."), std::string("No additional requirements."), false};
    tree["The Divine Right"]      = {x + 450, 4, 50, std::string("Under God"), std::string("True"), std::string("self.ideology[0] += 0.25"), 30, std::string("Gain 25 monarchist."), std::string("No additional requirements."), false};
    tree["Restore the Throne"]    = {x + 450, 5, 50, std::string("The Divine Right"), std::string("True"), std::string("self.ideology[0] += 0.25"), 30, std::string("Gain 25 monarchist."), std::string("No additional requirements."), false};

    tree["Patriotism"]            = {x + 650, 2, 50, std::string("Ideology Focus"), std::string("True"), std::string("self.ideology[1] -= 0.25"), 30, std::string("Gain 25 authoritarian."), std::string("No additional requirements."), false};
    tree["National Unity"]        = {x + 650, 3, 50, std::string("Patriotism"), std::string("True"), std::string("self.ideology[0] += 0.25"), 30, std::string("Gain 25 right."), std::string("No additional requirements."), false};
    tree["Us Above All"]          = {x + 650, 4, 50, std::string("National Unity"), std::string("True"), std::string("self.ideology[1] -= 0.25"), 30, std::string("Gain 25 nationalist."), std::string("No additional requirements."), false};
    tree["Fascism"]               = {x + 650, 5, 50, std::string("Us Above All"), std::string("True"), std::string("self.ideology[1] -= 0.25"), 30, std::string("Gain 25 nationalist."), std::string("No additional requirements."), false};

    tree["End The Status Quo"]    = {x + 325, 6, 10, std::string("Ideology Focus"), std::string("True"), std::string("self.baseStability -= 5"), 15, std::string("Lose 5 stability."), std::string("No additional requirements."), false};

    tree["Policy Focus"]          = {x + 1050, 1, 50, std::string("Political Effort"), std::string("True"), std::string("self.politicalMultiplier += 0.1"), 30, std::string("Gain 10% political power gain."), std::string("No additional requirements."), false};
    tree["Reform I"]              = {x + 850, 2, 50, std::string("Policy Focus"), std::string("True"), std::string("self.baseStability += 5"), 30, std::string("Gain 5 stability."), std::string("No additional requirements."), false};
    tree["Reform II"]             = {x + 850, 3, 50, std::string("Reform I"), std::string("True"), std::string("self.baseStability += 10"), 30, std::string("Gain 10 stability."), std::string("No additional requirements."), false};
    tree["Reform III"]            = {x + 850, 4, 50, std::string("Reform II"), std::string("True"), std::string("self.baseStability += 15"), 30, std::string("Gain 15 stability."), std::string("No additional requirements."), false};

    tree["Geopolitics"]           = {x + 1150, 2, 50, std::string("Policy Focus"), std::string("True"), std::string("self.politicalMultiplier += 0.1"), 30, std::string("Gain 10% political power gain."), std::string("No additional requirements."), false};
    tree["The New Order"]         = {x + 1050, 3, 100, std::string("Geopolitics"), std::string("self.faction == None"), std::string("self.canMakeFaction = True"), 60, std::string("Can create a faction."), std::string("Not in a faction."), false};
    tree["Territorial Expansion"] = {x + 1050, 4, 100, std::string("The New Order"), std::string("faction_leader"), std::string("self.expandedInvitations = True"), 60, std::string("Can expand faction across borders."), std::string("Leader of a faction."), false};
    tree["Gain Autonomy"]         = {x + 1250, 3, 100, std::string("Geopolitics"), std::string("is_puppet"), std::string("self.puppetTo = None"), 60, std::string("Gain independence."), std::string("Puppet to a country."), false};


    tree["Industrial Effort"]     = {200, y, 50, std::string(""), std::string("True"), std::string("self.money += 10000000"), 30, std::string("Gain $10m."), std::string("No additional requirements."), false};
    tree["Infrastructure I"]      = {0, y + 1, 50, std::string("Industrial Effort"), std::string("True"), std::string("self.addFactory()"), 30, std::string("Creates a factory."), std::string("No additional requirements."), false};
    tree["Infrastructure II"]     = {0, y + 2, 50, std::string("Infrastructure I"), std::string("True"), std::string("self.addFactory()"), 30, std::string("Creates 2 factories."), std::string("No additional requirements."), false};
    tree["Infrastructure III"]    = {0, y + 3, 50, std::string("Infrastructure II"), std::string("True"), std::string("self.addFactory()"), 30, std::string("Creates 3 factories."), std::string("No additional requirements."), false};
    tree["Investment"]            = {300, y + 1, 50, std::string("Industrial Effort"), std::string("True"), std::string("self.money += 20000000"), 30, std::string("Gain $20m."), std::string("No additional requirements."), false};
    tree["Technology I"]          = {200, y + 2, 50, std::string("Investment"), std::string("True"), std::string("self.moneyMultiplier += 0.1"), 60, std::string("Gain 10% industry production."), std::string("No additional requirements."), false};
    tree["Technology II"]         = {200, y + 3, 50, std::string("Technology I"), std::string("True"), std::string("self.moneyMultiplier += 0.1"), 60, std::string("Gain 10% industry production."), std::string("No additional requirements."), false};
    tree["Construction I"]        = {400, y + 2, 50, std::string("Investment"), std::string("True"), std::string("self.buildSpeed -= 0.1"), 60, std::string("Gain 10% build speed."), std::string("No additional requirements."), false};
    tree["Construction II"]       = {400, y + 3, 50, std::string("Construction I"), std::string("True"), std::string("self.buildSpeed -= 0.1"), 60, std::string("Gain 10% build speed."), std::string("No additional requirements."), false};


    tree["Military Effort"]       = {1000, y, 50, std::string(""), std::string("True"), std::string("self.money += 10000000"), 60, std::string("Gain $10m."), std::string("No additional requirements."), false};
    tree["Modernization"]         = {800, y + 1, 50, std::string("Military Effort"), std::string("True"), std::string("self.money += 10000000"), 60, std::string("Gain $10m."), std::string("No additional requirements."), false};
    tree["Defense I"]             = {600, y + 2, 50, std::string("Modernization"), std::string("True"), std::string("self.defenseMultiplier += 0.1"), 60, std::string("Gain 10% division defense."), std::string("No additional requirements."), false};
    tree["Defense II"]            = {600, y + 3, 50, std::string("Defense I"), std::string("True"), std::string("self.defenseMultiplier += 0.1"), 60, std::string("Gain 10% division defense."), std::string("No additional requirements."), false};
    tree["Offense I"]             = {800, y + 2, 50, std::string("Modernization"), std::string("True"), std::string("self.attackMultiplier += 0.1"), 60, std::string("Gain 10% division attack."), std::string("No additional requirements."), false};
    tree["Offense II"]            = {800, y + 3, 50, std::string("Offense I"), std::string("True"), std::string("self.attackMultiplier += 0.1"), 60, std::string("Gain 10% division attack."), std::string("No additional requirements."), false};
    tree["Transportation I"]      = {1000, y + 2, 50, std::string("Modernization"), std::string("True"), std::string("self.transportMultiplier += 0.1"), 60, std::string("Gain 10% division speed."), std::string("No additional requirements."), false};
    tree["Transportation II"]     = {1000, y + 3, 50, std::string("Transportation I"), std::string("True"), std::string("self.transportMultiplier += 0.1"), 60, std::string("Gain 10% division speed."), std::string("No additional requirements."), false};

    tree["Defense of the Nation"] = {1300, y + 1, 50, std::string("Military Effort"), std::string("True"), std::string("self.money += 10000000"), 60, std::string("Gain $10m."), std::string("No additional requirements."), false};
    tree["Militia I"]             = {1200, y + 2, 50, std::string("Defense of the Nation"), std::string("True"), std::string("self.training.append([1, 0])"), 60, std::string("Train a division."), std::string("No additional requirements."), false};
    tree["Militia II"]            = {1200, y + 3, 50, std::string("Militia I"), std::string("True"), std::string("self.training.append([2, 0])"), 60, std::string("Train 2 divisions."), std::string("No additional requirements."), false};
    tree["Militia III"]           = {1200, y + 4, 50, std::string("Militia II"), std::string("True"), std::string("self.training.append([3, 0])"), 60, std::string("Train 3 divisions."), std::string("No additional requirements."), false};

    tree["Militarism"]            = {1400, y + 2, 100, std::string("Defense of the Nation"), std::string("military_size_lt_4"), std::string("self.militarySize += 1"), 60, std::string("Gain 1 military size."), std::string("Less than max military size."), false};
    tree["Conscription"]          = {1400, y + 3, 100, std::string("Militarism"), std::string("military_size_lt_4"), std::string("self.militarySize += 1"), 60, std::string("Gain 1 military size."), std::string("Less than max military size."), false};

    return tree;
}


std::vector<PoliticalOption> getOptions(const std::string& countryName, GameState& gs) {
    std::vector<PoliticalOption> options;

    Country* country = gs.getCountry(countryName);
    if (!country) return options;


    if (country->stability < 60.0f) {
        PoliticalOption opt;
        opt.name = "Improve Policy";
        opt.description = "Increase base stability by 2.";
        opt.category = "politics";
        opt.ppCost = 25;
        opt.effects = {"self.baseStability += 2"};
        options.push_back(opt);
    }


    if (!country->faction.empty()) {
        PoliticalOption opt;
        opt.name = "Leave Faction";
        opt.description = "Leave the current faction.";
        opt.category = "diplomacy";
        opt.ppCost = 50;
        opt.effects = {"leave_faction"};
        options.push_back(opt);
    }

    else if (country->canMakeFaction) {
        PoliticalOption opt;
        opt.name = "Create Faction";
        opt.description = "Form a new faction.";
        opt.category = "diplomacy";
        opt.ppCost = 100;
        opt.effects = {"create_faction"};
        options.push_back(opt);
    }


    auto hasRegion = [&](int rid) -> bool {
        return std::find(country->regions.begin(), country->regions.end(), rid) != country->regions.end();
    };
    auto isCanal = [&](int rid) -> bool {
        return std::find(gs.canals.begin(), gs.canals.end(), rid) != gs.canals.end();
    };

    if (hasRegion(2567) && isCanal(2567)) {
        PoliticalOption opt;
        opt.name = "Blow the Panama Canal";
        opt.category = "politics";
        opt.ppCost = 50;
        opt.effects = {"destroy_canal_2567"};
        options.push_back(opt);
    }
    if (hasRegion(418) && isCanal(418)) {
        PoliticalOption opt;
        opt.name = "Dam the Danish Straits";
        opt.category = "politics";
        opt.ppCost = 75;
        opt.effects = {"destroy_canal_418"};
        options.push_back(opt);
    }
    if ((hasRegion(1802) && isCanal(1802)) || (hasRegion(1868) && isCanal(1868))) {
        PoliticalOption opt;
        opt.name = "Blow the Suez Canal";
        opt.category = "politics";
        opt.ppCost = 50;
        opt.effects = {"destroy_canal_1802_1868"};
        options.push_back(opt);
    }
    if ((hasRegion(1244) && isCanal(1244)) || (hasRegion(1212) && isCanal(1212))) {
        PoliticalOption opt;
        opt.name = "Dam the Bosphorus";
        opt.category = "politics";
        opt.ppCost = 75;
        opt.effects = {"destroy_canal_1244_1212"};
        options.push_back(opt);
    }
    if ((hasRegion(1485) && isCanal(1485)) || (hasRegion(1502) && isCanal(1502))) {
        PoliticalOption opt;
        opt.name = "Dam Gibraltar";
        opt.category = "politics";
        opt.ppCost = 75;
        opt.effects = {"destroy_canal_1485_1502"};
        options.push_back(opt);
    }


    if (country->culture == "Russian") {

        {
            PoliticalOption opt; opt.name = "Annex Tuva"; opt.category = "politics"; opt.ppCost = 75;
            opt.effects = {"annex_culture_Tuvan"}; opt.description = "Integrate Tuva."; options.push_back(opt);
        }

        if (country->ideologyName == "nationalist" && country->atWarWith.empty()) {

            {
                PoliticalOption opt; opt.name = "Union State"; opt.category = "politics"; opt.ppCost = 100;
                opt.effects = {"annex_culture_Belarusian"}; opt.description = "Form a union state with Belarus."; options.push_back(opt);
            }

            if (hasRegion(944)) {
                PoliticalOption opt; opt.name = "Annex Crimea"; opt.category = "politics"; opt.ppCost = 50;
                opt.effects = {"self.addRegions([943, 967, 928])"}; opt.description = "Integrate Crimea."; options.push_back(opt);
            }
        }

        if (country->ideologyName == "communist" && country->atWarWith.empty()) {

            {
                PoliticalOption opt; opt.name = "Annex Estonia"; opt.category = "politics"; opt.ppCost = 50;
                opt.effects = {"annex_culture_Estonian"}; opt.description = "Annex Estonia."; options.push_back(opt);
            }
            {
                PoliticalOption opt; opt.name = "Annex Latvia"; opt.category = "politics"; opt.ppCost = 50;
                opt.effects = {"annex_culture_Latvian"}; opt.description = "Annex Latvia."; options.push_back(opt);
            }
            {
                PoliticalOption opt; opt.name = "Annex Lithuania"; opt.category = "politics"; opt.ppCost = 50;
                opt.effects = {"annex_culture_Lithuanian"}; opt.description = "Annex Lithuania."; options.push_back(opt);
            }
        }
    }
    else if (country->culture == "German") {
        if (country->ideologyName == "nationalist" && country->atWarWith.empty()) {
            {
                PoliticalOption opt; opt.name = "Anschluss"; opt.category = "politics"; opt.ppCost = 100;
                opt.effects = {"annex_culture_Austrian"}; opt.description = "Unify with Austria."; options.push_back(opt);
            }
            if (hasRegion(611)) {
                PoliticalOption opt; opt.name = "Munich Agreement"; opt.category = "politics"; opt.ppCost = 50;
                opt.effects = {"self.addRegions([682, 641, 649, 680, 711])"}; opt.description = "Demand the Sudetenland."; options.push_back(opt);
            }
        }

        {
            PoliticalOption opt; opt.name = "Release Bavaria"; opt.category = "politics"; opt.ppCost = 75;
            opt.effects = {"independence_Bavaria"}; opt.description = "Grant Bavaria independence."; options.push_back(opt);
        }
    }
    else if (country->culture == "British") {
        if (country->stability < 50.0f) {
            PoliticalOption opt;
            opt.name = "Dissolve The UK";
            opt.description = "Release Scotland, Wales, and Northern Ireland.";
            opt.category = "politics";
            opt.ppCost = 100;
            opt.effects = {"dissolve_uk"};
            options.push_back(opt);
        }
    }
    else if (country->culture == "American") {

        auto hasCulture = [&](const std::string& c) {
            for (auto& [cn, ctry] : gs.countries) { if (ctry && ctry->culture == c) return true; } return false;
        };
        if (!hasCulture("Californian")) {
            PoliticalOption opt; opt.name = "Release California"; opt.category = "politics"; opt.ppCost = 100;
            opt.effects = {"independence_California"}; opt.description = "Grant California independence."; options.push_back(opt);
        }
        if (!hasCulture("Texan")) {
            PoliticalOption opt; opt.name = "Release Texas"; opt.category = "politics"; opt.ppCost = 100;
            opt.effects = {"independence_Texas"}; opt.description = "Grant Texas independence."; options.push_back(opt);
        }
        if (!hasCulture("Hawaiian")) {
            PoliticalOption opt; opt.name = "Release Hawaii"; opt.category = "politics"; opt.ppCost = 50;
            opt.effects = {"independence_Hawaii"}; opt.description = "Grant Hawaii independence."; options.push_back(opt);
        }
    }
    else if (country->culture == "French") {
        if (!country->atWarWith.empty()) {}
        PoliticalOption opt; opt.name = "Release Brittany"; opt.category = "politics"; opt.ppCost = 50;
        opt.effects = {"independence_Brittany"}; opt.description = "Grant Brittany independence."; options.push_back(opt);
    }
    else if (country->culture == "Japanese") {
        if (country->atWarWith.empty()) {
            PoliticalOption opt; opt.name = "Annex Russian Islands"; opt.category = "politics"; opt.ppCost = 50;
            opt.effects = {"self.addRegions([697, 1026, 973, 906, 878, 795, 762, 645])"}; opt.description = "Claim the Kuril Islands and Sakhalin."; options.push_back(opt);
        }
    }
    else if (country->culture == "Indian") {
        if (country->atWarWith.empty()) {
            PoliticalOption opt; opt.name = "Annex Kashmir"; opt.category = "politics"; opt.ppCost = 100;
            opt.effects = {"self.addRegions([1482, 1607, 1626, 1447, 1519])"}; opt.description = "Integrate Kashmir."; options.push_back(opt);
        }
    }
    else if (country->culture == "Chinese") {
        PoliticalOption opt1; opt1.name = "Release Tibet"; opt1.category = "politics"; opt1.ppCost = 50;
        opt1.effects = {"independence_Tibet"}; opt1.description = "Grant Tibet independence."; options.push_back(opt1);
        PoliticalOption opt2; opt2.name = "Release Xinjiang"; opt2.category = "politics"; opt2.ppCost = 100;
        opt2.effects = {"independence_Xinjiang"}; opt2.description = "Grant Xinjiang independence."; options.push_back(opt2);
    }
    else if (country->culture == "Canadian") {
        PoliticalOption opt; opt.name = "Release Quebec"; opt.category = "politics"; opt.ppCost = 50;
        opt.effects = {"independence_Quebec"}; opt.description = "Grant Quebec independence."; options.push_back(opt);
    }
    else if (country->culture == "Spanish") {
        PoliticalOption opt1; opt1.name = "Release Catalonia"; opt1.category = "politics"; opt1.ppCost = 25;
        opt1.effects = {"independence_Catalonia"}; opt1.description = "Grant Catalonia independence."; options.push_back(opt1);
        PoliticalOption opt2; opt2.name = "Release Basque Country"; opt2.category = "politics"; opt2.ppCost = 25;
        opt2.effects = {"independence_Basque_Country"}; opt2.description = "Grant Basque Country independence."; options.push_back(opt2);
    }
    else if (country->culture == "Italian") {
        PoliticalOption opt1; opt1.name = "Release Sardinia"; opt1.category = "politics"; opt1.ppCost = 25;
        opt1.effects = {"independence_Sardinia"}; opt1.description = "Grant Sardinia independence."; options.push_back(opt1);
        PoliticalOption opt2; opt2.name = "Release Sicily"; opt2.category = "politics"; opt2.ppCost = 25;
        opt2.effects = {"independence_Sicily"}; opt2.description = "Grant Sicily independence."; options.push_back(opt2);
    }

    return options;
}


std::vector<PoliticalOption> getCountryOptions(const std::string& countryName,
                                                 GameState& gs) {


    std::vector<PoliticalOption> options;
    Country* country = gs.getCountry(gs.controlledCountry);
    Country* target = gs.getCountry(countryName);
    if (!country || !target) return options;


    if (std::find(country->atWarWith.begin(), country->atWarWith.end(), countryName) == country->atWarWith.end()) {
        PoliticalOption opt;
        opt.name = "Declare War";
        opt.description = "Declare war on " + replaceAll(countryName, "_", " ") + ".";
        opt.category = "aggressive";
        opt.ppCost = 25;
        opt.effects = {"declare_war_" + countryName};
        options.push_back(opt);
    }


    if (std::find(country->atWarWith.begin(), country->atWarWith.end(), countryName) == country->atWarWith.end() &&
        std::find(country->militaryAccess.begin(), country->militaryAccess.end(), countryName) == country->militaryAccess.end()) {
        int cost = 50;
        if (country->faction == target->faction && !country->faction.empty()) cost -= 25;
        if (country->ideologyName != target->ideologyName) cost += 25;
        PoliticalOption opt;
        opt.name = "Get Military Access";
        opt.description = "Gain military access through " + replaceAll(countryName, "_", " ") + ".";
        opt.category = "friendly";
        opt.ppCost = std::max(0, cost);
        opt.effects = {"military_access_" + countryName};
        options.push_back(opt);
    }


    if (!country->faction.empty() && country->faction == target->faction &&
        !country->atWarWith.empty()) {
        PoliticalOption opt;
        opt.name = "Call To Arms";
        opt.description = "Request " + replaceAll(countryName, "_", " ") + " joins your wars.";
        opt.category = "friendly";
        opt.ppCost = 25;
        opt.effects = {"call_to_arms_" + countryName};
        options.push_back(opt);
    }


    if (!country->faction.empty() && country->factionLeader && target->faction.empty() &&
        std::find(country->atWarWith.begin(), country->atWarWith.end(), countryName) == country->atWarWith.end()) {
        PoliticalOption opt;
        opt.name = "Invite To Faction";
        opt.description = "Invite " + replaceAll(countryName, "_", " ") + " to join your faction.";
        opt.category = "friendly";
        opt.ppCost = 25;
        opt.effects = {"invite_to_faction_" + countryName};
        options.push_back(opt);
    }

    return options;
}


std::vector<DiplomaticDemand> getDemands(const std::string& countryName,
                                          const std::string& targetCountry,
                                          GameState& gs) {
    std::vector<DiplomaticDemand> demands;

    Country* country = gs.getCountry(countryName);
    Country* target = gs.getCountry(targetCountry);
    if (!country || !target) return demands;


    if (country->ideologyName != target->ideologyName &&
        country->ideologyName != "nonaligned") {
        DiplomaticDemand d;
        d.type = "install_government";
        d.target = targetCountry;
        d.cost = 0;
        demands.push_back(d);
    }


    if (!country->faction.empty() &&
        target->faction != country->faction) {
        DiplomaticDemand d;
        d.type = "invite_to_faction";
        d.target = targetCountry;
        d.cost = 0;
        demands.push_back(d);
    }


    if (!target->faction.empty()) {
        DiplomaticDemand d;
        d.type = "remove_from_faction";
        d.target = targetCountry;
        d.cost = 0;
        demands.push_back(d);
    }


    for (auto& [culture, cultureRegions] : target->cultures) {
        if (cultureRegions.empty()) continue;

        DiplomaticDemand d;
        d.type = "annex_culture_regions";
        d.target = targetCountry;
        d.cost = 0;
        d.args.push_back(culture);
        demands.push_back(d);
    }

    return demands;
}
