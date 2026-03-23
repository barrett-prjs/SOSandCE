#pragma once
#include "core/common.h"

class GameState;
class Country;


enum class Resource {
    Oil,
    Steel,
    Aluminum,
    Tungsten,
    Chromium,
    Rubber,
    COUNT
};

inline const char* resourceName(Resource r) {
    switch (r) {
        case Resource::Oil:       return "Oil";
        case Resource::Steel:     return "Steel";
        case Resource::Aluminum:  return "Aluminum";
        case Resource::Tungsten:  return "Tungsten";
        case Resource::Chromium:  return "Chromium";
        case Resource::Rubber:    return "Rubber";
        default:                  return "Unknown";
    }
}

constexpr int RESOURCE_COUNT = static_cast<int>(Resource::COUNT);

inline Resource resourceFromString(const std::string& s) {
    if (s == "oil")       return Resource::Oil;
    if (s == "steel")     return Resource::Steel;
    if (s == "aluminum")  return Resource::Aluminum;
    if (s == "tungsten")  return Resource::Tungsten;
    if (s == "chromium")  return Resource::Chromium;
    if (s == "rubber")    return Resource::Rubber;
    return Resource::Oil;
}


class ResourceManager {
public:
    ResourceManager();


    std::array<float, RESOURCE_COUNT> production{};
    std::array<float, RESOURCE_COUNT> consumption{};
    std::array<float, RESOURCE_COUNT> stockpile{};
    std::array<float, RESOURCE_COUNT> tradeImports{};
    std::array<float, RESOURCE_COUNT> tradeExports{};
    std::array<float, RESOURCE_COUNT> deficit{};

    float maxStockpile = 1000.0f;


    float getNet(Resource r) const;
    float getAvailable(Resource r) const;
    bool  hasDeficit(Resource r) const;


    void recalculate();
    void update(GameState& gs, const std::string& countryName);
    void addProduction(Resource r, float amount);
    void addConsumption(Resource r, float amount);
    void resetFlows();


    template<typename CountryT>
    void tick(CountryT& country, float speed) {
        recalculate();
    }
    void setStartingStockpile(int regionCount) {
        float base = std::max(180.0f, regionCount * 5.0f);
        for (int i = 0; i < RESOURCE_COUNT; ++i) {
            stockpile[i] = std::min(base, maxStockpile);
        }
    }


    float getProductionPenalty() const {
        float penalty = 1.0f;
        if (deficit[static_cast<int>(Resource::Steel)] > 0.0f)    penalty *= 0.5f;
        if (deficit[static_cast<int>(Resource::Aluminum)] > 0.0f) penalty *= 0.7f;
        return penalty;
    }
    float getCombatPenalty() const {
        float penalty = 1.0f;
        if (deficit[static_cast<int>(Resource::Oil)] > 0.0f)      penalty *= 0.6f;
        if (deficit[static_cast<int>(Resource::Rubber)] > 0.0f)   penalty *= 0.8f;
        if (deficit[static_cast<int>(Resource::Tungsten)] > 0.0f) penalty *= 0.85f;
        if (deficit[static_cast<int>(Resource::Chromium)] > 0.0f) penalty *= 0.85f;
        return penalty;
    }


    std::unordered_map<int, std::unordered_map<Resource, float>> regionProduction;
    void addRegionResources(int regionId, const std::unordered_map<Resource, float>& res);
    void removeRegionResources(int regionId);
};


struct TradeContract {
    std::string exporter;
    std::string importer;
    Resource    resource = Resource::Oil;
    float       amount   = 0.0f;
    float       price    = 0.0f;
    bool        active   = true;

    void update(GameState& gs);
    void cancel();
    void cancelWithCleanup(GameState& gs);
    float totalValue() const;
};

struct TradeApproval {
    bool approved = false;
    float maxAmount = 0.0f;
};

TradeApproval evaluateTradeOffer(const Country* exporter,
                                 const Country* importer,
                                 Resource resource,
                                 float requestedAmount,
                                 const GameState& gs);
