#pragma once
#include "core/common.h"

class GameState;


std::vector<int> pathfind(int startRegion, int finalRegion,
                          bool ignoreEnemy, bool ignoreWater,
                          const std::set<std::string>& militaryAccess,
                          const std::set<std::string>& atWarWith,
                          int maxIterations, GameState& gs);
