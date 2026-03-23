#include "game/combat.h"


CombatStats computeCombatStats(int , int armsFactoryCount) {
    CombatStats cs;
    cs.attack   = 1.0f + armsFactoryCount * 0.03f;
    cs.defense  = 1.0f + armsFactoryCount * 0.02f;
    cs.armor    = armsFactoryCount * 0.03f;
    cs.piercing = armsFactoryCount * 0.03f;
    cs.speed    = 5.0f;
    cs.fuelUse  = 1.0f;
    cs.supplyUse = 2.0f;
    return cs;
}
