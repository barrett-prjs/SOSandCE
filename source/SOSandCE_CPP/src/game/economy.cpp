#include "game/economy.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/buildings.h"

namespace {

float tradeRatePerHour() {
    return 1.0f / 6.0f;
}

float activeTradeCommitment(const GameState& gs, const std::string& countryName, Resource resource, bool exports) {
    float total = 0.0f;
    for (const auto& contract : gs.tradeContracts) {
        if (!contract.active || contract.resource != resource) continue;
        if (exports && contract.exporter == countryName) total += contract.amount;
        if (!exports && contract.importer == countryName) total += contract.amount;
    }
    return total;
}

float tradeAttitude(const Country* exporter, const Country* importer, const GameState& gs) {
    if (!exporter || !importer) return 0.0f;
    if (exporter->name == importer->name) return 1.0f;

    float score = 0.0f;
    if (!exporter->faction.empty() && exporter->faction == importer->faction) score += 0.45f;
    if (exporter->ideologyName == importer->ideologyName) score += 0.20f;
    if (exporter->puppetTo == importer->name || importer->puppetTo == exporter->name) score += 0.35f;
    if (exporter->hasLandAccessTo(importer->name, gs) || importer->hasLandAccessTo(exporter->name, gs)) score += 0.15f;
    if (std::find(exporter->bordering.begin(), exporter->bordering.end(), importer->name) != exporter->bordering.end()) score += 0.08f;

    for (const auto& enemy : exporter->atWarWith) {
        if (std::find(importer->atWarWith.begin(), importer->atWarWith.end(), enemy) != importer->atWarWith.end()) {
            score += 0.18f;
            break;
        }
    }

    if (std::find(exporter->atWarWith.begin(), exporter->atWarWith.end(), importer->name) != exporter->atWarWith.end()) score -= 1.0f;
    return std::clamp(score, -1.0f, 1.0f);
}

}


ResourceManager::ResourceManager() {
    production.fill(0.0f);
    consumption.fill(0.0f);
    stockpile.fill(0.0f);
    tradeImports.fill(0.0f);
    tradeExports.fill(0.0f);
    deficit.fill(0.0f);
}

float ResourceManager::getNet(Resource r) const {
    int i = static_cast<int>(r);
    return production[i] + tradeImports[i] - consumption[i] - tradeExports[i];
}

float ResourceManager::getAvailable(Resource r) const {
    int i = static_cast<int>(r);
    return stockpile[i];
}

bool ResourceManager::hasDeficit(Resource r) const {
    int i = static_cast<int>(r);
    return deficit[i] > 0.0f;
}

void ResourceManager::resetFlows() {
    production.fill(0.0f);
    consumption.fill(0.0f);
}

void ResourceManager::addProduction(Resource r, float amount) {
    production[static_cast<int>(r)] += amount;
}

void ResourceManager::addConsumption(Resource r, float amount) {
    consumption[static_cast<int>(r)] += amount;
}

void ResourceManager::addRegionResources(int regionId, const std::unordered_map<Resource, float>& res) {
    regionProduction[regionId] = res;
}

void ResourceManager::removeRegionResources(int regionId) {
    regionProduction.erase(regionId);
}

void ResourceManager::recalculate() {
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        production[i] = 1.0f;
    }
    for (auto& [regionId, resMap] : regionProduction) {
        for (auto& [res, amount] : resMap) {
            production[static_cast<int>(res)] += amount;
        }
    }
}


void ResourceManager::update(GameState& gs, const std::string& countryName) {
    Country* country = gs.getCountry(countryName);
    if (!country) return;


    float rate = tradeRatePerHour();


    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        production[i] = 1.0f;
    }

    for (int regionId : country->regions) {
        auto it = regionProduction.find(regionId);
        if (it == regionProduction.end()) continue;

        auto regionBuildings = country->buildingManager.getInRegion(regionId);

        for (auto& [res, amount] : it->second) {
            float bonus = 0.0f;
            float refineryMult = 1.0f;

            for (auto& bld : regionBuildings) {

                if (bld.type == BuildingType::Mine) {
                    bonus += 0.5f;
                }

                if (bld.type == BuildingType::OilWell && res == Resource::Oil) {
                    bonus += 0.5f;
                }

                if (bld.type == BuildingType::Refinery && res == Resource::Oil) {
                    refineryMult *= 1.3f;
                }
            }

            float multiplier = std::min((1.0f + bonus) * refineryMult, 5.0f);
            production[static_cast<int>(res)] += amount * multiplier;
        }
    }


    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        consumption[i] = 0.0f;
    }

    int divCount = static_cast<int>(country->divisions.size());
    int factoryCount = country->factories;


    consumption[static_cast<int>(Resource::Oil)]    += divCount * 0.5f;
    consumption[static_cast<int>(Resource::Steel)]  += divCount * 0.3f;
    consumption[static_cast<int>(Resource::Rubber)] += divCount * 0.1f;


    consumption[static_cast<int>(Resource::Steel)]    += factoryCount * 0.2f;
    consumption[static_cast<int>(Resource::Aluminum)] += factoryCount * 0.15f;


    int armsCount = country->buildingManager.countAll(BuildingType::MilitaryFactory);
    consumption[static_cast<int>(Resource::Tungsten)] += armsCount * 0.3f;
    consumption[static_cast<int>(Resource::Chromium)] += armsCount * 0.2f;


    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        float net = production[i] - consumption[i];
        stockpile[i] += net * rate;
        stockpile[i] = std::max(0.0f, std::min(maxStockpile, stockpile[i]));
        deficit[i] = (stockpile[i] <= 0.0f && net < 0.0f) ? 1.0f : 0.0f;
    }
}


void TradeContract::update(GameState& gs) {
    if (!active) return;

    Country* expObj = gs.getCountry(exporter);
    Country* impObj = gs.getCountry(importer);
    if (!expObj || !impObj) {
        active = false;
        return;
    }

    for (auto& enemy : expObj->atWarWith) {
        if (enemy == importer) {
            active = false;
            return;
        }
    }

    TradeApproval approval = evaluateTradeOffer(expObj, impObj, resource, amount, gs);
    if (!approval.approved) {
        cancelWithCleanup(gs);
        return;
    }

    float rate = tradeRatePerHour();

    int ri = static_cast<int>(resource);

    float available = expObj->resourceManager.stockpile[ri];
    float affordable = price > 0.0f ? std::max(0.0f, impObj->money / price) : amount * rate;
    float actual = std::min({amount * rate, available, affordable});

    if (actual > 0.0f) {
        expObj->resourceManager.stockpile[ri] -= actual;
        expObj->resourceManager.tradeExports[ri] += amount;
        impObj->resourceManager.stockpile[ri] += actual;
        impObj->resourceManager.tradeImports[ri] += amount;

        float payment = actual * price;
        impObj->money -= payment;
        expObj->money += payment;
    } else if (price > 0.0f && impObj->money < price) {
        cancelWithCleanup(gs);
    }
}

void TradeContract::cancel() {
    active = false;
}

void TradeContract::cancelWithCleanup(GameState& gs) {
    active = false;
    int ri = static_cast<int>(resource);
    Country* expObj = gs.getCountry(exporter);
    Country* impObj = gs.getCountry(importer);
    if (expObj) expObj->resourceManager.tradeExports[ri] = 0.0f;
    if (impObj) impObj->resourceManager.tradeImports[ri] = 0.0f;
}

float TradeContract::totalValue() const {
    return amount * price;
}

TradeApproval evaluateTradeOffer(const Country* exporter,
                                 const Country* importer,
                                 Resource resource,
                                 float requestedAmount,
                                 const GameState& gs) {
    TradeApproval result;
    if (!exporter || !importer) return result;
    if (requestedAmount <= 0.0f) return result;
    if (exporter->name == importer->name) return result;
    if (std::find(exporter->atWarWith.begin(), exporter->atWarWith.end(), importer->name) != exporter->atWarWith.end()) return result;

    int ri = static_cast<int>(resource);
    float attitude = tradeAttitude(exporter, importer, gs);
    float stock = exporter->resourceManager.stockpile[ri];
    float production = exporter->resourceManager.production[ri];
    float consumption = exporter->resourceManager.consumption[ri];
    float committedExports = activeTradeCommitment(gs, exporter->name, resource, true);
    float projectedNet = production - consumption - committedExports;

    float reserve = 80.0f + consumption * 6.0f;
    if (std::find(exporter->atWarWith.begin(), exporter->atWarWith.end(), importer->name) == exporter->atWarWith.end() &&
        !exporter->atWarWith.empty()) {
        reserve += 60.0f;
    }
    if (attitude < 0.25f) reserve += 40.0f;
    if (attitude > 0.55f) reserve -= 20.0f;
    reserve = std::clamp(reserve, 60.0f, exporter->resourceManager.maxStockpile * 0.9f);

    float maxAmount = std::max(0.0f, stock - reserve);
    if (projectedNet > 0.0f) {
        maxAmount += projectedNet * (1.5f + std::max(0.0f, attitude) * 2.5f);
    }

    float acceptableDeficit = attitude > 0.75f ? -4.0f : (attitude > 0.45f ? -2.0f : 0.0f);
    if (projectedNet - requestedAmount < acceptableDeficit) {
        maxAmount = std::min(maxAmount, std::max(0.0f, projectedNet - acceptableDeficit));
    }

    result.maxAmount = std::max(0.0f, maxAmount);
    result.approved = attitude > -0.25f && result.maxAmount + 0.01f >= requestedAmount;
    return result;
}
