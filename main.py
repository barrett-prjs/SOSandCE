import os
os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "hide"
import pygame
import random
import math
import sys
import heapq
import pickle
import shutil
import webbrowser

import numpy as np
from random import Random
from datetime import datetime

from countryData import Countries
from regionData import *
from helperFunctions import *
from politicalOptions import *
from mapFunctions import *

from economy import ResourceManager, TradeContract, RESOURCE_NAMES, RESOURCE_DEFS
from buildings import BuildingManager, BUILDING_DEFS
from combat import CombatStats, resolve_combat_tick
from politics import generate_leader, generate_cabinet, ElectionSystem, PoliticalEventManager
from puppet import PuppetState, create_puppet, get_puppet_states, get_overlord, dissolve_puppet
from peace_conference import PeaceConference
from focus_tree import FocusTreeLoader, FocusTreeEngine, create_decision_tree
from ai import AIController
import game_state as gs

if getattr(sys, "frozen", False):
    app_path = os.path.dirname(sys.executable)
else:
    app_path = os.path.dirname(os.path.abspath(__file__))

flagsDir = os.path.join(app_path, "flags")
iconsDir = os.path.join(app_path, "icons")
imgDir = os.path.join(app_path, "icons")
backgroundsDir = os.path.join(app_path, "backgrounds")
soundDir = os.path.join(app_path, "snd")
startsDir = os.path.join(app_path, "starts")
musicDir = os.path.join(app_path, "music")
savesDir = os.path.join(app_path, "saves")
screenshotsDir = os.path.join(app_path, "screenshots")

WIDTH = 1200
HEIGHT = 675
running = True

camx = 0
camy = 0
zoom = 1

treeCamx = 0
treeCamy = 0
treeZoom = 1

clicked = None
selected = None
selectedMapType = 1
selectedRegions = []
currentFlag = None
pressed = False
openedTab = "political"
buttons = []
politicalButtonHovered = None
openedPoliticalTab = None
politicalTabs = {}
currentMap = 1
showDivisions = False
showCities = False
showResources = False
oldDivisions = False
xPressed = 0
yPressed = 0
timePressed = 0
holdingPopup = None
currentlyBuilding = "civilian_factory"
selectedCountry = None
mapName = None
combatants = []
releasables = []
treatyOptions = None
lastWidth = WIDTH
lastHeight = HEIGHT

currentMusic = "game"
currentHoveredSound = None

factoryBuildSpeed = 120
portBuildSpeed = 60

puppet_states = []
trade_contracts = []
active_peace_conference = None
ai_controllers = {}
political_event_manager = None

sideBarSize = 0.2
holdingSideBar = False
sideBarScroll = 0
sideBarAnimation = 0
controlledCountryFlag = None

toast_messages = []
_toast_font = None

def show_toast(text, duration_ms=1500):
    expire = pygame.time.get_ticks() + duration_ms
    toast_messages.append((text, expire))
    failedClickSound.play()

def draw_toasts():
    now = pygame.time.get_ticks()
    to_remove = []
    y_pos = HEIGHT // 3
    for i, (text, expire) in enumerate(toast_messages):
        if now >= expire:
            to_remove.append(i)
            continue
        remaining = expire - now
        alpha = min(255, int(remaining / 300 * 255))
        ts = pygame.Surface((WIDTH, uiSize * 2), pygame.SRCALPHA)
        ts.fill((0, 0, 0, min(180, alpha)))
        screen.blit(ts, (0, y_pos))
        col = (255, 100, 100, alpha)
        drawText(screen, text, int(uiSize * 0.9), WIDTH // 2, y_pos + uiSize, 'center', (255, 100, 100))
        y_pos += uiSize * 2 + 4
    for i in reversed(to_remove):
        toast_messages.pop(i)

if not os.path.isfile("settings"):
    uiSize = 16
    musicVolume = 1
    soundVolume = 1
    FPS = 60
else:
    with open("settings", "rb") as file:
        variables = pickle.load(file)

    uiSize = variables["uiSize"]
    musicVolume = variables["musicVolume"]
    soundVolume = variables["soundVolume"]
    FPS = variables["FPS"]

countryList = []
factionList = []
popupList = []
battleList = []
controlledCountry = None
selectedDivisions = []
cities = []
ports = []
canals = [1802, 1868, 2567, 1244, 1212, 1485, 1502, 418]

hour = 1
day = 1
month = 1
year = 2024
speed = 0
startDate = [hour, day, month, year]
tick = 0

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)


def drawText(surf, text, size, x, y, pos="center", color=WHITE):
    font = pygame.font.Font("arial.ttf", size)
    text_surface = font.render(text, True, color)
    text_rect = text_surface.get_rect()
    if pos == "center":
        text_rect.center = (x, y)
    elif pos == "midleft":
        text_rect.midleft = (x, y)
    elif pos == "midright":
        text_rect.midright = (x, y)
    surf.blit(text_surface, text_rect)


def getText(text, size, pos="center", color=WHITE):
    font = pygame.font.Font("arial.ttf", size)
    text_surface = font.render(text, True, color)
    text_rect = text_surface.get_rect()
    surface = pygame.Surface((text_rect.width, text_rect.height), pygame.SRCALPHA)
    if pos == "center":
        text_rect.center = surface.get_rect().center
    elif pos == "midleft":
        text_rect.midleft = (0, surface.get_rect().centery)
    elif pos == "midright":
        text_rect.midright = (surface.get_rect().width, surface.get_rect().centery)
    surface.blit(text_surface, text_rect)
    return surface


def setupCountries(ignoreFill=True):
    for id in range(1, 3715):
        x, y = regions.getLocation(id)
        color = map.get_at((round(x), round(y)))[:3]
        name = countries.colorToCountry(color)
        if name != None:
            if name not in countryList:
                globals()[name] = Country(name, [])
            globals()[name].addRegion(id, ignoreFill)
    for country in countryList:
        globals()[country].spawnDivisions()
    for country in countryList:
        for i in range(int(len(globals()[country].regions) * globals()[country].stability / 400)):
            globals()[country].addFactory()
    for country in countryList:
        if len(globals()[country].regions) == 1:
            continue
        beachTiles = 0
        for r1 in globals()[country].regions:
            x, y = regions.getLocation(r1)
            if industryMap.get_at((round(x), round(y))) == (255, 255, 255):
                for r2 in regions.getConnections(r1):
                    x, y = regions.getLocation(r2)
                    if industryMap.get_at((round(x), round(y))) == (126, 142, 158):
                        beachTiles += 1
                        break
        for i in range(0, math.ceil(beachTiles / 10)):
            globals()[country].addPort()


def spawnCountry(country, spawnRegions=None):
    if spawnRegions == None:
        spawnRegions = countries.getClaims(country)
    if country not in countryList:
        globals()[country] = Country(country, spawnRegions)
    else:
        globals()[country].addRegions(spawnRegions)


def spawnFaction(countriesInFaction, name=None):
    if name != None:
        name = name.replace(' ', '_')
    if name == None and controlledCountry != countriesInFaction[0]:
        name = getFactionName(countriesInFaction[0])
    elif name == None or name == '' or name.lower() in [i.lower() for i in factionList] or name.lower() in [i.lower() for i in countryList]:
        if controlledCountry == countriesInFaction[0]:
            popupList.append(TextBox(
                'Faction Name',
                [['Okay', f'spawnFaction({countriesInFaction}, "self")', 0, 5.25]],
                22, 5, WIDTH / 2, HEIGHT / 2
            ))
            return
    if name not in factionList:
        globals()[name] = Faction(name, countriesInFaction)
    if controlledCountry != None:
        popupList.append(Popup(
            f"The Rise of the {name.replace('_', ' ')}",
            [f"{countriesInFaction[0].replace('_', ' ')} has created an alliance."],
            [['Okay', '', 0, 5.25]],
            xSize=22, ySize=5,
            flag1=countriesInFaction[0], flag2=countriesInFaction[0]
        ))


class Faction:
    def __init__(self, name, countriesInFaction):
        factionList.append(name)

        self.type = "faction"
        self.members = []
        self.name = name
        self.color = countries.getColor(countriesInFaction[0])
        self.spirit = []
        self.ideology = getIdeologyName(globals()[countriesInFaction[0]].ideology)
        self.factionWar = []
        self.lastTimeActed = 0
        self.factionLeader = countriesInFaction[0]
        globals()[countriesInFaction[0]].factionLeader = True

        try:
            pygame.image.load(os.path.join("flags", f"{self.name.lower()}_flag.png")).convert()
            self.flag = self.name.lower()
        except Exception:
            self.flag = countriesInFaction[0].lower()

        for country in countriesInFaction:
            if country in countryList:
                self.addCountry(country, False, False)

        self.reloadDivisionColors()

    def update(self):
        if self.factionLeader not in countryList and len(self.members) != 0:
            largest = None
            regionCount = 0
            for country in self.members:
                if len(globals()[country].regions) > regionCount:
                    largest = country
                    regionCount = len(globals()[country].regions)
            if largest != None:
                self.factionLeader = self.members[0]
                globals()[self.members[0]].factionLeader = True

        if self.members == []:
            self.kill()

    def getBattleBorder(self):
        atWarWith = set()
        totalRegions = set()

        for country_name in self.members:
            country = globals()[country_name]
            atWarWith.update(country.atWarWith)
            totalRegions.update(country.regions)

        borderRegions = set()
        for region in totalRegions:
            for connection in regions.getConnections(region):
                if regions.getOwner(connection) in atWarWith:
                    borderRegions.add(region)
                    break

        return list(borderRegions)

    def reloadDivisionColors(self):
        for country in self.members:
            if controlledCountry in self.members and country != controlledCountry:
                globals()[country].resetDivColor((0, 0, 255))
            elif controlledCountry != country:
                globals()[country].resetDivColor((0, 0, 0))

    def addCountry(self, country, sendPrompt=True, createPopup=True):
        if sendPrompt == False or country != controlledCountry:
            c = globals()[country]

            if c.faction != None and c.faction in factionList and c.faction in globals():
                globals()[c.faction].removeCountry(country)

            for id in c.regions:
                x, y = regions.getLocation(id)
                fillWithBorder(factionMap, map, x, y, self.color)

            c.faction = self.name
            c.factionColor = self.color
            self.members.append(country)

            if controlledCountry in self.members and country != controlledCountry:
                globals()[country].resetDivColor((0, 0, 255))
            elif controlledCountry != country:
                globals()[country].resetDivColor((0, 0, 0))
            elif controlledCountry == country:
                self.reloadDivisionColors()

            for faction in self.factionWar:
                for enemy in globals()[faction].members:
                    c.declareWar(enemy)

            if self.factionWar:
                for member in self.members:
                    if member not in c.militaryAccess:
                        c.militaryAccess.append(member)

            if country == controlledCountry:
                return
            if controlledCountry == None:
                return
            if not createPopup:
                return

            name = self.name.replace("_", " ")
            popupList.append(Popup(
                f"New {name} Member",
                [f'{country.replace("_", " ")} has joined {name}'],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=22, ySize=5,
                flag1=self.flag, flag2=country,
            ))
        else:
            name = self.name.replace("_", " ")
            text1 = f"We have been invited to {name}."
            text2 = "We have a choice to join this faction or reject their offer."
            buttons = [
                ["Accept", f"{self.name}.addCountry('{country}', False)", -4.5, 6.75],
                ["Reject", "", 4.5, 6.75],
            ]
            popupList.append(Popup(
                f"Invitation to {name}",
                [text1, text2],
                buttons=buttons, ySize=6.5,
                flag1=self.flag, flag2=country,
            ))
            self.didChangeIdeology = True

    def removeCountry(self, country, ignoreFill=False, doPopup=True):
        if country in self.members:
            globals()[country].factionColor = (255, 255, 255)
            globals()[country].faction = None

            if not ignoreFill:
                for id in globals()[country].regions:
                    x, y = regions.getLocation(id)
                    fillWithBorder(factionMap, map, x, y, (255, 255, 255))

            self.members.remove(country)

            if country == controlledCountry and doPopup:
                self.reloadDivisionColors()
                name = self.name.replace("_", " ")
                popupList.append(Popup(
                    f"The {globals()[country].culture} Schism",
                    [f"We have withdrew from {name}."],
                    buttons=[["Okay", "", 0, 5.25]],
                    xSize=22, ySize=5,
                    flag1=country, flag2=self.flag,
                ))

            if self.members == []:
                self.kill()

    def declareWar(self, faction):
        global currentMusic

        if faction == None:
            return

        if faction not in self.factionWar:
            self.factionWar.append(faction)

        if self.name not in globals()[faction].factionWar:
            globals()[faction].factionWar.append(self.name)

        for c1 in self.members:
            for c2 in self.members:
                if c1 not in globals()[c2].militaryAccess:
                    globals()[c2].militaryAccess.append(c1)

        for c1 in globals()[faction].members:
            for c2 in globals()[faction].members:
                if c1 not in globals()[c2].militaryAccess:
                    globals()[c2].militaryAccess.append(c1)

        for country in self.members:
            for enemyCountry in globals()[faction].members:
                if enemyCountry not in globals()[country].atWarWith:
                    globals()[country].declareWar(enemyCountry, True, False)

        if controlledCountry in globals()[faction].members:
            name = self.name.replace("_", " ")
            popupList.append(Popup(
                f"Transmission From {name}",
                [f"{name} has declared war."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=25, ySize=5,
                flag1=self.flag, flag2=controlledCountry,
            ))
        elif controlledCountry in self.members:
            name = faction.replace("_", " ")
            popupList.append(Popup(
                f"Transmission From {name}",
                [f"{name} has declared war."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=25, ySize=5,
                flag1=globals()[faction].flag, flag2=controlledCountry,
            ))

        if (currentMusic == "game" and controlledCountry in globals()[faction].members) or \
           (currentMusic == "game" and controlledCountry in self.members):
            pygame.mixer.music.load(os.path.join(musicDir, "warMusic.mp3"))
            pygame.mixer.music.play(-1, fade_ms=2000)
            pygame.mixer.music.set_volume(musicVolume)
            currentMusic = "war"

    def kill(self):
        if controlledCountry != None:
            name = self.name.replace("_", " ")
            popupList.append(Popup(
                f"The End of {name}",
                [f"{name} has collapsed."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=22, ySize=5,
                flag1=self.flag, flag2=self.flag,
            ))

        for country in list(self.members):
            self.removeCountry(country)

        if self.name in factionList:
            factionList.remove(self.name)


class Country:
    def __init__(self, name, startRegions):
        countryList.append(name)

        self.totalMilitary = 0
        self.population = 0
        self.manPower = 0
        self.usedManPower = 0
        self.politicalPower = 0
        self.politicalMultiplier = 1
        self.moneyMultiplier = 1
        self.money = 0
        self.factories = 0
        self.baseStability = countries.getBaseStability(name)
        self.stability = self.baseStability
        self.lastStability = self.baseStability

        self.decisionTree = create_decision_tree(name)
        self.focusTreeEngine = FocusTreeEngine(
            FocusTreeLoader().load_tree(name))
        self.focus = None
        self.canMakeFaction = False
        self.hasChangedCountry = False
        self.expandedInvitations = False
        self.puppetTo = None

        self.resourceManager = ResourceManager()
        self.buildingManager = BuildingManager()
        self.leader = generate_leader(countries.getCulture(name),
                                      getIdeologyName(countries.getIdeologyName(name)))
        self.cabinet = generate_cabinet(countries.getCulture(name))
        self.electionSystem = ElectionSystem(
            getIdeologyName(countries.getIdeologyName(name)))
        self.combat_stats = CombatStats()

        self.faction = None
        self.factionColor = (255, 255, 255)
        self.factionLeader = False

        self.type = "country"
        self.name = name
        self.color = countries.getColor(self.name)
        self.regions = []
        self.ideology = countries.getIdeologyName(self.name)
        self.ideologyName = getIdeologyName(self.ideology)
        self.culture = countries.getCulture(self.name)
        self.coreRegions = countries.getClaims(self.name)
        self.regionsBeforeWar = []
        self.resetRegionsBeforeWar = True
        self.militarySize = 2
        self.capital = None
        self.cities = []
        self.addRegions(startRegions)
        self.cultures = {}
        self.lastIdeology = self.ideologyName

        self.atWarWith = []
        self.militaryAccess = [self.name]
        self.divisions = []
        self.divisionColor = (0, 0, 0)
        self.battleBorder = []
        self.training = []
        self.lastDayTrained = day
        self.deployRegion = None
        self.warScore = 0
        self.defenseMultiplier = 1
        self.attackMultiplier = 1
        self.transportMultiplier = 1
        self.toNavalInvade = []

        self.lastTimeActed = 0
        self.bordering = []
        self.lastBordering = []
        self.checkBordering = True
        self.checkBattleBorder = False
        self.lastAttackedBy = None

        self.building = []
        self.buildSpeed = 1

        self.didChangeIdeology = False
        self.capitulated = False

    def update(self):
        if self.resetRegionsBeforeWar and self.atWarWith == []:
            self.regionsBeforeWar = [i for i in self.regions]
            self.resetRegionsBeforeWar = False

        self.ideologyName = getIdeologyName(self.ideology)
        if self.lastIdeology != self.ideologyName:
            self.reloadCultureMap()
        self.lastIdeology = self.ideologyName

        if self.lastDayTrained != day:
            for cycle in self.training:
                if cycle[1] > 0:
                    cycle[1] -= 1

            legacy_daily = int(32000 * (1 + len(self.regions) / 150))
            for cycle in self.building:
                if cycle[1] > 0:
                    if self.money > legacy_daily and cycle[2] in self.regions:
                        self.money -= legacy_daily
                        cycle[1] -= 1
                    elif cycle[2] not in self.regions:
                        self.building.remove(cycle)

                if cycle[1] == 0:
                    if cycle[0] == 'factory':
                        self.addFactory(cycle[2])
                        self.building.remove(cycle)
                    elif cycle[0] == 'port':
                        self.addPort(cycle[2])
                        self.building.remove(cycle)

            if self.focus != None:
                self.focus[1] += -1
                if self.focus[1] <= 0:
                    focusCopy = [i for i in self.focus]
                    focus_name = focusCopy[0]
                    self.focus = None
                    for i in focusCopy[2]:
                        exec(i)
                    if focus_name in self.decisionTree:
                        self.decisionTree[focus_name][9] = True
                    fte = getattr(self, 'focusTreeEngine', None)
                    if fte:
                        fte.completed_focuses.add(focus_name)
                        node = fte.tree.get(focus_name)
                        if node and node.get('exclusive_group'):
                            fte.locked_groups.add(node['exclusive_group'])
                    if self.name == controlledCountry:
                        clappingSound.play()
                        popupList.append(Popup(
                            'Focus Completed',
                            [f'Finished "{focus_name}"'],
                            buttons=[['Okay', '', 0, 5.25]],
                            xSize=22, ySize=5,
                            flag1=self.name, flag2=self.name
                        ))

            self.lastDayTrained = day

        self.totalMilitary = 0
        for div in self.divisions:
            self.totalMilitary += div.units

        if self.totalMilitary < self.usedManPower:
            self.usedManPower -= len(self.regions) * speed / 10
        self.usedManPower = max(0, self.usedManPower)

        self.manPower = max(0, (1.00249688279 ** self.militarySize - 1) * self.population - self.usedManPower)

        self.stability = self.baseStability - math.log(len(self.atWarWith) + 1) * 3
        if self.faction:
            self.stability += 5
        self.stability = max(0, self.stability)
        self.stability = min(100, self.stability)
        self.baseStability = max(0, self.baseStability)
        self.baseStability = min(100, self.baseStability)

        if self.lastStability > self.stability and self.stability < 50 and self.name == controlledCountry:
            chance = 100 - self.stability * 2
            potential = random.randrange(0, 100)
            if chance > potential:
                possibleEnemies = [i for i in countries.getAllCountries(globals()[self.name].culture) if i not in countryList]
                if possibleEnemies == []:
                    enemy = 'Anarchist_State'
                else:
                    enemy = random.choice(possibleEnemies)
                self.civilWar(enemy)

        self.lastStability = self.baseStability - math.log(len(self.atWarWith) + 1) * 3

        self.politicalPower += 0.25 * speed / 5 / 12 * self.politicalMultiplier
        civ_count = self.buildingManager.get_building_count('civilian_factory')
        base_income = 5000 * (civ_count + 1)
        self.money += base_income * speed / 5 / 12 * self.moneyMultiplier
        div_upkeep = len(self.divisions) * DIVISION_UPKEEP_PER_DAY * speed / 5 / 12
        self.money -= div_upkeep
        self.money = max(0, self.money)
        self.money = min(self.money, len(self.regions) * 2500000)

        self.ideology[0] = min(1, self.ideology[0])
        self.ideology[0] = max(-1, self.ideology[0])
        self.ideology[1] = min(1, self.ideology[1])
        self.ideology[1] = max(-1, self.ideology[1])

        if self.lastDayTrained == day:
            self.resourceManager.tick(self, regions, speed)
            just_built = self.buildingManager.tick(speed, self.resourceManager.get_production_penalty(), self)
            for jb in just_built:
                if jb['type'] == 'dockyard':
                    self.addPort(jb['region'])
                bcolor = BUILDING_COLORS.get(jb['type'])
                if bcolor:
                    bx, by = regions.getLocation(jb['region'])
                    fill(industryMap, bx, by, bcolor)
                    if controlledCountry == self.name:
                        fill(modifiedIndustryMap, bx, by, bcolor)
            arms_count = self.buildingManager.get_building_count("arms_factory")
            self.combat_stats.recalculate(
                sum(d.divisionStack for d in self.divisions) if self.divisions else 1,
                arms_count)
            if hasattr(self, 'leader') and self.leader:
                bonus = getattr(self.leader, 'traits', {})
                self.moneyMultiplier = 1 + bonus.get('economy_bonus', 0)
                self.attackMultiplier = max(self.attackMultiplier,
                                            1 + bonus.get('military_bonus', 0))
            if hasattr(self, 'cabinet') and self.cabinet:
                cab_mods = self.cabinet.get_all_modifiers()
                self.moneyMultiplier += cab_mods.get('economy', 0)
                self.attackMultiplier = max(self.attackMultiplier,
                                            self.attackMultiplier + cab_mods.get('military', 0))
                self.politicalMultiplier = 1 + cab_mods.get('diplomacy', 0)
            if hasattr(self, 'electionSystem') and self.electionSystem:
                result = self.electionSystem.trigger_election(self, day)
                if result:
                    if result.get('new_leader'):
                        self.leader = result['new_leader']
                    if result.get('ideology_changed') and result.get('new_ideology'):
                        self.ideologyName = result['new_ideology']
                    if self.name == controlledCountry:
                        popupList.append(Popup(
                            result.get('title', 'Election'),
                            [result.get('text', 'An election was held.')],
                            buttons=[['Okay', '', 0, 5.25]],
                            xSize=22, ySize=5,
                            flag1=self.name, flag2=self.name
                        ))

        if (self.regions == [] or len(self.divisions) == 0 or self.cities == []) and len(self.regions) < len(self.regionsBeforeWar) and len(self.atWarWith) != 0 and not self.capitulated:
            for div in self.divisions:
                if div.region in self.regions:
                    div.kill()

            if len(self.atWarWith) != 0 and self.lastAttackedBy != None:
                globals()[self.lastAttackedBy].annexCountry(self.culture, country=self.name)
            elif len(self.atWarWith) != 0:
                globals()[self.atWarWith[0]].annexCountry(self.culture, country=self.name)

            if self.atWarWith != [] and controlledCountry != None:
                popupList.append(Popup(
                    f'The Fall of {self.name.replace("_", " ")}',
                    [f'{self.name.replace("_", " ")} has capitulated.'],
                    buttons=[['Okay', '', 0, 5.25]],
                    xSize=22, ySize=5,
                    flag1=self.name, flag2=self.name
                ))

            if self.atWarWith == []:
                self.kill()

            self.capitulated = True

        if self.atWarWith != []:
            for country in self.atWarWith:
                if not globals()[country].capitulated:
                    break
            else:
                peaceTreaty(self.name)

        if controlledCountry != self.name and speed > 0:
            self.runAI()

        if self.checkBattleBorder and self.atWarWith != []:
            originalBattleBorder = [i for i in self.battleBorder]
            self.battleBorder = self.getBattleBorder()
            if set(originalBattleBorder) != set(self.battleBorder):
                for div in self.divisions:
                    div.locked = False
                    div.navalLocked = False
            self.checkBattleBorder = False

        if self.checkBordering:
            originalBordering = [i for i in self.bordering]
            self.bordering = self.getBorderCountries()
            if set(self.bordering) != set(originalBordering):
                for div in self.divisions:
                    div.locked = False
                    div.navalLocked = False
            self.checkBordering = False

        for div in self.divisions:
            div.update()

    def runAI(self):
        if self.atWarWith != []:
            self.micro()

        if self.regions != [] and self.atWarWith == []:
            ownedPorts = set(ports).intersection(set(self.regions))
            if len(ownedPorts) / len(self.regions) < 0.01 or len(ownedPorts) == 0:
                for r in self.regions:
                    is_coastal = False
                    for conn in regions.getConnections(r):
                        cx, cy = regions.getLocation(conn)
                        if industryMap.get_at((round(cx), round(cy)))[:3] == (126, 142, 158):
                            is_coastal = True
                            break
                    if is_coastal and self.buildingManager.can_build(r, 'dockyard'):
                        dyn = get_dynamic_cost('dockyard', self)
                        if self.money >= dyn:
                            self.money -= dyn
                            bdef = BUILDING_DEFS.get('dockyard', {})
                            self.buildingManager.queue.append({
                                'type': 'dockyard', 'region': r,
                                'days_remaining': bdef.get('days', 60),
                                'total_days': bdef.get('days', 60),
                            })
                            break

        if len(self.building) < self.factories / 2 + 1 and len(self.regions) > len(self.building) + self.factories and self.atWarWith == []:
            self.build('factory', factoryBuildSpeed + random.randrange(-factoryBuildSpeed // 4, factoryBuildSpeed // 4))

    def micro(self):
        friendlyCountries = [country for country in countryList if self.name in globals()[country].militaryAccess and country in self.militaryAccess]
        enemyCountries = self.atWarWith

        friendlyDivCount = sum(div.divisionStack for country in friendlyCountries for div in globals()[country].divisions if not div.locked)
        enemyDivCount = sum(div.divisionStack for country in enemyCountries for div in globals()[country].divisions if not div.locked)

        friendlyStackCount = sum([len([d for d in globals()[country].divisions if not d.locked]) for country in friendlyCountries])
        enemyStackCount = sum([len([d for d in globals()[country].divisions if not d.locked]) for country in enemyCountries])

        friendlyDivisions = [div for country in friendlyCountries for div in globals()[country].divisions]

        border = self.battleBorder
        borderCount = len(self.battleBorder)

        stacksPerRegion = 2
        divsPerRegion = 8
        optimalStackSize = 4

        optimalStackCount = stacksPerRegion * borderCount
        optimalDivCount = divsPerRegion * borderCount

        commands = []

        if friendlyDivCount < optimalDivCount:
            while not (int(self.manPower / 10000) > 0 or self.militarySize >= 4):
                self.militarySize += 1
                self.manPower = max(0, (1.00249688279 ** self.militarySize - 1) * self.population - self.usedManPower)

            difference = optimalDivCount - friendlyDivCount
            divisionsToAdd = min(difference, int(self.manPower / 10000))

            if divisionsToAdd != 0:
                if self.deployRegion != None:
                    self.addDivision(divisionsToAdd, regions.getCityRegion(self.deployRegion))
                else:
                    self.addDivision(divisionsToAdd)

                self.mergeDivisions([div for div in self.divisions if div.region is self.divisions[-1].region and div.commands == []])

        if friendlyStackCount > optimalStackCount:
            unitMap = {}
            for div in self.divisions:
                if div.commands == [] and not div.fighting:
                    if unitMap.get(div.region) == None:
                        unitMap[div.region] = [div]
                    else:
                        unitMap[div.region].append(div)

            unitMap = {key: item for key, item in unitMap.items() if len(item) > 1}

            if unitMap != {}:
                potentiallyToMerge = None
                maxStacks = 0
                for key, item in unitMap.items():
                    if len(item) > maxStacks:
                        maxStacks = len(item)
                        potentiallyToMerge = item

                if potentiallyToMerge != None:
                    self.mergeDivisions(sorted(potentiallyToMerge, key=lambda div: div.divisionStack))

        if friendlyStackCount < optimalStackCount and set(self.bordering) & set(self.atWarWith) != {}:
            warDivisions = [div for div in self.divisions if not div.locked and not div.fighting and div.region in self.battleBorder]
            if warDivisions != []:
                div = max(warDivisions, key=lambda div: div.divisionStack)
                if div.divisionStack > 1:
                    self.divideDivision(div)

        if border != [] and not all(div.locked for div in self.divisions):
            availableDivs = [div for div in self.divisions if not div.locked and not div.fighting and div.commands == []]

            priorityMap = {}
            for region in border:
                priorityMap[region] = 2
                if region in self.regions:
                    priorityMap[region] += 1

            for region in list(set(self.regions) & set(ports)):
                priorityMap[region] = 1

            for div in friendlyDivisions:
                if div.region in priorityMap:
                    priorityMap[div.region] -= 1
                    if div in availableDivs:
                        availableDivs.remove(div)
                elif div.commands:
                    if div.commands[-1] in priorityMap:
                        priorityMap[div.commands[-1]] -= 1

            for div in availableDivs:
                region = max(priorityMap, key=priorityMap.get)
                priorityMap[region] -= 1
                if priorityMap[region] == 0:
                    break
                div.command(region, True, True)
                if div.commands == []:
                    div.attempts += 1
                    if div.attempts > 10:
                        div.locked = True

            availableDivs = [div for div in self.divisions if not div.fighting and div.commands == [] and (div.region in border or regions.getOwner(div.region) == None)]

            for div in availableDivs:
                optimalRegions = []
                optimalCount = 0
                for region in regions.getConnections(div.region):
                    if regions.getOwner(region) in self.atWarWith:
                        thisCount = 0
                        for close in regions.getConnections(region):
                            if close in self.regions:
                                thisCount += 1
                        if thisCount > optimalCount:
                            optimalCount = thisCount
                            optimalRegions = [region]
                        elif thisCount == optimalCount:
                            optimalRegions.append(region)
                if optimalRegions != []:
                    div.command(random.choice(optimalRegions), False, True)
            return

        if friendlyDivCount > enemyDivCount:
            if self.toNavalInvade == []:
                for country in self.atWarWith:
                    for region in globals()[country].regions:
                        for closeRegion in regions.getConnections(region):
                            if regions.getPopulation(closeRegion) == 0:
                                self.toNavalInvade.append(region)
                                break
                return

            if all(div.commands == [] for div in self.divisions) and not all(div.navalLocked for div in self.divisions):
                divs = [div for div in self.divisions if not div.navalLocked]
                toMove = random.choice(self.toNavalInvade)
                for div in divs:
                    div.command(toMove, False, False, 300)
                    if div.commands == []:
                        div.navalLocked = True
                self.toNavalInvade.remove(toMove)

    def getBorderCountries(self):
        bordered = set()
        for region in self.regions:
            connections = regions.getConnections(region)
            for connection in connections:
                country = regions.getOwner(connection)
                if country not in (self.name, None):
                    bordered.add(country)
        return list(bordered)

    def callToArms(self, country):
        for i in globals()[country].atWarWith:
            if i not in self.atWarWith:
                self.declareWar(i)
        for c in globals()[self.faction].members:
            if c not in self.militaryAccess:
                self.militaryAccess.append(c)
        if self.name not in globals()[country].militaryAccess:
            globals()[country].militaryAccess.append(self.name)

    def addFactory(self, region=None):
        if region == -1:
            regionList = [i for i in self.regions]
            random.shuffle(regionList)
            for r in regionList:
                x, y = regions.getLocation(r)
                if industryMap.get_at((round(x), round(y))) == (255, 255, 255):
                    region = r
                    break

        if region == -1:
            return
        if region in self.regions:
            x, y = regions.getLocation(region)
            if industryMap.get_at((round(x), round(y))) not in ((0, 255, 0), (255, 255, 0)):
                self.factories += 1
                if industryMap.get_at((round(x), round(y))) == (0, 0, 255):
                    ports.remove(region)
                fill(industryMap, x, y, (0, 255, 0))
                if controlledCountry == self.name:
                    fill(modifiedIndustryMap, x, y, (0, 255, 0))

    def build(self, name, days, region=-1):
        if region != -1 and region not in self.regions:
            return
        if region == -1:
            regionList = [i for i in self.regions]
            random.shuffle(regionList)
            for r in regionList:
                x, y = regions.getLocation(r)
                if industryMap.get_at((round(x), round(y))) == (255, 255, 255):
                    region = r
                    break
            else:
                return
        connects = False
        if name == 'port':
            for r in regions.getConnections(region):
                x1, y1 = regions.getLocation(r)
                if industryMap.get_at((round(x1), round(y1))) == (126, 142, 158):
                    connects = True
        if name == 'port' and not connects:
            return
        if region in ports:
            ports.remove(region)
        for build in self.building:
            if build[2] == region:
                self.building.remove(build)
        self.building.append([name, round(days * self.buildSpeed), region])
        x, y = regions.getLocation(region)
        color = (75, 75, 75)
        if name == 'port':
            color = (0, 0, 100)
        elif name == 'factory':
            color = (0, 100, 0)
        fill(industryMap, x, y, color)
        if controlledCountry == self.name:
            fill(modifiedIndustryMap, x, y, color)
        if self.name == controlledCountry:
            buildSound.play()

    def destroy(self, region):
        if region in self.regions:
            x, y = regions.getLocation(region)
            x, y = round(x), round(y)
            color = industryMap.get_at((x, y))
            if color not in ((255, 255, 0), (255, 0, 255)):
                if region in ports:
                    ports.remove(region)
                if color == (0, 255, 0):
                    self.factories -= 1
                for building in self.building:
                    if building[2] == region:
                        self.building.remove(building)
                fill(industryMap, x, y, (255, 255, 255))
                if self.name == controlledCountry:
                    fill(modifiedIndustryMap, x, y, (255, 255, 255))

    def reloadCultureMap(self):
        for region in self.regions:
            x, y = regions.getLocation(region)
            fillWithBorder(ideologyMap, map, x, y, getIdeologyColor(self.ideology))

    def addPort(self, region=-1):
        if region == -1:
            potentialRegions = []
            for r1 in self.regions:
                x, y = regions.getLocation(r1)
                if industryMap.get_at((round(x), round(y))) == (255, 255, 255):
                    for r2 in regions.getConnections(r1):
                        x, y = regions.getLocation(r2)
                        if industryMap.get_at((round(x), round(y))) == (126, 142, 158):
                            potentialRegions.append(r1)
                            break
            if potentialRegions != []:
                region = random.choice(potentialRegions)
        if region == -1 or region == None or region not in self.regions:
            return
        x, y = regions.getLocation(region)
        if industryMap.get_at((round(x), round(y))) in ((255, 255, 0),):
            return
        connects = False
        for r in regions.getConnections(region):
            x1, y1 = regions.getLocation(r)
            if industryMap.get_at((round(x1), round(y1))) == (126, 142, 158):
                connects = True
        if not connects or region not in self.regions:
            return
        if industryMap.get_at((round(x), round(y))) == (0, 255, 0):
            self.factories -= 1
        ports.append(region)
        fill(industryMap, x, y, (0, 0, 255))
        if controlledCountry == self.name:
            fill(modifiedIndustryMap, x, y, (0, 0, 255))

    def setIdeology(self, ideology):
        self.ideology = ideology
        self.ideologyName = getIdeologyName(self.ideology)
        self.didChangeIdeology = False

    def revolution(self, ideology):
        global popupList

        country = countries.getCountryType(self.culture, ideology)
        self.hasChangedCountry = True

        if country in countryList and self.ideologyName == countries.getIdeology(country):
            self.annexCountry(self.culture, country)
            self.replaceCountry(country)
            popupList.append(Popup(
                "A Nation Reborn",
                [f"Our nation has enshrined {getIsm(ideology)}."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=25, ySize=5,
                flag1=country, flag2=country,
            ))
        elif country in countryList:
            changeCountry(country, True)
            self.declareWar(country, False, False)
            popupList.append(Popup(
                f"The {self.culture.title()} Civil War",
                ["Our nation has fallen into civil war."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=25, ySize=5,
                flag1=country, flag2=self.name,
                sound=declareWarSound,
            ))
        elif self.ideologyName == countries.getIdeology(country):
            self.replaceCountry(country)
            popupList.append(Popup(
                "A Nation Reborn",
                [f"Our nation has enshrined {getIsm(ideology)}."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=25, ySize=5,
                flag1=country, flag2=country,
            ))
        else:
            self.civilWar(country)
            changeCountry(country, True)
            popupList = []
            popupList.append(Popup(
                f"The {self.culture.title()} Civil War",
                ["Our nation has fallen into civil war."],
                buttons=[["Okay", "", 0, 5.25]],
                xSize=25, ySize=5,
                flag1=country, flag2=self.name,
                sound=declareWarSound,
            ))

        if clicked == self.name:
            changeClicked(country)

    def replaceCountry(self, name):
        global selected
        if countries.colorToCountry(selected) == self.name:
            selected = countries.getColor(name)
        globals()[name] = Country(name, [])
        globals()[name].totalMilitary = self.totalMilitary
        globals()[name].population = self.population
        globals()[name].manPower = self.manPower
        globals()[name].usedManPower = self.usedManPower
        globals()[name].politicalPower = self.politicalPower
        globals()[name].money = self.money
        globals()[name].factories = self.factories
        globals()[name].baseStability = self.baseStability
        globals()[name].stability = self.stability
        globals()[name].training = self.training
        globals()[name].building = self.building
        globals()[name].politicalMultiplier = self.politicalMultiplier
        globals()[name].buildSpeed = self.buildSpeed
        globals()[name].decisionTree = self.decisionTree
        globals()[name].moneyMultiplier = self.moneyMultiplier
        globals()[name].focus = self.focus
        globals()[name].canMakeFaction = self.canMakeFaction
        globals()[name].hasChangedCountry = self.hasChangedCountry
        globals()[name].expandedInvitations = self.expandedInvitations
        globals()[name].puppetTo = self.puppetTo
        globals()[name].defenseMultiplier = self.defenseMultiplier
        globals()[name].attackMultiplier = self.attackMultiplier
        globals()[name].transportMultiplier = self.transportMultiplier
        globals()[name].addRegions(self.regions, True, ignoreResources=True)
        for div in self.divisions:
            div.country = name
            div.reloadIcon(div.color)
            globals()[name].divisions.append(div)
        if controlledCountry == self.name:
            changeCountry(name)
        for country in self.atWarWith:
            globals()[name].declareWar(country, False, False)
        reloadIndustryMap()
        self.atWarWith = []
        self.kill(True)

    def changeDeployment(self):
        if self.cities != []:
            for popup in popupList:
                if popup.title == 'Choose Deployment Location':
                    return
            buttons = []
            for num in range(len(self.cities)):
                buttons.append([
                    self.cities[num],
                    f"{self.name}.deployRegion = '{self.cities[num]}'",
                    -9 + num % 3 * 9,
                    3.625 + (num - num % 3) * 0.825
                ])
            popupList.append(Popup(
                'Choose Deployment Location', [],
                buttons=buttons, xSize=27.5,
                ySize=3.325 + (num - num % 3) * 0.825
            ))

    def independence(self, country):
        if country not in countryList:
            globals()[country] = Country(country, [x for x in countries.getClaims(country) if x in self.regions])
            self.militaryAccess.append(country)
        self.checkBattleBorder = True
        self.checkBordering = True

    def civilWar(self, country, popup=True):
        points = [regions.getLocation(id) for id in self.regions]
        angle = random.uniform(0, 2 * np.pi)
        direction = np.array([np.cos(angle), np.sin(angle)])
        centroid = np.mean(points, axis=0)
        reference_point = min(points, key=lambda p: np.linalg.norm(np.array(p) - centroid))
        list1, list2 = [], []
        for point in range(len(points)):
            vector = np.array(points[point]) - reference_point
            dot_product = np.dot(vector, direction)
            if dot_product > 0:
                list1.append(self.regions[point])
            else:
                list2.append(self.regions[point])
        for div in self.divisions:
            if div.region in list1:
                list1.remove(div.region)
        spawnCountry(country, list1)
        globals()[country].spawnDivisions()
        self.declareWar(country, False, False)
        for c in countryList:
            if c != country and countries.getCulture(c) == countries.getCulture(country) and c != self.name:
                globals()[country].declareWar(c, False, False)
        if controlledCountry in countryList and popup and self.regions == []:
            popupList.append(Popup(
                f'{self.culture} Coup',
                [f'{globals()[country].ideologyName.capitalize()} forces have overthrown {self.name.replace("_", " ")}.'],
                buttons=[['Okay', '', 0, 5.25]],
                xSize=22, ySize=5,
                flag1=self.name, flag2=country
            ))
            return
        if controlledCountry in countryList and popup:
            popupList.append(Popup(
                f'{self.culture} Civil War',
                [f'{globals()[country].ideologyName.capitalize()} partisans have risen up in {self.name.replace("_", " ")}.'],
                buttons=[['Okay', '', 0, 5.25]],
                xSize=22, ySize=5,
                flag1=self.name, flag2=country
            ))

    def declareWar(self, country, ignoreFaction=False, popup=True):
        global currentMusic
        if country in countryList and country not in self.atWarWith:
            self.checkBordering = True
            globals()[country].checkBordering = True
            self.checkBattleBorder = True
            globals()[country].checkBattleBorder = True
            self.resetRegionsBeforeWar = True
            globals()[country].resetRegionsBeforeWar = True
            if country in self.militaryAccess:
                self.militaryAccess.remove(country)
            if self.name in globals()[country].militaryAccess:
                globals()[country].militaryAccess.remove(self.name)
            if controlledCountry == self.name and globals()[country].faction == self.faction and self.faction != None:
                globals()[self.faction].removeCountry(self.name)
            if controlledCountry == self.name:
                globals()[country].resetDivColor((255, 0, 0))
            elif controlledCountry == country:
                self.resetDivColor((255, 0, 0))
            self.atWarWith.append(country)
            globals()[country].atWarWith.append(self.name)
            if controlledCountry in (country, self.name) and popup:
                enemy = self.name if country == controlledCountry else country
                popupList.append(Popup(
                    f'Transmission From {enemy.replace("_", " ")}',
                    [f'{enemy.replace("_", " ")} has declared war.'],
                    buttons=[['Okay', '', 0, 5.25]],
                    xSize=25, ySize=5,
                    flag1=self.name, flag2=enemy,
                    sound=declareWarSound
                ))
                if currentMusic == 'game':
                    pygame.mixer.music.load(os.path.join(musicDir, 'warMusic.mp3'))
                    pygame.mixer.music.play(-1, fade_ms=2000)
                    pygame.mixer.music.set_volume(musicVolume)
                    currentMusic = 'war'
            if ignoreFaction or self.faction == None:
                return
            if self.name == controlledCountry and globals()[country].faction != self.faction:
                globals()[self.faction].declareWar(globals()[country].faction)

    def makePeace(self, country, popup=True):
        if country in countryList and country in self.atWarWith:
            self.checkBordering = True
            globals()[country].checkBordering = True
            self.checkBattleBorder = True
            globals()[country].checkBattleBorder = True
            self.resetRegionsBeforeWar = True
            globals()[country].resetRegionsBeforeWar = True
            if controlledCountry == self.name:
                globals()[country].resetDivColor((0, 0, 0))
            elif controlledCountry == country:
                self.resetDivColor((0, 0, 0))
            self.atWarWith.remove(country)
            globals()[country].atWarWith.remove(self.name)
            if controlledCountry in (country, self.name) and popup:
                enemy = self.name if country == controlledCountry else country
                popupList.append(Popup(
                    f'Transmission From {enemy.replace("_", " ")}',
                    [f'{enemy.replace("_", " ")} has made peace.'],
                    buttons=[['Okay', '', 0, 5.25]],
                    xSize=25, ySize=5,
                    flag1=self.name, flag2=enemy
                ))

    def spawnDivisions(self):
        while not (self.militarySize >= 4 or self.manPower >= 10000):
            self.manPower = (1.00249688279 ** self.militarySize - 1) * self.population - self.usedManPower
            if self.manPower >= 10000:
                break
            self.militarySize += 1
        totalUnits = (1.00249688279 ** self.militarySize - 1) * self.population * 2 / 3
        totalDivisionCount = int(math.sqrt(len(self.regions) * 100) / 10)
        if totalUnits < 10000:
            totalUnits = 10000
        divisions = [1 for i in range(min(math.floor(totalUnits / 10000), math.floor(self.manPower / 10000)))]
        while len(divisions) > totalDivisionCount:
            index1 = random.randrange(0, len(divisions) - 1)
            index2 = index1 + 1
            divisions = divisions[:index1] + [divisions[index1] + divisions[index2]] + divisions[index1 + 1:index2] + divisions[index2 + 1:]
        for div in divisions:
            self.addDivision(div)

    def addRegion(self, id, ignoreFill=False, divRegion=None, ignoreResources=False):
        self.checkBordering = True
        if id not in self.regions:
            self.regions.append(id)
            country = regions.getOwner(id)
            if country != self.name and country != None and country in countryList:
                globals()[country].removeRegion(id)
            regions.updateOwner(id, self.name)
            x, y = regions.getLocation(id)
            color = cultureMap.get_at((round(x), round(y)))[:3]
            culture = countries.getCulture(countries.colorToCountry(color))
            if hasattr(self, 'cultures'):
                if culture not in self.cultures.keys():
                    self.cultures[culture] = [id]
                else:
                    self.cultures[culture].append(id)
            if country != None and country in countryList:
                if culture in globals()[country].cultures.keys():
                    if id in globals()[country].cultures[culture]:
                        globals()[country].cultures[culture].remove(id)
            city = regions.getCity(id)
            if city != None and hasattr(self, 'cities'):
                if city not in self.cities:
                    self.cities.append(city)
                if country != None and country in countryList:
                    if country != self.name and city in globals()[country].cities:
                        globals()[country].cities.remove(city)
                    if globals()[country].capital == city:
                        globals()[country].capital = None
                        if globals()[country].cities != []:
                            globals()[country].capital = random.choice(globals()[country].cities)
                if regions.getCityCulture(city) == self.culture:
                    self.capital = city
                elif self.capital == None:
                    self.capital = city
                if hasattr(self, 'deployRegion'):
                    if self.deployRegion == None or self.deployRegion not in self.cities or self.deployRegion != self.capital:
                        if self.capital in self.cities:
                            self.deployRegion = self.capital
                        elif self.cities != []:
                            self.deployRegion = random.choice(self.cities)
            color = industryMap.get_at((round(x), round(y)))[:3]
            if color == (0, 255, 0) and not ignoreResources:
                self.factories += 1
                globals()[country].factories -= 1
            if color in ((0, 100, 0), (75, 75, 75), (0, 0, 100)):
                fill(industryMap, x, y, (255, 255, 255))
            if country == controlledCountry:
                fill(modifiedIndustryMap, x, y, (100, 100, 100))
            if not ignoreResources:
                self.population += abs(regions.getPopulation(id))
            if country != self.name and country != None and country in countryList:
                globals()[country].population -= regions.getPopulation(id)
            if not ignoreFill:
                fillFixBorder(map, x, y, self.color)
                fillWithBorder(ideologyMap, map, x, y, getIdeologyColor(self.ideology))
                fillWithBorder(factionMap, map, x, y, self.factionColor)
                if controlledCountry == self.name:
                    color = industryMap.get_at((round(x), round(y)))
                    fill(modifiedIndustryMap, x, y, color)
            if country != None:
                if len(globals()[country].regions) == 0 and self.name == controlledCountry and self.name not in globals()[country].atWarWith:
                    popupList.append(Popup(
                        f'The Annexation of {country.replace("_", " ")}',
                        [f'{country.replace("_", " ")} has been annexed by {self.name.replace("_", " ")}.'],
                        buttons=[['Okay', '', 0, 5.25]],
                        xSize=25, ySize=5,
                        flag1=self.name, flag2=country
                    ))
        if hasattr(self, 'atWarWith'):
            if self.atWarWith != []:
                countriesToCheck = [i for i in self.atWarWith]
                for country in countryList:
                    if self.name in globals()[country].militaryAccess and country not in countriesToCheck:
                        countriesToCheck.append(country)
                for country in countriesToCheck:
                    toCheck = [i for i in regions.getConnections(id)]
                    toCheck.append(id)
                    if divRegion != None:
                        toCheck.append(divRegion)
                    for region in toCheck:
                        regionOwner = regions.getOwner(region)
                        for connection in regions.getConnections(region):
                            enemyCountry = regions.getOwner(connection)
                            if enemyCountry in globals()[country].atWarWith and regionOwner in globals()[country].militaryAccess:
                                if region not in globals()[country].battleBorder:
                                    globals()[country].battleBorder.append(region)
                                break
                        else:
                            if region in globals()[country].battleBorder:
                                globals()[country].battleBorder.remove(region)

    def addRegions(self, regionList, ignoreFill=False, ignoreResources=False, culture=None):
        for region in list(regionList):
            if culture == None or culture == countries.getCulture(regions.getOwner(region)):
                self.addRegion(region, ignoreFill, ignoreResources=ignoreResources)

    def removeRegion(self, id):
        if id in self.regions:
            self.regions.remove(id)

    def addDivision(self, divisions=1, region=None, ignoreManpower=False, ignoreTotalMilitary=False):
        if self.manPower >= divisions * 10000 or ignoreManpower:
            if len(self.regions) == 0:
                return
        else:
            return
        if region == None:
            region = random.choice(self.regions)
        self.divisions.append(Division(self.name, divisions, region, self.divisionColor))
        if not ignoreManpower:
            self.usedManPower += divisions * 10000

    def trainDivision(self, divisions=1):
        if divisions <= 0:
            return
        if self.manPower < divisions * 10000:
            if self.name == controlledCountry:
                show_toast(f"Not enough manpower (need {prefixNumber(divisions * 10000)})")
            return
        train_cost = TRAINING_COST_PER_DIV * divisions
        if self.money < train_cost:
            if self.name == controlledCountry:
                show_toast(f"Not enough money to train (need ${train_cost:,})")
            return
        self.money -= train_cost
        arms = self.buildingManager.get_building_count('arms_factory')
        base_days = 14
        reduced = max(3, int(base_days * max(0.25, 1.0 - arms * 0.15)))
        self.training.append([divisions, reduced])
        self.usedManPower += divisions * 10000
        self.totalMilitary += divisions * 10000

    def annexCountry(self, culture, country=None):
        if country != None:
            self.manPower += globals()[country].totalMilitary + globals()[country].manPower
            self.addRegions(globals()[country].regions)
        else:
            for c in countryList:
                if globals()[c].culture == culture:
                    self.manPower += globals()[c].totalMilitary + globals()[c].manPower
                    self.addRegions(globals()[c].regions)
                    return

    def resetDivColor(self, color):
        if color == None:
            color = self.divisionColor
        else:
            self.divisionColor = color
        for div in self.divisions:
            div.reloadIcon(self.divisionColor)
            div.color = self.divisionColor

    def getAccess(self):
        totalRegions = []
        for country in self.militaryAccess:
            if country in countryList:
                totalRegions = totalRegions + globals()[country].regions
        return totalRegions

    def getBattleBorder(self, ignoreAccess=False):
        borderRegions = []
        if ignoreAccess:
            toCheck = [i for i in self.regions]
        else:
            toCheck = self.getAccess()
        for region in toCheck:
            for connection in regions.getConnections(region):
                if regions.getOwner(connection) in self.atWarWith and region not in borderRegions:
                    borderRegions.append(region)
        return borderRegions

    def divideDivision(self, division):
        if round(division.divisionStack / 2) == 0 or division.fighting or division.divisionStack == 1:
            return
        stackSize = division.divisionStack // 2
        modifier = division.divisionStack % 2
        self.addDivision(stackSize + modifier, division.region, True, True)
        self.addDivision(stackSize, division.region, True, True)
        try:
            self.divisions[-1].resources = min(self.divisions[-2].maxResources, division.resources / 2)
            self.divisions[-2].resources = min(self.divisions[-2].maxResources, division.resources / 2)
            self.divisions[-2].units = min(self.divisions[-2].maxUnits, division.units * (stackSize + modifier) / stackSize / 2)
            self.divisions[-1].units = min(self.divisions[-2].maxUnits, division.units * stackSize / stackSize / 2)
        except IndexError:
            pass
        if division in self.divisions:
            self.divisions.remove(division)

    def mergeDivisions(self, divisions):
        sharedTiles = {}
        for div in divisions:
            if div.region not in sharedTiles and not div.fighting:
                sharedTiles[div.region] = [div]
            elif not div.fighting:
                sharedTiles[div.region].append(div)
        for region, divs in sharedTiles.items():
            totalDivisions = 0
            totalUnits = 0
            totalResources = 0
            for div in divs:
                totalDivisions += div.divisionStack
                totalUnits += div.units
                totalResources += div.resources
            self.addDivision(totalDivisions, region, True, True)
            self.divisions[-1].units = totalUnits
            self.divisions[-1].resources = totalResources
            self.divisions[-1].reloadIcon(self.divisionColor)
            for div in divs:
                if div in self.divisions:
                    self.divisions.remove(div)

    def kill(self, ignorePopup=False):
        global clicked, controlledCountry

        if not ignorePopup:
            if self.name == controlledCountry:
                popupList.append(Popup(
                    "The End of an Era",
                    [f"Our nation fell after {max(year - startDate[3], 0)} years, {max(month - startDate[2], 0)} months, and {max(day - startDate[1], 0)} days."],
                    buttons=[["Okay", "", 0, 5.25]],
                    xSize=22, ySize=5,
                    flag1=self.name, flag2=self.name,
                    sound=endGameSound,
                ))
                clicked = None

        for region in self.regions:
            if regions.getOwner(region) == self.name:
                regions.updateOwner(region, None)

        for country in self.atWarWith:
            if country in countryList and self.name in globals()[country].atWarWith:
                globals()[country].atWarWith.remove(self.name)

        if self.faction != None and self.name in globals()[self.faction].members:
            globals()[self.faction].members.remove(self.name)

        if self.name in countryList:
            countryList.remove(self.name)

        if controlledCountry == self.name:
            controlledCountry = None
            for country in countryList:
                globals()[country].resetDivColor((0, 0, 0))


def workerFunction(commandChunk):
    for command, args in commandChunk:
        if callable(command):
            command(*args)


class Division:
    def __init__(self, country, divisions, region, color=(0, 0, 0), units=None, resources=None, lastCountry=None):
        self.divisionStack = divisions
        self.maxUnits = 10000 * self.divisionStack
        if units == None:
            self.units = self.maxUnits
        else:
            self.units = units
        self.maxResources = 100 * self.divisionStack
        if resources == None:
            self.resources = self.maxResources
        else:
            self.resources = resources

        self.healthSize = -1
        self.resourceSize = -1

        self.country = country
        self.region = region
        self.location = regions.getLocation(region)
        self.currentColor = color
        self.color = color
        self.image = None
        self.reloadIcon(color)
        if lastCountry == None:
            self.lastCountry = self.country
        else:
            self.lastCountry = lastCountry
        self.type = 'division'

        self.xBlit = 0
        self.yBlit = 0

        self.attack = math.log(divisions) * 10 + 1
        self.defense = math.log(divisions) * 10 + 1
        self.movementSpeed = 5
        self.resourceUse = 1000

        c_obj = globals().get(country)
        if c_obj and hasattr(c_obj, 'combat_stats'):
            cs = c_obj.combat_stats
            self.attack += cs.attack * 0.1
            self.defense += cs.defense * 0.1
            self.movementSpeed += cs.speed * 0.1

        self.commands = []
        self.movement = self.movementSpeed

        self.fighting = False
        self.recovering = False

        self.attempts = 0
        self.locked = False
        self.navalLocked = False

    def reloadIcon(self, backGroundColor=None):
        if backGroundColor == None:
            backGroundColor = globals()[self.country].divisionColor

        flag = globals()[self.country.lower() + '_flag']

        healthSize = min(math.ceil(round(max((flag.get_width() + uiSize * 2) * self.units / self.maxUnits, 0)) / 10) * 10, flag.get_width() + uiSize * 2)
        resourceSize = min(math.ceil(round(max((flag.get_width() + uiSize * 2) * self.resources / self.maxResources, 0)) / 10) * 10, flag.get_width() + uiSize * 2)

        if backGroundColor != self.currentColor or int(healthSize / 10) != int(self.healthSize / 10) or int(resourceSize / 10) != int(self.resourceSize / 10) or self.image == None:
            text = getText(str(self.divisionStack), flag.get_height(), 'center')

            self.image = pygame.Surface((flag.get_width() + uiSize * 2 + flag.get_height() / 5, flag.get_height() + flag.get_height() / 5 + uiSize // 4))
            self.image.fill(backGroundColor)

            foreground = pygame.Surface((flag.get_width() + uiSize * 2, flag.get_height() + uiSize // 2 - uiSize // 4))
            foreground.fill((0, 0, 0))

            healthBar = pygame.Surface((healthSize, math.ceil(uiSize / 8)))
            healthBar.fill((34, 177, 76))

            resourceBar = pygame.Surface((resourceSize, math.ceil(uiSize // 8)))
            resourceBar.fill((185, 122, 87))

            self.image.blit(foreground, (flag.get_height() / 10, flag.get_height() / 10))
            self.image.blit(flag, (flag.get_height() / 10, flag.get_height() / 10))
            self.image.blit(text, ((foreground.get_width() - flag.get_width()) / 2 + flag.get_width() - text.get_width() / 2 + flag.get_height() / 10, flag.get_height() / 10))

            self.image.blit(healthBar, (flag.get_height() / 10, flag.get_height() + flag.get_height() / 10))
            self.image.blit(resourceBar, (flag.get_height() / 10, flag.get_height() + flag.get_height() / 10 + healthBar.get_height()))

        self.healthSize = healthSize
        self.resourceSize = resourceSize
        self.currentColor = backGroundColor

    def update(self):
        if self.units <= 0:
            self.kill()

        if self.commands != []:
            command = self.commands[0]

            x, y = regions.getLocation(command)
            color = map.get_at((round(x), round(y)))
            country = countries.colorToCountry(color[0:3])

            infra_bonus = 1.0
            c_obj = globals().get(self.country)
            if c_obj and hasattr(c_obj, 'buildingManager'):
                r_effects = c_obj.buildingManager.get_region_effects(self.region)
                infra_bonus += r_effects.get('movement_speed', 0)
            if country in globals()[self.country].militaryAccess or color[0:3] == (126, 142, 158):
                self.movement -= speed / 2 / 5 * globals()[self.country].transportMultiplier * infra_bonus
            else:
                self.movement -= speed / 10 / 5 * globals()[self.country].transportMultiplier * infra_bonus

            if self.movement < 0:
                self.movement = 0

            divInRegion = False
            if globals()[self.country].atWarWith != [] and regions.getOwner(command) != None:
                for country in globals()[self.country].atWarWith:
                    for div in globals()[country].divisions:
                        if div.region == command:
                            divInRegion = True
                            if not div.fighting and not self.fighting and self.resources >= self.maxResources:
                                battleList.append(Battle(self, div))
                                self.commands = []
                                break

            if self.movement <= 0 and self.commands != [] and not divInRegion:
                self.movement = self.movementSpeed
                command = self.commands.pop(0)
                self.move(command)

        if not self.resources >= self.maxResources and not self.fighting and globals()[self.country].money > speed / 5 * self.divisionStack / 2 * self.resourceUse:
            self.resources += speed / 5 * self.divisionStack / 2

            globals()[self.country].money -= speed / 5 * self.divisionStack / 2 * self.resourceUse * 2.5

            self.reloadIcon(self.currentColor)

            if self.resources < self.maxResources / 2:
                self.resources = self.maxResources / 2
            if self.resources >= self.maxResources:
                self.resources = self.maxResources

        self.updateLocation()

    def updateLocation(self):
        if self.location == None:
            return
        x, y = self.location

        x = normalize(x, map.get_width(), camx)

        self.xBlit = (x + camx - 0.5) * zoom + WIDTH / 2 - self.image.get_width() / 2
        self.yBlit = (y + camy - 0.5) * zoom + HEIGHT / 2

    def move(self, region):
        self.lastCountry = regions.getOwner(self.region)
        oldCountry = regions.getOwner(region)
        oldRegion = self.region
        self.region = region
        self.location = regions.getLocation(self.region)

        if oldCountry in globals()[self.country].atWarWith:
            if self.country != controlledCountry and self.lastCountry == None:
                self.commands == []

            if globals()[self.country].faction == None:
                globals()[self.country].addRegion(self.region, divRegion=oldRegion)
                globals()[oldCountry].lastAttackedBy = self.country
            else:
                for country in globals()[globals()[self.country].faction].members:
                    if region in globals()[country].regionsBeforeWar:
                        globals()[country].addRegion(self.region, divRegion=oldRegion)
                        globals()[oldCountry].lastAttackedBy = country
                        return

                if self.lastCountry in countryList and oldCountry in globals()[self.lastCountry].atWarWith:
                    globals()[self.lastCountry].addRegion(self.region, divRegion=oldRegion)
                    globals()[oldCountry].lastAttackedBy = self.lastCountry
                    return

                globals()[self.country].addRegion(self.region, divRegion=oldRegion)
                globals()[oldCountry].lastAttackedBy = self.country

    def command(self, region, ignoreEnemy=False, ignoreWater=True, iterations=100):
        if region == self.region:
            return

        startRegion = self.region
        finalRegion = region
        ignoreEnemy = ignoreEnemy
        ignoreWater = ignoreWater
        militaryAccess = globals()[self.country].militaryAccess
        atWarWith = globals()[self.country].atWarWith

        mapWidth = map.get_width()
        militaryAccess = globals()[self.country].militaryAccess
        atWarWith = globals()[self.country].atWarWith

        self.commands = pathfind(startRegion, finalRegion, ignoreEnemy, ignoreWater, militaryAccess, atWarWith, iterations)

        if self.commands != []:
            self.attempts = 0

    def kill(self):
        if self in globals()[self.country].divisions:
            globals()[self.country].divisions.remove(self)


def pathfind(startRegion, finalRegion, ignoreEnemy, ignoreWater, militaryAccess, atWarWith, maxIterations):
    commands = []
    frontier = []
    heapq.heappush(frontier, (0, startRegion))
    cameFrom = {}
    costSoFar = {}
    cameFrom[startRegion] = None
    costSoFar[startRegion] = 0

    xPos, y1 = regions.getLocation(finalRegion)

    militaryAccess = set(militaryAccess)
    atWarWith = set(atWarWith)

    while frontier:
        currentRegion = heapq.heappop(frontier)[1]

        if currentRegion == finalRegion:
            break

        currentRegionOwner = regions.getOwner(currentRegion)

        for nextRegion in regions.getConnections(currentRegion):
            nextRegionOwner = regions.getOwner(nextRegion)

            if (nextRegionOwner in militaryAccess
                    or (nextRegionOwner in atWarWith and not ignoreEnemy)
                    or (currentRegion in ports and nextRegionOwner == None and not ignoreWater)
                    or (nextRegionOwner == None and currentRegionOwner == None and not ignoreWater)
                    or (currentRegionOwner == None and nextRegion in canals and not ignoreWater)
                    or (currentRegion in canals and nextRegion in canals and not ignoreWater)
                    or (currentRegion in canals and nextRegionOwner == None and not ignoreWater)):

                newCost = costSoFar[currentRegion] + 1

                if nextRegion not in costSoFar or newCost < costSoFar[nextRegion]:
                    costSoFar[nextRegion] = newCost

                    x2, y2 = regions.getLocation(nextRegion)
                    x1 = wrap(xPos, x2, 1275)
                    x2 = wrap(x2, xPos, 1275)
                    priority = newCost + math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2)
                    heapq.heappush(frontier, (priority, nextRegion))
                    cameFrom[nextRegion] = currentRegion

        if len(cameFrom) > maxIterations:
            break

    if finalRegion in cameFrom:
        currentRegion = finalRegion
        while currentRegion != startRegion:
            commands.append(currentRegion)
            currentRegion = cameFrom[currentRegion]
        commands.reverse()
        return commands

    return []


class Battle:
    def __init__(self, attacker, defender):
        self.attacker = attacker
        self.defender = defender
        self.location = [(self.attacker.location[0] + self.defender.location[0]) / 2,
                         (self.attacker.location[1] + self.defender.location[1]) / 2]
        self.progress = 0
        self.image = None
        self.reloadImage()

        self.attackerBiome = regions.getBiomeInfo(biomeMap.get_at((round(attacker.location[0]), round(attacker.location[1])))[0:3])
        self.defenderBiome = regions.getBiomeInfo(biomeMap.get_at((round(defender.location[0]), round(defender.location[1])))[0:3])

        self.attacker.fighting = True
        self.defender.fighting = True

    def reloadImage(self):
        color = (50, 50, 50)
        if controlledCountry == self.defender.country:
            if self.progress:
                color = (215, 0, 0)
            else:
                color = (0, 215, 0)
        elif controlledCountry == self.attacker.country:
            if self.progress:
                color = (0, 215, 0)
            else:
                color = (215, 0, 0)

        self.image = pygame.Surface((uiSize * 2, uiSize * 2))
        self.image.fill((255, 0, 255))

        pygame.draw.circle(self.image, color, (uiSize, uiSize), uiSize)
        pygame.draw.circle(self.image, (0, 0, 0), (uiSize, uiSize), uiSize, uiSize // 8)

        battle = pygame.transform.scale(battle_element, (uiSize, uiSize))
        battle.set_colorkey(BLACK)
        self.image.blit(battle, (uiSize // 2, uiSize // 2))

        self.image.set_colorkey((255, 0, 255))

    def update(self):
        a_country = globals()[self.attacker.country]
        d_country = globals()[self.defender.country]

        a_cs = getattr(a_country, 'combat_stats', None)
        d_cs = getattr(d_country, 'combat_stats', None)

        a_piercing = a_cs.piercing if a_cs else 0
        d_armor = d_cs.armor if d_cs else 0
        d_piercing = d_cs.piercing if d_cs else 0
        a_armor = a_cs.armor if a_cs else 0
        a_speed = a_cs.speed if a_cs else 5
        d_speed = d_cs.speed if d_cs else 5

        atk_pen_mult = 1.0 if a_piercing >= d_armor else 0.5
        def_pen_mult = 1.0 if d_piercing >= a_armor else 0.5

        a_resource_pen = 1.0
        d_resource_pen = 1.0
        if hasattr(a_country, 'resourceManager'):
            a_resource_pen = a_country.resourceManager.get_combat_penalty()
        if hasattr(d_country, 'resourceManager'):
            d_resource_pen = d_country.resourceManager.get_combat_penalty()

        flank = 1.0 + max(0, a_speed - d_speed) * 0.05

        self.attacker.resources -= (self.defender.defense * self.defenderBiome[2] * speed / 25
                                    * self.attacker.divisionStack * a_country.attackMultiplier
                                    * def_pen_mult * d_resource_pen)
        self.defender.resources -= (self.attacker.attack * self.attackerBiome[1] * speed / 25
                                    * self.defender.divisionStack * d_country.defenseMultiplier
                                    * atk_pen_mult * a_resource_pen * flank)

        self.progress = self.attacker.resources > self.defender.resources

        if self.attacker.country == controlledCountry or self.defender.country == controlledCountry:
            self.reloadImage()

        if self.defender.resources < 0:
            self.attackerWin()

        if self.attacker.resources < 0:
            self.defenderWin()

        if self.attacker.resources < 0 or self.defender.resources < 0 or not self.defender or not self.attacker:
            chance = random.randrange(1, 6)
            if chance == 1:
                globals()[self.defender.country].destroy(self.defender.region)

            self.attacker.recovering = True
            self.defender.recovering = True

            self.attacker.fighting = False
            self.defender.fighting = False

            self.attacker.locked = False
            self.defender.locked = False

            self.attacker.reloadIcon(self.attacker.color)
            self.defender.reloadIcon(self.defender.color)

            if self.attacker.units < 0:
                self.attacker.kill()
            if self.defender.units < 0:
                self.defender.kill()

            self.attacker.commands = []
            self.defender.commands = []

            battleList.remove(self)
            del self

    def defenderWin(self):
        self.attacker.units -= max(self.defender.defense * 20 * self.defender.divisionStack, 0)
        self.defender.units -= max(self.attacker.attack * 5 * self.attacker.divisionStack, 0)

    def attackerWin(self):
        self.attacker.units -= max(self.defender.defense * 5 * self.defender.divisionStack, 0)
        self.defender.units -= max(self.attacker.attack * 20 * self.attacker.divisionStack, 0)

        oldRegion = self.defender.region

        maxConnections = 0
        toRetreat = None
        for connection in regions.getConnections(self.defender.region):
            connections = 0
            if connection in globals()[self.defender.country].getAccess():
                for region in regions.getConnections(connection):
                    if region in globals()[self.defender.country].getAccess():
                        connections += 1
                if connections > maxConnections:
                    toRetreat = connection
                    maxConnections = connections

        if toRetreat == None:
            self.defender.kill()
        else:
            self.defender.move(toRetreat)
            self.defender.movement = self.defender.movementSpeed

        divisionInRegion = False
        if globals()[self.defender.country].atWarWith != []:
            for country in globals()[self.attacker.country].atWarWith:
                for div in globals()[country].divisions:
                    if div.region == oldRegion:
                        divisionInRegion = True

        if not divisionInRegion:
            self.attacker.move(oldRegion)

    def draw(self):
        xBlit = (self.location[0] + camx - 0.5) * zoom + WIDTH / 2
        yBlit = (self.location[1] + camy - 0.5) * zoom + HEIGHT / 2

        screen.blit(self.image, (xBlit - uiSize, yBlit - uiSize))


class Popup:
    def __init__(self, title, text, buttons=[['Okay', '', 0, 5.25]], xSize=22, ySize=6, x=None, y=None, flag1=None, flag2=None, sound=None, btnHalfWidth=4):
        if x == None:
            x = WIDTH / 2
            if len(popupList) > 0:
                x = random.randrange(int(xSize * uiSize / 2), int(WIDTH - xSize * uiSize / 2))
        if y == None:
            y = HEIGHT / 2
            if len(popupList) > 0:
                y = random.randrange(int(ySize * uiSize / 2 + uiSize * 2), int(HEIGHT - ySize * uiSize / 2))
        self.title = title
        self.text = text
        self.buttons = buttons
        self.xSize = xSize
        self.ySize = ySize
        self.xBase = x
        self.yBase = y
        self.x = x
        self.y = y
        self.xOffset = 0
        self.yOffset = 0
        self.flag1 = flag1
        self.flag2 = flag2
        self.btnHalfWidth = btnHalfWidth
        self.type = 'popup'
        self.pressed = False
        self.image = self.reloadImage()
        if sound != None:
            sound.play()
        else:
            popupSound.play()

    def reloadImage(self):
        surface = pygame.Surface((self.xSize * uiSize, self.ySize * uiSize + 2 * uiSize))
        titleSurface = pygame.Surface((self.xSize * uiSize, uiSize * 2))
        surface.fill((50, 50, 50))
        surface.blit(titleSurface, (0, 0))
        drawText(surface, self.title, uiSize, self.xSize * uiSize / 2, uiSize)
        if self.flag1 != None:
            flag1 = pygame.image.load(os.path.join(flagsDir, f"{self.flag1.lower()}_flag.png")).convert()
            flag1 = pygame.transform.scale(flag1, (int(uiSize * flag1.get_width() / flag1.get_height() * 1.5), uiSize * 1.5))
            surface.blit(flag1, (uiSize * 2 - flag1.get_width() / 2, uiSize * 0.25))
        if self.flag2 != None:
            flag2 = pygame.image.load(os.path.join(flagsDir, f"{self.flag2.lower()}_flag.png")).convert()
            flag2 = pygame.transform.scale(flag2, (int(uiSize * flag2.get_width() / flag2.get_height() * 1.5), uiSize * 1.5))
            surface.blit(flag2, (surface.get_width() - uiSize * 2 - flag2.get_width() / 2, uiSize * 0.25))
        for i in range(len(self.text)):
            drawText(surface, self.text[i], uiSize, self.xSize * uiSize / 2, uiSize * (3 + i * 1.5))
        return surface

    def update(self):
        global pressed, holdingPopup
        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        if not pressed1:
            self.pressed = False
        if pressed1 and not pressed and not holdingPopup:
            for button in self.buttons:
                if self.xBase + button[2] * uiSize - uiSize * self.btnHalfWidth <= xMouse <= self.xBase + button[2] * uiSize + uiSize * self.btnHalfWidth:
                    if self.yBase - self.image.get_height() / 2 - uiSize * 0.875 + button[3] * uiSize <= yMouse <= self.yBase - self.image.get_height() / 2 + uiSize * 0.875 + button[3] * uiSize:
                        pressed = True
                        toExecute = button[1]
                        if len(self.text) >= 1:
                            toExecute = button[1].replace('self', self.text[0])
                        exec(toExecute)
                        if self in popupList:
                            popupList.remove(self)
        if not holdingPopup:
            if self.x - self.image.get_width() / 2 <= xMouse <= self.x + self.image.get_width() / 2:
                if self.yBase - self.image.get_height() / 2 <= yMouse <= self.yBase - self.image.get_height() / 2 + uiSize * 2:
                    if not self.pressed and pressed1:
                        holdingPopup = self
                        self.xOffset = xMouse
                        self.yOffset = yMouse
                        pressed = True
        if holdingPopup == self:
            self.xBase = self.x - self.xOffset + xMouse
            self.yBase = self.y - self.yOffset + yMouse
        if holdingPopup == self and not pressed1:
            holdingPopup = None
            self.x = self.xBase
            self.y = self.yBase
            pressed = True
        if pressed1:
            self.pressed = True

    def draw(self):
        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        screen.blit(self.image, (self.xBase - self.image.get_width() / 2, self.yBase - self.image.get_height() / 2))
        for button in self.buttons:
            color = (0, 0, 0)
            bhw = self.btnHalfWidth
            if self.xBase + button[2] * uiSize - uiSize * bhw <= xMouse <= self.xBase + button[2] * uiSize + uiSize * bhw:
                if self.yBase - self.image.get_height() / 2 - uiSize * 0.875 + button[3] * uiSize <= yMouse <= self.yBase - self.image.get_height() / 2 + uiSize * 0.875 + button[3] * uiSize:
                    color = (75, 75, 75)
                    if pressed1:
                        color = (150, 150, 150)
            pygame.draw.rect(screen, color, pygame.Rect(
                self.xBase - uiSize * bhw + button[2] * uiSize,
                self.yBase - self.image.get_height() / 2 - uiSize * 0.875 + button[3] * uiSize,
                uiSize * bhw * 2, uiSize * 1.75
            ))
            drawText(screen, button[0], uiSize, self.xBase + button[2] * uiSize, self.yBase - self.image.get_height() / 2 + button[3] * uiSize)


class TextBox(Popup):
    def __init__(self, title, buttons, xSize, ySize, x, y):
        self.text = ['']
        self.maxChars = 16
        super().__init__(title, self.text, buttons, xSize=xSize, ySize=ySize, x=x, y=y)
        self.type = 'textbox'
        self.holding = {
            'a': False, 'b': False, 'c': False, 'd': False, 'e': False,
            'f': False, 'g': False, 'h': False, 'i': False, 'j': False,
            'k': False, 'l': False, 'm': False, 'n': False, 'o': False,
            'p': False, 'q': False, 'r': False, 's': False, 't': False,
            'u': False, 'v': False, 'w': False, 'x': False, 'y': False,
            'z': False, 'SPACE': False, 'BACKSPACE': False
        }

    def update(self):
        super().update()
        pressed = self.getPressed()
        if pressed != None:
            if pressed == 'BACKSPACE':
                self.text[0] = self.text[0][:-1]
            elif pressed == 'SPACE' and self.maxChars > len(self.text[0]):
                self.text[0] += ' '
            elif self.maxChars > len(self.text[0]):
                self.text[0] += pressed
            self.image = self.reloadImage()

    def getPressed(self):
        keystate = pygame.key.get_pressed()
        for key in self.holding.keys():
            if keystate[pygame.key.key_code(key)] and not self.holding[key]:
                self.holding[key] = True
                if keystate[pygame.K_LSHIFT] or keystate[pygame.K_RSHIFT]:
                    return key.upper()
                else:
                    return key
            if not keystate[pygame.key.key_code(key)]:
                self.holding[key] = False
        return None


def independence(country, mainCountry=None):
    if country in countryList:
        return
    if mainCountry == None and controlledCountry != None:
        mainCountry = controlledCountry
    culture = countries.getCulture(country)
    regionsToSpawn = [i for i in countries.getClaims(country) if i in globals()[mainCountry].regions]
    popupList.append(Popup(
        f"{culture} Independence",
        [f"{country.replace('_', ' ')} has declared independence."],
        buttons=[
            ['Accept', f'spawnCountry("{country}", {regionsToSpawn}), {country}.spawnDivisions()', -4.5, 5.25],
            ['Reject', '', 4.5, 5.25],
            ['Play As', f'spawnCountry("{country}", {regionsToSpawn}), changeCountry("{country}", True, deletePopups=True), {country}.spawnDivisions()', 0, 7.75]
        ],
        xSize=22, ySize=7.5,
        flag1=mainCountry, flag2=country
    ))
    globals()[mainCountry].militaryAccess.append(country)
    globals()[mainCountry].checkBattleBorder = True
    globals()[mainCountry].checkBordering = True


class EventManager:
    def __init__(self, days):
        self.eventTimer = days
        self.eventFrequency = days
        self.lastDay = day
        self.globalTension = 0

    def update(self):
        if self.globalTension > 100:
            self.factionWar()
        if self.eventFrequency == 0:
            return
        if self.lastDay != day:
            self.eventTimer -= 1
            self.lastDay = day
        if self.eventTimer == 0:
            self.eventTimer = self.eventFrequency
            self.runEvent()

    def runEvent(self):
        events = []
        events.append(self.localWar)
        events.append(self.localWar)
        if factionList != []:
            events.append(self.joinFaction)
            events.append(self.joinFaction)
        if len(countryList) < 150:
            events.append(self.independence)
            events.append(self.independence)
        if len(factionList) < 4:
            events.append(self.createFaction)
            events.append(self.createFaction)
        events.append(self.civilWar)
        if controlledCountry != None:
            events.append(self.countryEvent)
        events.append(self.independence)
        randomEvent = random.choice(events)
        randomEvent()

    def countryEvent(self):
        self.globalTension += 1
        events = []
        canConflict = []
        for country in globals()[controlledCountry].bordering:
            if globals()[country].ideologyName not in (globals()[controlledCountry].ideologyName, 'nonaligned'):
                canConflict.append(country)
        if canConflict != []:
            country = random.choice(canConflict)
            events.append((self.borderConflict, (controlledCountry, country)))
        if events != []:
            randomEvent = random.choice(events)
            randomEvent[0](randomEvent[1])

    def borderConflict(self, defenderAndAttacker):
        defender, attacker = defenderAndAttacker
        if defender == controlledCountry:
            globals()[controlledCountry].baseStability -= 2
            popupList.append(Popup(
                f"{globals()[attacker].culture} Border Skirmish",
                [f"{globals()[attacker].name.replace('_', ' ')} has attacked troops on our border."],
                [['Okay', '', 0, 5.25]],
                xSize=22, ySize=5,
                flag1=defender, flag2=attacker
            ))

    def createFaction(self):
        self.globalTension += 1.5
        ideologies = ['communist', 'nationalist', 'liberal', 'monarchist']
        for faction in factionList:
            if globals()[faction].ideology in ideologies:
                ideologies.remove(globals()[faction].ideology)
        ideology = random.choice(ideologies)
        biggestCountry = None
        numRegions = 0
        for country in countryList:
            if globals()[country].ideologyName == ideology and len(globals()[country].regions) > numRegions and country != controlledCountry:
                biggestCountry = country
                numRegions = len(globals()[country].regions)
        if biggestCountry != None:
            spawnFaction([biggestCountry])

    def civilWar(self):
        self.globalTension += 1
        countriesShuffled = [i for i in countryList if i != controlledCountry]
        random.shuffle(countriesShuffled)
        toCivilWar = None
        for country in countriesShuffled:
            if [i for i in countries.getAllCountries(globals()[country].culture) if i not in countryList] == []:
                continue
            if globals()[country].atWarWith != []:
                continue
            toCivilWar = country
            break
        if toCivilWar != None:
            enemy = random.choice([i for i in countries.getAllCountries(globals()[toCivilWar].culture) if i not in countryList])
            globals()[toCivilWar].civilWar(enemy)

    def independence(self, country=None):
        self.globalTension += 0.5
        allCultures = countries.getCultures()
        currentCultures = []
        for c in countryList:
            if countries.getCulture(c) not in currentCultures:
                currentCultures.append(countries.getCulture(c))
        noCountry = [i for i in allCultures if i not in currentCultures]
        if noCountry == []:
            return
        culture = random.choice(noCountry)
        country = countries.getCountryType(culture)
        claims = countries.getClaims(country)
        if len(claims) == 0:
            return
        region = random.choice(claims)
        x, y = regions.getLocation(region)
        color = map.get_at((round(x), round(y)))[:3]
        mainCountry = countries.colorToCountry(color)
        if mainCountry == None:
            return
        regionsToSpawn = [i for i in claims if i in globals()[mainCountry].regions]
        if controlledCountry == None:
            return
        if controlledCountry == mainCountry:
            independence(country, mainCountry)
            return
        spawnCountry(country, regionsToSpawn)
        globals()[country].spawnDivisions()
        popupList.append(Popup(
            f"{culture} Independence",
            [f"{country.replace('_', ' ')} has declared independence."],
            [['Okay', '', 0, 5.25]],
            xSize=22, ySize=5,
            flag1=mainCountry, flag2=country
        ))
        globals()[mainCountry].checkBattleBorder = True
        globals()[mainCountry].checkBordering = True

    def joinFaction(self):
        self.globalTension += 0.5
        modifiedFactionList = [i for i in factionList if globals()[i].members[0] != controlledCountry]
        if not modifiedFactionList:
            return
        faction = random.choice(modifiedFactionList)
        countryToAdd = None
        members = [i for i in globals()[faction].members]
        random.shuffle(members)
        for country in members:
            borderCountries = globals()[country].getBorderCountries()
            random.shuffle(borderCountries)
            for bordering in borderCountries:
                if globals()[bordering].ideologyName in (globals()[faction].ideology, 'nonaligned') and globals()[bordering].faction == None and globals()[bordering].atWarWith == []:
                    countryToAdd = bordering
                    break
            else:
                continue
            break
        if countryToAdd != None:
            globals()[faction].addCountry(countryToAdd)

    def localWar(self):
        self.globalTension += 1.75
        shuffledCountries = [i for i in countryList]
        random.shuffle(shuffledCountries)
        attackingCountry = None
        defendingCountry = None
        for country in shuffledCountries:
            if globals()[country].stability > 60 or country == controlledCountry:
                continue
            for borderingCountry in globals()[country].getBorderCountries():
                if globals()[borderingCountry].ideologyName not in ('nonaligned', globals()[country].ideologyName) and not globals()[country].faction and not globals()[borderingCountry].faction and borderingCountry not in globals()[country].atWarWith:
                    attackingCountry = country
                    defendingCountry = borderingCountry
                    break
        if attackingCountry == None or defendingCountry == None:
            return
        globals()[attackingCountry].declareWar(defendingCountry)
        if controlledCountry == None:
            return
        popupList.append(Popup(
            f"The {countries.getCulture(attackingCountry)}-{countries.getCulture(defendingCountry)} War",
            [f"{attackingCountry.replace('_', ' ')} has declared war on {defendingCountry.replace('_', ' ')}."],
            [['Okay', '', 0, 5.25]],
            xSize=22, ySize=5,
            flag1=attackingCountry, flag2=defendingCountry
        ))

    def factionWar(self):
        self.globalTension = 0
        if len(factionList) >= 2:
            faction1 = random.choice(factionList)
            faction2 = random.choice([i for i in factionList if i != faction1])
            globals()[faction1].declareWar(faction2)


def distance(point1, point2):
    return math.sqrt((point1[0] - point2[0]) ** 2 + (point1[1] - point2[1]) ** 2)


def largestDistance(points):
    max_distance = 0
    for i in range(len(points)):
        for j in range(i + 1, len(points)):
            dist = distance(points[i], points[j])
            if dist > max_distance:
                max_distance = dist
    return max_distance


def changeCountry(country, reloadMap=True, deletePopups=False):
    global controlledCountry, controlledCountryFlag, camx, camy, zoom, popupList

    if country in countryList:
        if controlledCountry in countryList:
            globals()[controlledCountry].resetDivColor((0, 0, 0))

        controlledCountry = country
        controlledCountryFlag = pygame.image.load(
            os.path.join('flags', f'{controlledCountry.lower()}_flag.png')
        ).convert()

        if reloadMap:
            reloadIndustryMap()

    toCheck = list(
        set(
            globals()[controlledCountry].cultures.get(
                globals()[controlledCountry].culture, []
            )
        ).intersection(set(globals()[controlledCountry].regions))
    )

    if toCheck == []:
        toCheck = globals()[controlledCountry].regions

    i = 0
    xSum = 0
    ySum = 0
    locations = []
    for region in toCheck:
        x, y = regions.getLocation(region)
        xSum -= x
        ySum -= y
        locations.append((x, y))
        i += 1

    camx = xSum / i
    camy = ySum / i
    distance = largestDistance(locations)
    if distance == 0:
        zoom = 46.005119909369675
    else:
        zoom = (WIDTH / distance + 16) / 5
        zoom = max(min(zoom, 46.005119909369675), 0.6944444444444445)

    for c in countryList:
        if c in globals()[controlledCountry].atWarWith:
            color = (255, 0, 0)
        elif c == controlledCountry:
            color = (0, 255, 0)
        elif globals()[controlledCountry].faction == None:
            color = (0, 0, 0)
        elif c in globals()[globals()[controlledCountry].faction].members:
            color = (0, 0, 255)
        else:
            color = (0, 0, 0)

        globals()[c].divisionColor = color
        globals()[c].resetDivColor(color)

    if deletePopups:
        popupList = []


BUILDING_COLORS = {
    'civilian_factory': (0, 0, 255),
    'arms_factory': (255, 0, 0),
    'dockyard': (0, 255, 255),
    'mine': (139, 90, 43),
    'oil_well': (30, 30, 30),
    'refinery': (255, 165, 0),
    'infrastructure': (255, 255, 0),
}


def reloadIndustryMap():
    global modifiedIndustryMap

    modifiedIndustryMap = industryMap.copy()

    for cname in countryList:
        c_obj = globals().get(cname)
        if c_obj is None or not hasattr(c_obj, 'buildingManager'):
            continue
        for region_id, blist in c_obj.buildingManager.buildings.items():
            if not blist:
                continue
            last_type = blist[-1].get('type', '')
            color = BUILDING_COLORS.get(last_type)
            if color is None:
                continue
            x, y = regions.getLocation(region_id)
            fill(industryMap, round(x), round(y), color)
            fill(modifiedIndustryMap, round(x), round(y), color)

    if controlledCountry == None:
        return

    for region in range(1, 3715):
        x, y = regions.getLocation(region)
        color = industryMap.get_at((round(x), round(y)))

        if region not in globals()[controlledCountry].regions and color != (126, 142, 158):
            fill(modifiedIndustryMap, round(x), round(y), (100, 100, 100))


RESOURCE_COLORS = {
    'oil': (50, 50, 60),
    'steel': (100, 140, 220),
    'aluminum': (170, 220, 255),
    'tungsten': (220, 150, 40),
    'chromium': (200, 60, 200),
    'rubber': (40, 190, 40),
}

resourceMap = None


def generateResourceMap():
    global resourceMap
    if industryMap is None:
        return
    resourceMap = industryMap.copy()

    for region_id in range(1, 3715):
        x, y = regions.getLocation(region_id)
        rx, ry = round(x), round(y)
        res_data = regions.getResources(region_id)
        if res_data:
            dominant = max(res_data, key=res_data.get)
            color = RESOURCE_COLORS.get(dominant, (200, 200, 200))
            fill(resourceMap, rx, ry, color)
        else:
            owner = regions.getOwner(region_id)
            if owner and controlledCountry and owner == controlledCountry:
                fill(resourceMap, rx, ry, (25, 25, 25))
            elif owner:
                fill(resourceMap, rx, ry, (15, 15, 15))


def getFactionName(country):
    factionNames = list(('Alliance', 'Confederation', 'Dominion', 'Federation',
                         'League', 'Coalition', 'Brotherhood', 'Commonwealth',
                         'Syndicate', 'Authority', 'Order', 'Consortium',
                         'Assembly', 'Collective', 'Alliance', 'Bloc', 'Front',
                         'Pact', 'Accord', 'Community', 'Guild', 'Compact',
                         'Directorate', 'Realm'))

    factionDescriptors = []
    factionDescriptors.append(getIdeologyName(globals()[country].ideology).capitalize())
    factionDescriptors.append(globals()[country].culture)

    if globals()[country].capital != None:
        factionDescriptors.append(globals()[country].capital)

    names = []
    for region in globals()[country].regions:
        x, y = regions.getLocation(region)
        name = regions.getRegionAdjective(regionsMap.get_at((round(x), round(y)))[:3])
        if name not in names and name != 'Error':
            names.append(name)

    factionDescriptors = factionDescriptors + names

    return f'{random.choice(factionDescriptors)}_{random.choice(factionNames)}'


def regionClicked(mousex, mousey):
    xAdjust = -((-camx - WIDTH / 2 / zoom) - math.trunc(-camx - WIDTH / 2 / zoom))
    yAdjust = -((-camy - HEIGHT / 2 / zoom) - math.trunc(-camy - HEIGHT / 2 / zoom))

    x = math.ceil((mousex - xAdjust - WIDTH / 2) / zoom - camx + zoom - int(zoom)) % map.get_width()
    y = math.ceil((mousey - yAdjust - HEIGHT / 2) / zoom - camy + zoom - int(zoom))

    x = max(0, x)
    x = min(x, map.get_width())
    y = max(0, y)
    y = min(y, map.get_height())

    return regions.getRegion(regionsMap.get_at((x, y))[:3])


def textPopup():
    popupList.append(TextBox(
        'Save Name',
        [list(('Okay', 'if len("self") > 0:\n  saveGame("self")', 0, 5.25))],
        22, 5,
        WIDTH / 2, HEIGHT / 2
    ))


def overwritePopup():
    saves = os.listdir(savesDir)
    numSaves = len([name for name in saves])
    btns = [list(('Back', 'mainPopup()', 0, 3.625))] + \
           [[f'{saves[i - 1]}', f"saveGame('{saves[i - 1]}')", 0, 3.625 + i * 2.5]
            for i in range(1, len(saves) + 1)]
    popupList.append(Popup(
        'Overwrite Save', [],
        btns, 9.5, 3.5 + numSaves * 2.5,
        WIDTH / 2, HEIGHT / 2
    ))


def savesPopup():
    popupList.append(Popup(
        'Save Game', [],
        [list(('Back', 'mainPopup()', 0, 3.625)),
         list(('New Save', 'textPopup()', 0, 6.125)),
         list(('Overwrite Save', 'overwritePopup()', 0, 8.625))],
        9.5, 8.5,
        WIDTH / 2, HEIGHT / 2
    ))


def mainPopup():
    popupList.append(Popup(
        'Menu', [],
        [list(('Resume', '', 0, 3.625)),
         list(('Save Game', 'savesPopup()', 0, 6.125)),
         list(('Main Menu', 'setup(), pygame.mixer.music.stop(), mainMenu()', 0, 8.625)),
         list(('Exit Game', 'sys.exit()', 0, 11.125))],
        9.5, 11,
        WIDTH / 2, HEIGHT / 2
    ))


def startPopup():
    popupList.append(Popup(
        'A New Dawn',
        ['The world stands on a precipice as nations scramble to forge the',
         f'new world order. Our country, {controlledCountry.replace("_", " ")}, finds itself',
         'struggling, but with your leadership, we will push forward into this',
         'brave new era. Whether our country rises to greatness or collapses',
         'as enemy forces march into our capital is up to your leadership.',
         '',
         '',
         'Tutorials:'],
        [list(('Start', '', 0, 11.125)),
         list(('Controls', 'controlsPopup()', -9, 16.125)),
         list(('Politics', 'politicsPopup()', 0, 16.125)),
         list(('Industry', 'industryPopup()', 9, 16.125)),
         list(('Military', 'militaryPopup()', -9, 18.625)),
         list(('Factions', 'factionsPopup()', 0, 18.625)),
         list(('Events', 'eventsPopup()', 9, 18.625)),
         list(('Economy', 'economyPopup()', -9, 21.125)),
         list(('Focus Tree', 'focusTreePopup()', 0, 21.125)),
         list(('Leaders', 'leadersPopup()', 9, 21.125))],
        27.5, 21.325,
        WIDTH / 2, HEIGHT / 2,
        controlledCountry, controlledCountry
    ))


def controlsPopup():
    text = list(('W / A / S / D, up / left / down / right : camera controls',
                 'space : open / close side bar, close decision tree',
                 'escape : open menu',
                 'f1 : screenshot',
                 'f2 : disable / enable UI',
                 'f3 : disable / enable timelapse divisions',
                 'f4 : tutorial',
                 'f5 : population statistics',
                 'f6 : military size statistics',
                 'f7 : region count statistics',
                 'f8 : industry statistics',
                 'left click : UI controls',
                 'right click : division controls',
                 'brackets, scroll wheel\u200b : zoom in / out'))
    popupList.append(Popup(
        'Controls', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def politicsPopup():
    text = list(('Political actions require political power (PP).',
                 'PP is gained passively and boosted by your cabinet.',
                 'Actions include annexations, ideology shifts, diplomacy,',
                 'faction creation, and more.',
                 'Your country now has a Leader with trait bonuses and a',
                 'Cabinet with 4 ministers (economy, military, diplomacy,',
                 'intelligence) that apply modifiers to your country.',
                 'Random events like scandals, economic shocks, military',
                 'incidents, and leader death can occur over time.',
                 'Elections may change your leader and ideology.',
                 'Ideology affects alliances and available focus paths.'))
    popupList.append(Popup(
        'Politics', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def industryPopup():
    text = list(('Industry now includes 9 building types.',
                 'Civilian factories speed up construction and produce money.',
                 'Arms factories boost military equipment and combat stats.',
                 'Dockyards support naval construction.',
                 'Mines increase resource extraction in a region.',
                 'Oil wells extract oil, refineries process it.',
                 'Airbases and naval bases support military projection.',
                 'Infrastructure boosts movement, supply, and build speed.',
                 'Open the industry tab and click Change Construction to',
                 'choose which building to place, then right click a region.',
                 'Buildings are visible on the production map (press 3).',
                 'Each building type has its own color on the map.',
                 'Ports and canals still function as before.'))
    popupList.append(Popup(
        'Industry', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def militaryPopup():
    text = list(('To move a division, right click it to select, then right',
                 'click on the region you want it to move to.',
                 'Drag over multiple divisions to select them simultaneously.',
                 'Hold right click over multiple regions to draw front lines.',
                 'Use buttons on screen to merge or divide division stacks.',
                 'Combat now uses expanded stats: attack, defense, armor,',
                 'piercing, speed, fuel use, and supply use.',
                 'If your piercing exceeds enemy armor, you deal full damage.',
                 'Otherwise you deal half damage. Speed gives flanking bonuses.',
                 'Resource deficits (oil, rubber, etc.) reduce combat power.',
                 'Arms factories boost your armor and piercing stats.',
                 'Combat stats are visible in the political sidebar.',
                 'Divisions can only move onto water via ports or canals.'))
    popupList.append(Popup(
        'Military', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def eventsPopup():
    text = list(('Events occur randomly as time goes on.',
                 'An event being triggered will increase world tension.',
                 'A world war will happen if world tension goes over 100.',
                 'Examples include the creation of factions, wars, civil wars,',
                 'border skirmishes, and more.'))
    popupList.append(Popup(
        'Events', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def factionsPopup():
    text = list(('Factions are groups of countries in defensive pacts.',
                 'Being in a faction will deter countries from attacking you,',
                 'and will come to your aid in the event that you are.',
                 'You can join a faction if invited, or create your own through',
                 'political actions in the decision tree and side bar.',
                 'If you are the leader of a faction, you must invite countries',
                 'through political actions for them to join.',
                 'Countries can only be in a faction together if they all have',
                 'the same ideology, or are nonaligned.',
                 'Being in a faction provides a small stability boost.'))
    popupList.append(Popup(
        'Factions', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def economyPopup():
    text = list(('Your country now manages 6 strategic resources:',
                 'Oil, Steel, Aluminum, Tungsten, Chromium, and Rubber.',
                 'Resources are produced by provinces that contain them.',
                 'Click a region to see its resources in the sidebar.',
                 'Mines boost resource extraction; oil wells produce oil.',
                 'Resources are consumed by your military and industry.',
                 'Deficits reduce combat readiness and production speed.',
                 'Check the Industry tab to see resource stockpiles and',
                 'net production for each resource (green = surplus).',
                 'Puppet states provide resource tribute to overlords.'))
    popupList.append(Popup(
        'Economy & Resources', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def focusTreePopup():
    text = list(('The Focus Tree (Decision Tree) drives your strategy.',
                 'Open it from the political sidebar button.',
                 'Each focus costs political power and takes days to finish.',
                 'Focuses give bonuses: factories, stability, attack, money.',
                 'Hover over a focus to see its effects and requirements.',
                 'Green dependency lines = prerequisite met.',
                 'Red dependency lines = prerequisite NOT met.',
                 'Focus colors: Black = available, Dark red = locked,',
                 'Dark green = completed, Yellow border = too expensive.',
                 'Some focuses are mutually exclusive (picking one locks',
                 'the other branch). USA, Germany, and Russia have large',
                 'unique trees with 50+ focuses each.'))
    popupList.append(Popup(
        'Focus Tree', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


def leadersPopup():
    text = list(('Each country now has a national Leader and a Cabinet.',
                 'Leaders have traits that give economy and military bonuses.',
                 'Cabinet ministers cover 4 portfolios: Economy, Military,',
                 'Diplomacy, and Intelligence, each with their own modifier.',
                 'Leader and cabinet names match the country culture.',
                 'Leaders can die of old age; a new leader will take over.',
                 'Democratic countries hold periodic elections that may',
                 'change the ruling ideology and install a new leader.',
                 'Check the Political tab to see your leader and cabinet.'))
    popupList.append(Popup(
        'Leaders & Cabinet', text,
        [['Okay', 'startPopup()', 0, len(text) * 1.5 + 0.5 + 3]],
        24, len(text) * 1.5 + 1 + 2,
        WIDTH / 2, HEIGHT / 2
    ))


class Controller:

    def camera(self):
        global camx, camy, zoom, sideBarScroll
        keystate = pygame.key.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        cameraSpeed = 8

        canMove = True
        for popup in popupList:
            if popup.type == 'textbox':
                canMove = False
                break

        if keystate[pygame.K_LSHIFT] or keystate[pygame.K_RSHIFT]:
            cameraSpeed *= 3

        if (keystate[pygame.K_s] and canMove) or (keystate[pygame.K_DOWN] and canMove):
            camy -= cameraSpeed * 60 / FPS / zoom

        if (keystate[pygame.K_w] and canMove) or (keystate[pygame.K_UP] and canMove):
            camy += cameraSpeed * 60 / FPS / zoom

        if (keystate[pygame.K_d] and canMove) or (keystate[pygame.K_RIGHT] and canMove):
            camx -= cameraSpeed * 60 / FPS / zoom

        if (keystate[pygame.K_a] and canMove) or (keystate[pygame.K_LEFT] and canMove):
            camx += cameraSpeed * 60 / FPS / zoom

        if camx < -map.get_width():
            camx += map.get_width()
        elif camx > 0:
            camx -= map.get_width()

        if camy <= -map.get_width() / 2:
            camy = -map.get_width() / 2
        if camy >= 0:
            camy = 0

        for event in pygame.event.get():
            pygame.event.post(event)
            if event.type == pygame.MOUSEBUTTONDOWN:
                if xMouse > sideBarSize * sideBarAnimation * WIDTH + 3:
                    if event.button == 4 and zoom < 237.3763137997696:
                        zoom *= 1.2
                        camx = camx - (xMouse - WIDTH / 2) / 5 / zoom
                        camy = camy - (yMouse - HEIGHT / 2) / 5 / zoom
                    elif event.button == 5 and zoom > 0.6944444444444445:
                        zoom /= 1.2
                        camx = camx + (xMouse - WIDTH / 2) / 5 / zoom
                        camy = camy + (yMouse - HEIGHT / 2) / 5 / zoom
                else:
                    if event.button == 4:
                        sideBarScroll -= 80
                    elif event.button == 5:
                        sideBarScroll += 80
                    if sideBarScroll < 0:
                        sideBarScroll = 0

    def window(self):
        global WIDTH, HEIGHT
        for event in pygame.event.get():
            pygame.event.post(event)

            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

    def sideBar(self):
        global holdingSideBar, sideBarSize, xPressed, yPressed, selected, openedTab, openedPoliticalTab
        keystate = pygame.key.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        if pressed1 and sideBarSize * WIDTH * sideBarAnimation - 4 <= xMouse <= sideBarSize * WIDTH * sideBarAnimation + 4:
            holdingSideBar = True
            sideBarSize = xMouse / WIDTH
            if sideBarSize < 0.1:
                sideBarSize = 0.1
            elif sideBarSize > 1:
                sideBarSize = 1
            xPressed = 0
            yPressed = 0
        elif holdingSideBar and pressed1:
            holdingSideBar = True
            sideBarSize = xMouse / WIDTH
            if sideBarSize < 0.1:
                sideBarSize = 0.1
            elif sideBarSize > 1:
                sideBarSize = 1
            xPressed = 0
            yPressed = 0
        if holdingPopup:
            xPressed = 0
            yPressed = 0

        for event in pygame.event.get():
            pygame.event.post(event)
            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                if controlledCountry == None:
                    pass
                elif buttons[0][0] <= xMouse <= buttons[0][1] and buttons[0][2] <= yMouse <= buttons[0][3]:
                    selected = countries.getColor(controlledCountry)
                    openedTab = 'political'
                    showResources = False
                    clickedSound.play()
                elif buttons[1][0] <= xMouse <= buttons[1][1] and buttons[1][2] <= yMouse <= buttons[1][3]:
                    selected = countries.getColor(controlledCountry)
                    openedTab = 'military'
                    showResources = False
                    clickedSound.play()
                elif buttons[2][0] <= xMouse <= buttons[2][1] and buttons[2][2] <= yMouse <= buttons[2][3]:
                    selected = countries.getColor(controlledCountry)
                    openedTab = 'industry'
                    clickedSound.play()

                if politicalTabs != {}:
                    for name, location in politicalTabs.items():
                        if location[0] <= xMouse <= location[1] and location[2] <= yMouse <= location[3]:
                            if openedPoliticalTab != name:
                                openedPoliticalTab = name
                            else:
                                openedPoliticalTab = None
                            clickedSound.play()

                if politicalButtonHovered == None:
                    continue

                canDoAction = True
                for resource, cost in politicalButtonHovered[1].items():
                    if getattr(globals()[controlledCountry], resource) < cost:
                        canDoAction = False

                if canDoAction:
                    for resource, cost in politicalButtonHovered[1].items():
                        setattr(globals()[controlledCountry], resource, getattr(globals()[controlledCountry], resource) - cost)
                    for action in politicalButtonHovered[2]:
                        exec(action, globals(), locals())
                    clickedSound.play()
                else:
                    missing = []
                    for resource, cost in politicalButtonHovered[1].items():
                        if getattr(globals()[controlledCountry], resource) < cost:
                            missing.append(f"{resource}: need {cost:.0f}")
                    show_toast(f"Not enough resources ({', '.join(missing)})")

    def input(self):
        global camx, camy, zoom, clicked, selected, openedTab, currentMap, speed
        global showDivisions, showCities, showResources, holdingSideBar, pressed, selectedRegions
        global selectedDivisions, selectedRegion, currentlyBuilding, flagImage
        global xPressed, yPressed, sideBarScroll, sideBarSize
        global _buildPlacedThisClick

        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        keystate = pygame.key.get_pressed()

        self.window()
        self.camera()
        self.sideBar()

        if pressed3:
            if regionClicked(xMouse, yMouse) not in selectedRegions and regionClicked(xMouse, yMouse) != None and len(selectedRegions) < len(selectedDivisions):
                selectedRegions.append(regionClicked(xMouse, yMouse))

            if openedTab == 'industry' and controlledCountry != None and clicked == controlledCountry and not getattr(self, '_buildPlacedThisClick', False):
                selectedRegion = regionClicked(xMouse, yMouse)
                if selectedRegion != None:
                    cc = globals()[controlledCountry]
                    x, y = regions.getLocation(selectedRegion)
                    color = industryMap.get_at((round(x), round(y)))

                    if currentlyBuilding == 'factory' and color[:3] in ((255, 255, 255), (0, 0, 255), (0, 0, 100)):
                        cc.build('factory', factoryBuildSpeed, selectedRegion)
                        self._buildPlacedThisClick = True
                    elif currentlyBuilding == 'port' and color[:3] in ((255, 255, 255), (0, 255, 0), (0, 100, 0)):
                        cc.build('port', portBuildSpeed, selectedRegion)
                        self._buildPlacedThisClick = True
                    elif currentlyBuilding == 'destroy':
                        cc.destroy(selectedRegion)
                        if hasattr(cc, 'buildingManager'):
                            cc.buildingManager.destroy_random_in_region(selectedRegion)
                        self._buildPlacedThisClick = True
                    elif currentlyBuilding in BUILDING_DEFS:
                        if selectedRegion in cc.regions:
                            _blocked = False
                            if currentlyBuilding == 'dockyard':
                                is_coastal = False
                                for r in regions.getConnections(selectedRegion):
                                    rx, ry = regions.getLocation(r)
                                    if industryMap.get_at((round(rx), round(ry)))[:3] == (126, 142, 158):
                                        is_coastal = True
                                        break
                                if not is_coastal:
                                    show_toast("Dockyards must be built on the coast")
                                    self._buildPlacedThisClick = True
                                    _blocked = True
                            if _blocked:
                                pass
                            elif not cc.buildingManager.can_build(selectedRegion, currentlyBuilding):
                                show_toast(f"Max {currentlyBuilding.replace('_',' ').title()} reached in this region")
                                self._buildPlacedThisClick = True
                            else:
                                dyn_cost = get_dynamic_cost(currentlyBuilding, cc)
                                if cc.money < dyn_cost:
                                    show_toast(f"Not enough money (need ${dyn_cost:,.0f})")
                                    self._buildPlacedThisClick = True
                                else:
                                    cc.money -= dyn_cost
                                    bdef = BUILDING_DEFS[currentlyBuilding]
                                    total_days = bdef.get('days', 120)
                                    cc.buildingManager.queue.append({
                                        'type': currentlyBuilding,
                                        'region': selectedRegion,
                                        'days_remaining': total_days,
                                        'total_days': total_days,
                                    })
                                    bx, by = regions.getLocation(selectedRegion)
                                    build_color = BUILDING_COLORS.get(currentlyBuilding, (100, 100, 100))
                                    dim_color = (max(0, build_color[0]//2), max(0, build_color[1]//2), max(0, build_color[2]//2))
                                    fill(industryMap, bx, by, dim_color)
                                    if controlledCountry == cc.name:
                                        fill(modifiedIndustryMap, bx, by, dim_color)
                                        buildSound.play()
                                    if currentlyBuilding in EXTRACTION_BUILDINGS:
                                        showResources = False
                                    self._buildPlacedThisClick = True
                        else:
                            show_toast("You don't own this region")
                            self._buildPlacedThisClick = True
        elif not pressed3:
            selectedRegions = []
            self._buildPlacedThisClick = False

        if not pressed1:
            holdingSideBar = False
            timePressed = 0
        if not pressed1 and pressed:
            pressed = False

        selectedDivisionThisFrame = False

        canMove = True
        for popup in popupList:
            if popup.type == 'textbox':
                canMove = False
                break

        for event in pygame.event.get():
            pygame.event.post(event)

            if event.type == pygame.KEYDOWN:
                if (event.key == pygame.K_PLUS and speed < 6) or (event.key == pygame.K_EQUALS and speed < 10):
                    speed += 1
                    popupSound.play()

                if (event.key == pygame.K_MINUS and speed > 0) or (event.key == pygame.K_UNDERSCORE and speed > 0):
                    speed -= 1
                    popupSound.play()

                if event.key == pygame.K_LEFTBRACKET and zoom < 237.3763137997696:
                    zoom *= 1.2
                elif event.key == pygame.K_RIGHTBRACKET and zoom > 0.6944444444444445:
                    zoom /= 1.2

                if event.key == pygame.K_SPACE and controlledCountry != None and clicked == None and canMove:
                    clicked = controlledCountry
                    openMenuSound.play()

                elif event.key == pygame.K_SPACE and controlledCountry != None and canMove:
                    clicked = None
                    closeMenuSound.play()

                elif event.key == pygame.K_SPACE and clicked in countryList and canMove:
                    changeCountry(clicked, True)
                    clickedSound.play()

                if event.key == pygame.K_1:
                    currentMap += 1
                    if currentMap == 5:
                        currentMap = 1
                    clickedSound.play()

                elif event.key == pygame.K_2 and showDivisions:
                    showDivisions = False
                    clickedSound.play()
                elif event.key == pygame.K_2:
                    showDivisions = True
                    clickedSound.play()

                elif event.key == pygame.K_3 and showCities:
                    showCities = False
                    clickedSound.play()
                elif event.key == pygame.K_3:
                    showCities = True
                    clickedSound.play()

                if controlledCountry != None and event.key == pygame.K_SLASH:
                    for division in selectedDivisions.copy():
                        division.reloadIcon(globals()[controlledCountry].divisionColor)
                        globals()[controlledCountry].divideDivision(division)
                        selectedDivisions.remove(division)
                    clickedSound.play()

                if event.key == pygame.K_ESCAPE:
                    mainPopup()
                    speed = 0

                if event.key == pygame.K_F1:
                    now = datetime.now()
                    name = now.strftime('%d-%m-%Y %H-%M-%S.png')
                    name.replace('\n', '')
                    pygame.image.save(map, os.path.join(screenshotsDir, name))
                    popupList.append(Popup('Screenshot Taken', [f'Saved as "{name}".'], [['Okay', '', 0, 5.25]], 5, ySize=len([f'Saved as "{name}".',]) * 1.5 + 0.5 + 3, x=WIDTH / 2, y=HEIGHT / 2))

                if event.key == pygame.K_F2:
                    if userInterface.showUI:
                        userInterface.showUI = False
                    else:
                        userInterface.showUI = True

                if event.key == pygame.K_F3:
                    if userInterface.oldDivisions:
                        userInterface.oldDivisions = False
                    else:
                        userInterface.oldDivisions = True

                if event.key == pygame.K_F4:
                    startPopup()

                if event.key == pygame.K_F5:
                    biggestCountries = sorted(countryList, key=lambda country: globals()[country].population)
                    biggestCountries.reverse()

                    toDisplay = []
                    for country in range(min(len(biggestCountries), 10)):
                        toDisplay.append(f"{country + 1} - {biggestCountries[country].replace('_', ' ')} ({prefixNumber(globals()[biggestCountries[country]].population)})")

                    if controlledCountry != None and controlledCountry not in biggestCountries[:10]:
                        toDisplay.append(f"{biggestCountries.index(controlledCountry) + 1} - {controlledCountry.replace('_', ' ')} ({prefixNumber(globals()[controlledCountry].population)})")

                    popupList.append(Popup('Countries By Population Size', toDisplay, [['Okay', '', 0, len(toDisplay) * 1.5 + 0.5 + 3]], 22, len(toDisplay) * 1.5 + 1 + 2, WIDTH / 2, HEIGHT / 2))

                if event.key == pygame.K_F6:
                    biggestCountries = sorted(countryList, key=lambda country: sum([div.divisionStack for div in globals()[country].divisions]))
                    biggestCountries.reverse()

                    toDisplay = []
                    for country in range(min(len(biggestCountries), 10)):
                        toDisplay.append(f"{country + 1} - {biggestCountries[country].replace('_', ' ')} ({sum([div.divisionStack for div in globals()[biggestCountries[country]].divisions])})")

                    if controlledCountry != None and controlledCountry not in biggestCountries[:10]:
                        toDisplay.append(f"{biggestCountries.index(controlledCountry) + 1} - {controlledCountry.replace('_', ' ')} ({sum([div.divisionStack for div in globals()[controlledCountry].divisions])})")

                    popupList.append(Popup('Countries By Division Count', toDisplay, [['Okay', '', 0, len(toDisplay) * 1.5 + 0.5 + 3]], 22, len(toDisplay) * 1.5 + 1 + 2, WIDTH / 2, HEIGHT / 2))

                if event.key == pygame.K_F7:
                    biggestCountries = sorted(countryList, key=lambda country: len(globals()[country].regions))
                    biggestCountries.reverse()

                    toDisplay = []
                    for country in range(min(len(biggestCountries), 10)):
                        toDisplay.append(f"{country + 1} - {biggestCountries[country].replace('_', ' ')} ({len(globals()[biggestCountries[country]].regions)})")

                    if controlledCountry != None and controlledCountry not in biggestCountries[:10]:
                        toDisplay.append(f"{biggestCountries.index(controlledCountry) + 1} - {controlledCountry.replace('_', ' ')} ({len(globals()[controlledCountry].regions)})")

                    popupList.append(Popup('Countries By Region Count', toDisplay, [['Okay', '', 0, len(toDisplay) * 1.5 + 0.5 + 3]], 22, len(toDisplay) * 1.5 + 1 + 2, WIDTH / 2, HEIGHT / 2))

                if event.key == pygame.K_F8:
                    biggestCountries = sorted(countryList, key=lambda country: globals()[country].factories)
                    biggestCountries.reverse()

                    toDisplay = []
                    for country in range(min(len(biggestCountries), 10)):
                        toDisplay.append(f"{country + 1} - {biggestCountries[country].replace('_', ' ')} ({globals()[biggestCountries[country]].factories})")

                    if controlledCountry != None and controlledCountry not in biggestCountries[:10]:
                        toDisplay.append(f"{biggestCountries.index(controlledCountry) + 1} - {controlledCountry.replace('_', ' ')} ({globals()[controlledCountry].factories})")

                    popupList.append(Popup('Countries By Factory Count', toDisplay, [['Okay', '', 0, len(toDisplay) * 1.5 + 0.5 + 3]], 22, len(toDisplay) * 1.5 + 1 + 2, WIDTH / 2, HEIGHT / 2))

            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:
                    i = 0
                    for name in userInterface.lastRegionNames:
                        image = userInterface.cachedRegionNames[name][0]
                        if WIDTH - image.get_width() - uiSize * 2.5 <= xMouse <= WIDTH - uiSize * 2.5:
                            if uiSize * 2.5 + uiSize * i <= yMouse <= uiSize * 5 + uiSize * i:
                                camx, camy, zoom = regions.getWorldRegionLocation(name)
                                pressed = True
                                clickedSound.play()
                                break
                        i += 2.5

                    if selectedDivisions != []:
                        if WIDTH / 2 - uiSize * 2 < xMouse < WIDTH / 2 and HEIGHT - uiSize * 2 < yMouse:
                            globals()[controlledCountry].mergeDivisions(selectedDivisions)
                            for division in selectedDivisions.copy():
                                selectedDivisions.remove(division)
                            clickedSound.play()

                        elif WIDTH / 2 < xMouse < WIDTH / 2 + uiSize * 2 and HEIGHT - uiSize * 2 < yMouse:
                            for division in selectedDivisions.copy():
                                division.reloadIcon(globals()[controlledCountry].divisionColor)
                                globals()[controlledCountry].divideDivision(division)
                                selectedDivisions.remove(division)
                            clickedSound.play()

                    if xMouse >= WIDTH - uiSize * 2 and HEIGHT - uiSize * 6 >= yMouse >= HEIGHT - uiSize * 8 and showResources:
                        showResources = False
                        clickedSound.play()
                    elif xMouse >= WIDTH - uiSize * 2 and HEIGHT - uiSize * 6 >= yMouse >= HEIGHT - uiSize * 8:
                        showResources = True
                        generateResourceMap()
                        clickedSound.play()

                    if xMouse >= WIDTH - uiSize * 2 and HEIGHT - uiSize * 4 >= yMouse >= HEIGHT - uiSize * 6:
                        currentMap += 1
                        if currentMap == 5:
                            currentMap = 1
                        clickedSound.play()

                    elif xMouse >= WIDTH - uiSize * 2 and HEIGHT - uiSize * 2 >= yMouse >= HEIGHT - uiSize * 4 and showDivisions:
                        showDivisions = False
                        clickedSound.play()
                    elif xMouse >= WIDTH - uiSize * 2 and HEIGHT - uiSize * 2 >= yMouse >= HEIGHT - uiSize * 4:
                        showDivisions = True
                        clickedSound.play()

                    elif xMouse >= WIDTH - uiSize * 2 and yMouse >= HEIGHT - uiSize * 2 and showCities:
                        showCities = False
                        clickedSound.play()
                    elif xMouse >= WIDTH - uiSize * 2 and yMouse >= HEIGHT - uiSize * 2:
                        showCities = True
                        clickedSound.play()

                    if xMouse >= WIDTH - uiSize * 2 and uiSize * 4 >= yMouse >= uiSize * 2 and speed < 10:
                        speed += 1
                        clickedSound.play()
                    elif xMouse >= WIDTH - uiSize * 2 and uiSize * 6 >= yMouse >= uiSize * 4 and speed > 0:
                        speed -= 1
                        clickedSound.play()

                    hoveredPopup = False
                    for popup in popupList:
                        if popup.x - popup.image.get_width() / 2 <= xMouse <= popup.x + popup.image.get_width() / 2:
                            if popup.y - popup.image.get_height() / 2 <= yMouse <= popup.y + popup.image.get_height() / 2:
                                hoveredPopup = True
                                break

                    if controlledCountry != None and yMouse > uiSize * 2 and holdingPopup == None and showDivisions and not hoveredPopup:
                        for div in globals()[controlledCountry].divisions:
                            if div.xBlit < xMouse < div.xBlit + div.image.get_width():
                                if div.yBlit < yMouse < div.yBlit + div.image.get_height():
                                    div.reloadIcon((200, 200, 200))
                                    if div not in selectedDivisions:
                                        div.commands = []
                                        div.movement = div.movementSpeed
                                        selectedDivisions.append(div)
                                    selectedDivisionThisFrame = True
                                    pressed = True
                                    selectDivSound.play()
                                    break

                    if not holdingSideBar and not holdingPopup:
                        xPressed = xMouse
                        yPressed = yMouse

            if event.type == pygame.MOUSEBUTTONUP:
                if event.button == 3:
                    if len(selectedDivisions) > 0:
                        i = 0
                        for div in selectedDivisions:
                            if i < len(selectedRegions):
                                div.command(selectedRegions[i], False, ignoreWater=False, iterations=300)
                                i += 1
                                if i >= len(selectedRegions):
                                    i = 0
                        moveDivSound.play()

                    for div in selectedDivisions:
                        div.reloadIcon(div.color)
                    selectedDivisions = []

                if event.button == 1:
                    if controlledCountry != None and yMouse > uiSize * 2 and xPressed != 0 and yPressed != 0 and showDivisions:
                        if not (pygame.K_LSHIFT or pygame.K_RSHIFT):
                            for div in selectedDivisions:
                                div.reloadIcon(div.color)
                                div.commands.clear()
                            selectedDivisions.clear()

                        for div in globals()[controlledCountry].divisions:
                            if min(xPressed, xMouse) < div.xBlit + div.image.get_width() / 2 < max(xPressed, xMouse):
                                if min(yPressed, yMouse) < div.yBlit + div.image.get_height() / 2 < max(yPressed, yMouse):
                                    div.reloadIcon((200, 200, 200))
                                    if div not in selectedDivisions:
                                        div.commands.clear()
                                        div.movement = div.movementSpeed
                                        selectedDivisions.append(div)
                                        selectedDivisionThisFrame = True
                                    pressed = True

                        if selectedDivisions != []:
                            selectDivSound.play()

                    if xMouse > sideBarSize * sideBarAnimation * WIDTH + 3 and not holdingSideBar and not selectedDivisionThisFrame and yMouse > uiSize * 2 and not selectedDivisionThisFrame and xMouse < WIDTH - uiSize * 2 and yMouse < HEIGHT - uiSize * 4 and not pressed:
                        pressed = True

                        def getSelected(function=countries.colorToCountry):
                            selected = screen.get_at((xMouse, yMouse))[0:3]
                            if function(selected) == None:
                                for y in range(3):
                                    for x in range(3):
                                        selected = screen.get_at((min(round(xMouse + (x - 1) * zoom), WIDTH - 1), min(round(yMouse + (y - 1) * zoom), HEIGHT - 1)))[0:3]
                                        if function(selected) != None:
                                            return selected
                            return selected

                        lastClicked = clicked

                        if math.sqrt((xPressed - xMouse) ** 2 + (yPressed - yMouse) ** 2) > 20:
                            pass
                        elif currentMap == 1:
                            color = getSelected()
                            clicked = countries.colorToCountry(color)
                            if clicked != None:
                                flagImage = pygame.image.load(os.path.join('flags', f"{countries.colorToCountry(getSelected()).lower()}_flag.png")).convert()
                                flagImage = pygame.transform.scale(flagImage, (flagImage.get_width() * 256 / flagImage.get_height(), 256))

                        elif currentMap == 2:
                            colors = {
                                (255, 255, 255): 'Nonaligned',
                                (255, 117, 117): 'Communist',
                                (66, 170, 255): 'Nationalist',
                                (154, 237, 151): 'Liberal',
                                (192, 154, 236): 'Monarchist',
                            }
                            color = getSelected(colors.get)
                            clicked = colors.get(color)

                        elif currentMap == 3:
                            for faction in factionList:
                                if globals()[faction].color == getSelected():
                                    clicked = faction
                                    flagImage = globals()[faction].flag
                                    break
                                else:
                                    clicked = None
                                    flagImage = None

                        elif currentMap == 4:
                            clicked = regionClicked(xMouse, yMouse)
                            if clicked != None:
                                x, y = regions.getLocation(clicked)
                                color = biomeMap.get_at((round(x), round(y)))[:3]
                                if color == (126, 142, 158):
                                    clicked = None

                        if clicked != lastClicked and clicked != None:
                            openMenuSound.play()
                        elif clicked != lastClicked and clicked == None and lastClicked != None:
                            closeMenuSound.play()

                    xPressed = 0
                    yPressed = 0

        pygame.event.clear()


class TreatyController(Controller):

    def input(self):
        self.window()
        self.camera()
        self.sideBar()


class MapDrawer:

    def __init__(self):
        self.camx = camx * zoom
        self.camy = camy * zoom
        self.zoom = zoom
        self.WIDTH = WIDTH
        self.HEIGHT = HEIGHT
        self.currentMap = currentMap
        self.map = None

    def render(self, map, camx, camy, zoom, color=(0, 0, 0)):
        map_width, map_height = map.get_size()

        windowWidth = WIDTH
        windowHeight = HEIGHT

        visibleWidth = int(windowWidth / zoom) + 3
        visibleHeight = int(windowHeight / zoom) + 3

        fixedCamx = camx
        fixedCamy = camy

        visible_area = pygame.Rect(
            int(-fixedCamx - windowWidth / 2 / zoom),
            int(-fixedCamy - windowHeight / 2 / zoom),
            visibleWidth,
            visibleHeight
        )

        if -fixedCamx < map_width / 2:
            visible_area2 = pygame.Rect(
                int(-fixedCamx - windowWidth / 2 / zoom) + map_width,
                int(-fixedCamy - windowHeight / 2 / zoom),
                visibleWidth,
                visibleHeight
            )
        else:
            visible_area2 = pygame.Rect(
                int(-fixedCamx - windowWidth / 2 / zoom) - map_width,
                int(-fixedCamy - windowHeight / 2 / zoom),
                visibleWidth,
                visibleHeight
            )

        visible_map = pygame.Surface((visibleWidth * 2, visibleHeight)).convert()
        visible_map.fill(color)

        visible_map.blit(map, (0, 0), visible_area)
        visible_map.blit(map, (0, 0), visible_area2)

        newMap = pygame.transform.scale(visible_map, (int(visibleWidth * 2 * zoom), int(visibleHeight * zoom)))

        return newMap

    def draw(self, surface, map, camx, camy, zoom, currentMap=0):
        self.map = self.render(map, camx, camy, zoom)

        xAdjust = -((-camx - WIDTH / 2 / zoom) - math.trunc(-camx - WIDTH / 2 / zoom)) * zoom
        yAdjust = -((-camy - HEIGHT / 2 / zoom) - math.trunc(-camy - HEIGHT / 2 / zoom)) * zoom

        surface.blit(
            self.map,
            (WIDTH / 2 - self.map.get_width() / 4 + xAdjust,
             HEIGHT / 2 - self.map.get_height() / 2 + yAdjust)
        )


mapDrawer = MapDrawer()


def interpolate_color(color1, color2, t):
    return tuple(int(color1[i] + (color2[i] - color1[i]) * t) for i in range(3))


def getSkyColor(hour):
    if not (0 <= hour <= 24):
        raise ValueError('Hour must be between 0 and 24.')

    if hour == 24:
        hour = 0

    key_times = {
        4: (0, 0, 0),
        6: (218, 204, 165),
        7: (169, 182, 190),
        9: (134, 170, 196),
        16: (134, 170, 196),
        17: (91, 103, 177),
        18: (119, 100, 132),
        19: (131, 69, 92),
        21: (0, 0, 0),
    }

    times = sorted(key_times.keys())

    for i in range(len(times) - 1):
        if times[i] <= hour < times[i + 1]:
            t = (hour - times[i]) / (times[i + 1] - times[i])
            return interpolate_color(key_times[times[i]], key_times[times[i + 1]], t)

    t = (hour - times[-1]) / (24 - times[-1])
    return interpolate_color(key_times[times[-1]], key_times[times[0]], t)


def getSunColor(hour):
    if not (0 <= hour <= 24):
        raise ValueError('Hour must be between 0 and 24.')

    if hour == 24:
        hour = 0

    key_times = {
        0: (253, 209, 24),
        5: (249, 91, 2),
        8: (255, 255, 255),
        15: (255, 255, 255),
        18: (253, 209, 24),
        20: (249, 91, 2),
    }

    times = sorted(key_times.keys())

    for i in range(len(times) - 1):
        if times[i] <= hour < times[i + 1]:
            t = (hour - times[i]) / (times[i + 1] - times[i])
            return interpolate_color(key_times[times[i]], key_times[times[i + 1]], t)

    t = (hour - times[-1]) / (24 - times[-1])
    return interpolate_color(key_times[times[-1]], key_times[times[0]], t)


def updateTime():
    global hour, day, month, year

    hour += 0.1 * speed

    if hour > 24:
        hour -= 24
        day += 1

    if day > getMonthLength(month):
        day -= getMonthLength(month)
        month += 1

    if month > 12:
        month -= 12
        year += 1


class UserInterface:

    def __init__(self):
        self.WIDTH = None
        self.uiSize = None

        self.worldTension = None

        self.hour = None
        self.day = None
        self.month = None
        self.year = None
        self.speed = None

        self.flagImage = None
        self.controlledCountry = None
        self.politicalPower = None
        self.money = None
        self.stability = None
        self.factories = None
        self.manPower = None

        self.hovered = None

        self.politicalInfo = self.getInfoBox(['Political Power', 'Used for political actions.', 'Gained passively over time.'])
        self.manpowerInfo = self.getInfoBox(['Manpower', 'Used to recruit divisions.', 'Based on population and military size.'])
        self.moneyInfo = self.getInfoBox(['Money', 'Used to build infrastructure.', 'Used to refill division resources.', 'Produced by factories.'])
        self.factoryInfo = self.getInfoBox(['Factories', 'Generates money passively.', 'Can be destroyed if in combat.', 'Constructed by right clicking on a region in the industry menu.'])
        self.stabilityInfo = self.getInfoBox(['Stability', 'Can cause a civil war if too low.', 'Can be changed by political actions.'])
        self.tensionInfo = self.getInfoBox(['World Tension', 'Progession towards a world war.', 'Increases by events.'])
        self.timeInfo = self.getInfoBox(['Current Date', 'Lighter shade indicates game speed.'])

        self.cachedRegionNames = {}
        self.lastRegionNames = []

        for name, adjective in regions.getAllWorldRegions():
            self.cachedRegionNames[name] = (
                self.getInfoBox([f"{adjective} Front"]),
                self.getInfoBox([f"{adjective} Front"], (75, 75, 75)),
                self.getInfoBox([f"{adjective} Front"], (150, 150, 150))
            )

        self.showUI = True
        self.oldDivisions = False

    def drawFactionInfo(self):
        factionReference = globals()[clicked]
        flag = pygame.image.load(os.path.join('flags', f"{factionReference.flag.lower()}_flag.png")).convert()

        x = WIDTH * sideBarSize / 2 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation

        scaled_width = int(WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation)
        scaled_height = int(flag.get_height() * scaled_width / flag.get_width())
        resizedFlagImage = pygame.transform.scale(flag, (scaled_width, scaled_height))

        screen.blit(resizedFlagImage, (x, WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2))
        yOffset = WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2 + resizedFlagImage.get_height() + uiSize * sideBarAnimation * sideBarSize * 8

        name = factionReference.name.replace('_', ' ')
        drawText(screen, name, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize / 2 * sideBarAnimation, WIDTH * sideBarSize / 3 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 / 2 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2)

        drawText(screen, f"Faction Leader: {factionReference.factionLeader.replace('_', ' ')}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        drawText(screen, f"Members: {len(factionReference.members)}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        drawText(screen, f"Ideology: {factionReference.ideology.title()}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

    def drawRegionInfo(self):
        x = WIDTH * sideBarSize / 2 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation

        xLocation, yLocation = regions.getLocation(clicked)
        biomeColor = biomeMap.get_at((round(xLocation), round(yLocation)))[:3]
        regionColor = worldRegionsMap.get_at((round(xLocation), round(yLocation)))[:3]
        cultureColor = cultureMap.get_at((round(xLocation), round(yLocation)))[:3]

        timeZone = xLocation / map.get_width() * 24
        currentTime = (hour + 18 + timeZone) % 24

        if regions.getCity(clicked) not in list(cities):
            name = f"{regions.getWorldAdjective(regionColor)} Region"
        else:
            name = regions.getCity(clicked)

        id = clicked
        culture = countries.getCulture(countries.colorToCountry(cultureColor))
        biome, attackMultiplier, defenseMultiplier, movementMultiplier = regions.getBiomeInfo(biomeColor)

        sunColor = getSunColor(currentTime)
        skyColor = getSkyColor(currentTime)
        shading = min(1, max(0.2, math.sin((currentTime + 18) / 12 * math.pi) + 0.5))
        r, g, b = biomeColor
        groundColor = (r * shading, g * shading, b * shading)

        imageWidth = int(WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation)
        imageHeight = int(2 * imageWidth / 3)
        image = pygame.surface.Surface((imageWidth, imageHeight))
        image.fill(skyColor)

        sunSize = imageWidth / 25 + max(0, math.sin((currentTime + 6) / 12 * math.pi + math.pi / 4) * imageWidth / 40) + max(0, math.sin((currentTime + 6) / 12 * math.pi - math.pi / 4) * imageWidth / 40)
        pygame.draw.circle(image, sunColor, (imageWidth / 2 + math.cos((currentTime + 6) / 12 * math.pi) * imageWidth / 2.5, imageHeight / 2 + math.sin((currentTime + 6) / 12 * math.pi) * imageWidth / 5), sunSize)

        stars = Random()
        stars.seed(0)
        for i in range(20):
            angle = math.radians(stars.randrange(45, 315))
            dist = stars.randrange(20, 150) / 100
            starSize = stars.randrange(50, 100) / 100
            shade = stars.randrange(200, 255)
            if starSize > shading * 2:
                pygame.draw.circle(image, (shade, shade, shade), (imageWidth / 2 + math.cos(angle + (currentTime + 6) / 12 * math.pi) * dist * imageWidth / 2.5, imageHeight / 2 + math.sin(angle + (currentTime + 6) / 12 * math.pi) * dist * imageWidth / 5), imageWidth / 50 * starSize)

        image.fill(groundColor, (0, imageHeight / 2, imageWidth, imageHeight))

        drawText(screen, name, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize / 2 * sideBarAnimation, WIDTH * sideBarSize / 3 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 / 2 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2)

        screen.blit(image, (x, WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2))
        yOffset = WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2 + image.get_height() + uiSize * sideBarAnimation * sideBarSize * 8

        drawText(screen, f"Culture: {culture}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        drawText(screen, f"Biome: {biome}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        drawText(screen, f"Attack Multiplier: {attackMultiplier}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        drawText(screen, f"Defense Multiplier: {defenseMultiplier}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        res_data = regions.getResources(clicked)
        if res_data:
            drawText(screen, "Resources:", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft', (200, 200, 100))
            yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            for rname, ramount in res_data.items():
                drawText(screen, f"  {rname.capitalize()}: {ramount}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft', (180, 220, 150))
                yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)
        else:
            drawText(screen, "Resources: None", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft', (120, 120, 120))
            yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        owner_name = regions.getOwner(clicked)
        if owner_name and owner_name in countryList:
            owner_obj = globals().get(owner_name)
            if owner_obj and hasattr(owner_obj, 'buildingManager'):
                rbuilds = owner_obj.buildingManager.get_region_buildings(clicked)
                if rbuilds:
                    drawText(screen, "Buildings:", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft', (200, 200, 100))
                    yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)
                    for b in rbuilds:
                        bname = b['type'].replace('_', ' ').title()
                        drawText(screen, f"  {bname}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft', (180, 220, 150))
                        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

    def drawCountryInfo(self):
        global politicalTabs, politicalButtonHovered

        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        x = WIDTH * sideBarSize / 2 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation

        resizedFlagImage = pygame.transform.scale(flagImage, (int(WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation), int(256 * WIDTH * sideBarSize / flagImage.get_width() * 0.9333333333333333 * sideBarAnimation)))
        screen.blit(resizedFlagImage, (x, WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2))
        yOffset = WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2 + resizedFlagImage.get_height() + uiSize * sideBarAnimation * sideBarSize * 8

        name = clicked.replace('_', ' ')
        drawText(screen, name, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize / 2 * sideBarAnimation, WIDTH * sideBarSize / 3 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 / 2 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2)

        name = globals()[clicked].faction
        if name != None:
            name = name.replace('_', ' ')
        drawText(screen, f"Faction: {name}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        if globals()[clicked].factionLeader and globals()[clicked].faction != None:
            drawText(screen, 'Faction Leader', int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
            yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        if controlledCountry != None:
            if clicked in globals()[controlledCountry].militaryAccess:
                drawText(screen, 'Military Access: Yes', int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
            else:
                drawText(screen, 'Military Access: No', int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
            yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        drawText(screen, f"Ideology: {getIdeologyName(globals()[clicked].ideology).capitalize()}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
        yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

        if controlledCountry in countryList:
            options = getCountryOptions(countries, countryList, globals()[controlledCountry], globals()[clicked])
            politicalTabs = {}

            icon = pygame.transform.scale(
                pygame.image.load(os.path.join(iconsDir, 'political_power_icon.png')).convert(),
                (int(sideBarAnimation * WIDTH * sideBarSize / 10), int(sideBarAnimation * WIDTH * sideBarSize / 10))
            ).convert_alpha()
            icon.set_colorkey((0, 0, 0))

            for title, options in options.items():
                color = (0, 0, 0)
                politicalTabs[title] = [x, x + resizedFlagImage.get_width(), yOffset, yOffset + int(sideBarAnimation * WIDTH * sideBarSize / 8)]

                if x <= xMouse <= x + resizedFlagImage.get_width():
                    if yOffset <= yMouse <= yOffset + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                        color = (75, 75, 75)
                        self.hovered = title
                        if pressed1:
                            color = (150, 150, 150)

                pygame.draw.rect(screen, color, pygame.Rect(x, yOffset, resizedFlagImage.get_width(), int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                drawText(screen, title, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, yOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16))
                yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

                if title == openedPoliticalTab and options != []:
                    pygame.draw.rect(screen, (25, 25, 25), pygame.Rect(x, yOffset, resizedFlagImage.get_width(), len(options) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32))
                    yButtonOffset = yOffset + sideBarAnimation * WIDTH * sideBarSize / 32
                    for option in options:
                        color = (0, 0, 0)
                        if 2 * x <= xMouse <= 2 * x + resizedFlagImage.get_width() - 2 * x:
                            if yButtonOffset <= yMouse <= yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                                color = (75, 75, 75)
                                self.hovered = option
                                politicalButtonHovered = option
                                if pressed1:
                                    color = (150, 150, 150)

                        pygame.draw.rect(screen, color, pygame.Rect(2 * x, yButtonOffset, resizedFlagImage.get_width() - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                        drawText(screen, option[0], int(sideBarAnimation * WIDTH * sideBarSize / 14), 3 * x, yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16), 'midleft')
                        screen.blit(icon, (2 * x + resizedFlagImage.get_width() - 2.4 * x - icon.get_width(), yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16) - icon.get_height() / 2))
                        drawText(screen, str(option[1]['politicalPower']), int(sideBarAnimation * WIDTH * sideBarSize / 14), 2 * x + resizedFlagImage.get_width() - 3.4 * x - icon.get_width(), yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16), 'midright')
                        yButtonOffset += sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32

                    yOffset += int(len(options) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32)

                yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 16)

    def drawSelfInfo(self):
        global politicalButtonHovered, buttons, politicalTabs

        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        x = WIDTH * sideBarSize / 2 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation
        y = x * 2 + uiSize * 2 - sideBarScroll * sideBarSize

        color = (0, 0, 0)
        if 2 * x <= xMouse <= WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x + 2 * x:
            if y <= yMouse <= y + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                color = (75, 75, 75)
                self.hovered = 'Decision Tree'
                politicalButtonHovered = ('Decision Tree', {}, [f"decisionTree('{controlledCountry}')"])
                if pressed1:
                    color = (150, 150, 150)

        pygame.draw.rect(screen, color, pygame.Rect(2 * x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
        drawText(screen, 'Decision Tree', int(sideBarAnimation * WIDTH * sideBarSize / 14), x + WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation / 2, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 2, 'center')
        y += int(sideBarAnimation * WIDTH * sideBarSize / 8) + int(sideBarAnimation * WIDTH * sideBarSize / 16)

        buttons = []
        size = 271.5 * sideBarSize * sideBarAnimation * WIDTH / 1100
        images = [pygame.transform.scale(pygame.image.load(os.path.join('icons', f'{image}_icon.png')).convert(), (size, size)) for image in ('political_power', 'star', 'industry')]
        for image in images:
            image.convert_alpha()
            image.set_colorkey((0, 0, 0))

        for i in range(-1, 2):
            x1 = i * (size + int(sideBarAnimation * WIDTH * sideBarSize / 16)) + WIDTH * sideBarSize / 2 * sideBarAnimation - size / 2
            x2 = x1 + size
            y1 = y
            y2 = y1 + size
            buttons.append([x1, x2, y1, y2])
            color = (0, 0, 0)
            if buttons[-1][0] <= xMouse <= buttons[-1][1]:
                if buttons[-1][2] <= yMouse <= buttons[-1][3]:
                    color = (75, 75, 75)
                    self.hovered = i
                    if pressed1:
                        color = (150, 150, 150)
            pygame.draw.rect(screen, color, pygame.Rect(x1, y1, size, size))
            screen.blit(images[i + 1], (x1, y))

        y += size + int(sideBarAnimation * WIDTH * sideBarSize / 10)

        if openedTab == 'political':
            cc = globals()[controlledCountry]
            name = cc.faction
            if name != None:
                name = name.replace('_', ' ')
            drawText(screen, f'Faction: {name}', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            if cc.factionLeader and cc.faction != None:
                drawText(screen, 'Faction Leader', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft')
                y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            drawText(screen, f'Ideology: {getIdeologyName(cc.ideology).capitalize()}', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            if cc.focus == None:
                drawText(screen, 'Focus: None', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft')
            else:
                drawText(screen, f'Focus: {cc.focus[0]} ({cc.focus[1]} Days)', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            if hasattr(cc, 'leader') and cc.leader:
                drawText(screen, f'Leader: {cc.leader.name}', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft')
                y += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            if hasattr(cc, 'cabinet') and cc.cabinet:
                for portfolio, minister in cc.cabinet.ministers.items():
                    txt = f'{portfolio.capitalize()}: {minister["name"]} ({minister["modifier"]:+.0%})'
                    drawText(screen, txt, int(sideBarAnimation * WIDTH * sideBarSize / 18), x, y, 'midleft')
                    y += int(sideBarAnimation * WIDTH * sideBarSize / 10)

            if cc.puppetTo:
                drawText(screen, f'Puppet of: {cc.puppetTo.replace("_", " ")}', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft', (255, 100, 100))
                y += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            puppet_list = get_puppet_states(controlledCountry, puppet_states)
            if puppet_list:
                drawText(screen, f'Puppets: {len(puppet_list)}', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft', (100, 255, 100))
                y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            if hasattr(cc, 'combat_stats'):
                cs = cc.combat_stats.get_stat_dict()
                drawText(screen, 'Combat Stats:', int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y, 'midleft', (200, 200, 100))
                y += int(sideBarAnimation * WIDTH * sideBarSize / 10)
                for sname in ['attack', 'defense', 'armor', 'piercing', 'speed']:
                    drawText(screen, f'  {sname.capitalize()}: {cs[sname]}', int(sideBarAnimation * WIDTH * sideBarSize / 16), x, y, 'midleft')
                    y += int(sideBarAnimation * WIDTH * sideBarSize / 10)

            compass = pygame.Surface((2, 2))
            compass.set_at((0, 0), (255, 117, 117))
            compass.set_at((1, 0), (66, 170, 255))
            compass.set_at((0, 1), (154, 237, 151))
            compass.set_at((1, 1), (192, 154, 236))
            compass = pygame.transform.scale(compass, (WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation))
            screen.blit(compass, (x, y))
            pygame.draw.circle(screen, (255, 255, 255), (x + compass.get_width() / 2, y + compass.get_height() / 2), compass.get_width() / 5)
            economic, social = globals()[controlledCountry].ideology
            pygame.draw.circle(screen, (0, 0, 0), (x + compass.get_width() / 2 + economic * compass.get_width() / 2, y + compass.get_height() / 2 + social * compass.get_width() / 2), compass.get_width() / 30)
            y = y + compass.get_height() + int(sideBarAnimation * WIDTH * sideBarSize / 16)

            options = getOptions(countries, countryList, globals()[controlledCountry], canals)
            politicalTabs = {}

            icon = pygame.transform.scale(pygame.image.load(os.path.join(iconsDir, 'political_power_icon.png')).convert(), (int(sideBarAnimation * WIDTH * sideBarSize / 10), int(sideBarAnimation * WIDTH * sideBarSize / 10))).convert_alpha()
            icon.set_colorkey((0, 0, 0))

            for title, options in options.items():
                color = (0, 0, 0)
                politicalTabs[title] = [x, x + compass.get_width(), y, y + int(sideBarAnimation * WIDTH * sideBarSize / 8)]
                if x <= xMouse <= x + compass.get_width():
                    if y <= yMouse <= y + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                        color = (75, 75, 75)
                        self.hovered = title
                        if pressed1:
                            color = (150, 150, 150)
                pygame.draw.rect(screen, color, pygame.Rect(x, y, compass.get_width(), int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                drawText(screen, title, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, y + int(sideBarAnimation * WIDTH * sideBarSize / 16))
                y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

                if title == openedPoliticalTab and options != []:
                    pygame.draw.rect(screen, (25, 25, 25), pygame.Rect(x, y, compass.get_width(), len(options) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32))
                    yButtonOffset = y + sideBarAnimation * WIDTH * sideBarSize / 32
                    for option in options:
                        color = (0, 0, 0)
                        if 2 * x <= xMouse <= 2 * x + compass.get_width() - 2 * x:
                            if yButtonOffset <= yMouse <= yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                                color = (75, 75, 75)
                                self.hovered = option
                                politicalButtonHovered = option
                                if pressed1:
                                    color = (150, 150, 150)

                        pygame.draw.rect(screen, color, pygame.Rect(2 * x, yButtonOffset, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                        drawText(screen, option[0], int(sideBarAnimation * WIDTH * sideBarSize / 14), 3 * x, yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16), 'midleft')
                        screen.blit(icon, (2 * x + WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2.4 * x - icon.get_width(), yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16) - icon.get_height() / 2))
                        drawText(screen, str(option[1]['politicalPower']), int(sideBarAnimation * WIDTH * sideBarSize / 14), 2 * x + WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 3.4 * x - icon.get_width(), yButtonOffset + int(sideBarAnimation * WIDTH * sideBarSize / 16), 'midright')
                        yButtonOffset += sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32

                    y += int(len(options) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32)

                y += int(sideBarAnimation * WIDTH * sideBarSize / 16)

        elif openedTab == 'military':
            drawText(screen, f"Size: {getMilitarySizeName(globals()[controlledCountry].militarySize)} ({round((1.00249688279 ** globals()[controlledCountry].militarySize - 1) * 100, 2)}%)", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            drawText(screen, f"Deployment: {globals()[controlledCountry].deployRegion}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            icon = pygame.transform.scale(pygame.image.load(os.path.join(iconsDir, 'man_power_icon.png')).convert(), (int(sideBarAnimation * WIDTH * sideBarSize / 10), int(sideBarAnimation * WIDTH * sideBarSize / 10))).convert_alpha()
            icon.set_colorkey((0, 0, 0))

            color = (0, 0, 0)
            if 2 * x <= xMouse <= WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x + 2 * x:
                if y <= yMouse <= y + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                    color = (75, 75, 75)
                    self.hovered = 'Change Deployment'
                    politicalButtonHovered = ('Change Deployment Location', {}, [f"{controlledCountry}.changeDeployment()"])
                    if pressed1:
                        color = (150, 150, 150)

            pygame.draw.rect(screen, color, pygame.Rect(2 * x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
            drawText(screen, 'Change Deployment Location', int(sideBarAnimation * WIDTH * sideBarSize / 14), 3 * x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 2, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8) + int(sideBarAnimation * WIDTH * sideBarSize / 16)

            cc_obj = globals()[controlledCountry]
            fs_info = int(sideBarAnimation * WIDTH * sideBarSize / 16)
            row_info = int(sideBarAnimation * WIDTH * sideBarSize / 10)
            daily_up = len(cc_obj.divisions) * DIVISION_UPKEEP_PER_DAY
            train_1_cost = TRAINING_COST_PER_DIV
            arms_c = cc_obj.buildingManager.get_building_count('arms_factory')
            t_days = max(3, int(14 * max(0.25, 1.0 - arms_c * 0.15)))
            drawText(screen, f"Army Upkeep: ${daily_up:,}/day", fs_info, x, y + row_info // 4, 'midleft', (255, 150, 80))
            y += row_info
            drawText(screen, f"Train Cost: ${train_1_cost:,} | {t_days} days", fs_info, x, y + row_info // 4, 'midleft', (200, 200, 200))
            y += row_info
            civ_c2 = cc_obj.buildingManager.get_building_count('civilian_factory')
            net_inc = 5000 * (civ_c2 + 1) * cc_obj.moneyMultiplier - daily_up
            if net_inc < 0:
                drawText(screen, f"WARNING: Losing ${abs(net_inc):,.0f}/day", fs_info, x, y + row_info // 4, 'midleft', (255, 80, 80))
                y += row_info
            y += int(sideBarAnimation * WIDTH * sideBarSize / 16)

            btn_fs = int(sideBarAnimation * WIDTH * sideBarSize / 17)
            btn_h = int(sideBarAnimation * WIDTH * sideBarSize / 7)
            btn_w = WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x
            color = (0, 0, 0)
            if 2 * x <= xMouse <= 2 * x + btn_w:
                if y <= yMouse <= y + btn_h:
                    color = (75, 75, 75)
                    politicalButtonHovered = ('Train Division', {}, [f"{controlledCountry}.trainDivision()"])
                    self.hovered = 'Train Division'
                    if pressed1:
                        color = (150, 150, 150)
            pygame.draw.rect(screen, color, pygame.Rect(2 * x, y, btn_w, btn_h))
            drawText(screen, f'Train 1 Div (${train_1_cost:,})', btn_fs, 3 * x, y + btn_h * 0.3, 'midleft')
            drawText(screen, f'10k manpower | {t_days}d', max(btn_fs - 3, 6), 3 * x, y + btn_h * 0.7, 'midleft', (160, 160, 160))
            y += btn_h + int(sideBarAnimation * WIDTH * sideBarSize / 16)

            all_div_count = math.floor(cc_obj.manPower / 10000)
            all_cost = all_div_count * TRAINING_COST_PER_DIV
            color = (0, 0, 0)
            if 2 * x <= xMouse <= 2 * x + btn_w:
                if y <= yMouse <= y + btn_h:
                    color = (75, 75, 75)
                    politicalButtonHovered = ('Train All Divisions', {}, [f"{controlledCountry}.trainDivision({all_div_count})"])
                    self.hovered = 'Train All Divisions'
                    if pressed1:
                        color = (150, 150, 150)

            pygame.draw.rect(screen, color, pygame.Rect(2 * x, y, btn_w, btn_h))
            drawText(screen, f'Train All (${all_cost:,})', btn_fs, 3 * x, y + btn_h * 0.3, 'midleft')
            drawText(screen, f'{prefixNumber(all_div_count * 10000)} manpower', max(btn_fs - 3, 6), 3 * x, y + btn_h * 0.7, 'midleft', (160, 160, 160))
            y += btn_h + int(sideBarAnimation * WIDTH * sideBarSize / 16)

            pygame.draw.rect(screen, (0, 0, 0), pygame.Rect(x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
            drawText(screen, 'Training:', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 2, 'center')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            if globals()[controlledCountry].training == []:
                return

            pygame.draw.rect(screen, (25, 25, 25), pygame.Rect(x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation, min(len(globals()[controlledCountry].training), 6) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32))
            yButtonOffset = 0
            i = 0
            for training in globals()[controlledCountry].training:
                i += 1
                if i <= 5:
                    color = (0, 0, 0)
                    if 2 * x <= xMouse <= WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation:
                        if yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 16) - int(sideBarAnimation * WIDTH * sideBarSize / 32) <= yMouse <= yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 16) - int(sideBarAnimation * WIDTH * sideBarSize / 32) + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                            if training[1] == 0:
                                color = (75, 75, 75)
                                self.hovered = training
                                if globals()[controlledCountry].deployRegion == None:
                                    politicalButtonHovered = (f'Deploy {training[0]} Division', {}, [f"{controlledCountry}.addDivision({training[0]}, None, True)", f"{controlledCountry}.training.remove({training})"])
                                else:
                                    politicalButtonHovered = (f'Deploy {training[0]} Division', {}, [f"{controlledCountry}.addDivision({training[0]}, {regions.getCityRegion(globals()[controlledCountry].deployRegion)}, True)", f"{controlledCountry}.training.remove({training})"])
                                if pressed1:
                                    color = (150, 150, 150)
                    pygame.draw.rect(screen, color, pygame.Rect(2 * x, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 16) - int(sideBarAnimation * WIDTH * sideBarSize / 32), WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                    if training[1] != 0:
                        drawText(screen, f'Training {training[0]} Division(s) ({training[1]} Days)', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 8) - int(sideBarAnimation * WIDTH * sideBarSize / 32), 'center')
                    else:
                        drawText(screen, f'Deploy {training[0]} Division(s)', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 8) - int(sideBarAnimation * WIDTH * sideBarSize / 32), 'center')
                    yButtonOffset += sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32

                if i == 6:
                    pygame.draw.rect(screen, (0, 0, 0), pygame.Rect(2 * x, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 16) - int(sideBarAnimation * WIDTH * sideBarSize / 32), WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                    drawText(screen, f'+{len(globals()[controlledCountry].training) - 5} More', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 8) - int(sideBarAnimation * WIDTH * sideBarSize / 32), 'center')

            y += int(min(len(globals()[controlledCountry].training), 6) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32)

        elif openedTab == 'industry':
            cc = globals()[controlledCountry]
            civ_c = cc.buildingManager.get_building_count('civilian_factory')
            daily_income = 5000 * (civ_c + 1) * cc.moneyMultiplier
            daily_upkeep = len(cc.divisions) * DIVISION_UPKEEP_PER_DAY
            net_income = daily_income - daily_upkeep
            inc_color = (100, 255, 100) if net_income >= 0 else (255, 100, 100)
            drawText(screen, f"Income: {prefixNumber(daily_income)}/d", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft', (100, 255, 100))
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            drawText(screen, f"Army Upkeep: -{prefixNumber(daily_upkeep)}/d", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft', (255, 150, 80))
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            drawText(screen, f"Net: {prefixNumber(net_income)}/d", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft', inc_color)
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            drawText(screen, "Resources:", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft', (200, 200, 100))
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            fs_small = int(sideBarAnimation * WIDTH * sideBarSize / 18)
            row_h = int(sideBarAnimation * WIDTH * sideBarSize / 10)
            res_status = cc.resourceManager.get_supply_status()
            for rname in RESOURCE_NAMES:
                rs = res_status[rname]
                net_color = (100, 255, 100) if rs['net'] >= 0 else (255, 100, 100)
                stock_txt = f"{rname.capitalize()}: {rs['stockpile']:.0f}"
                detail_txt = f"  +{rs['production']:.1f} -{rs['consumption']:.1f} = {rs['net']:+.1f}/d"
                drawText(screen, stock_txt, fs_small, x, y + row_h / 4, 'midleft', net_color)
                y += row_h
                drawText(screen, detail_txt, max(fs_small - 2, 6), x, y + row_h / 4, 'midleft', (160, 160, 160))
                y += row_h
            y += int(sideBarAnimation * WIDTH * sideBarSize / 16)

            prod_pen = cc.resourceManager.get_production_penalty()
            combat_pen = cc.resourceManager.get_combat_penalty()
            if prod_pen < 1.0:
                drawText(screen, f"Production penalty: {prod_pen:.0%}", fs_small, x, y + row_h / 4, 'midleft', (255, 150, 80))
                y += row_h
            if combat_pen < 1.0:
                drawText(screen, f"Combat penalty: {combat_pen:.0%}", fs_small, x, y + row_h / 4, 'midleft', (255, 100, 100))
                y += row_h

            drawText(screen, "Buildings:", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft', (200, 200, 100))
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            bm = cc.buildingManager
            for btype in BUILDING_DEFS:
                count = bm.get_building_count(btype)
                if count > 0:
                    label = btype.replace('_', ' ').title()
                    drawText(screen, f"{label}: {count}", int(sideBarAnimation * WIDTH * sideBarSize / 16), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 10) / 4, 'midleft')
                    y += int(sideBarAnimation * WIDTH * sideBarSize / 10)

            build_label = currentlyBuilding.replace('_', ' ').title()
            if currentlyBuilding in BUILDING_DEFS:
                bcost = get_dynamic_cost(currentlyBuilding, cc)
                bdesc = BUILDING_DEFS[currentlyBuilding].get('description', '')
                build_label += f" (${bcost:,})"
            drawText(screen, f"Selected: {build_label}", int(sideBarAnimation * WIDTH * sideBarSize / 14), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 4, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)
            if currentlyBuilding in BUILDING_DEFS:
                drawText(screen, bdesc, max(int(sideBarAnimation * WIDTH * sideBarSize / 18), 6), x, y + int(sideBarAnimation * WIDTH * sideBarSize / 10) / 4, 'midleft', (160, 160, 160))
                y += int(sideBarAnimation * WIDTH * sideBarSize / 10)

            color = (0, 0, 0)
            if 2 * x <= xMouse <= WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x + 2 * x:
                if y <= yMouse <= y + int(sideBarAnimation * WIDTH * sideBarSize / 8):
                    color = (75, 75, 75)
                    politicalButtonHovered = ('Change Construction', {}, ['changeConstruction()'])
                    self.hovered = 'Change Construction'
                    if pressed1:
                        color = (150, 150, 150)

            pygame.draw.rect(screen, color, pygame.Rect(2 * x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
            drawText(screen, 'Change Construction', int(sideBarAnimation * WIDTH * sideBarSize / 14), 3 * x, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 2, 'midleft')
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8) + int(sideBarAnimation * WIDTH * sideBarSize / 16)

            pygame.draw.rect(screen, (0, 0, 0), pygame.Rect(x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
            drawText(screen, 'Construction Queue:', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, y + int(sideBarAnimation * WIDTH * sideBarSize / 8) / 2, 'center')
            yButtonOffset = 0
            y += int(sideBarAnimation * WIDTH * sideBarSize / 8)

            queue = bm.get_queue_info()
            old_building = cc.building
            all_builds = queue + [{'type': b[0], 'days_remaining': b[1], 'region': b[2]} for b in old_building]

            if len(all_builds) != 0:
                pygame.draw.rect(screen, (25, 25, 25), pygame.Rect(x, y, WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation, min(len(all_builds), 6) * int(sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32) + sideBarAnimation * WIDTH * sideBarSize / 32))

            i = 0
            for b in all_builds:
                if i > 5:
                    if i == 6:
                        pygame.draw.rect(screen, (0, 0, 0), pygame.Rect(2 * x, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 16) - int(sideBarAnimation * WIDTH * sideBarSize / 32), WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                        drawText(screen, f'+{len(all_builds) - 5} More', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 8) - int(sideBarAnimation * WIDTH * sideBarSize / 32), 'center')
                    break
                i += 1
                bname = b['type'].replace('_', ' ').title()
                bdays = max(b['days_remaining'], 0)
                pygame.draw.rect(screen, BLACK, pygame.Rect(2 * x, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 16) - int(sideBarAnimation * WIDTH * sideBarSize / 32), WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation - 2 * x, int(sideBarAnimation * WIDTH * sideBarSize / 8)))
                drawText(screen, f'Building {bname} ({bdays} Days)', int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize * sideBarAnimation / 2, yButtonOffset + y + int(sideBarAnimation * WIDTH * sideBarSize / 8) - int(sideBarAnimation * WIDTH * sideBarSize / 32), 'center')
                yButtonOffset += sideBarAnimation * WIDTH * sideBarSize / 8 + sideBarAnimation * WIDTH * sideBarSize / 32

    def drawIdeologyInfo(self):
        x = WIDTH * sideBarSize / 2 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation

        ideologyInfo = {
            'Monarchist': ('Society: Tradition and Continuity', 'Economy: Feudalism', 'Politics: Monarchy'),
            'Nationalist': ('Society: National Unity', 'Economy: Private Ownership', 'Politics: Dictatorship'),
            'Liberal': ('Society: Individual Rights', 'Economy: Private Ownership', 'Politics: Democracy'),
            'Communist': ('Society: Classless Society', 'Economy: Collective Ownership', 'Politics: One-Party State'),
            'Nonaligned': ('No Defined Ideology', ''),
        }

        ideologyFlags = {
            'Liberal': 'liberalism_flag.png',
            'Nationalist': 'nationalism_flag.png',
            'Communist': 'communism_flag.png',
            'Nonaligned': 'switzerland_flag.png',
            'Monarchist': 'monarchism_flag.png',
        }

        image = pygame.image.load(os.path.join('flags', ideologyFlags[clicked]))

        scaled_width = int(WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation)
        scaled_height = int(image.get_height() * scaled_width / image.get_width())
        image = pygame.transform.scale(image, (scaled_width, scaled_height))

        drawText(screen, clicked, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize / 2 * sideBarAnimation, WIDTH * sideBarSize / 3 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 / 2 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2)

        screen.blit(image, (x, WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2))
        yOffset = WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 2 + image.get_height() + uiSize * sideBarAnimation * sideBarSize * 8

        for line in ideologyInfo[clicked]:
            drawText(screen, line, int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4 * sideBarAnimation * sideBarSize, yOffset, 'midleft')
            yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 8)

    def drawInfo(self):
        global politicalButtonHovered
        politicalButtonHovered = None

        if clicked in factionList:
            self.drawFactionInfo()
            return
        if clicked in countryList and controlledCountry != clicked:
            self.drawCountryInfo()
            return
        if clicked in ('Liberal', 'Communist', 'Nonaligned', 'Monarchist', 'Nationalist'):
            self.drawIdeologyInfo()
            return
        if type(clicked) == int:
            self.drawRegionInfo()
            return
        if controlledCountry in countryList and openedTab != None:
            self.drawSelfInfo()

    def drawSideBar(self):
        global sideBarAnimation
        pygame.mouse.set_cursor()

        if sideBarAnimation < 1 and clicked != None:
            sideBarAnimation += 12.0 / FPS
            if sideBarAnimation >= 1:
                sideBarAnimation = 1

        if sideBarAnimation > 0 and clicked == None:
            sideBarAnimation -= 12.0 / FPS
            if sideBarAnimation <= 0:
                sideBarAnimation = 0

        if sideBarAnimation <= 0:
            return

        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        if WIDTH * sideBarSize * sideBarAnimation - 4 <= xMouse <= WIDTH * sideBarSize * sideBarAnimation + 4:
            if yMouse > uiSize * 2:
                pygame.mouse.set_cursor(7)

        pygame.draw.rect(screen, (50, 50, 50), pygame.Rect(0, 0, WIDTH * sideBarSize * sideBarAnimation, HEIGHT + uiSize * 2))

        if clicked != None:
            self.drawInfo()

    def updateTopBar(self):
        didReset = False

        if not (self.WIDTH == WIDTH and self.uiSize == uiSize and self.controlledCountry == controlledCountry):
            self.topBar = pygame.Surface((WIDTH, uiSize * 2))
            self.topBar.fill((25, 25, 25))
            self.WIDTH = WIDTH
            self.uiSize = uiSize
            didReset = True

        xOffset = uiSize * 0.25

        if self.worldTension != eventManager.globalTension or didReset:
            if didReset:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(WIDTH - uiSize * 4 - xOffset, uiSize - uiSize * 1.5 / 2, uiSize * 4, uiSize * 1.5))
            else:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(WIDTH - uiSize * 2.5 - xOffset, uiSize - uiSize * 1.5 / 2, uiSize * 2.5, uiSize * 1.5))
            self.topBar.blit(liberal_icon, (WIDTH - uiSize * 4 - xOffset, uiSize - uiSize * 1.5 / 2))
            drawText(self.topBar, f"{int(eventManager.globalTension)}%", uiSize, WIDTH - xOffset - uiSize * 0.125, uiSize, 'midright')
            self.worldTension = eventManager.globalTension

        xOffset += uiSize * 4.25

        if not (self.hour == hour and self.day == day and self.month == month and self.year == year and self.speed == speed) or didReset:
            pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(WIDTH - uiSize * 10 - xOffset, uiSize - uiSize * 1.5 / 2, uiSize * 10, uiSize * 1.5))
            pygame.draw.rect(self.topBar, (50, 50, 50), pygame.Rect(WIDTH - uiSize * 10 - xOffset, uiSize - uiSize * 1.5 / 2, uiSize * 10 * speed / 10, uiSize * 1.5))
            drawText(self.topBar, f"{round(hour)}:00, {day} {getMonthName(month)}, {year}", uiSize, WIDTH - xOffset - uiSize * 5, uiSize, 'center')
            self.hour = hour
            self.day = day
            self.month = month
            self.year = year
            self.speed = speed

        if controlledCountry not in countryList or controlledCountry == None:
            return

        xOffset = 0

        if self.controlledCountry != controlledCountry or didReset:
            self.flagImage = pygame.transform.scale(controlledCountryFlag, (controlledCountryFlag.get_width() / controlledCountryFlag.get_height() * uiSize * 1.9, uiSize * 1.9))
            self.topBar.blit(self.flagImage, (uiSize / 16, uiSize / 16))
            self.controlledCountry = controlledCountry

        xOffset += self.flagImage.get_width() + uiSize * 0.25

        if self.politicalPower != globals()[controlledCountry].politicalPower or didReset:
            if didReset:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset, uiSize * 0.25, uiSize * 4, uiSize * 1.5))
                self.topBar.blit(political_power_icon, (xOffset, uiSize * 0.25))
            else:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset + uiSize * 1.5, uiSize * 0.25, uiSize * 2.5, uiSize * 1.5))
            drawText(self.topBar, prefixNumber(int(globals()[controlledCountry].politicalPower)), uiSize, xOffset + uiSize * 3.9, uiSize, 'midright')
            self.politicalPower = globals()[controlledCountry].politicalPower

        xOffset += uiSize * 4.25

        if self.manPower != globals()[controlledCountry].manPower or didReset:
            if didReset:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset, uiSize - uiSize * 0.75, uiSize * 4, uiSize * 1.5))
                self.topBar.blit(man_power_icon, (xOffset, uiSize - uiSize * 1.5 / 2))
            else:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset + uiSize * 1.5, uiSize - uiSize * 0.75, uiSize * 2.5, uiSize * 1.5))
            drawText(self.topBar, prefixNumber(int(globals()[controlledCountry].manPower)), uiSize, xOffset + uiSize * 3.9, uiSize, 'midright')
            self.manPower = globals()[controlledCountry].manPower

        xOffset += uiSize * 4.25

        if self.money != globals()[controlledCountry].money or didReset:
            if didReset:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset, uiSize - uiSize * 0.75, uiSize * 4, uiSize * 1.5))
                self.topBar.blit(money_icon, (xOffset, uiSize - uiSize * 1.5 / 2))
            else:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset + uiSize * 1.5, uiSize - uiSize * 0.75, uiSize * 2.5, uiSize * 1.5))
            money = max(0, int(globals()[controlledCountry].money))
            drawText(self.topBar, prefixNumber(money)[-5:], uiSize, xOffset + uiSize * 3.9, uiSize, 'midright')
            self.money = globals()[controlledCountry].money

        xOffset += uiSize * 4.25

        if self.factories != globals()[controlledCountry].factories or didReset:
            if didReset:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset, uiSize - uiSize * 0.75, uiSize * 4, uiSize * 1.5))
                self.topBar.blit(industry_icon, (xOffset, uiSize - uiSize * 1.5 / 2))
            else:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset + uiSize * 1.5, uiSize - uiSize * 0.75, uiSize * 2.5, uiSize * 1.5))
            drawText(self.topBar, prefixNumber(int(globals()[controlledCountry].factories)), uiSize, xOffset + uiSize * 3.9, uiSize, 'midright')
            self.factories = globals()[controlledCountry].factories

        xOffset += uiSize * 4.25

        if self.stability != globals()[controlledCountry].stability or didReset:
            if didReset:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset, uiSize - uiSize * 0.75, uiSize * 4, uiSize * 1.5))
                self.topBar.blit(nonaligned_icon, (xOffset, uiSize - uiSize * 1.5 / 2))
            else:
                pygame.draw.rect(self.topBar, (0, 0, 0), pygame.Rect(xOffset + uiSize * 1.5, uiSize - uiSize * 0.75, uiSize * 2.5, uiSize * 1.5))
            drawText(self.topBar, f"{round(globals()[controlledCountry].stability)}%", uiSize, xOffset + uiSize * 3.9, uiSize, 'midright')
            self.stability = globals()[controlledCountry].stability

    def drawTopBar(self):
        xMouse, yMouse = pygame.mouse.get_pos()

        self.updateTopBar()
        screen.blit(self.topBar, (0, 0))

        xOffset = 0
        if controlledCountry != None:
            xOffset = self.flagImage.get_width() + uiSize * 0.25

        resourceInfo = [self.politicalInfo, self.manpowerInfo, self.moneyInfo, self.factoryInfo, self.stabilityInfo]

        if 0 <= xMouse <= xOffset and 0 <= yMouse <= uiSize * 2 and controlledCountry in countryList and controlledCountry != None:
            rect = self.getInfoBox([controlledCountry.replace('_', ' ')])
            screen.blit(rect, (0, uiSize * 2))
            return

        if WIDTH - uiSize * 4.25 <= xMouse <= WIDTH and 0 <= yMouse <= uiSize * 2:
            screen.blit(self.tensionInfo, (WIDTH - self.tensionInfo.get_width(), uiSize * 2))
            return

        if WIDTH - uiSize * 4.5 - uiSize * 10 <= xMouse <= WIDTH - uiSize * 4.5 and 0 <= yMouse <= uiSize * 2:
            screen.blit(self.timeInfo, (WIDTH - uiSize * 4.5 - uiSize * 10, uiSize * 2))
            return

        if controlledCountry in countryList and controlledCountry != None:
            for info in resourceInfo:
                if xOffset <= xMouse <= xOffset + uiSize * 4.25 and 0 <= yMouse <= uiSize * 2:
                    screen.blit(info, (xOffset, uiSize * 2))
                    return
                xOffset += uiSize * 4.25

    def getInfoBox(self, text, backgroundColor=(0, 0, 0)):
        surfaces = []
        for line in text:
            surfaces.append(getText(line, uiSize, 'midleft'))

        rect = pygame.surface.Surface((max([s.get_width() for s in surfaces]) + uiSize, len(surfaces) * uiSize * 1.5 + uiSize * 0.5))
        rect.fill(backgroundColor)

        for surface in surfaces:
            rect.blit(surface, (uiSize / 2, surfaces.index(surface) * uiSize * 1.5 + uiSize / 2))

        return rect

    def drawInfrastructure(self, onlyCities=False):
        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        if not onlyCities:
            for port in ports:
                x, y = regions.getLocation(port)
                x = normalize(x, map.get_width(), camx)
                xFinal = (x + camx - 0.5) * zoom + WIDTH / 2
                yFinal = (y + camy - 0.5) * zoom + HEIGHT / 2
                screen.blit(anchor_element, (xFinal - anchor_element.get_width() / 2, yFinal - anchor_element.get_height() / 2))

            for canal in canals:
                x, y = regions.getLocation(canal)
                x = normalize(x, map.get_width(), camx)
                xFinal = (x + camx - 0.5) * zoom + WIDTH / 2
                yFinal = (y + camy - 0.5) * zoom + HEIGHT / 2
                screen.blit(boat_element, (xFinal - boat_element.get_width() / 2, yFinal - boat_element.get_height() / 2))

        blitName = False
        for city in cities:
            x, y = regions.getLocation(regions.getCityRegion(city))
            color = map.get_at((round(x), round(y)))[:3]
            country = countries.colorToCountry(color)

            x, y = regions.getCityLocation(city)
            x = normalize(x, map.get_width(), camx)
            xFinal = (x + camx - 0.5) * zoom + WIDTH / 2
            yFinal = (y + camy - 0.5) * zoom + HEIGHT / 2
            xOffset = 0

            if city == globals()[country].capital:
                screen.blit(star_element, (xFinal - star_element.get_width() / 2, yFinal - star_element.get_height() / 2))
                xOffset = star_element.get_width() / 2
            else:
                screen.blit(cityImage, (xFinal - cityImage.get_width() / 2, yFinal - cityImage.get_height() / 2))
                xOffset = cityImage.get_width() / 4

            if xFinal - uiSize < xMouse < xFinal + uiSize and yFinal - uiSize < yMouse < yFinal + uiSize and not blitName:
                rect = self.getInfoBox([city])
                xText = xFinal + xOffset + uiSize / 4
                yText = yFinal - rect.get_height() / 2
                blitName = True

        if blitName:
            screen.blit(rect, (xText, yText))

    def drawDivisions(self):
        if not self.oldDivisions:
            divisionsInRegion = {}
            for country in countryList:
                index = globals()[country]
                for div in index.divisions:
                    divisionsInRegion[div.location] = divisionsInRegion.get(div.location, 0) + 1

            for country in countryList:
                index = globals()[country]
                for div in index.divisions:
                    if div.commands == [] or div.fighting:
                        continue

                    x, y = regions.getLocation(div.commands[0])
                    x = normalize(x, map.get_width(), camx)
                    xFinal = (x + camx - 0.5) * zoom + WIDTH / 2
                    yFinal = (y + camy - 0.5) * zoom + HEIGHT / 2

                    angle = math.atan2(yFinal - div.yBlit, xFinal - div.xBlit - div.image.get_width() / 2)
                    distance = math.sqrt((-div.xBlit - div.image.get_width() / 2 + xFinal) ** 2 + (-div.yBlit + yFinal) ** 2)

                    if (-uiSize * 4 <= div.xBlit + div.image.get_width() / 2 <= WIDTH + uiSize * 4 and
                        -uiSize * 4 <= div.yBlit <= HEIGHT + uiSize * 4) or \
                       (-uiSize * 4 <= xFinal <= WIDTH + uiSize * 4 and
                        -uiSize * 4 <= yFinal <= HEIGHT + uiSize * 4):

                        pygame.draw.line(screen, (255, 0, 0),
                            (div.xBlit + div.image.get_width() / 2, div.yBlit),
                            (xFinal, yFinal),
                            round(uiSize / 3))

                        pygame.draw.line(screen, (255, 125, 0),
                            (div.xBlit + div.image.get_width() / 2, div.yBlit),
                            (div.xBlit + div.image.get_width() / 2 + distance * math.cos(angle) * ((div.movementSpeed - div.movement) / div.movementSpeed),
                             div.yBlit + distance * math.sin(angle) * ((div.movementSpeed - div.movement) / div.movementSpeed)),
                            int(uiSize / 5))

            regionCount = {}
            for country in countryList:
                index = globals()[country]
                for div in index.divisions:
                    location = div.location
                    regionCount[location] = regionCount.get(location, 0) + 1
                    y_offset = div.image.get_height() * (regionCount[location] - 1) - div.image.get_height() * divisionsInRegion[location] / 2

                    if -div.image.get_width() <= div.xBlit <= WIDTH + div.image.get_width() and \
                       -div.image.get_height() <= div.yBlit + y_offset <= HEIGHT + div.image.get_height():
                        screen.blit(div.image, (div.xBlit, div.yBlit + y_offset))

                    div.yBlit += y_offset

            for battle in battleList:
                battle.draw()
            return

        for country in countryList:
            index = globals()[country]
            for div in index.divisions:
                x, y = div.location
                xFinal = (x + camx - 0.5) * zoom + WIDTH / 2
                yFinal = (y + camy - 0.5) * zoom + HEIGHT / 2
                flag = globals()[f"{div.country.lower()}_flag"]
                screen.blit(flag, (xFinal - flag.get_width() / 2, yFinal - flag.get_height() / 2))

        for battle in battleList:
            battle.draw()

    def drawCommands(self):
        xMouse, yMouse = pygame.mouse.get_pos()

        xStart = 0
        yStart = 0
        mapSize = map.get_width()

        if len(selectedRegions) > 1:
            for region in selectedRegions:
                x, y = regions.getLocation(region)
                xEnd = (x + camx - 0.5) * zoom + WIDTH / 2
                yEnd = (y + camy - 0.5) * zoom + HEIGHT / 2

                if region != selectedRegions[0]:
                    pygame.draw.line(screen, (255, 0, 0), (xStart, yStart), (xEnd, yEnd), round(uiSize / 2))

                xStart = xEnd
                yStart = yEnd

        if math.sqrt(abs(xPressed ** 2 - xMouse ** 2)) > uiSize and \
           math.sqrt(abs(yPressed ** 2 - yMouse ** 2)) > uiSize and \
           xPressed != 0 and yPressed != 0 and not holdingPopup:
            pygame.draw.polygon(screen, WHITE, (
                (xPressed, yPressed),
                (xPressed, yMouse),
                (xMouse, yMouse),
                (xMouse, yPressed)
            ), uiSize // 4)
            pressed = True

    def drawButtons(self):
        global buttons
        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        buttons = [[0, 0, 0, 0] for i in range(3)]

        color = (0, 0, 0)
        if xMouse > WIDTH - uiSize * 2 and HEIGHT - uiSize * 6 > yMouse > HEIGHT - uiSize * 8:
            color = (75, 75, 75)
            self.hovered = 'resources view'
            if pressed1:
                color = (150, 150, 150)
        pygame.draw.rect(screen, color, pygame.Rect(WIDTH - uiSize * 2, HEIGHT - uiSize * 8, uiSize * 2, uiSize * 2))

        res_btn = pygame.transform.scale(placeholder, (int(uiSize * 1.5), int(uiSize * 1.5)))
        screen.blit(res_btn, (WIDTH - res_btn.get_width() / 2 - uiSize, HEIGHT - res_btn.get_height() / 2 - uiSize * 7))

        if not showResources:
            screen.blit(cross_element, (WIDTH - nonaligned_icon.get_width() / 2 - uiSize, HEIGHT - nonaligned_icon.get_height() / 2 - uiSize * 7))

        color = (0, 0, 0)
        if xMouse > WIDTH - uiSize * 2 and HEIGHT - uiSize * 4 > yMouse > HEIGHT - uiSize * 6:
            color = (75, 75, 75)
            self.hovered = 'map view'
            if pressed1:
                color = (150, 150, 150)
        pygame.draw.rect(screen, color, pygame.Rect(WIDTH - uiSize * 2, HEIGHT - uiSize * 6, uiSize * 2, uiSize * 2))

        icons = [flag_icon, nonaligned_icon, heart_icon, tree_icon]
        screen.blit(icons[currentMap - 1], (WIDTH - flag_icon.get_width() / 2 - uiSize, HEIGHT - flag_icon.get_height() / 2 - uiSize * 5))

        color = (0, 0, 0)
        if xMouse > WIDTH - uiSize * 2 and HEIGHT - uiSize * 2 > yMouse > HEIGHT - uiSize * 4:
            color = (75, 75, 75)
            self.hovered = 'div view'
            if pressed1:
                color = (150, 150, 150)
        pygame.draw.rect(screen, color, pygame.Rect(WIDTH - uiSize * 2, HEIGHT - uiSize * 4, uiSize * 2, uiSize * 2))

        screen.blit(man_power_icon, (WIDTH - flag_icon.get_width() / 2 - uiSize, HEIGHT - flag_icon.get_height() / 2 - uiSize * 3))

        if not showDivisions:
            screen.blit(cross_element, (WIDTH - nonaligned_icon.get_width() / 2 - uiSize, HEIGHT - nonaligned_icon.get_height() / 2 - uiSize * 3))

        color = (0, 0, 0)
        if xMouse > WIDTH - uiSize * 2 and yMouse > HEIGHT - uiSize * 2:
            color = (75, 75, 75)
            self.hovered = 'cities view'
            if pressed1:
                color = (150, 150, 150)
        pygame.draw.rect(screen, color, pygame.Rect(WIDTH - uiSize * 2, HEIGHT - uiSize * 2, uiSize * 2, uiSize * 2))

        screen.blit(star_icon, (WIDTH - flag_icon.get_width() / 2 - uiSize, HEIGHT - flag_icon.get_height() / 2 - uiSize))

        if not showCities:
            screen.blit(cross_element, (WIDTH - nonaligned_icon.get_width() / 2 - uiSize, HEIGHT - nonaligned_icon.get_height() / 2 - uiSize))

        color = (0, 0, 0)
        if xMouse > WIDTH - uiSize * 2 and uiSize * 4 > yMouse > uiSize * 2:
            color = (75, 75, 75)
            self.hovered = 'speed up'
            if pressed1:
                color = (150, 150, 150)
        pygame.draw.rect(screen, color, pygame.Rect(WIDTH - uiSize * 2, uiSize * 2, uiSize * 2, uiSize * 2))

        screen.blit(plus_icon, (WIDTH - plus_icon.get_width() / 2 - uiSize, uiSize * 3 - plus_icon.get_height() / 2))

        color = (0, 0, 0)
        if xMouse > WIDTH - uiSize * 2 and uiSize * 6 > yMouse > uiSize * 4:
            color = (75, 75, 75)
            self.hovered = 'speed down'
            if pressed1:
                color = (150, 150, 150)
        pygame.draw.rect(screen, color, pygame.Rect(WIDTH - uiSize * 2, uiSize * 4, uiSize * 2, uiSize * 2))

        screen.blit(minus_icon, (WIDTH - minus_icon.get_width() / 2 - uiSize, uiSize * 5 - minus_icon.get_height() / 2))

        if selectedDivisions != []:
            color = (0, 0, 0)
            if WIDTH / 2 - uiSize * 2 < xMouse < WIDTH / 2 and HEIGHT - uiSize * 2 < yMouse:
                color = (75, 75, 75)
                self.hovered = 'div action 1'
                if pressed1:
                    color = (150, 150, 150)
            pygame.draw.rect(screen, color, pygame.Rect(WIDTH / 2 - uiSize * 2, HEIGHT - uiSize * 2, uiSize * 2, uiSize * 2))

            screen.blit(merge_icon, (WIDTH / 2 - merge_icon.get_width() - uiSize // 4, HEIGHT - merge_icon.get_height() - uiSize // 4))

            color = (0, 0, 0)
            if WIDTH / 2 < xMouse < WIDTH / 2 + uiSize * 2 and HEIGHT - uiSize * 2 < yMouse:
                color = (75, 75, 75)
                self.hovered = 'div action 2'
                if pressed1:
                    color = (150, 150, 150)
            pygame.draw.rect(screen, color, pygame.Rect(WIDTH / 2, HEIGHT - uiSize * 2, uiSize * 2, uiSize * 2))

            screen.blit(unmerge_icon, (WIDTH / 2 + uiSize // 4, HEIGHT - unmerge_icon.get_height() - uiSize // 4))

        if controlledCountry != None and controlledCountry in countryList:
            if globals()[controlledCountry].battleBorder != 0:
                worldRegionsInBattleBorder = []
                for region in globals()[controlledCountry].battleBorder:
                    x, y = regions.getLocation(region)
                    color = worldRegionsMap.get_at((round(x), round(y)))
                    worldRegion = regions.getWorldRegion(color[:3])
                    if worldRegion not in worldRegionsInBattleBorder:
                        worldRegionsInBattleBorder.append(worldRegion)

                worldRegionsInBattleBorder.sort()

                if len(worldRegionsInBattleBorder) > len(self.lastRegionNames):
                    newFront.play()

                self.lastRegionNames = [i for i in worldRegionsInBattleBorder]

                i = 0
                for worldRegion in worldRegionsInBattleBorder:
                    image = self.cachedRegionNames[worldRegion][0]
                    if WIDTH - image.get_width() - uiSize * 2.5 <= xMouse <= WIDTH - uiSize * 2.5:
                        if uiSize * 2.5 + uiSize * i <= yMouse <= uiSize * 5 + uiSize * i:
                            image = self.cachedRegionNames[worldRegion][1]
                            self.hovered = worldRegion
                            if pressed1:
                                image = self.cachedRegionNames[worldRegion][2]
                    screen.blit(image, (WIDTH - image.get_width() - uiSize * 2.5, uiSize * 2.5 + uiSize * i))
                    i += 2.5

    def drawPopups(self):
        for popup in popupList:
            popup.draw()

    def draw(self):
        lastHovered = self.hovered
        self.hovered = None

        if showCities:
            self.drawInfrastructure()

        if showDivisions:
            self.drawDivisions()

        if self.showUI:
            self.drawPopups()

        self.drawCommands()

        if self.showUI:
            self.drawButtons()
            self.drawSideBar()
            self.drawTopBar()

        if lastHovered != self.hovered and self.hovered != None:
            hoveredSound.play()


treaty_selected_provinces = []
treaty_map_surface = None

def generate_treaty_map(enemy_name, selected):
    surf = map.copy()
    player_color = globals()[controlledCountry].color if controlledCountry else (0, 200, 0)
    enemy_obj = globals().get(enemy_name)
    if not enemy_obj:
        return surf
    enemy_color = enemy_obj.color
    for region_id in enemy_obj.regions:
        rx, ry = regions.getLocation(region_id)
        if region_id in selected:
            fill(surf, round(rx), round(ry), (0, 255, 0))
        else:
            fill(surf, round(rx), round(ry), (200, 50, 50))
    return surf


class TreatyUserInterface(UserInterface):

    def __init__(self):
        super().__init__()
        self.regionsHovered = []
        self.cachedButtons = []
        self.lastTab = None
        self.lastCountry = None
        self.lastWidth = 0
        self.lastOptions = None

    def drawInfo(self):
        global politicalTabs, politicalButtonHovered, treaty_selected_provinces

        xMouse, yMouse = pygame.mouse.get_pos()
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()

        x = WIDTH * sideBarSize / 2 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation
        sw = WIDTH * sideBarSize * 0.9333333333333333 * sideBarAnimation

        resizedFlagImage = pygame.transform.scale(flagImage, (sw, 256 * WIDTH * sideBarSize / flagImage.get_width() * 0.9333333333333333 * sideBarAnimation))
        screen.blit(resizedFlagImage, (x, WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 4))
        yOffset = WIDTH * sideBarSize / 1.5 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 4 + resizedFlagImage.get_height() + int(sideBarAnimation * WIDTH * sideBarSize / 8)

        enemy_name = clicked.replace('_', ' ') if clicked else ''
        drawText(screen, enemy_name, int(sideBarAnimation * WIDTH * sideBarSize / 14), WIDTH * sideBarSize / 2 * sideBarAnimation, WIDTH * sideBarSize / 3 * sideBarAnimation - WIDTH * sideBarSize * 0.4666666666666667 / 2 * sideBarAnimation - sideBarScroll * sideBarSize + uiSize * 4)

        if clicked:
            enemy_obj = globals().get(clicked)
            total_enemy = len(enemy_obj.regions) if enemy_obj else 0
            sel_count = len(treaty_selected_provinces)
            drawText(screen, f"Selected: {sel_count} / {total_enemy} provinces", int(sideBarAnimation * WIDTH * sideBarSize / 14), x + uiSize / 4, yOffset, 'midleft', (200, 200, 100))
            yOffset += int(sideBarAnimation * WIDTH * sideBarSize / 7)

        politicalTabs = {}
        politicalButtonHovered = None
        fs = int(sideBarAnimation * WIDTH * sideBarSize / 14)
        btn_h = int(sideBarAnimation * WIDTH * sideBarSize / 8)
        btn_gap = int(sideBarAnimation * WIDTH * sideBarSize / 16)

        treaty_buttons = [
            ('Annex All', 'treaty_annex_all'),
            ('Annex Selected', 'treaty_annex_selected'),
            ('Puppet', 'treaty_puppet'),
            ('Install Government', 'treaty_install_govt'),
            ('Finalize Deal', 'treaty_finalize'),
        ]

        for label, action_id in treaty_buttons:
            color = (0, 0, 0)
            if 2 * x <= xMouse <= sw and yOffset <= yMouse <= yOffset + btn_h:
                color = (75, 75, 75)
                self.hovered = label
                politicalButtonHovered = (label, {}, [f"treaty_action('{action_id}')"])
                if pressed1:
                    color = (150, 150, 150)
            pygame.draw.rect(screen, color, pygame.Rect(2 * x, yOffset, sw - 2 * x, btn_h))
            drawText(screen, label, fs, x + sw / 2, yOffset + btn_h / 2, 'center')
            yOffset += btn_h + btn_gap

        if clicked:
            enemy_obj = globals().get(clicked)
            if enemy_obj:
                releasable_cultures = []
                for culture in enemy_obj.cultures.keys():
                    if len(enemy_obj.cultures[culture]) > 0 and countries.getCountryType(culture) is not None:
                        releasable_cultures.append(culture)
                if releasable_cultures:
                    drawText(screen, "Release:", fs, x + uiSize / 4, yOffset, 'midleft', (200, 200, 100))
                    yOffset += btn_h
                    for culture in releasable_cultures:
                        color = (0, 0, 0)
                        if 2 * x <= xMouse <= sw and yOffset <= yMouse <= yOffset + btn_h:
                            color = (75, 75, 75)
                            self.hovered = culture
                            culture_regs = enemy_obj.cultures[culture]
                            politicalButtonHovered = (f'Release {culture}', {}, [f"treaty_release('{culture}')"])
                            if pressed1:
                                color = (150, 150, 150)
                        pygame.draw.rect(screen, color, pygame.Rect(2 * x, yOffset, sw - 2 * x, btn_h))
                        drawText(screen, culture.replace('_', ' '), fs, x + sw / 2, yOffset + btn_h / 2, 'center')
                        yOffset += btn_h + btn_gap

    def drawInfrastructure(self, onlyCities=False):
        super().drawInfrastructure(onlyCities)

        for region_id in treaty_selected_provinces:
            rx, ry = regions.getLocation(region_id)
            rx = normalize(rx, map.get_width(), camx)
            xFinal = (rx + camx - 0.5) * zoom + WIDTH / 2
            yFinal = (ry + camy - 0.5) * zoom + HEIGHT / 2
            screen.blit(regionImage, (xFinal - regionImage.get_width() / 2, yFinal - regionImage.get_height() / 2))

    def draw(self, name):
        lastHovered = self.hovered
        self.hovered = None

        self.drawInfrastructure(True)
        self.drawSideBar()

        pygame.draw.rect(screen, (0, 0, 0), pygame.Rect(0, uiSize * 2, WIDTH, uiSize * 2))
        pygame.draw.rect(screen, (0, 0, 0), pygame.Rect(0, HEIGHT - uiSize * 2, WIDTH, uiSize * 2))

        drawText(screen, name, uiSize, WIDTH / 2, uiSize * 3, 'center')

        self.drawTopBar()

        draw_toasts()

        if self.hovered != lastHovered and self.hovered != None:
            hoveredSound.play()


def treaty_action(action_id):
    global treaty_selected_provinces, combatants, treaty_map_surface
    if not clicked:
        return
    enemy_obj = globals().get(clicked)
    player_obj = globals().get(controlledCountry)
    if not enemy_obj or not player_obj:
        return

    if action_id == 'treaty_annex_all':
        treaty_selected_provinces = list(enemy_obj.regions)
        treaty_map_surface = generate_treaty_map(clicked, treaty_selected_provinces)

    elif action_id == 'treaty_annex_selected':
        if treaty_selected_provinces:
            player_obj.addRegions([r for r in treaty_selected_provinces if r in enemy_obj.regions])
            treaty_selected_provinces = []
            treaty_map_surface = generate_treaty_map(clicked, treaty_selected_provinces)
            if enemy_obj.regions == []:
                combatants.remove(clicked)
        else:
            show_toast("No provinces selected")

    elif action_id == 'treaty_puppet':
        if enemy_obj.regions:
            puppet_states.append(PuppetState(controlledCountry, clicked))
            enemy_obj.ideology = list(player_obj.ideology)
            if player_obj.faction:
                enemy_obj.faction = player_obj.faction
            player_obj.militaryAccess.append(clicked)
            combatants.remove(clicked)
        else:
            show_toast("No provinces to puppet")

    elif action_id == 'treaty_install_govt':
        enemy_obj.ideology = list(player_obj.ideology)
        combatants.remove(clicked)

    elif action_id == 'treaty_finalize':
        if treaty_selected_provinces:
            player_obj.addRegions([r for r in treaty_selected_provinces if r in enemy_obj.regions])
            treaty_selected_provinces = []
        combatants.remove(clicked)


def treaty_release(culture):
    global combatants
    if not clicked:
        return
    enemy_obj = globals().get(clicked)
    if not enemy_obj:
        return
    culture_regions = enemy_obj.cultures.get(culture, [])
    if culture_regions:
        spawnCountry(culture, culture_regions)
        combatants.remove(clicked)


def getCountryType(culture):
    for country in countryList:
        if globals()[country].culture == culture:
            return country
    return None


def changeConstruction():
    for popup in popupList:
        if popup.title == 'Choose Construction':
            return
    building_types = list(BUILDING_DEFS.keys()) + ['destroy']
    btn_list = []
    y_off = 0
    cc = globals().get(controlledCountry) if controlledCountry else None
    for bt in building_types:
        if bt == 'destroy':
            label = 'Destroy'
        else:
            if cc:
                cost = get_dynamic_cost(bt, cc)
            else:
                cost = BUILDING_DEFS[bt].get('cost', 0)
            days = BUILDING_DEFS[bt].get('days', 0)
            desc = BUILDING_DEFS[bt].get('description', '')
            label = f"{bt.replace('_', ' ').title()} (${cost:,} | {days}d)"
        btn_list.append(list((label, f'setConstruction("{bt}")', 0, 3.625 + y_off)))
        y_off += 2.5
    popupList.append(Popup(
        'Choose Construction', [],
        btn_list,
        22, 3.625 + y_off + 1,
        WIDTH / 2, HEIGHT / 2,
        btnHalfWidth=10
    ))


def destroyCanal(canalList, name, country="Canal"):
    for canal in canalList:
        if canal in canals:
            canals.remove(canal)
            x, y = regions.getLocation(canal)
            fill(industryMap, round(x), round(y), (255, 255, 255))
            if controlledCountry in countryList:
                fill(modifiedIndustryMap, round(x), round(y), (255, 255, 255))

    if controlledCountry in countryList:
        popupList.append(Popup(
            f'The Destruction of the {name}',
            [f'Divisions can no longer go through the {name}.'],
            [list(('Okay', '', 0, 5.25))],
            22, 5,
            flag1=country, flag2=controlledCountry, sound=declareWarSound
        ))


EXTRACTION_BUILDINGS = {k for k, v in BUILDING_DEFS.items() if v.get('category') == 'resource'}

TRAINING_COST_PER_DIV = 25000
DIVISION_UPKEEP_PER_DAY = 200

def get_dynamic_cost(building_type, country_obj):
    bdef = BUILDING_DEFS.get(building_type, {})
    base = bdef.get('cost', 0)
    region_count = len(country_obj.regions) if hasattr(country_obj, 'regions') else 0
    return int(base * (1 + region_count / 150))

def setConstruction(type):
    global currentlyBuilding, showResources
    currentlyBuilding = type
    if type in EXTRACTION_BUILDINGS:
        showResources = True
    else:
        showResources = False


class MenuBackground:
    def __init__(self):
        self.time = 0
        self.maxTime = 400
        self.map = map.copy()
        self.camx = -self.map.get_width() / 2
        self.camy = -self.map.get_height() / 2
        self.zoom = 5
        self.speed = 1 / zoom / 4
        self.angle = random.randrange(0, 360)
        self.angle = math.radians(self.angle)

    def update(self):
        self.camx += math.cos(self.angle) * self.speed * 60 / FPS
        self.camy += math.sin(self.angle) * self.speed * 60 / FPS

        self.time += 60 / FPS
        if self.time > self.maxTime:
            self.camx = random.randrange(-self.map.get_width(), 0)
            self.camy = random.randrange(
                -round(self.map.get_height() * 3 / 4),
                -round(self.map.get_height() / 4)
            )
            self.angle = random.randrange(0, 360)
            self.angle = math.radians(self.angle)
            self.time = 0

        mapDrawer.draw(screen, maps[currentMap - 1], self.camx, self.camy, self.zoom)

        if self.time < self.maxTime * 1 / 10:
            fade = pygame.Surface((WIDTH, HEIGHT))
            fade.fill((0, 0, 0))
            fade.set_alpha((self.maxTime * 1 / 10 - self.time) / (self.maxTime * 1 / 10) * 255)
            screen.blit(fade, (0, 0))
            return
        elif self.time > self.maxTime * 9 / 10:
            fade = pygame.Surface((WIDTH, HEIGHT))
            fade.fill((0, 0, 0))
            fade.set_alpha((-(self.maxTime * 9 / 10) + self.time) / self.maxTime * 2550)
            screen.blit(fade, (0, 0))
            return


def mainMenu():
    global runningMain, WIDTH, HEIGHT

    hovered = None
    lastHovered = None
    pressedButton = False

    def prerender():
        text = getText('A Game By Gavin Grubert', uiSize)

        credit = pygame.Surface((round(2 * uiSize + text.get_width()), round(2 * uiSize)))
        credit.blit(text, (credit.get_width() / 2 - text.get_width() / 2,
                           credit.get_height() / 2 - text.get_height() / 2))

        creditHovered = pygame.Surface((round(2 * uiSize + text.get_width()), round(2 * uiSize)))
        creditHovered.fill((75, 75, 75))
        creditHovered.blit(text, (credit.get_width() / 2 - text.get_width() / 2,
                                  credit.get_height() / 2 - text.get_height() / 2))

        logo = pygame.image.load(os.path.join('img', 'logo.png')).convert_alpha()
        logo = pygame.transform.scale(logo, (int(logo.get_width() / logo.get_height() * uiSize * 12), uiSize * 12))

        buttons = list(('New Game', 'Load Game', 'Settings', 'Quit'))

        window = pygame.Surface((round(13 * uiSize),
                                 int(2 * uiSize + (len(buttons) * 2.5 + 0.75) * uiSize)))
        window.fill((50, 50, 50))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(0, 0, 13 * uiSize, 2 * uiSize))
        drawText(window, 'CE Version 1.1 by Barrett', uiSize, window.get_width() / 2, uiSize)

        for i in range(len(buttons)):
            pygame.draw.rect(window, (0, 0, 0),
                             pygame.Rect(window.get_width() / 2 - uiSize * 4,
                                         uiSize * 2.75 + i * uiSize * 2.5,
                                         uiSize * 8, uiSize * 1.75))
            drawText(window, buttons[i], uiSize,
                     window.get_width() / 2, uiSize * 3.625 + i * uiSize * 2.5)

        return (buttons, window, logo, credit, creditHovered)

    buttons, window, logo, credit, creditHovered = prerender()

    if not pygame.mixer.music.get_busy():
        pygame.mixer.music.load(os.path.join(musicDir, 'menuMusic.mp3'))
        pygame.mixer.music.set_volume(0.5 * musicVolume)
        pygame.mixer.music.play(-1)

    runningMain = True
    while runningMain:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()

        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN:
                if hovered != None:
                    clickedSound.play()

                if event.button == 1:
                    if hovered == 0:
                        startScreen()
                        return
                    elif hovered == 1:
                        saveGameMenu()
                        if not runningMain:
                            return
                    elif hovered == 2:
                        settings()
                        buttons, window, logo, credit, creditHovered = prerender()
                    elif hovered == 3:
                        pygame.quit()
                        sys.exit()
                    elif hovered == 4:
                        webbrowser.open('https://gavgrub.itch.io/')

            if event.type == pygame.QUIT:
                sys.exit()

            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

        hovered = None

        menuBackground.update()

        screen.blit(logo, (WIDTH / 2 - logo.get_width() / 2,
                           HEIGHT / 2 - logo.get_height() / 2 - uiSize * 6))

        screen.blit(window, (WIDTH / 2 - 13 * uiSize / 2,
                             HEIGHT / 2 + 1 * uiSize))

        buttonHeight = 3.75
        for i in range(len(buttons)):
            if WIDTH / 2 - uiSize * 8 / 2 <= xMouse <= WIDTH / 2 - uiSize * 8 / 2 + uiSize * 8:
                if HEIGHT / 2 + buttonHeight * uiSize + i * uiSize * 2.5 <= yMouse <= \
                   HEIGHT / 2 + buttonHeight * uiSize + i * uiSize * 2.5 + uiSize * 1.75:
                    color = (75, 75, 75)
                    hovered = i
                    if pressed1 and not pressedButton:
                        color = (150, 150, 150)
                    pygame.draw.rect(screen, color,
                                     pygame.Rect(WIDTH / 2 - uiSize * 8 / 2,
                                                 HEIGHT / 2 + buttonHeight * uiSize + i * uiSize * 2.5,
                                                 uiSize * 8, uiSize * 1.75))
                    drawText(screen, buttons[i], uiSize,
                             WIDTH / 2,
                             HEIGHT / 2 + (buttonHeight + 0.75 + 0.125) * uiSize + i * uiSize * 2.5)

        if xMouse < credit.get_width() and yMouse > HEIGHT - credit.get_height():
            screen.blit(creditHovered, (0, HEIGHT - credit.get_height()))
            hovered = 4
        else:
            screen.blit(credit, (0, HEIGHT - credit.get_height()))

        if hovered != lastHovered and hovered != None:
            hoveredSound.play()
        lastHovered = hovered

        pygame.display.flip()


def settings():
    global FPS, uiSize, musicVolume, soundVolume, runningMain, WIDTH, HEIGHT, userInterface

    currentUiSize = uiSize
    currentFPS = FPS
    currentMusicVolume = musicVolume
    currentSoundVolume = soundVolume

    hovered = None
    lastHovered = None
    pressedButton = False

    def render():
        buttons = [f'UI Size: {currentUiSize}',
                    f'FPS: {currentFPS}',
                    f'Sound Volume: {round(currentSoundVolume * 100)}',
                    f'Music Volume: {round(currentMusicVolume * 100)}',
                    'Apply',
                    'Back']

        window = pygame.Surface((round(9.5 * uiSize),
                                 int(2 * uiSize + (len(buttons) * 2.5 + 0.75) * uiSize)))
        window.fill((50, 50, 50))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(0, 0, 9.5 * uiSize, 2 * uiSize))
        drawText(window, 'Settings', uiSize, window.get_width() / 2, uiSize)

        for i in range(len(buttons)):
            pygame.draw.rect(window, (0, 0, 0),
                             pygame.Rect(window.get_width() / 2 - uiSize * 4,
                                         uiSize * 2.75 + i * uiSize * 2.5,
                                         uiSize * 8, uiSize * 1.75))
            drawText(window, buttons[i], uiSize,
                     window.get_width() / 2, uiSize * 3.625 + i * uiSize * 2.5)

        return (buttons, window)

    buttons, window = render()

    runningMain = True
    while runningMain:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()

        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN:
                if hovered != None:
                    clickedSound.play()

                if event.button == 1:
                    if hovered == 0:
                        currentUiSize += 4
                        if currentUiSize > 24:
                            currentUiSize = 8
                    elif hovered == 1:
                        currentFPS += 30
                        if currentFPS > 120:
                            currentFPS = 30
                    elif hovered == 2:
                        currentSoundVolume += 0.1
                        if currentSoundVolume > 1:
                            currentSoundVolume = 0
                    elif hovered == 3:
                        currentMusicVolume += 0.1
                        if currentMusicVolume > 1:
                            currentMusicVolume = 0
                    elif hovered == 4:
                        FPS = currentFPS
                        uiSize = currentUiSize
                        musicVolume = currentMusicVolume
                        soundVolume = currentSoundVolume

                        with open('settings', 'wb') as file:
                            variables = {}
                            variables['uiSize'] = uiSize
                            variables['musicVolume'] = musicVolume
                            variables['soundVolume'] = soundVolume
                            variables['FPS'] = FPS
                            pickle.dump(variables, file)

                        loadImages(flagsDir, uiSize)
                        loadImages(iconsDir, uiSize * 1.5, (0, 0, 0))
                        loadSounds(soundDir, soundVolume)

                        pygame.mixer.music.set_volume(0.5 * musicVolume)

                        for c in countryList:
                            globals()[c].resetDivColor()
                        userInterface = UserInterface()
                    elif hovered == 5:
                        return

                    buttons, window = render()

            if event.type == pygame.QUIT:
                sys.exit()

            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

        hovered = None

        menuBackground.update()

        screen.blit(window, (WIDTH / 2 - window.get_width() / 2,
                             HEIGHT / 2 - window.get_height() / 2))

        buttonHeight = 3.75
        for i in range(len(buttons)):
            if WIDTH / 2 - uiSize * 4 <= xMouse <= WIDTH / 2 + uiSize * 4:
                if (HEIGHT / 2 - window.get_height() / 2 + uiSize * 2.75 + i * uiSize * 2.5 <= yMouse <=
                    HEIGHT / 2 - window.get_height() / 2 + uiSize * 2.75 + i * uiSize * 2.5 + uiSize * 1.75):
                    color = (75, 75, 75)
                    hovered = i
                    if pressed1 and not pressedButton:
                        color = (150, 150, 150)
                    pygame.draw.rect(screen, color,
                                     pygame.Rect(WIDTH / 2 - uiSize * 4,
                                                 HEIGHT / 2 - window.get_height() / 2 + uiSize * 2.75 + i * uiSize * 2.5,
                                                 uiSize * 8, uiSize * 1.75))
                    drawText(screen, buttons[i], uiSize,
                             WIDTH / 2,
                             HEIGHT / 2 - window.get_height() / 2 + uiSize * 2.75 + i * uiSize * 2.5 + buttonHeight * uiSize / 4)

        if hovered != lastHovered and hovered != None:
            hoveredSound.play()
        lastHovered = hovered

        pygame.display.flip()


def changeClicked(current):
    global clicked, releasables, flagImage

    clicked = current
    releasables = []

    for culture in globals()[current].cultures.keys():
        if len(globals()[current].cultures[culture]) <= 0 or countries.getCountryType(culture) == None:
            pass
        else:
            releasables = [culture] + releasables

        flagImage = pygame.image.load(
            os.path.join('flags', f'{current.lower()}_flag.png')
        ).convert()
        flagImage = pygame.transform.scale(
            flagImage, (flagImage.get_width() * 256 / flagImage.get_height(), 256)
        )


def updateTreatyOptions():
    global treatyOptions
    treatyOptions = getDemands(countryList, globals()[controlledCountry],
                               globals()[clicked], releasables)


def peaceTreaty(mainCountry):
    global combatants, currentMap, currentMusic, treaty_selected_provinces, treaty_map_surface
    global pressed, clicked, WIDTH, HEIGHT

    if globals()[mainCountry].capital != None:
        treatyName = f'Treaty of {globals()[mainCountry].capital}'
    else:
        treatyName = f'Treaty of {globals()[mainCountry].name}'

    combatants = [i for i in globals()[mainCountry].atWarWith]

    victors = []
    for country in countryList:
        if len(set(globals()[country].atWarWith).intersection(set(combatants))) > 0 and \
           country not in combatants:
            victors.append(country)
    involvedCountries = victors + combatants

    if controlledCountry in involvedCountries:
        currentMap = 1

    for victor in victors:
        for enemy in combatants:
            globals()[enemy].makePeace(victor, False)

    current = None

    if currentMusic == 'war' and controlledCountry in involvedCountries:
        pygame.mixer.music.load(os.path.join(musicDir, 'gameMusic.mp3'))
        pygame.mixer.music.play(-1, fade_ms=2000)
        pygame.mixer.music.set_volume(musicVolume)
        currentMusic = 'game'

    if controlledCountry in victors:
        if currentMusic == 'war':
            discussionSound.play()

        for country in involvedCountries:
            for region in globals()[country].regionsBeforeWar:
                if region not in globals()[country].regions and \
                   regions.getOwner(region) in involvedCountries:
                    globals()[country].addRegion(region)

        for victor in victors:
            for enemy in combatants:
                if victor == controlledCountry:
                    continue

                if globals()[victor].culture == globals()[enemy].culture:
                    globals()[victor].annexCountry(globals()[victor].culture, enemy)
                elif globals()[victor].culture in globals()[enemy].cultures.keys():
                    globals()[victor].addRegions(
                        globals()[enemy].cultures[globals()[victor].culture]
                    )

        treaty_selected_provinces = []
        _treaty_click_held = False

        while combatants != []:
            pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
            xMouse, yMouse = pygame.mouse.get_pos()

            clock.tick(FPS)

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()

                if event.type == pygame.VIDEORESIZE:
                    WIDTH, HEIGHT = pygame.display.get_surface().get_size()

                if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                    sidebar_w = WIDTH * sideBarSize * sideBarAnimation
                    if xMouse > sidebar_w:
                        rgn = regionClicked(xMouse, yMouse)
                        if rgn is not None and clicked:
                            enemy_obj = globals().get(clicked)
                            if enemy_obj and rgn in enemy_obj.regions:
                                if rgn in treaty_selected_provinces:
                                    treaty_selected_provinces.remove(rgn)
                                else:
                                    treaty_selected_provinces.append(rgn)
                                treaty_map_surface = generate_treaty_map(clicked, treaty_selected_provinces)

            for country_n in countryList:
                for div in globals()[country_n].divisions:
                    div.updateLocation()

            if current not in combatants or current == None:
                if combatants:
                    current = combatants[0]
                    changeClicked(current)
                    treaty_selected_provinces = []
                    treaty_map_surface = generate_treaty_map(current, treaty_selected_provinces)
                else:
                    break

            screen.fill((0, 0, 0))

            if treaty_map_surface is not None:
                mapDrawer.draw(screen, treaty_map_surface, camx, camy, zoom, 0)
            else:
                mapDrawer.draw(screen, maps[currentMap - 1], camx, camy, zoom, currentMap - 1)

            treatyUserInterface.draw(treatyName)

            pygame.display.flip()

            treatyController.input()

        treaty_selected_provinces = []
        treaty_map_surface = None

    for country in involvedCountries:
        if globals()[country].regions == []:
            globals()[country].kill()
        if globals()[country].capitulated:
            globals()[country].capitulated = False
            globals()[country].usedManPower = 0
        globals()[country].battleBorder = globals()[country].getBattleBorder()

        for div in globals()[country].divisions:
            if regions.getOwner(div.region) != country:
                if globals()[country].regions != []:
                    div.move(random.choice(globals()[country].regions))

    for battle in battleList:
        if battle.attacker.country in involvedCountries or \
           battle.defender.country in involvedCountries:
            battleList.remove(battle)
            battle.attacker.fighting = False
            battle.defender.fighting = False

    if controlledCountry in victors:
        clappingSound.play()

    if controlledCountry == None:
        return

    if len(globals()[controlledCountry].atWarWith) > 0 and \
       controlledCountry in involvedCountries:
        pygame.mixer.music.load(os.path.join(musicDir, 'warMusic.mp3'))
        pygame.mixer.music.play(-1, fade_ms=2000)
        pygame.mixer.music.set_volume(musicVolume)
        currentMusic = 'war'


def decisionTree(country):
    global treeZoom, treeCamx, treeCamy, WIDTH, HEIGHT

    clickedSound.play()

    hovered = None
    hoveredImage = None
    isPressed = False
    didClicked = False

    tree = globals()[country].decisionTree
    fte = getattr(globals()[country], 'focusTreeEngine', None)

    def get_focus_state(name, items):
        if items[9]:
            return 'completed'
        prereqs_met = all(tree[n][9] for n in items[3] if n in tree)
        if not prereqs_met:
            return 'locked'
        try:
            cond_met = eval(items[4].replace('self', country))
        except Exception:
            cond_met = False
        if not cond_met:
            return 'locked'
        if fte:
            node = fte.tree.get(name)
            if node:
                group = node.get('exclusive_group')
                if group and group in fte.locked_groups and name not in fte.completed_focuses:
                    return 'locked'
        if globals()[country].politicalPower < items[2]:
            return 'too_expensive'
        return 'available'

    def get_focus_color(state):
        if state == 'completed':
            return (20, 60, 20)
        elif state == 'available':
            return (0, 0, 0)
        elif state == 'too_expensive':
            return (40, 30, 0)
        elif state == 'locked':
            return (50, 15, 15)
        return (30, 30, 30)

    def reloadButtons():
        xButtonSize = uiSize * 10 * treeZoom
        yButtonSize = uiSize * 1.75 * treeZoom
        yOffset = uiSize * 4 + uiSize * 2 * treeZoom
        xOffset = uiSize * 2 * treeZoom
        scaleFactor = uiSize / 16 * treeZoom

        buttons = []
        for name, items in tree.items():
            state = get_focus_state(name, items)
            bg = get_focus_color(state)
            rect = pygame.surface.Surface((xButtonSize, yButtonSize))
            rect.fill(bg)
            if state == 'completed':
                pygame.draw.rect(rect, (40, 120, 40), rect.get_rect(), max(1, round(2 * treeZoom)))
            elif state == 'locked':
                pygame.draw.rect(rect, (80, 30, 30), rect.get_rect(), max(1, round(2 * treeZoom)))
            elif state == 'available':
                pygame.draw.rect(rect, (80, 80, 80), rect.get_rect(), max(1, round(2 * treeZoom)))
            elif state == 'too_expensive':
                pygame.draw.rect(rect, (100, 80, 20), rect.get_rect(), max(1, round(2 * treeZoom)))
            txt_color = (200, 200, 200) if state != 'completed' else (150, 255, 150)
            drawText(rect, name, round(uiSize * treeZoom),
                     xButtonSize / 2, yButtonSize / 2, 'center', txt_color)
            buttons.append(rect)

        return (xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons)

    xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons = reloadButtons()

    runningThisMenu = True
    while runningThisMenu:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()
        keystate = pygame.key.get_pressed()

        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_LEFTBRACKET and treeZoom < 2.1435888100000002:
                    treeZoom *= 1.1
                    xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons = reloadButtons()
                elif event.key == pygame.K_RIGHTBRACKET and treeZoom > 0.2176291357901484:
                    treeZoom /= 1.1
                    xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons = reloadButtons()

            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

            if event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 4 and treeZoom < 2.1435888100000002:
                    treeZoom *= 1.1
                    xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons = reloadButtons()
                elif event.button == 5 and treeZoom > 0.2176291357901484:
                    treeZoom /= 1.1
                    xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons = reloadButtons()

        if keystate[pygame.K_SPACE]:
            runningThisMenu = False

        cameraSpeed = uiSize / 4 * 60 / FPS
        if keystate[pygame.K_LSHIFT] or keystate[pygame.K_RSHIFT]:
            cameraSpeed *= 3

        if keystate[pygame.K_s] or keystate[pygame.K_DOWN]:
            treeCamy -= cameraSpeed / treeZoom
        if keystate[pygame.K_w] or keystate[pygame.K_UP]:
            treeCamy += cameraSpeed / treeZoom
        if keystate[pygame.K_d] or keystate[pygame.K_RIGHT]:
            treeCamx -= cameraSpeed / treeZoom
        if keystate[pygame.K_a] or keystate[pygame.K_LEFT]:
            treeCamx += cameraSpeed / treeZoom

        treeCamx = min(0, treeCamx)
        treeCamy = min(0, treeCamy)

        screen.fill((50, 50, 50))

        if isPressed and not pressed1:
            isPressed = False

        for name, items in tree.items():
            for name2 in items[3]:
                if name2 not in tree:
                    continue
                items2 = tree[name2]
                x1 = xOffset + items[0] * scaleFactor + treeCamx * treeZoom + xButtonSize / 2
                y1 = yOffset + items[1] * uiSize * 4 * scaleFactor + treeCamy * treeZoom + yButtonSize / 2
                x2 = xOffset + items2[0] * scaleFactor + treeCamx * treeZoom + xButtonSize / 2
                y2 = yOffset + items2[1] * uiSize * 4 * scaleFactor + treeCamy * treeZoom + yButtonSize / 2
                dep_met = items2[9]
                line_color = (60, 180, 60) if dep_met else (180, 60, 60)
                pygame.draw.line(screen, line_color, (x1, y1), (x2, y2),
                                 max(1, round(uiSize // 4 * treeZoom)))

        lastHovered = hovered
        hovered = None
        hoveredxPos = None
        hoveredyPos = None

        i = 0
        hoveredName = None
        for name, items in tree.items():
            xPos = xOffset + items[0] * scaleFactor + treeCamx * treeZoom
            yPos = yOffset + items[1] * uiSize * 4 * scaleFactor + treeCamy * treeZoom

            state = get_focus_state(name, items)

            if xPos <= xMouse <= xPos + xButtonSize and \
               yPos <= yMouse <= yPos + yButtonSize:
                hovered = items
                hoveredName = name
                hoveredxPos = xPos
                hoveredyPos = yPos

                if state == 'available' and pressed1:
                    color = (150, 150, 150)
                    if not isPressed and globals()[country].focus == None:
                        globals()[country].focus = [name, items[6], items[5]]
                        globals()[country].politicalPower -= items[2]
                        items[9] = True
                        if fte:
                            fte.completed_focuses.add(name)
                            node = fte.tree.get(name)
                            if node and node.get('exclusive_group'):
                                fte.locked_groups.add(node['exclusive_group'])
                        isPressed = True
                        clickedSound.play()
                        xButtonSize, yButtonSize, xOffset, yOffset, scaleFactor, buttons = reloadButtons()
                    elif not isPressed:
                        failedClickSound.play()
                        isPressed = True
                elif pressed1 and not isPressed:
                    failedClickSound.play()
                    isPressed = True

                if state != 'completed':
                    hover_color = (75, 75, 75) if state == 'available' else (60, 30, 30)
                    pygame.draw.rect(screen, hover_color,
                                     pygame.Rect(xPos, yPos, xButtonSize, yButtonSize))
                    txt_color = WHITE if state == 'available' else (180, 120, 120)
                    drawText(screen, name, round(uiSize * treeZoom),
                             xPos + xButtonSize / 2, yPos + yButtonSize / 2, 'center', txt_color)
                else:
                    screen.blit(buttons[i], (xPos, yPos))
            else:
                screen.blit(buttons[i], (xPos, yPos))

            i += 1

        if lastHovered != hovered and hovered != None:
            desc_text = hovered[7] if hovered[7] else "No description."
            req_text = hovered[8] if hovered[8] else ""
            cost_text = f'{hovered[2]} political power, {hovered[6]} days.'

            prereq_names = [n for n in hovered[3] if n in tree]
            prereq_text = ""
            if prereq_names:
                met = [n for n in prereq_names if tree[n][9]]
                unmet = [n for n in prereq_names if not tree[n][9]]
                prereq_text = "Requires: " + ", ".join(prereq_names)
                if unmet:
                    prereq_text += f" (MISSING: {', '.join(unmet)})"

            state = get_focus_state(hoveredName, hovered) if hoveredName else 'locked'
            if state == 'completed':
                status_text = "[COMPLETED]"
                status_color = (100, 255, 100)
            elif state == 'available':
                status_text = "[AVAILABLE]"
                status_color = (100, 200, 255)
            elif state == 'too_expensive':
                status_text = "[NOT ENOUGH PP]"
                status_color = (255, 200, 50)
            else:
                status_text = "[LOCKED]"
                status_color = (255, 80, 80)

            lines_to_measure = [status_text, cost_text, desc_text]
            if prereq_text:
                lines_to_measure.append(prereq_text)
            if req_text:
                lines_to_measure.append(req_text)

            fs = max(round(uiSize * 0.8), 8)
            popupSize = max(getText(t, fs).get_width() for t in lines_to_measure) + uiSize
            popupSize = min(popupSize, int(WIDTH * 0.4))

            line_count = 3 + (1 if prereq_text else 0) + (1 if req_text else 0)
            popupHeight = int(uiSize * 1.4 * line_count + uiSize)

            hoveredImage = pygame.Surface((popupSize, popupHeight))
            hoveredImage.fill((20, 20, 20))
            pygame.draw.rect(hoveredImage, (100, 100, 100), hoveredImage.get_rect(), 1)

            ty = int(uiSize * 0.5)
            drawText(hoveredImage, status_text, fs, uiSize // 2, ty, 'midleft', status_color)
            ty += int(uiSize * 1.4)
            drawText(hoveredImage, cost_text, fs, uiSize // 2, ty, 'midleft', (200, 200, 200))
            ty += int(uiSize * 1.4)
            drawText(hoveredImage, desc_text, fs, uiSize // 2, ty, 'midleft', (220, 220, 180))
            ty += int(uiSize * 1.4)
            if prereq_text:
                p_color = (180, 180, 180) if not any(not tree[n][9] for n in prereq_names) else (255, 140, 140)
                drawText(hoveredImage, prereq_text, fs, uiSize // 2, ty, 'midleft', p_color)
                ty += int(uiSize * 1.4)
            if req_text:
                drawText(hoveredImage, req_text, fs, uiSize // 2, ty, 'midleft', (160, 160, 160))

            hoveredSound.play()

        if hovered != None:
            xPos = xOffset + hovered[0] * scaleFactor + treeCamx * treeZoom
            yPos = yOffset + hovered[1] * uiSize * 4 * scaleFactor + treeCamy * treeZoom
            tip_x = xPos + xButtonSize + uiSize
            tip_y = yPos - hoveredImage.get_height() / 2 + yButtonSize / 2
            if tip_x + hoveredImage.get_width() > WIDTH:
                tip_x = xPos - hoveredImage.get_width() - uiSize
            if tip_y < 0:
                tip_y = 0
            if tip_y + hoveredImage.get_height() > HEIGHT:
                tip_y = HEIGHT - hoveredImage.get_height()
            screen.blit(hoveredImage, (tip_x, tip_y))

        pygame.draw.rect(screen, (0, 0, 0),
                         pygame.Rect(0, uiSize * 2, WIDTH, uiSize * 2))

        color = (0, 0, 0)
        if yMouse >= HEIGHT - uiSize * 2:
            color = (75, 75, 75)
            if pressed1:
                color = (150, 150, 150)
                runningThisMenu = False
        pygame.draw.rect(screen, color,
                         pygame.Rect(0, HEIGHT - uiSize * 2, WIDTH, uiSize * 2))
        drawText(screen, 'Exit', uiSize, WIDTH / 2, HEIGHT - uiSize * 1, 'center')

        drawText(screen, f'{country.replace("_", " ")} Political Decision Tree',
                 uiSize, WIDTH / 2, uiSize * 3, 'center')

        userInterface.drawTopBar()

        pygame.display.flip()


def startScreen():
    global controlledCountry, currentMusic, camx, camy, zoom, selectedCountry
    global running, WIDTH, HEIGHT, pressed

    controlledCountry = None
    currentMusic = 'game'

    camx = -map.get_width() / 2
    camy = -map.get_height() / 2
    zoom = 1

    majorOrder = ('French', 'American', 'British', 'German', 'Japanese',
                  'Russian', 'Chinese', 'Indian')
    minorOrder = ('Canadian', 'Australian', 'Irish', 'Spanish', 'Dutch',
                  'Belgian', 'Italian', 'Swedish', 'Finnish', 'Polish',
                  'Ukrainian', 'Belarusian', 'Hungarian', 'Romanian',
                  'Turkish', 'Mexican', 'Brazilian', 'Argentinian',
                  'Egyptian', 'Ethiopian', 'Nigerian', 'South African',
                  'Israeli', 'Palestinian', 'Saudi', 'Iraqi', 'Iranian',
                  'Pakistani', 'North Korean', 'Korean', 'Vietnamese',
                  'Thai', 'Indonesian')

    minorOrder = [i for i in minorOrder if getCountryType(i) != None]

    loadedFlags = [pygame.image.load(os.path.join('flags', f'{getCountryType(i).lower()}_flag.png')).convert()
                   for i in majorOrder if getCountryType(i) != None]
    majorFlags = [pygame.transform.scale(i, (uiSize * 6, int(i.get_height() / i.get_width() * uiSize * 6)))
                  for i in loadedFlags]

    loadedIcons = [pygame.image.load(os.path.join('img', f'{i}.png')).convert_alpha()
                   for i in majorOrder]
    icons = [pygame.transform.scale(i, (uiSize * 6, uiSize * 6)) for i in loadedIcons]

    loadedFlags = [pygame.image.load(os.path.join('flags', f'{getCountryType(i).lower()}_flag.png')).convert()
                   for i in minorOrder if getCountryType(i) != None]
    minorFlags = [pygame.transform.scale(i, (int(i.get_width() / i.get_height() * uiSize * 1.5), uiSize * 1.5))
                  for i in loadedFlags]

    buttons = list(('Back', 'Maps', 'More Countries', 'Start Game'))

    hovered = None
    selectedCountry = None
    pressedButton = True
    hoveredNoise = None

    def reloadWindow():
        window = pygame.Surface((59 * uiSize, 34 * uiSize))
        window.fill((50, 50, 50))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(0, 0, 29.5 * uiSize * 2, 2 * uiSize))
        drawText(window, 'Choose Country', uiSize, window.get_width() / 2, uiSize)
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 - uiSize * 57.25 / 2, uiSize * 17, uiSize * 57.25, uiSize * 5.5))
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 - uiSize * 57.25 / 2, uiSize * 23.25, uiSize * 23.875, uiSize * 10))
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + uiSize * 4.75, uiSize * 23.25, uiSize * 23.875, uiSize * 10))
        for i in range(len(buttons)):
            pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 - uiSize * 4, uiSize * 23.25 + i * uiSize * 2.5, uiSize * 8, uiSize * 1.75))
            drawText(window, buttons[i], uiSize, window.get_width() / 2, uiSize * 24.125 + i * uiSize * 2.5)
        if selectedCountry == None:
            drawText(window, 'Select a Country', uiSize, window.get_width() / 2 - 28.125 * uiSize, 24.25 * uiSize, 'midleft')
        else:
            infoX = window.get_width() / 2 - 28.125 * uiSize
            drawText(window, 'Selected Country: ' + selectedCountry.replace('_', ' '), uiSize, infoX, 24.25 * uiSize, 'midleft')
            drawText(window, 'Ideology: ' + getIdeologyName(globals()[selectedCountry].ideology).capitalize(), uiSize, infoX, 25.75 * uiSize, 'midleft')
            theFaction = globals()[selectedCountry].faction
            if theFaction != None:
                theFaction = theFaction.replace('_', ' ')
            text = f'Faction: {theFaction}'
            if globals()[selectedCountry].factionLeader:
                text += ' (Faction Leader)'
            drawText(window, text, uiSize, infoX, 27.25 * uiSize, 'midleft')
            active = sum([div.divisionStack for div in globals()[selectedCountry].divisions]) * 10000
            reserve = max(globals()[selectedCountry].manPower - globals()[selectedCountry].totalMilitary, 0)
            drawText(window, f'Military: {prefixNumber(active)} (Active), {prefixNumber(reserve)} (Reserve)', uiSize, infoX, 28.75 * uiSize, 'midleft')
            drawText(window, f'Regions: {len(globals()[selectedCountry].regions)}', uiSize, infoX, 30.25 * uiSize, 'midleft')
            drawText(window, f'Factories: {globals()[selectedCountry].factories}', uiSize, infoX, 31.75 * uiSize, 'midleft')
        xOffset = 0
        yOffset = 0
        for i in range(len(minorOrder)):
            window.blit(minorFlags[i], (window.get_width() / 2 - uiSize * 28 + xOffset, 17.75 * uiSize + yOffset))
            xOffset += minorFlags[i].get_width() + uiSize
            if i != len(minorOrder) - 1:
                if window.get_width() / 2 - 28.125 * uiSize + xOffset > uiSize * 57.75 - minorFlags[i].get_width():
                    xOffset = 0
                    yOffset += minorFlags[i].get_height() + uiSize
        for i in range(8):
            x = window.get_width() / 2 + 7.25 * i * uiSize - 28.625 * uiSize
            y = 2.75 * uiSize
            width = 6.5 * uiSize
            height = 13.5 * uiSize
            pygame.draw.rect(window, (0, 0, 0), pygame.Rect(x, y, width, height))
            drawText(window, getCountryType(majorOrder[i]).replace('_', ' '), uiSize,
                     window.get_width() / 2 + 7.25 * i * uiSize - 28.375 * uiSize + uiSize * 6.25 / 2 - uiSize / 8,
                     15.5 * uiSize)
            pygame.draw.rect(window, (25, 25, 25), pygame.Rect(x, 9.75 * uiSize, 6.5 * uiSize, 5 * uiSize))
            window.blit(majorFlags[i], (window.get_width() / 2 + 7.25 * i * uiSize - 28.375 * uiSize,
                                        12.25 * uiSize - majorFlags[i].get_height() / 2))
            window.blit(icons[i], (window.get_width() / 2 + 7.25 * i * uiSize - 28.375 * uiSize,
                                   6.375 * uiSize - icons[i].get_height() / 2))
        return window

    window = reloadWindow()

    runningThisMenu = True
    while runningThisMenu:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()

        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                if hovered != None and type(hovered) == str:
                    clickedSound.play()
                    changeCountry(hovered)
                    selectedCountry = hovered
                    window = reloadWindow()
                    pressed = True
                for i in range(len(buttons)):
                    if (WIDTH / 2 - uiSize * 8 / 2 <= xMouse <= WIDTH / 2 - uiSize * 8 / 2 + uiSize * 8 and
                            HEIGHT / 2 + 6.5 * uiSize + i * uiSize * 2.5 <= yMouse <= HEIGHT / 2 + 6.5 * uiSize + i * uiSize * 2.5 + uiSize * 1.75):
                        if i == 0:
                            clickedSound.play()
                            mainMenu()
                            return
                        elif i == 1:
                            clickedSound.play()
                            mapsMenu()
                            return
                        elif i == 2:
                            clickedSound.play()
                            countriesMenu()
                            window = reloadWindow()
                        elif i == 3:
                            popupList = []
                            startGameSound.play()
                            pygame.mixer.music.load(os.path.join(musicDir, 'gameMusic.mp3'))
                            pygame.mixer.music.set_volume(0.5 * musicVolume)
                            pygame.mixer.music.play(-1)
                            runningThisMenu = False

            if event.type == pygame.QUIT:
                runningThisMenu = False
                running = False

            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

        hovered = None

        menuBackground.update()
        screen.blit(window, (WIDTH / 2 - window.get_width() / 2, HEIGHT / 2 - window.get_height() / 2))

        for i in range(8):
            x = WIDTH / 2 + 7.25 * i * uiSize - 28.625 * uiSize
            y = HEIGHT / 2 - 14.25 * uiSize
            width = uiSize * 6.5
            height = uiSize * 13.5
            if x <= xMouse <= x + width and y <= yMouse <= y + height:
                color = (75, 75, 75)
                hovered = getCountryType(majorOrder[i])
                if pressed1:
                    color = (150, 150, 150)
                pygame.draw.rect(screen, color, pygame.Rect(x, y, width, math.ceil(height)))
                drawText(screen, getCountryType(majorOrder[i]).replace('_', ' '), uiSize,
                         round(WIDTH / 2 + 7.25 * i * uiSize - 28.375 * uiSize + uiSize * 6.25 / 2 - uiSize / 8),
                         round(HEIGHT / 2 - 1.5 * uiSize))
                if color == (0, 0, 0):
                    flagBackgroundColor = (25, 25, 25)
                else:
                    flagBackgroundColor = color
                pygame.draw.rect(screen, flagBackgroundColor, pygame.Rect(x, HEIGHT / 2 - 7.25 * uiSize, uiSize * 6.5, uiSize * 5))
                screen.blit(majorFlags[i], (round(WIDTH / 2 + 7.25 * i * uiSize - 28.375 * uiSize),
                                            round(HEIGHT / 2 - 4.75 * uiSize - majorFlags[i].get_height() / 2)))
                screen.blit(icons[i], (WIDTH / 2 + 7.25 * i * uiSize - 28.375 * uiSize,
                                       HEIGHT / 2 - 10.625 * uiSize - icons[i].get_height() / 2))

        xOffset = 0
        yOffset = 0
        for i in range(len(minorOrder)):
            flagX = WIDTH / 2 - 28 * uiSize + xOffset
            flagY = HEIGHT / 2 + 0.75 * uiSize + yOffset
            if (flagX - uiSize / 4 <= xMouse <= flagX + minorFlags[i].get_width() + uiSize / 4 and
                    flagY - uiSize / 4 <= yMouse <= flagY + minorFlags[i].get_height() + uiSize / 4):
                color = (75, 75, 75)
                hovered = getCountryType(minorOrder[i])
                if pressed1:
                    color = (150, 150, 150)
                pygame.draw.rect(screen, color, pygame.Rect(
                    WIDTH / 2 - 28 * uiSize + xOffset - uiSize / 4,
                    HEIGHT / 2 + 0.75 * uiSize + yOffset - uiSize / 4,
                    minorFlags[i].get_width() + uiSize / 2, minorFlags[i].get_height() + uiSize / 2))
                screen.blit(minorFlags[i], (WIDTH / 2 - 28 * uiSize + xOffset,
                                            HEIGHT / 2 + 0.75 * uiSize + yOffset))
            xOffset += minorFlags[i].get_width() + uiSize
            if i != len(minorOrder) - 1:
                if WIDTH / 2 - 28 * uiSize + xOffset + minorFlags[i + 1].get_width() > WIDTH / 2 + 28 * uiSize:
                    xOffset = 0
                    yOffset += minorFlags[i].get_height() + uiSize

        for i in range(len(buttons)):
            btnLeft = WIDTH / 2 - uiSize * 8 / 2
            btnTop = HEIGHT / 2 + 6.25 * uiSize + i * uiSize * 2.5
            if (btnLeft <= xMouse <= btnLeft + uiSize * 8 and
                    btnTop <= yMouse <= btnTop + uiSize * 1.75):
                color = (75, 75, 75)
                hovered = i
                if pressed1 and not pressedButton:
                    color = (150, 150, 150)
                pygame.draw.rect(screen, color, pygame.Rect(
                    btnLeft, btnTop, uiSize * 8, math.ceil(uiSize * 1.75)))
                drawText(screen, buttons[i], uiSize, WIDTH / 2,
                         HEIGHT / 2 + 7.125 * uiSize + i * uiSize * 2.5)

        if hoveredNoise != hovered and hovered != None:
            hoveredSound.play()
        hoveredNoise = hovered
        pygame.display.flip()

    if controlledCountry != None:
        startPopup()


def mapsMenu():
    global mapName, WIDTH, HEIGHT, menuBackground

    orderedMaps = []
    orderedDates = []
    orderedTags = []
    orderedCreators = []

    mapImage = map.copy()
    mapImage = pygame.transform.scale(map, (uiSize * 10 * mapImage.get_width() / mapImage.get_height(), uiSize * 10))

    for dir in os.listdir(startsDir):
        orderedMaps.append(dir)
        with open(os.path.join(f"{startsDir}\\{dir}", 'startDate.txt')) as date:
            startDate = [int(x) for x in date.read().strip('[]').split(', ')]
        orderedDates.append(startDate[3])
        with open(os.path.join(f"{startsDir}\\{dir}", 'tags.txt')) as tag:
            orderedTags.append(tag.read())
        with open(os.path.join(f"{startsDir}\\{dir}", 'creator.txt')) as tag:
            orderedCreators.append(tag.read())

    tag = orderedTags[orderedMaps.index(mapName)]
    creator = orderedCreators[orderedMaps.index(mapName)]

    scroll = 0
    hovered = None
    hoveredNoise = None

    def reloadMenu(window):
        menu = pygame.Surface((uiSize * 28.25, uiSize * 30.375))
        menu.fill((25, 25, 25))
        for m in range(0, len(orderedMaps)):
            y = int(uiSize * 1.75 + uiSize * 2.5 * m + scroll * uiSize * 2.5)
            if 0 <= y and y <= menu.get_height():
                drawText(menu, orderedMaps[m], int(uiSize), int(uiSize * 1.5), int(y), 'midleft')
        window.blit(menu, (window.get_width() / 2 - 0.375 * uiSize - menu.get_width(), uiSize * 2.75))

    def reloadWindow():
        window = pygame.Surface((29.5 * uiSize * 2, 34 * uiSize))
        window.fill((50, 50, 50))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(0, 0, 29.5 * uiSize * 2, 2 * uiSize))
        drawText(window, 'Maps', uiSize, window.get_width() / 2, uiSize)
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize, 2.75 * uiSize, uiSize * 28.25, uiSize * 12))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize, 14.75 * uiSize, uiSize * 28.25, 2 * uiSize))
        drawText(window, mapName, uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2, 15.75 * uiSize)
        mapImage = map.copy()
        mapImage = pygame.transform.scale(map, (uiSize * 10 * mapImage.get_width() / mapImage.get_height(), uiSize * 10))
        window.blit(mapImage, (window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2 - mapImage.get_width() / 2, uiSize * 3.75))
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + 0.375 * uiSize, 17.5 * uiSize, uiSize * 28.25, uiSize * 12.75))
        infoX = window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2
        drawText(window, f"Start Date: {day} {getMonthName(month)}, {year}", uiSize, infoX, uiSize * 20, 'midleft')
        drawText(window, f"Countries: {len(countryList)}", uiSize, infoX, uiSize * 18.5, 'midleft')
        drawText(window, f"Factions: {factionList}".replace('[', '').replace(']', '').replace("'", '').replace('_', ' '), uiSize, infoX, uiSize * 21.5, 'midleft')
        drawText(window, f"Tags: {tag}", uiSize, infoX, uiSize * 23, 'midleft')
        drawText(window, f"Map Creator: {creator}", uiSize, infoX, uiSize * 24.5, 'midleft')
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + uiSize * 18.125 - uiSize * 8, 31 * uiSize, uiSize * 8, math.ceil(uiSize * 1.75)))
        drawText(window, 'Exit', uiSize, window.get_width() / 2 + uiSize * 18.125 - uiSize * 4, 31.825 * uiSize)
        reloadMenu(window)
        return window

    window = reloadWindow()
    runningThisMenu = True

    while runningThisMenu:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()

        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:
                    if hovered and hovered != mapName and type(hovered) == str:
                        clickedSound.play()
                        setupGame(hovered, 14)
                        menuBackground = MenuBackground()
                        tag = orderedTags[orderedMaps.index(hovered)]
                        creator = orderedCreators[orderedMaps.index(hovered)]
                        window = reloadWindow()
                        popupList = []
                if WIDTH / 2 - 28.625 * uiSize <= xMouse <= WIDTH / 2 - 28.625 * uiSize + 28.25 * uiSize:
                    if HEIGHT / 2 - 14.25 * uiSize <= yMouse <= HEIGHT / 2 - 14.25 * uiSize + 30.375 * uiSize:
                        if event.button == 4 and scroll < 0:
                            scroll += 1
                            reloadMenu(window)
                        elif event.button == 5 and scroll > -len(orderedMaps) + 1:
                            scroll -= 1
                            reloadMenu(window)

            if event.type == pygame.QUIT:
                sys.exit()

            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

        hovered = None
        menuBackground.update()
        screen.blit(window, (WIDTH / 2 - 29.5 * uiSize, HEIGHT / 2 - 17 * uiSize))

        x = WIDTH / 2 - 28.625 * uiSize + 0.75 * uiSize
        y = HEIGHT / 2 - 13.5 * uiSize
        for m in range(0, len(orderedMaps)):
            rowY = int(uiSize * 1.75 + uiSize * 2.5 * m + scroll * uiSize * 2.5)
            if 0 <= rowY <= uiSize * 30.375:
                cellY = y + uiSize * 2.5 * m + scroll * uiSize * 2.5
                if cellY <= yMouse <= cellY + uiSize * 2:
                    if x <= xMouse <= x + uiSize * 26.75:
                        color = (75, 75, 75)
                        if pressed1:
                            color = (150, 150, 150)
                        pygame.draw.rect(screen, color, pygame.Rect(x, cellY, uiSize * 26.75, uiSize * 2))
                        hovered = orderedMaps[m]
                        drawText(screen, orderedMaps[m], int(uiSize), int(x) + 0.75 * uiSize, int(cellY) + uiSize, 'midleft')

        x = uiSize * 18.125
        if WIDTH / 2 + x - 8 * uiSize <= xMouse <= WIDTH / 2 + x:
            if HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5 <= yMouse <= HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5 + uiSize * 1.75:
                hovered = 1
                if pressed1:
                    runningThisMenu = False
                    clickedSound.play()
                pygame.draw.rect(screen, (75, 75, 75), pygame.Rect(WIDTH / 2 + x - 8 * uiSize, HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5, uiSize * 8, uiSize * 1.75))
                drawText(screen, 'Exit', uiSize, WIDTH / 2 + x - 4 * uiSize, HEIGHT / 2 + 7.3 * uiSize + 7.5 * uiSize)

        if hoveredNoise != hovered and hovered != None:
            hoveredSound.play()
        hoveredNoise = hovered
        pygame.display.flip()

    startScreen()


def saveGameMenu():
    global mapName, runningMain, WIDTH, HEIGHT, pressed

    orderedMaps = []
    clicked = None
    mapImage = None

    for dir in os.listdir(savesDir):
        orderedMaps.append(dir)

    scroll = 0
    hovered = None
    hoveredNoise = None
    pressed = False

    def reloadMenu(window):
        menu = pygame.Surface((uiSize * 28.25, uiSize * 30.375))
        menu.fill((25, 25, 25))
        for m in range(0, len(orderedMaps)):
            y = int(uiSize * 1.75 + uiSize * 2.5 * m + scroll * uiSize * 2.5)
            if 0 <= y and y <= menu.get_height():
                drawText(menu, orderedMaps[m].replace('_', ' '), int(uiSize), int(uiSize * 1.5), int(uiSize * 1.75 + uiSize * 2.5 * m + scroll * uiSize * 2.5), 'midleft')
        window.blit(menu, (window.get_width() / 2 - 0.375 * uiSize - menu.get_width(), uiSize * 2.75))

    def reloadWindow():
        window = pygame.Surface((round(29.5 * uiSize * 2), round(34 * uiSize)))
        window.fill((50, 50, 50))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(0, 0, 29.5 * uiSize * 2, 2 * uiSize))
        drawText(window, 'Saves', uiSize, window.get_width() / 2, uiSize)
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize, 2.75 * uiSize, uiSize * 28.25, uiSize * 12))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize, 14.75 * uiSize, uiSize * 28.25, 2 * uiSize))
        if clicked == None:
            drawText(window, 'Select a Save', uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2, 15.75 * uiSize)
        else:
            drawText(window, clicked.replace('_', ' '), uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2, 15.75 * uiSize)
            mapImage = pygame.image.load(os.path.join(savesDir, clicked, 'map.png'))
            mapImage = pygame.transform.scale(mapImage, (uiSize * 10 * mapImage.get_width() / mapImage.get_height(), uiSize * 10))
            window.blit(mapImage, (window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2 - mapImage.get_width() / 2, uiSize * 3.75))
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + 0.375 * uiSize, 17.5 * uiSize, uiSize * 28.25, uiSize * 10.25 - uiSize * 2.5))
        if clicked != None:
            with open(os.path.join(savesDir, clicked, 'worldData'), 'rb') as file:
                variables = pickle.load(file)
            drawText(window, f"Country: {variables['controlledCountry']}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 18.5, 'midleft')
            drawText(window, f"Date: {variables['day']} {getMonthName(variables['month'])}, {variables['year']}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 20, 'midleft')
            with open(os.path.join(savesDir, clicked, 'countryData'), 'rb') as file:
                variables = pickle.load(file)
            drawText(window, f"Countries: {len(variables)}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 21.5, 'midleft')
            with open(os.path.join(savesDir, clicked, 'factionData'), 'rb') as file:
                variables = pickle.load(file)
            factions = [name for name in variables.keys()]
            drawText(window, f"Factions: {factions}".replace('[', '').replace(']', '').replace("'", '').replace('_', ' '), uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 23, 'midleft')
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + uiSize * 18.125 - uiSize * 8, 31 * uiSize - uiSize * 2.5 * 2, uiSize * 8, math.ceil(uiSize * 1.75)))
        drawText(window, 'Play', uiSize, window.get_width() / 2 + uiSize * 18.125 - uiSize * 4, 31.825 * uiSize - uiSize * 2.5 * 2)
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + uiSize * 18.125 - uiSize * 8, 31 * uiSize - uiSize * 2.5, uiSize * 8, math.ceil(uiSize * 1.75)))
        drawText(window, 'Delete', uiSize, window.get_width() / 2 + uiSize * 18.125 - uiSize * 4, 31.825 * uiSize - uiSize * 2.5)
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + uiSize * 18.125 - uiSize * 8, 31 * uiSize, uiSize * 8, math.ceil(uiSize * 1.75)))
        drawText(window, 'Back', uiSize, window.get_width() / 2 + uiSize * 18.125 - uiSize * 4, 31.825 * uiSize)
        reloadMenu(window)
        return window

    window = reloadWindow()
    runningThisMenu = True

    while runningThisMenu:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()
        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:
                    if hovered and hovered != mapName and type(hovered) == str:
                        clicked = hovered
                        window = reloadWindow()
                if WIDTH / 2 - 28.625 * uiSize <= xMouse <= WIDTH / 2 - 28.625 * uiSize + uiSize * 28.25:
                    if HEIGHT / 2 - 14.25 * uiSize <= yMouse <= HEIGHT / 2 - 14.25 * uiSize + uiSize * 30.375:
                        if event.button == 4 and scroll < 0:
                            scroll += 1
                            reloadMenu(window)
                        elif event.button == 5 and scroll > -len(orderedMaps) + 1:
                            scroll -= 1
                            reloadMenu(window)
            if event.type == pygame.QUIT:
                sys.exit()
            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

        hovered = None
        if not pressed1:
            pressed = False
        menuBackground.update()
        screen.blit(window, (WIDTH / 2 - 29.5 * uiSize, HEIGHT / 2 - 17 * uiSize))

        x = WIDTH / 2 - 28.625 * uiSize + 0.75 * uiSize
        y = HEIGHT / 2 - 13.5 * uiSize
        for m in range(0, len(orderedMaps)):
            if 0 <= int(uiSize * 1.75 + uiSize * 2.5 * m + scroll * uiSize * 2.5) <= uiSize * 30.375:
                if y + uiSize * 2.5 * m + scroll * uiSize * 2.5 <= yMouse <= y + uiSize * 2.5 * m + scroll * uiSize * 2.5 + uiSize * 2:
                    if x <= xMouse <= uiSize * 26.75 + x:
                        color = (75, 75, 75)
                        if pressed1:
                            color = (150, 150, 150)
                        pygame.draw.rect(screen, color, pygame.Rect(x, y + uiSize * 2.5 * m + scroll * uiSize * 2.5, uiSize * 26.75, uiSize * 2))
                        hovered = orderedMaps[m]
                        drawText(screen, orderedMaps[m], int(uiSize), int(x) + uiSize * 0.75, int(y + uiSize * 2.5 * m + scroll * uiSize * 2.5) + uiSize, 'midleft')

        x = uiSize * 18.125
        color = (75, 75, 75)
        if WIDTH / 2 + x - uiSize * 8 <= xMouse <= WIDTH / 2 + x:
            if HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5 <= yMouse <= HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5 + uiSize * 1.75:
                hovered = 3
                if pressed1:
                    clickedSound.play()
                    return
                pygame.draw.rect(screen, (75, 75, 75), pygame.Rect(WIDTH / 2 + x - uiSize * 8, HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5, uiSize * 8, math.ceil(uiSize * 1.75)))
                drawText(screen, 'Back', uiSize, WIDTH / 2 + x - uiSize * 4, HEIGHT / 2 + 7.3 * uiSize + uiSize * 7.5)

        x = uiSize * 18.125
        color = (75, 75, 75)
        if WIDTH / 2 + x - uiSize * 8 <= xMouse <= WIDTH / 2 + x:
            if HEIGHT / 2 + 6.5 * uiSize + uiSize * 2.5 <= yMouse <= HEIGHT / 2 + 6.5 * uiSize + uiSize * 2.5 + uiSize * 1.75:
                hovered = 1
                if pressed1:
                    color = (150, 150, 150)
                if pressed1 and clicked != None:
                    runningMain = False
                    clickedSound.play()
                    loadGame(clicked)
                    return
                if pressed1 and not pressed:
                    failedClickSound.play()
                    pressed = True
                pygame.draw.rect(screen, color, pygame.Rect(WIDTH / 2 + x - uiSize * 8, HEIGHT / 2 + 6.5 * uiSize + uiSize * 2.5, uiSize * 8, math.ceil(uiSize * 1.75)))
                drawText(screen, 'Play', uiSize, WIDTH / 2 + x - uiSize * 4, HEIGHT / 2 + 7.3 * uiSize + uiSize * 2.5)

        x = uiSize * 18.125
        color = (75, 75, 75)
        if WIDTH / 2 + x - uiSize * 8 <= xMouse <= WIDTH / 2 + x:
            if HEIGHT / 2 + 6.5 * uiSize + 2 * uiSize * 2.5 <= yMouse <= HEIGHT / 2 + 6.5 * uiSize + 2 * uiSize * 2.5 + uiSize * 1.75:
                hovered = 2
                if pressed1:
                    color = (150, 150, 150)
                if pressed1 and clicked != None:
                    clickedSound.play()
                    shutil.rmtree(os.path.join(savesDir, clicked))
                    orderedMaps.remove(clicked)
                    pressed = True
                    clicked = None
                    window = reloadWindow()
                if pressed1 and not pressed:
                    failedClickSound.play()
                    pressed = True
                pygame.draw.rect(screen, color, pygame.Rect(WIDTH / 2 + x - uiSize * 8, HEIGHT / 2 + 6.5 * uiSize + 2 * uiSize * 2.5, uiSize * 8, math.ceil(uiSize * 1.75)))
                drawText(screen, 'Delete', uiSize, WIDTH / 2 + x - uiSize * 4, HEIGHT / 2 + 7.3 * uiSize + uiSize * 5)

        if hoveredNoise != hovered and hovered != None:
            hoveredSound.play()
        hoveredNoise = hovered
        pygame.display.flip()


def countriesMenu():
    global selectedCountry, WIDTH, HEIGHT

    orderedCountries = sorted([i.replace('_', ' ') for i in countryList])
    loadedFlags = [pygame.image.load(os.path.join('flags', f"{i.replace(' ', '_').lower()}_flag.png")).convert() for i in orderedCountries]
    loadedFlags = [pygame.transform.scale(i, (int(i.get_width() / i.get_height() * uiSize * 1.5), uiSize * 1.5)) for i in loadedFlags]
    scroll = 0

    hovered = None
    hoveredNoise = None

    def reloadMenu(window):
        menu = pygame.Surface((uiSize * 28.25, uiSize * 30.375))
        menu.fill((25, 25, 25))
        for country in range(-scroll, min(-scroll + 12, len(countryList))):
            menu.blit(loadedFlags[country], (uiSize, uiSize + uiSize * 2.5 * country + scroll * 2.5 * uiSize))
            drawText(menu, orderedCountries[country], int(uiSize), int(uiSize * 2 + loadedFlags[country].get_width()), int(uiSize * 1.75 + uiSize * 2.5 * country + scroll * uiSize * 2.5), 'midleft')
        window.blit(menu, (window.get_width() / 2 - 0.375 * uiSize - menu.get_width(), uiSize * 2.75))

    def reloadWindow():
        window = pygame.Surface((round(29.5 * uiSize * 2), round(34 * uiSize)))
        window.fill((50, 50, 50))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(0, 0, 29.5 * uiSize * 2, 2 * uiSize))
        drawText(window, 'Maps', uiSize, window.get_width() / 2, uiSize)

        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize, 2.75 * uiSize, uiSize * 28.25, uiSize * 12))
        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize, 14.75 * uiSize, uiSize * 28.25, 2 * uiSize))
        pygame.draw.rect(window, (25, 25, 25), pygame.Rect(window.get_width() / 2 + 0.375 * uiSize, 17.5 * uiSize, uiSize * 28.25, uiSize * 12.75))

        if selectedCountry != None:
            drawText(window, f"{selectedCountry.replace('_', ' ')}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2, 15.75 * uiSize)

            flag = pygame.image.load(os.path.join('flags', f"{selectedCountry}_flag.png")).convert()
            flag = pygame.transform.scale(flag, (int(flag.get_width() / flag.get_height() * uiSize * 10), uiSize * 10))
            window.blit(flag, (window.get_width() / 2 + 29 * uiSize - 28.625 * uiSize + uiSize * 28.25 / 2 - flag.get_width() / 2, window.get_height() / 2 - 13.25 * uiSize))

            drawText(window, f"Ideology: {getIdeologyName(globals()[selectedCountry].ideology).capitalize()}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 20, 'midleft')
            drawText(window, f"Faction: {globals()[selectedCountry].faction}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 18.5, 'midleft')
            drawText(window, f"Military: {prefixNumber(globals()[selectedCountry].totalMilitary)} (Active), {prefixNumber(max(globals()[selectedCountry].manPower - globals()[selectedCountry].totalMilitary, 0))} (Reserve)", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 21.5, 'midleft')
            drawText(window, f"Regions: {len(globals()[selectedCountry].regions)}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 23, 'midleft')
            drawText(window, f"Factories: {globals()[selectedCountry].factories}", uiSize, window.get_width() / 2 + 29.0 * uiSize - 28.625 * uiSize + uiSize / 2, uiSize * 24.5, 'midleft')

        pygame.draw.rect(window, (0, 0, 0), pygame.Rect(window.get_width() / 2 + uiSize * 18.125 - uiSize * 8, 31 * uiSize, uiSize * 8, uiSize * 1.75))
        drawText(window, 'Exit', uiSize, window.get_width() / 2 + uiSize * 18.125 - uiSize * 4, 31.825 * uiSize)

        reloadMenu(window)
        return window

    window = reloadWindow()
    runningThisMenu = True

    while runningThisMenu:
        pressed1, pressed2, pressed3 = pygame.mouse.get_pressed()
        xMouse, yMouse = pygame.mouse.get_pos()
        clock.tick(FPS)

        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:
                    if type(hovered) == str:
                        clickedSound.play()
                        changeCountry(hovered)
                        selectedCountry = hovered
                        window = reloadWindow()
                if WIDTH / 2 - 28.625 * uiSize <= xMouse <= WIDTH / 2 - 28.625 * uiSize + uiSize * 28.25 and HEIGHT / 2 - 14.25 * uiSize <= yMouse <= HEIGHT / 2 - 14.25 * uiSize + uiSize * 30.375:
                    if event.button == 4 and scroll < 0:
                        scroll += 1
                        reloadMenu(window)
                    elif event.button == 5 and -scroll < len(countryList) - 1:
                        scroll -= 1
                        reloadMenu(window)
            if event.type == pygame.QUIT:
                sys.exit()
            if event.type == pygame.VIDEORESIZE:
                WIDTH, HEIGHT = pygame.display.get_surface().get_size()

        hovered = None
        menuBackground.update()
        screen.blit(window, (WIDTH / 2 - window.get_width() / 2, HEIGHT / 2 - window.get_height() / 2))

        xButtonSize = uiSize * 26.75
        yButtonSize = uiSize * 2
        i = 0
        for country in range(-scroll, min(-scroll + 12, len(countryList))):
            x = WIDTH / 2 - uiSize * 28.625 + uiSize * 0.75
            y = HEIGHT / 2 - uiSize * 14.25 + uiSize * 0.75
            if y + i * uiSize * 2.5 <= yMouse <= y + i * uiSize * 2.5 + yButtonSize and x <= xMouse <= x + xButtonSize:
                color = (75, 75, 75)
                if pressed1:
                    color = (150, 150, 150)
                hovered = orderedCountries[country].replace(' ', '_')
                pygame.draw.rect(screen, color, pygame.Rect(x, y + i * uiSize * 2.5, xButtonSize, yButtonSize))
                screen.blit(loadedFlags[country], (x + uiSize * 0.25, y + i * uiSize * 2.5 + uiSize * 0.25))
                drawText(screen, orderedCountries[country], int(uiSize), x + uiSize * 1.25 + loadedFlags[country].get_width(), y + i * uiSize * 2.5 + uiSize, 'midleft')
            i += 1

        x = uiSize * 18.125
        if WIDTH / 2 + x - uiSize * 8 <= xMouse <= WIDTH / 2 + x and HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5 <= yMouse <= HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5 + uiSize * 1.75:
            hovered = 1
            if pressed1:
                runningThisMenu = False
                clickedSound.play()
            pygame.draw.rect(screen, (75, 75, 75), pygame.Rect(WIDTH / 2 + x - uiSize * 8, HEIGHT / 2 + 6.5 * uiSize + 3 * uiSize * 2.5, uiSize * 8, uiSize * 1.75))
            drawText(screen, 'Exit', uiSize, WIDTH / 2 + x - uiSize * 4, HEIGHT / 2 + 7.25 * uiSize + 3 * uiSize * 2.5)

        if hoveredNoise != hovered and hovered != None:
            hoveredSound.play()
        hoveredNoise = hovered
        pygame.display.flip()

    startScreen()


def setupGame(name, eventFrequency=0):
    global mapName, ports, battleList, factionList, countryList
    global map, ideologyMap, factionMap, industryMap, modifiedIndustryMap, biomeMap, maps
    global camx, camy, zoom
    global startDate, hour, day, month, year
    global eventManager

    mapName = name
    ports = []
    battleList = []

    if factionList != []:
        for faction in [i for i in factionList]:
            globals()[faction].kill()

    if countryList != []:
        for country in [i for i in countryList]:
            globals()[country].kill(True)
            del globals()[country]

    dir = os.path.join(startsDir, name)

    ideologyMapExists = os.path.exists(f"{dir}\\ideologies.png")
    factionMapExists = os.path.exists(f"{dir}\\factions.png")

    map = pygame.image.load(os.path.join(dir, 'map.png')).convert()
    map = fixBorders(map)

    if not ideologyMapExists:
        ideologyMap = pygame.image.load(os.path.join('maps', 'blank.png')).convert()
    else:
        ideologyMap = pygame.image.load(os.path.join(dir, 'ideologies.png')).convert()

    if not factionMapExists:
        factionMap = pygame.image.load(os.path.join('maps', 'blank.png')).convert()
    else:
        factionMap = pygame.image.load(os.path.join(dir, 'factions.png')).convert()

    modifiedIndustryMap = industryMap.copy()
    industryMap = pygame.image.load(os.path.join('maps', 'industry.png')).convert()

    maps = [map, ideologyMap, factionMap, biomeMap]

    camx = -map.get_width() / 2
    camy = -map.get_height() / 2
    zoom = 1

    with open(os.path.join(dir, 'startDate.txt')) as date:
        startDate = [int(x) for x in date.read().strip('[]').split(', ')]

    hour = startDate[0]
    day = startDate[1]
    month = startDate[2]
    year = startDate[3]

    setupCountries(ideologyMapExists or factionMapExists)

    with open(os.path.join(dir, 'factions.txt')) as factions:
        for faction in factions.readlines():
            attributes = faction.replace('\n', '')
            attributes = [x for x in attributes.split(': ')]
            name = str(attributes[0]).strip("'")
            countriesInFaction = [str(x).strip("'") for x in attributes[1].strip('[]').split(', ')]
            globals()[name] = Faction(name, countriesInFaction)

    eventManager = EventManager(eventFrequency)

    global political_event_manager, ai_controllers, puppet_states, trade_contracts
    political_event_manager = PoliticalEventManager()
    ai_controllers = {}
    puppet_states = []
    trade_contracts = []

    for cname in countryList:
        c = globals().get(cname)
        if c:
            gs.register_country(cname, c)
            c.resourceManager.set_starting_stockpile(len(c.regions))
            c.buildingManager.set_starting_buildings(c.regions, c.factories)
            c.resourceManager.calculate_production(c, regions)
            civ_start = c.buildingManager.get_building_count('civilian_factory')
            c.money = 5000 * (civ_start + 1) * 30

    if not ideologyMapExists:
        pygame.image.save(ideologyMap, f"{dir}\\ideologies.png")
    if not factionMapExists:
        pygame.image.save(factionMap, f"{dir}\\factions.png")


def loadGame(name="world"):
    global countryList, factionList, popupList, battleList
    global clicked, sideBarScroll, sideBarAnimation, pressed, selectedDivisions
    global map, factionMap, ideologyMap, industryMap, biomeMap, maps, modifiedIndustryMap
    global controlledCountry, controlledCountryFlag, ports
    global eventManager, startDate, hour, day, month, year, currentMusic
    global camx, camy, zoom, speed, showDivisions, showCities, currentMap

    name = name.replace(' ', '_')
    saveDir = os.path.join(savesDir, name)

    countryList = []
    factionList = []
    popupList = []
    battleList = []
    clicked = None
    sideBarScroll = 0
    sideBarAnimation = 0
    pressed = True
    selectedDivisions = []

    map = pygame.image.load(os.path.join(saveDir, 'map.png'))
    factionMap = pygame.image.load(os.path.join(saveDir, 'factions.png'))
    ideologyMap = pygame.image.load(os.path.join(saveDir, 'ideologies.png'))
    industryMap = pygame.image.load(os.path.join(saveDir, 'industry.png'))

    maps = [map, ideologyMap, factionMap, biomeMap]

    for region in range(1, 3716):
        x, y = regions.getLocation(region)
        color = map.get_at((round(x), round(y)))
        country = countries.colorToCountry(color[:3])
        regions.updateOwner(region, country)

    with open(os.path.join(saveDir, 'worldData'), 'rb') as file:
        variables = pickle.load(file)

    controlledCountry = variables['controlledCountry']
    ports = variables['ports']
    canals = variables['canals']
    eventManager.globalTension = variables['globalTension']
    startDate = variables['startDate']
    hour = variables['hour']
    day = variables['day']
    month = variables['month']
    year = variables['year']
    currentMusic = variables['currentMusic']

    if currentMusic == 'game':
        pygame.mixer.music.load(os.path.join(musicDir, 'gameMusic.mp3'))
        pygame.mixer.music.play(-1)
        pygame.mixer.music.set_volume(musicVolume)
    elif currentMusic == 'war':
        pygame.mixer.music.load(os.path.join(musicDir, 'warMusic.mp3'))
        pygame.mixer.music.play(-1)
        pygame.mixer.music.set_volume(musicVolume)

    if controlledCountry != None:
        controlledCountryFlag = pygame.image.load(
            os.path.join('flags', f'{controlledCountry.lower()}_flag.png')
        ).convert()

    with open(os.path.join(saveDir, 'countryData'), 'rb') as file:
        variables = pickle.load(file)
    for country in variables:
        countryList.append(country)
        globals()[country] = variables[country]
        gs.register_country(country, variables[country])
    for country in countryList:
        for div in globals()[country].divisions:
            div.reloadIcon()
            div.fighting = False

    with open(os.path.join(saveDir, 'uiData'), 'rb') as file:
        variables = pickle.load(file)
    camx = variables['camx']
    camy = variables['camy']
    zoom = variables['zoom']
    speed = variables['speed']
    showDivisions = variables['showDivisions']
    showCities = variables['showCities']
    currentMap = variables['currentMap']

    with open(os.path.join(saveDir, 'factionData'), 'rb') as file:
        variables = pickle.load(file)
    for faction in variables:
        globals()[faction] = variables[faction]
        factionList.append(faction)

    reloadIndustryMap()


def saveGame(name="world"):
    name = name.replace(' ', '_')
    saveDir = os.path.join(savesDir, name)

    if not os.path.exists(saveDir):
        os.mkdir(saveDir)
    saveDir = os.path.join(savesDir, name)

    pygame.image.save(map, os.path.join(saveDir, 'map.png'))
    pygame.image.save(factionMap, os.path.join(saveDir, 'factions.png'))
    pygame.image.save(ideologyMap, os.path.join(saveDir, 'ideologies.png'))
    pygame.image.save(industryMap, os.path.join(saveDir, 'industry.png'))

    with open(os.path.join(saveDir, 'worldData'), 'wb') as file:
        variables = {
            'controlledCountry': controlledCountry,
            'globalTension': eventManager.globalTension,
            'ports': ports,
            'canals': canals,
            'startDate': startDate,
            'hour': hour,
            'day': day,
            'month': month,
            'year': year,
            'currentMusic': currentMusic,
        }
        pickle.dump(variables, file)

    with open(os.path.join(saveDir, 'countryData'), 'wb') as file:
        variables = {}
        for c in countryList:
            for div in globals()[c].divisions:
                div.image = None
            variables[c] = globals()[c]
        pickle.dump(variables, file)

    for country in countryList:
        for div in globals()[country].divisions:
            div.reloadIcon()

    with open(os.path.join(saveDir, 'uiData'), 'wb') as file:
        variables = {
            'camx': camx,
            'camy': camy,
            'zoom': zoom,
            'speed': speed,
            'currentMap': currentMap,
            'showDivisions': showDivisions,
            'showCities': showCities,
        }
        pickle.dump(variables, file)

    with open(os.path.join(saveDir, 'factionData'), 'wb') as file:
        variables = {}
        for faction in factionList:
            variables[faction] = globals()[faction]
        pickle.dump(variables, file)


def setup(name="Modern Day", time=14):
    global currentMap, clicked, popupList, battleList, menuBackground

    currentMap = 1
    clicked = None
    popupList = []
    battleList = []
    setupGame(name, time)
    menuBackground = MenuBackground()


def loadImages(file, size, colorkey=None):
    global cityImage, regionImage

    array = os.listdir(file)

    for i in range(len(os.listdir(file))):
        image = pygame.image.load(os.path.join(file, array[i])).convert()
        imageRect = image.get_rect()

        height = size
        width = int(size * imageRect.width / imageRect.height)

        name = array[i].replace('.png', '')

        globals()[name] = pygame.image.load(os.path.join(file, array[i])).convert_alpha()
        globals()[name] = pygame.transform.scale(globals()[name], (width, height))

        if colorkey != None:
            globals()[name].set_colorkey((0, 0, 0))

    cityImage = pygame.Surface((uiSize * 2, uiSize * 2))
    cityImage.fill((255, 0, 0))
    pygame.draw.circle(cityImage, WHITE, (uiSize, uiSize), uiSize // 2)
    pygame.draw.circle(cityImage, BLACK, (uiSize, uiSize), uiSize // 2, uiSize // 8)
    cityImage.set_colorkey((255, 0, 0))

    regionImage = pygame.Surface((uiSize * 2, uiSize * 2))
    regionImage.fill((255, 0, 0))
    pygame.draw.circle(regionImage, (0, 255, 0), (uiSize, uiSize), uiSize // 2)
    pygame.draw.circle(regionImage, BLACK, (uiSize, uiSize), uiSize // 2, uiSize // 8)
    regionImage.set_colorkey((255, 0, 0))


def loadSounds(file, volume=1):
    array = os.listdir(file)

    for i in range(len(os.listdir(file))):
        if '.wav' in array[i]:
            globals()[array[i].replace('.wav', '')] = pygame.mixer.Sound(
                os.path.join(file, array[i])
            )
            globals()[array[i].replace('.wav', '')].set_volume(volume)

    clickedSound.set_volume(0.5 * volume)
    hoveredSound.set_volume(0.25 * volume)


# ── Game initialization ──────────────────────────────────────────────────────

pygame.init()
pygame.mixer.init()
pygame.display.set_caption("Spirits of Steel: Community Edition")
pygame.display.set_icon(pygame.image.load("icon.png"))
screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.RESIZABLE | pygame.DOUBLEBUF)

drawText(screen, "GavGrub Presents", uiSize, WIDTH / 2, HEIGHT / 2)
pygame.display.flip()

clock = pygame.time.Clock()

countries = Countries()
regions = Regions()
userInterface = UserInterface()
treatyUserInterface = TreatyUserInterface()
controller = Controller()
treatyController = TreatyController()

biomeMap = pygame.image.load(os.path.join("maps", "biomes.png")).convert()
regionsMap = pygame.image.load(os.path.join("maps", "regions.png")).convert()
worldRegionsMap = pygame.image.load(os.path.join("maps", "worldRegions.png")).convert()
industryMap = pygame.image.load(os.path.join("maps", "industry.png")).convert()
cultureMap = pygame.image.load(os.path.join("maps", "cultures.png")).convert()
mapWidth = int(biomeMap.get_width())

cities = regions.getCities()

loadImages(flagsDir, uiSize)
loadImages(iconsDir, uiSize * 1.5, (0, 0, 0))

loadSounds(soundDir, soundVolume)

setup("Modern Day", 14)

mainMenu()

reloadIndustryMap()
generateResourceMap()

# ── Main game loop ───────────────────────────────────────────────────────────

while running:
    clock.tick(FPS)
    tick += 60 / FPS

    eventManager.update()

    if tick > 3:
        updateTime()

        for faction in factionList:
            globals()[faction].update()

        for country in countryList:
            globals()[country].update()

        for tc in trade_contracts:
            if tc.active:
                exp = globals().get(tc.exporter)
                imp = globals().get(tc.importer)
                if exp and imp:
                    tc.tick_with_objects(exp, imp, speed)
                else:
                    tc.active = False
        trade_contracts[:] = [tc for tc in trade_contracts if tc.active]

        for ps in puppet_states:
            if ps.active:
                ps.tick(speed)
                puppet_obj = globals().get(ps.puppet)
                overlord_obj = globals().get(ps.overlord)
                if puppet_obj and overlord_obj:
                    tribute_rate = ps.get_resource_tribute()
                    if tribute_rate > 0 and hasattr(puppet_obj, 'resourceManager') and hasattr(overlord_obj, 'resourceManager'):
                        rate = speed / 5 / 12
                        for rname in puppet_obj.resourceManager.production:
                            transfer = puppet_obj.resourceManager.production[rname] * tribute_rate * rate
                            if transfer > 0:
                                puppet_obj.resourceManager.stockpile[rname] = max(0, puppet_obj.resourceManager.stockpile[rname] - transfer)
                                overlord_obj.resourceManager.stockpile[rname] = min(overlord_obj.resourceManager.max_stockpile, overlord_obj.resourceManager.stockpile[rname] + transfer)
                if ps.check_revolt():
                    if puppet_obj and overlord_obj:
                        puppet_obj.puppetTo = None
                        puppet_obj.declareWar(ps.overlord)
                        ps.active = False
        puppet_states[:] = [ps for ps in puppet_states if ps.active]

        if political_event_manager and speed > 0:
            for country_name in countryList:
                c_obj = globals().get(country_name)
                if c_obj:
                    evt = political_event_manager.check_events(c_obj, day)
                    if evt and country_name == controlledCountry:
                        popupList.append(Popup(
                            evt['title'],
                            [evt['text']],
                            buttons=[['Okay', '', 0, 5.25]],
                            xSize=22, ySize=5,
                            flag1=country_name, flag2=country_name
                        ))
                        if evt.get('type') == 'leader_death' and evt.get('new_leader'):
                            c_obj.leader = evt['new_leader']
                            c_obj.stability = max(0, getattr(c_obj, 'stability', 50) - 10)
                        elif evt.get('type') == 'scandal':
                            c_obj.stability = max(0, getattr(c_obj, 'stability', 50) + evt.get('stability_change', 0))
                            c_obj.politicalPower = max(0, getattr(c_obj, 'politicalPower', 0) + evt.get('pp_change', 0))
                        elif evt.get('type') == 'economic_shock':
                            loss_pct = evt.get('money_loss_pct', 10)
                            c_obj.money = max(0, c_obj.money * (1 - loss_pct / 100))
                            c_obj.stability = max(0, getattr(c_obj, 'stability', 50) + evt.get('stability_change', 0))
                        elif evt.get('type') == 'military_incident':
                            c_obj.stability = max(0, getattr(c_obj, 'stability', 50) + evt.get('stability_change', 0))
                            if evt.get('manpower_change'):
                                c_obj.manPower = max(0, getattr(c_obj, 'manPower', 0) + evt['manpower_change'])
                            if evt.get('pp_change'):
                                c_obj.politicalPower = max(0, getattr(c_obj, 'politicalPower', 0) + evt['pp_change'])

        for country_name in countryList:
            if country_name == controlledCountry:
                continue
            if country_name not in ai_controllers:
                ai_controllers[country_name] = AIController(country_name)
            ai_ctrl = ai_controllers[country_name]
            c_obj = globals().get(country_name)
            if c_obj and speed > 0 and tick % 180 == 0:
                build = ai_ctrl.decide_build(c_obj, c_obj.buildingManager)
                if build:
                    c_obj.buildingManager.start_construction(build[1], build[0], c_obj)
                focus = ai_ctrl.decide_focus(c_obj, c_obj.focusTreeEngine)
                if focus:
                    c_obj.focusTreeEngine.start_focus(focus, c_obj)
                    if focus in c_obj.decisionTree:
                        c_obj.decisionTree[focus][9] = True

        for popup in reversed(popupList):
            popup.update()

        for battle in battleList:
            battle.update()

    screen.fill(BLACK)

    if showResources and resourceMap is not None:
        mapDrawer.draw(screen, resourceMap, camx, camy, zoom, len(maps) + 1)

        key_x = uiSize
        key_y = uiSize * 3
        key_w = uiSize * 10
        row_h = int(uiSize * 1.4)
        key_h = row_h * (len(RESOURCE_COLORS) + 1) + uiSize
        key_bg = pygame.Surface((key_w, key_h), pygame.SRCALPHA)
        key_bg.fill((0, 0, 0, 180))
        screen.blit(key_bg, (key_x, key_y))
        drawText(screen, "Resources", int(uiSize * 0.9), key_x + key_w // 2, key_y + int(uiSize * 0.5), 'center', (255, 255, 255))
        ky = key_y + row_h + int(uiSize * 0.3)
        for rname, rcolor in RESOURCE_COLORS.items():
            pygame.draw.rect(screen, rcolor, pygame.Rect(key_x + uiSize // 2, ky, int(uiSize * 1.2), int(uiSize * 0.9)))
            pygame.draw.rect(screen, (200, 200, 200), pygame.Rect(key_x + uiSize // 2, ky, int(uiSize * 1.2), int(uiSize * 0.9)), 1)
            drawText(screen, rname.capitalize(), int(uiSize * 0.8), key_x + uiSize * 2, ky + int(uiSize * 0.45), 'midleft', (230, 230, 230))
            ky += row_h
    elif openedTab == "industry" and selected != None and clicked == controlledCountry:
        mapDrawer.draw(screen, modifiedIndustryMap, camx, camy, zoom, len(maps))
    else:
        mapDrawer.draw(screen, maps[currentMap - 1], camx, camy, zoom, currentMap - 1)

    userInterface.draw()

    draw_toasts()

    pygame.display.flip()

    controller.input()

pygame.quit()
