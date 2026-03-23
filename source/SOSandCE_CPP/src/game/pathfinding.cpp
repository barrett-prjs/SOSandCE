#include "game/pathfinding.h"
#include "game/game_state.h"
#include "game/country.h"
#include "data/region_data.h"


struct PFNode {
    float priority;
    int   regionId;
    bool operator>(const PFNode& o) const { return priority > o.priority; }
};

static float wrapCoord(float pos, float finalPos, float mapSize) {
    if (std::abs(pos) - std::abs(finalPos) > mapSize / 2.0f) {
        float a = pos + mapSize;
        float b = pos - mapSize;
        pos = (std::abs(a) < std::abs(b)) ? a : b;
    }
    return pos;
}

std::vector<int> pathfind(int startRegion, int finalRegion,
                          bool ignoreEnemy, bool ignoreWater,
                          const std::set<std::string>& militaryAccess,
                          const std::set<std::string>& atWarWith,
                          int maxIterations, GameState& gs)
{
    if (startRegion == finalRegion) return {};

    auto& rd = RegionData::instance();
    Vec2 goalPos = rd.getLocation(finalRegion);
    float mapWidth = 1275.0f;
    if (gs.regionsMapSurf && gs.regionsMapSurf->w > 0) {
        mapWidth = static_cast<float>(gs.regionsMapSurf->w);
    } else if (gs.politicalMapSurf && gs.politicalMapSurf->w > 0) {
        mapWidth = static_cast<float>(gs.politicalMapSurf->w);
    }


    std::set<int> portSet(gs.ports.begin(), gs.ports.end());
    std::set<int> canalSet(gs.canals.begin(), gs.canals.end());

    std::priority_queue<PFNode, std::vector<PFNode>, std::greater<PFNode>> frontier;
    std::unordered_map<int, int>   cameFrom;
    std::unordered_map<int, float> costSoFar;

    cameFrom[startRegion] = -1;
    costSoFar[startRegion] = 0.0f;
    frontier.push({0.0f, startRegion});

    while (!frontier.empty()) {
        int currentRegion = frontier.top().regionId;
        frontier.pop();

        if (currentRegion == finalRegion) break;

        std::string currentOwner = rd.getOwner(currentRegion);

        for (int nextRegion : rd.getConnections(currentRegion)) {
            std::string nextOwner = rd.getOwner(nextRegion);


            bool canEnter = false;


            if (!nextOwner.empty() && militaryAccess.count(nextOwner))
                canEnter = true;


            if (!canEnter && !nextOwner.empty() && !ignoreEnemy && atWarWith.count(nextOwner))
                canEnter = true;


            if (!canEnter && portSet.count(currentRegion) && nextOwner.empty() && !ignoreWater)
                canEnter = true;


            if (!canEnter && nextOwner.empty() && currentOwner.empty() && !ignoreWater)
                canEnter = true;


            if (!canEnter && currentOwner.empty() && canalSet.count(nextRegion) && !ignoreWater)
                canEnter = true;


            if (!canEnter && canalSet.count(currentRegion) && canalSet.count(nextRegion) && !ignoreWater)
                canEnter = true;


            if (!canEnter && canalSet.count(currentRegion) && nextOwner.empty() && !ignoreWater)
                canEnter = true;

            if (!canEnter) continue;

            float newCost = costSoFar[currentRegion] + 1.0f;

            auto it = costSoFar.find(nextRegion);
            if (it == costSoFar.end() || newCost < it->second) {
                costSoFar[nextRegion] = newCost;

                Vec2 nPos = rd.getLocation(nextRegion);

                float x1 = wrapCoord(goalPos.x, nPos.x, mapWidth);
                float x2 = wrapCoord(nPos.x, goalPos.x, mapWidth);
                float dy = goalPos.y - nPos.y;
                float dx = x1 - x2;
                float priority = newCost + std::sqrt(dx * dx + dy * dy);

                frontier.push({priority, nextRegion});
                cameFrom[nextRegion] = currentRegion;
            }
        }


        if (static_cast<int>(cameFrom.size()) > maxIterations) break;
    }


    if (cameFrom.find(finalRegion) == cameFrom.end()) return {};

    std::vector<int> commands;
    int cur = finalRegion;
    while (cur != startRegion) {
        commands.push_back(cur);
        auto it = cameFrom.find(cur);
        if (it == cameFrom.end() || it->second == -1) break;
        cur = it->second;
    }
    std::reverse(commands.begin(), commands.end());
    return commands;
}
