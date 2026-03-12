# Spirits of Steel: Community Edition (CE Version 1.1)
## by Barrett

A major upgraded edition of the Spirits of Steel grand strategy game with deeper politics, diplomacy, economy, AI, peace conferences, combat, buildings, and decision/focus tree systems.

## How to Run

**Requirements:** Python 3.8+, pygame, numpy

```
pip install pygame numpy
python main.py
```

## Features

### Economy & Resources
- 6 strategic resources: Oil, Steel, Aluminum, Tungsten, Chromium, Rubber
- Provinces produce resources based on real-world geography
- Resource map overlay with color-coded legend (toggle via button or auto-opens for extraction buildings)
- Dynamic building costs that scale with country size
- Civilian factories generate income; Arms factories speed up troop training
- Division upkeep costs deducted daily; training costs shown up-front
- Resource deficits apply meaningful combat and production penalties
- Income, upkeep, and net balance displayed clearly in the UI

### Buildings (7 Types)
- **Civilian Factory** -- Generates daily income (base $5,000/day per factory)
- **Arms Factory** -- Reduces troop training time by 15% per factory (up to 75% reduction)
- **Dockyard** -- Built on coastal regions; creates a port for naval transport
- **Mine** -- Increases resource extraction in the region (+50% per mine)
- **Oil Well** -- Boosts oil output in the region (+75% per well)
- **Refinery** -- Boosts rubber and aluminum output (+40% per refinery)
- **Infrastructure** -- Improves movement speed (+15%) and construction speed (+10%) in the region
- **Destroy** -- Remove existing buildings from a region

### Combat
- 7 unit stats: Attack, Defense, Armor, Piercing, Speed, Fuel Use, Supply Use
- Armor vs Piercing interaction affects damage dealt
- Infrastructure bonuses affect movement speed
- Arms factories boost army effectiveness
- Biome combat modifiers still apply

### Peace Conferences
- Map-based province selection when winning a war
- Click provinces on the map to select/deselect them for annexation
- Province counter shows "X / Y provinces selected"
- Options: Annex All, Annex Selected, Puppet, Install Government
- Release nations based on cultural groups

### Puppet States
- Countries can be puppeted through peace conferences or focus trees
- Puppets share overlord's ideology and auto-join their faction
- Autonomy system (0-100) with revolt risk at high autonomy
- Resource tribute flows from puppet to overlord

### Political System
- Leaders and cabinet ministers with trait-based modifiers
- Regime-aware elections for democracies
- Random political events (leader death, scandals, economic shocks)
- Region-specific name pools for generated leaders (11 cultural regions)

### Focus Trees (JSON-Driven)
- **United States** -- 56 focuses (New Deal, Isolationism vs Interventionism, Manhattan Project)
- **Germany** -- 58 focuses (Autarky, Greater Reich vs Democratic paths, Wunderwaffen)
- **Russia** -- 73 focuses (Five Year Plans, Great Purge vs Reform, Deep Battle)
- **Global Fallback** -- 80 focuses for all other countries (Industry, Politics, Military, Diplomacy branches)
- Dependency chains, mutually exclusive groups, and country-specific overrides

### AI
- 5 personality profiles: Expansionist, Defensive, Economic, Ideological, Naval
- Smart building, trade, war declaration, and focus tree decisions
- Threat assessment considers military balance, ideology, and borders

### Player Feedback
- Toast notification system for failed actions (explains why an action failed)
- Building costs, training costs, and daily upkeep clearly shown in all relevant UI panels
- Warnings when training troops would cause a financial deficit

## Credits
CE Version 1.1 by Barrett. Built on the original Spirits of Steel engine.
