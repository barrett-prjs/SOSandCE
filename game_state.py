import os
import sys
import pickle

if getattr(sys, "frozen", False):
    APP_PATH = os.path.dirname(sys.executable)
else:
    APP_PATH = os.path.dirname(os.path.abspath(__file__))

FLAGS_DIR = os.path.join(APP_PATH, "flags")
ICONS_DIR = os.path.join(APP_PATH, "icons")
IMG_DIR = os.path.join(APP_PATH, "icons")
BACKGROUNDS_DIR = os.path.join(APP_PATH, "backgrounds")
SOUND_DIR = os.path.join(APP_PATH, "snd")
STARTS_DIR = os.path.join(APP_PATH, "starts")
MUSIC_DIR = os.path.join(APP_PATH, "music")
SAVES_DIR = os.path.join(APP_PATH, "saves")
SCREENSHOTS_DIR = os.path.join(APP_PATH, "screenshots")

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

sideBarSize = 0.2
holdingSideBar = False
sideBarScroll = 0
sideBarAnimation = 0
controlledCountryFlag = None

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

screen = None
clock = None
map = None
factionMap = None
ideologyMap = None
industryMap = None
biomeMap = None
modifiedIndustryMap = None
maps = []
eventManager = None
userInterface = None
controller = None
mapDrawer = None
menuBackground = None
flagImage = None

puppet_states = []
trade_contracts = []
peace_conferences = []

_country_registry = {}

def register_country(name, obj):
    _country_registry[name] = obj

def get_country(name):
    return _country_registry.get(name)

def get_all_countries():
    return _country_registry
