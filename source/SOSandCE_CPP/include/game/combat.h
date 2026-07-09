#pragma once
#include "core/common.h"


constexpr float COMBAT_SCALE        = 0.01f;
constexpr float MULTI_FRONT_PENALTY = 0.15f;
constexpr float ORG_DRAIN_RATE      = 2.0f;


struct CombatStats {
    float attack   = 1.0f;
    float defense  = 1.0f;
    float armor    = 0.0f;
    float piercing = 0.0f;
    float speed    = 1.0f;
    float fuelUse  = 1.0f;
    float supplyUse = 1.0f;

    CombatStats() = default;
    CombatStats(float a, float d, float ar, float p, float sp, float fu, float su)
        : attack(a), defense(d), armor(ar), piercing(p), speed(sp), fuelUse(fu), supplyUse(su) {}

    CombatStats(int divisionStack, int armsFactoryCount)
        : attack(1.0f + armsFactoryCount * 0.03f),
          defense(1.0f + armsFactoryCount * 0.02f),
          armor(armsFactoryCount * 0.03f),
          piercing(armsFactoryCount * 0.03f),
          speed(5.0f), fuelUse(1.0f), supplyUse(2.0f) {}

    void recalculate(int divisionStack, int armsFactoryCount) {
        attack   = 1.0f + armsFactoryCount * 0.03f;
        defense  = 1.0f + armsFactoryCount * 0.02f;
        armor    = armsFactoryCount * 0.03f;
        piercing = armsFactoryCount * 0.03f;
        speed    = 5.0f;
        fuelUse  = 1.0f;
        supplyUse = 2.0f;
    }


    CombatStats operator+(const CombatStats& o) const {
        return CombatStats(
            attack   + o.attack,
            defense  + o.defense,
            armor    + o.armor,
            piercing + o.piercing,
            speed    + o.speed,
            fuelUse  + o.fuelUse,
            supplyUse + o.supplyUse
        );
    }

    CombatStats operator*(float s) const {
        return CombatStats(
            attack * s, defense * s, armor * s,
            piercing * s, speed * s, fuelUse * s, supplyUse * s
        );
    }
};
