#include "game/peace_conference.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/puppet.h"


static constexpr float BASE_ANNEX_COST       = 5.0f;
static constexpr float CAPITAL_MULTIPLIER    = 3.0f;
static constexpr float FACTORY_MULTIPLIER    = 1.5f;
static constexpr float PUPPET_BASE_COST      = 50.0f;
static constexpr float PUPPET_PER_PROVINCE   = 2.0f;
static constexpr float LIBERATE_BASE_COST    = 30.0f;
static constexpr float INSTALL_GOV_BASE_COST = 40.0f;


PeaceConference::PeaceConference() = default;
PeaceConference::~PeaceConference() = default;

void PeaceConference::start(const std::vector<std::string>& winnerList,
                             const std::vector<std::string>& loserList,
                             GameState& gs) {
    winners = winnerList;
    losers = loserList;
    originalWinners = winnerList;
    originalLosers = loserList;
    active = true;
    finished = false;
    currentDemandIndex = 0;
    demands.clear();


    std::sort(winners.begin(), winners.end(), [&](const std::string& a, const std::string& b) {
        Country* ca = gs.getCountry(a);
        Country* cb = gs.getCountry(b);
        float sa = ca ? ca->warScore : 0.0f;
        float sb = cb ? cb->warScore : 0.0f;
        return sa > sb;
    });


    for (auto& w : winners) {
        Country* c = gs.getCountry(w);
        float ws = c ? c->warScore : 10.0f;
        warScoreBudget[w] = std::max(10.0f, ws);
    }
}

void PeaceConference::addDemand(const PeaceDemand& demand) {
    demands.push_back(demand);
}

void PeaceConference::removeDemand(int index) {
    if (index >= 0 && index < static_cast<int>(demands.size())) {
        demands.erase(demands.begin() + index);
    }
}

bool PeaceConference::canAfford(const std::string& country, float cost) const {
    auto it = warScoreBudget.find(country);
    if (it == warScoreBudget.end()) return false;
    return it->second >= cost;
}

void PeaceConference::processDemand(int index, GameState& gs) {
    if (index < 0 || index >= static_cast<int>(demands.size())) return;

    PeaceDemand& d = demands[index];
    if (!canAfford(d.demander, d.warScoreCost)) return;

    warScoreBudget[d.demander] -= d.warScoreCost;
    d.accepted = true;

    switch (d.type) {
        case DemandType::AnnexRegion: {
            Country* victor = gs.getCountry(d.demander);
            if (victor) {
                for (int rid : d.regions) {
                    victor->addRegion(rid, gs);
                }
            }
            break;
        }
        case DemandType::AnnexCountry: {
            Country* victor = gs.getCountry(d.demander);
            Country* target = gs.getCountry(d.target);
            if (victor && target) {
                std::vector<int> targetRegions = target->regions;
                for (int rid : targetRegions) {
                    victor->addRegion(rid, gs);
                }
            }
            break;
        }
        case DemandType::Puppet: {

            PuppetState ps;
            ps.overlord = d.demander;
            ps.puppet = d.target;
            ps.autonomy = 50.0f;
            ps.active = true;
            gs.puppetStates.push_back(ps);

            Country* target = gs.getCountry(d.target);
            if (target) {
                target->puppetTo = d.demander;
            }
            break;
        }
        case DemandType::ChangeIdeology: {
            Country* victor = gs.getCountry(d.demander);
            Country* target = gs.getCountry(d.target);
            if (victor && target) {
                target->ideology = victor->ideology;
                target->ideologyName = victor->ideologyName;
            }
            break;
        }
        case DemandType::Demilitarize: {
            Country* target = gs.getCountry(d.target);
            if (target) {

                while (!target->divisions.empty()) {
                    target->divisions.back()->kill(gs);
                }
                target->militarySize = 0;
            }
            break;
        }
        case DemandType::WarReparations: {
            Country* victor = gs.getCountry(d.demander);
            Country* target = gs.getCountry(d.target);
            if (victor && target) {
                float reparations = target->money * 0.5f;
                target->money -= reparations;
                victor->money += reparations;
            }
            break;
        }
        case DemandType::ReleaseCountry: {

            break;
        }
        default:
            break;
    }
}

void PeaceConference::processAll(GameState& gs) {
    for (int i = 0; i < static_cast<int>(demands.size()); ++i) {
        if (!demands[i].accepted) {
            processDemand(i, gs);
        }
    }
}

void PeaceConference::finish(GameState& gs) {
    processAll(gs);
    active = false;
    finished = true;


    for (auto& winner : originalWinners) {
        Country* wc = gs.getCountry(winner);
        if (!wc) continue;
        for (auto& loser : originalLosers) {
            wc->makePeace(loser, gs, false);
        }
    }

    gs.finalizePeaceResolution(originalWinners, originalLosers);
}

void PeaceConference::autoResolve(GameState& gs) {

    for (auto& winner : winners) {
        float budget = warScoreBudget[winner];
        if (budget <= 0.0f) continue;

        Country* wc = gs.getCountry(winner);
        if (!wc) continue;


        for (auto& loser : losers) {
            Country* lc = gs.getCountry(loser);
            if (!lc) continue;

            std::vector<int> loserRegions = lc->regions;
            for (int rid : loserRegions) {
                float cost = BASE_ANNEX_COST;
                if (budget < cost) break;


                bool isCore = false;
                for (int core : wc->coreRegions) {
                    if (core == rid) { isCore = true; break; }
                }
                if (isCore) cost *= 0.5f;

                if (budget >= cost) {
                    PeaceDemand d;
                    d.type = DemandType::AnnexRegion;
                    d.demander = winner;
                    d.target = loser;
                    d.regions = {rid};
                    d.warScoreCost = cost;
                    demands.push_back(d);
                    budget -= cost;
                }
            }
        }
        warScoreBudget[winner] = budget;
    }

    finish(gs);
}

std::vector<PeaceDemand> PeaceConference::getDemandsFor(const std::string& country) const {
    std::vector<PeaceDemand> result;
    for (auto& d : demands) {
        if (d.demander == country) {
            result.push_back(d);
        }
    }
    return result;
}

float PeaceConference::getRemainingWarScore(const std::string& country) const {
    auto it = warScoreBudget.find(country);
    if (it == warScoreBudget.end()) return 0.0f;
    return it->second;
}
