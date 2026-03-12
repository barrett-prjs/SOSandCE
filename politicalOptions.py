import math
from helperFunctions import *
from countryData import *

countries = Countries()


def createDecisionTree(country):

    x = 1600
    y = 0

    politicalTree = {

        "Political Effort": [x + 625, 0, 50, [], 'True', ['self.politicalPower += 50'], 30, 'Gain +50 political power.', 'No additional requirements.', False],

        "Ideology Focus": [x + 325, 1, 50, ['Political Effort'], 'True', ['self.politicalMultiplier += 0.1'], 30, 'Gain 10% political power gain.', 'No additional requirements.', False],

        "Workers Rights": [x, 2, 50, ['Ideology Focus'], 'True', ['self.ideology[1] -= 0.25'], 30, 'Gain 25 authoritarian.', 'No additional requirements.', False],
        "Class Struggle": [x, 3, 50, ['Workers Rights'], 'True', ['self.ideology[0] -= 0.25'], 30, 'Gain 25 left.', 'No additional requirements.', False],
        "Socialism": [x, 4, 50, ['Class Struggle'], 'True', ['self.ideology[1] -= 0.25', 'self.ideology[1] -= 0.25'], 30, 'Gain 25 communist.', 'No additional requirements.', False],
        "Proletariat Dictatorship": [x, 5, 50, ['Socialism'], 'True', ['self.ideology[1] -= 0.25', 'self.ideology[1] -= 0.25'], 30, 'Gain 25 communist.', 'No additional requirements.', False],

        "Progressivism": [x + 200, 2, 50, ['Ideology Focus'], 'True', ['self.ideology[0] -= 0.25'], 30, 'Gain 25 left.', 'No additional requirements.', False],
        "Life, Liberty, Property": [x + 200, 3, 50, ['Progressivism'], 'True', ['self.ideology[1] += 0.25'], 30, 'Gain 25 libertarian.', 'No additional requirements.', False],
        "Suffrage For All": [x + 200, 4, 50, ['Life, Liberty, Property'], 'True', ['self.ideology[0] -= 0.25', 'self.ideology[1] += 0.25'], 30, 'Gain 25 liberal.', 'No additional requirements.', False],
        "Enshrine Democracy": [x + 200, 5, 50, ['Suffrage For All'], 'True', ['self.ideology[0] -= 0.25', 'self.ideology[1] += 0.25'], 30, 'Gain 25 liberal.', 'No additional requirements.', False],

        "Traditionalism": [x + 450, 2, 50, ['Ideology Focus'], 'True', ['self.ideology[0] += 0.25'], 30, 'Gain 25 right.', 'No additional requirements.', False],
        "Under God": [x + 450, 3, 50, ['Traditionalism'], 'True', ['self.ideology[1] += 0.25'], 30, 'Gain 25 libertarian.', 'No additional requirements.', False],
        "The Divine Right": [x + 450, 4, 50, ['Under God'], 'True', ['self.ideology[0] += 0.25', 'self.ideology[1] += 0.25'], 30, 'Gain 25 monarchist.', 'No additional requirements.', False],
        "Restore the Throne": [x + 450, 5, 50, ['The Divine Right'], 'True', ['self.ideology[0] += 0.25', 'self.ideology[1] += 0.25'], 30, 'Gain 25 monarchist.', 'No additional requirements.', False],

        "Patriotism": [x + 650, 2, 50, ['Ideology Focus'], 'True', ['self.ideology[1] -= 0.25'], 30, 'Gain 25 authoritarian.', 'No additional requirements.', False],
        "National Unity": [x + 650, 3, 50, ['Patriotism'], 'True', ['self.ideology[0] += 0.25'], 30, 'Gain 25 right.', 'No additional requirements.', False],
        "Us Above All": [x + 650, 4, 50, ['National Unity'], 'True', ['self.ideology[1] -= 0.25', 'self.ideology[0] += 0.25'], 30, 'Gain 25 nationalist.', 'No additional requirements.', False],

        "Fascism": [x + 650, 5, 50, ['Us Above All'], 'True', ['self.ideology[1] -= 0.25', 'self.ideology[0] += 0.25'], 30, 'Gain 25 nationalist.', 'No additional requirements.', False],

        "End The Status Quo": [x + 325, 6, 10, ['Ideology Focus'], 'True', ['self.baseStability -= 5'], 15, 'Loose 5 stability.', 'No additional requirements.', False],

        "Policy Focus": [x + 1050, 1, 50, ['Political Effort'], 'True', ['self.politicalMultiplier += 0.1'], 30, 'Gain 10% political power gain.', 'No additional requirements.', False],

        "Reform I": [x + 850, 2, 50, ['Policy Focus'], 'True', ['self.baseStability += 5'], 30, 'Gain 5 stability.', 'No additional requirements.', False],
        "Reform II": [x + 850, 3, 50, ['Reform I'], 'True', ['self.baseStability += 10'], 30, 'Gain 10 stability.', 'No additional requirements.', False],
        "Reform III": [x + 850, 4, 50, ['Reform II'], 'True', ['self.baseStability += 15'], 30, 'Gain 15 stability.', 'No additional requirements.', False],

        "Geopolitics": [x + 1150, 2, 50, ['Policy Focus'], 'True', ['self.politicalMultiplier += 0.1'], 30, 'Gain 10% political power gain.', 'No additional requirements.', False],

        "The New Order": [x + 1050, 3, 100, ['Geopolitics'], 'self.faction == None', ['self.canMakeFaction = True'], 60, 'Can create a faction.', 'Not in a faction.', False],
        "Territorial Expansion": [x + 1050, 4, 100, ['The New Order'], 'not self.faction == None and self.factionLeader', ['self.expandedInvitations = True'], 60, 'Can expand faction across borders.', 'Leader of a faction.', False],

        "Gain Autonomy": [x + 1250, 3, 100, ['Geopolitics'], 'not self.puppetTo == None', ['self.puppetTo = None'], 60, 'Gain independence.', 'Puppet to a country.', False],

        "Industrial Effort": [200, y, 50, [], 'True', ['self.money += 10_000_000'], 30, 'Gain $10m.', 'No additional requirements.', False],

        "Infrastructure I": [0, y + 1, 50, ['Industrial Effort'], 'True', ['self.addFactory()'], 30, 'Creates a factory.', 'No additional requirements.', False],
        "Infrastructure II": [0, y + 2, 50, ['Infrastructure I'], 'True', ['self.addFactory()', 'self.addFactory()'], 30, 'Creates 2 factories.', 'No additional requirements.', False],
        "Infrastructure III": [0, y + 3, 50, ['Infrastructure II'], 'True', ['self.addFactory()', 'self.addFactory()', 'self.addFactory()'], 30, 'Creates 3 factories.', 'No additional requirements.', False],

        "Investment": [300, y + 1, 50, ['Industrial Effort'], 'True', ['self.money += 20_000_000'], 30, 'Gain $20m.', 'No additional requirements.', False],

        "Technology I": [200, y + 2, 50, ['Investment'], 'True', ['self.moneyMultiplier += 0.1'], 60, 'Gain 10% industry production.', 'No additional requirements.', False],
        "Technology II": [200, y + 3, 50, ['Technology I'], 'True', ['self.moneyMultiplier += 0.1'], 60, 'Gain 10% industry production.', 'No additional requirements.', False],

        "Construction I": [400, y + 2, 50, ['Investment'], 'True', ['self.buildSpeed -= 0.1'], 60, 'Gain 10% build speed.', 'No additional requirements.', False],
        "Construction II": [400, y + 3, 50, ['Construction I'], 'True', ['self.buildSpeed -= 0.1'], 60, 'Gain 10% build speed.', 'No additional requirements.', False],

        "Military Effort": [1000, y, 50, [], 'True', ['self.money += 10_000_000'], 60, 'Gain $10m.', 'No additional requirements.', False],

        "Modernization": [800, y + 1, 50, ['Military Effort'], 'True', ['self.money += 10_000_000'], 60, 'Gain $10m.', 'No additional requirements.', False],

        "Defense I": [600, y + 2, 50, ['Modernization'], 'True', ['self.defenseMultiplier += 0.1'], 60, 'Gain 10% division defense.', 'No additional requirements.', False],
        "Defense II": [600, y + 3, 50, ['Defense I'], 'True', ['self.defenseMultiplier += 0.1'], 60, 'Gain 10% division defense.', 'No additional requirements.', False],

        "Offense I": [800, y + 2, 50, ['Modernization'], 'True', ['self.attackMultiplier += 0.1'], 60, 'Gain 10% division attack.', 'No additional requirements.', False],
        "Offense II": [800, y + 3, 50, ['Offense I'], 'True', ['self.attackMultiplier += 0.1'], 60, 'Gain 10% division attack.', 'No additional requirements.', False],

        "Transportation I": [1000, y + 2, 50, ['Modernization'], 'True', ['self.transportMultiplier += 0.1'], 60, 'Gain 10% division speed.', 'No additional requirements.', False],
        "Transportation II": [1000, y + 3, 50, ['Transportation I'], 'True', ['self.transportMultiplier += 0.1'], 60, 'Gain 10% division speed.', 'No additional requirements.', False],

        "Defense of the Nation": [1300, y + 1, 50, ['Military Effort'], 'True', ['self.money += 10_000_000'], 60, 'Gain $10m.', 'No additional requirements.', False],

        "Militia I": [1200, y + 2, 50, ['Defense of the Nation'], 'True', ['self.training.append([1, 0])'], 60, 'Train a division.', 'No additional requirements.', False],
        "Militia II": [1200, y + 3, 50, ['Militia I'], 'True', ['self.training.append([2, 0])'], 60, 'Train 2 divisions.', 'No additional requirements.', False],
        "Militia III": [1200, y + 4, 50, ['Militia II'], 'True', ['self.training.append([3, 0])'], 60, 'Train 3 divisions.', 'No additional requirements.', False],

        "Militarism": [1400, y + 2, 100, ['Defense of the Nation'], 'not self.militarySize == 4', ['self.militarySize += 1'], 60, 'Gain 1 military size.', 'Less than 1% military size.', False],
        "Conscription": [1400, y + 3, 100, ['Militarism'], 'not self.militarySize == 4', ['self.militarySize += 1'], 60, 'Gain 1 military size.', 'Less than 1% military size.', False],
    }

    if countries.getCountryType(countries.getCulture(country), 'communist') != None and countries.getIdeology(country) != 'communist':
        politicalTree['Communism'] = [x + 25, 7, 50, ['End The Status Quo'], 'not self.hasChangedCountry', ["self.revolution('communist')"], 30, 'Change flag and color of country.', 'Has not changed country flag and color previously.', False]

    if countries.getCountryType(countries.getCulture(country), 'liberal') != None and countries.getIdeology(country) != 'liberal':
        politicalTree['Liberalism'] = [x + 225, 7, 50, ['End The Status Quo'], 'not self.hasChangedCountry', ["self.revolution('liberal')"], 30, 'Change flag and color of country.', 'Has not changed country flag and color previously.', False]

    if countries.getCountryType(countries.getCulture(country), 'monarchist') != None and countries.getIdeology(country) != 'monarchist':
        politicalTree['Monarchism'] = [x + 425, 7, 50, ['End The Status Quo'], 'not self.hasChangedCountry', ["self.revolution('monarchist')"], 30, 'Change flag and color of country.', 'Has not changed country flag and color previously.', False]

    if countries.getCountryType(countries.getCulture(country), 'nationalist') != None and countries.getIdeology(country) != 'nationalist':
        politicalTree['Nationalism'] = [x + 625, 7, 50, ['End The Status Quo'], 'not self.hasChangedCountry', ["self.revolution('nationalist')"], 30, 'Change flag and color of country.', 'Has not changed country flag and color previously.', False]

    return politicalTree


def getOptions(countries, countryList, country, canals=[]):
    options = {}

    cultures = set()
    for c in countryList:
        cultures.add(countries.getCulture(c))

    borderingCultures = set()
    for c in country.bordering:
        borderingCultures.add(countries.getCulture(c))

    options['General'] = []

    if country.stability < 60:
        options['General'].append(('Improve Policy', {'politicalPower': 25}, [f"globals()['{country.name}'].baseStability += 2"]))

    if country.faction != None:
        options['General'].append(('Leave Faction', {'politicalPower': 50}, [f"globals()['{country.faction}'].removeCountry('{country.name}')"]))
    elif country.canMakeFaction:
        options['General'].append(('Create Faction', {'politicalPower': 100}, [f"spawnFaction(['{country.name}'])"]))

    if 2567 in country.regions and 2567 in canals:
        options['General'].append(('Blow the Panama Canal', {'politicalPower': 50}, [f"destroyCanal([2567], 'Panama Canal', '{country.name}')"]))
    if 418 in country.regions and 418 in canals:
        options['General'].append(('Dam the Danish Straits', {'politicalPower': 75}, [f"destroyCanal([418], 'Danish Strait', '{country.name}')"]))
    if (1802 in country.regions and 1802 in canals) or (1868 in country.regions and 1868 in canals):
        options['General'].append(('Blow the Suez Canal', {'politicalPower': 50}, [f"destroyCanal([1802, 1868], 'Suez Canal', '{country.name}')"]))
    if (1244 in country.regions and 1244 in canals) or (1212 in country.regions and 1212 in canals):
        options['General'].append(('Dam the Bosphorus', {'politicalPower': 75}, [f"destroyCanal([1244, 1212], 'Bosphorus', '{country.name}')"]))
    if (1485 in country.regions and 1485 in canals) or (1502 in country.regions and 1502 in canals):
        options['General'].append(('Dam Gibraltar', {'politicalPower': 75}, [f"destroyCanal([1485, 1502], 'Gibraltar', '{country.name}')"]))

    match country.culture:

        case 'Russian':
            options['The Motherland'] = []

            if 'Tuvan' in borderingCultures:
                tuva = getCurrent('Tuvan', countryList)

                options['The Motherland'].append(('Annex Tuva', {'politicalPower': 75}, [f"globals()['{country.name}'].annexCountry('Tuva', '{tuva}')"]))

            if country.ideologyName == 'nationalist':
                if country.atWarWith == []:

                    if 'Belarusian' in borderingCultures:
                        belarus = getCurrent('Belarusian', countryList)

                        options['The Motherland'].append(('Union State', {'politicalPower': 100}, [f"globals()['{country.name}'].annexCountry('Belarusian', '{belarus}')"]))

                    if 944 in country.regions and not all(region in country.regions for region in (943, 967, 928)):
                        options['The Motherland'].append(('Annex Crimea', {'politicalPower': 50}, [f"globals()['{country.name}'].addRegions((943, 967, 928))"]))

            elif country.ideologyName == 'communist':

                if any(region in country.regions for region in (63, 464, 465, 483, 492, 503, 510, 511, 523, 524, 570, 576, 585, 586, 599, 611, 612, 621, 622, 623, 633, 640, 648, 656, 666, 690, 691, 699, 700, 701, 719, 720, 721, 722, 758, 765, 775)) and 'German_Republic' not in countryList:
                    options['The Motherland'].append(('Form East Germany', {'politicalPower': 100}, [f"independence('German_Republic'), '{country.name}'"]))

                if country.atWarWith == []:

                    if 'Estonian' in borderingCultures:
                        estonia = getCurrent('Estonian', countryList)
                        options['The Motherland'].append(('Annex Estonia', {'politicalPower': 50}, [f"globals()['{country.name}'].annexCountry('Estonian', '{estonia}')"]))

                    if 'Latvian' in borderingCultures:
                        latvia = getCurrent('Latvian', countryList)
                        options['The Motherland'].append(('Annex Latvia', {'politicalPower': 50}, [f"globals()['{country.name}'].annexCountry('Latvian', '{latvia}')"]))

                    if 'Lithuanian' in borderingCultures:
                        lithuania = getCurrent('Lithuanian', countryList)
                        options['The Motherland'].append(('Annex Lithuania', {'politicalPower': 50}, [f"globals()['{country.name}'].annexCountry('Lithuanian', '{lithuania}')"]))

            if any(region in country.regions for region in (58, 110, 114, 121, 122, 124, 125, 163, 173, 196, 215, 233, 252, 255, 262, 263, 269, 275, 283, 288, 294, 302, 303, 309, 320, 323, 324, 329, 350, 351, 352, 371, 372, 377, 378, 384, 385, 394, 398, 399, 400, 401, 406, 410, 411, 412, 416, 430, 435, 446, 447, 448, 449, 458, 471, 473, 474, 479, 488, 490, 491, 499, 506, 507, 515, 516, 517, 518, 519, 530, 532, 555, 558, 574, 580, 594, 595, 596, 637, 645, 663, 696, 697, 762, 795, 843, 878, 906, 918, 973, 1026, 1072, 1098, 557, 753)) and 'Siberian' not in cultures:
                options['The Motherland'].append(('Release Siberia', {'politicalPower': 100}, [f"independence('Siberia'), '{country.name}'"]))

            if 1094 in country.regions and 'Chechen' not in cultures:
                options['The Motherland'].append(('Release Chechnya', {'politicalPower': 25}, [f"independence('Chechnya'), '{country.name}'"]))

            if 1094 in country.regions and 'Chechen' not in cultures:
                options['The Motherland'].append(('Release Jewish Oblast', {'politicalPower': 25}, [f"independence('Jewish_Oblast'), '{country.name}'"]))

        case 'French':
            options['The Hexagon'] = []

            if any(region in country.regions for region in (756, 757, 763)) and 'Breton' not in cultures:
                options['The Hexagon'].append(('Release Brittany', {'politicalPower': 50}, [f"independence('Brittany'), '{country.name}'"]))

            if any(region in country.regions for region in (3280,)) and 'French Guianan' not in cultures:
                options['The Hexagon'].append(('Release Guyana', {'politicalPower': 25}, [f"independence('French_Guiana'), '{country.name}'"]))

            if any(region in country.regions for region in (3280,)) and 'New Caledonian' not in cultures:
                options['The Hexagon'].append(('Release NC', {'politicalPower': 25}, [f"independence('New_Caledonia'), '{country.name}'"]))

        case 'German':
            options['The Fatherland'] = []

            if country.ideologyName == 'nationalist':
                if country.atWarWith == []:

                    if 'Austrian' in borderingCultures:
                        austria = getCurrent('Austrian', countryList)

                        options['The Fatherland'].append(('Anschluss', {'politicalPower': 100}, [f"globals()['{country.name}'].annexCountry('Austrian', '{austria}')"]))

                    if 611 in country.regions and not all(region in country.regions for region in (682, 641, 649, 680, 711)):
                        toAnnex = list((682, 641, 649, 680, 711))
                        if 742 in country.regions:
                            toAnnex.append(723)
                        options['The Fatherland'].append(('Munich Agreement', {'politicalPower': 50}, [f"globals()['{country.name}'].addRegions({toAnnex})"]))

            if any(region in country.regions for region in (656, 666, 701, 721, 765, 758, 722)) and 'Bavarian' not in cultures:
                options['The Fatherland'].append(('Release Bavaria', {'politicalPower': 75}, [f"independence('Bavaria'), '{country.name}'"]))

            if any(region in country.regions for region in (691, 700, 720, 775, 719)) and 'Swabian' not in cultures:
                options['The Fatherland'].append(('Release Wurttemberg', {'politicalPower': 50}, [f"independence('Wurttemberg'), '{country.name}'"]))

            if 719 in country.regions and 'Alemanni' not in cultures:
                options['The Fatherland'].append(('Release Baden', {'politicalPower': 25}, [f"independence('Baden'), '{country.name}'"]))

        case 'American':
            options['The Union'] = []

            if any(region in country.regions for region in (1152, 1153, 1358, 1437, 1553, 1610, 1628, 1629, 1656)) and 'Californian' not in cultures:
                options['The Union'].append(('Release California', {'politicalPower': 100}, [f"independence('California'), '{country.name}'"]))

            if any(region in country.regions for region in (1514, 1621, 1643, 1658, 1659, 1674, 1675, 1681, 1723, 1744, 1763, 1797, 1812, 1813, 1838, 1865, 1904)) and 'Texan' not in cultures:
                options['The Union'].append(('Release Texas', {'politicalPower': 100}, [f"independence('Texas'), '{country.name}'"]))

            if any(region in country.regions for region in (2095, 2105, 2132, 2153)) and 'Hawaiian' not in cultures:
                options['The Union'].append(('Release Hawaii', {'politicalPower': 50}, [f"independence('Hawaii'), '{country.name}'"]))

        case 'British':
            options['The Commonwealth'] = []

            if country.stability < 50:
                options['The Commonwealth'].append(('Dissolve The UK', {'politicalPower': 100}, [
                    f"globals()['{country.name}'].replaceCountry('England')",
                    "independence('North_Ireland', 'England')",
                    "independence('Scotland', 'England')",
                    "independence('Wales', 'England')",
                ]))

            if 631 in country.regions and 'Cornish' not in cultures:
                options['The Commonwealth'].append(('Release Cornwall', {'politicalPower': 25}, [f"independence('Cornwall'), '{country.name}'"]))

        case 'Japanese':
            options['The Rising Sun'] = []

            if not all(region in country.regions for region in (697, 1026, 973, 906, 878, 795, 762, 645)) and country.atWarWith == []:
                options['The Rising Sun'].append(('Annex Russian Islands', {'politicalPower': 50}, [f"globals()['{country.name}'].addRegions([697, 1026, 973, 906, 878, 795, 762, 645])"]))

            if any(region in country.regions for region in (1884, 1948)) and 'Ryukyuan' not in cultures:
                options['The Rising Sun'].append(('Release Ryukyuan', {'politicalPower': 25}, [f"independence('Ryukyuan', '{country.name}')"]))

        case 'Indian':
            options['Bharat Mata'] = []

            if 1580 in country.regions and not all(region in country.regions for region in (1482, 1607, 1626, 1447, 1519)) and country.atWarWith == []:
                options['Bharat Mata'].append(('Annex Kashmir', {'politicalPower': 100}, [f"globals()['{country.name}'].addRegions([1482, 1607, 1626, 1447, 1519])"]))

            if any(region in country.regions for region in (1739, 1794)) and 'Punjbai' not in cultures:
                options['Bharat Mata'].append(('Release Punjab', {'politicalPower': 50}, [f"independence('Punjab', '{country.name}')"]))

        case 'Chinese':
            options['The Red Dragon'] = []

            if (2117 in country.regions and 'Hong Kongese' not in cultures) or (2131 in country.regions and 'Macanese' not in cultures):
                pp = 50
                if 'Hong Kongese' in cultures or 'Macanese' in cultures:
                    pp -= 25
                options['The Red Dragon'].append(('Release City States', {'politicalPower': pp}, [
                    f"independence('Hong_Kong', '{country.name}')",
                    f"independence('Macau', '{country.name}')",
                ]))

            if any(region in country.regions for region in (1567, 1627, 1740, 1759)) and 'Tibetan' not in cultures:
                options['The Red Dragon'].append(('Release Tibet', {'politicalPower': 50}, [f"independence('Tibet', '{country.name}')"]))

            if any(region in country.regions for region in (730, 772, 876, 915, 971, 1004, 1045, 1071, 1096, 1189, 1341, 1371, 1447, 1519)) and 'Uyghur' not in cultures:
                options['The Red Dragon'].append(('Release Xinjiang', {'politicalPower': 100}, [f"independence('Xinjiang', '{country.name}')"]))

        case 'Liechtensteiner':
            options['The Furstentum'] = []

            if not all(region in country.regions for region in (162, 244, 264, 295)) and country.atWarWith == []:
                options['The Furstentum'].append(('Purchase Alaska', {'politicalPower': 100}, [f"globals()['{country.name}'].addRegions([162, 244, 264, 295])"]))

        case 'Canadian':
            options['The True North'] = []

            if 270 in country.regions and not all(region in country.regions for region in (305, 608, 773, 709)) and country.atWarWith == []:
                options['The True North'].append(('NL Referendum', {'politicalPower': 50}, [f"globals()['{country.name}'].addRegions([305, 608, 773, 709])"]))

            if any(region in country.regions for region in (270, 583, 597, 677, 716, 737, 738, 787, 882, 895, 896)) and 'Quebecois' not in cultures:
                options['The True North'].append(('Release Quebec', {'politicalPower': 50}, [f"independence('Quebec', '{country.name}')"]))

            if any(region in country.regions for region in (312, 618, 664)) and 'Albertan' not in cultures:
                options['The True North'].append(('Wexit', {'politicalPower': 25}, [f"independence('Alberta'), '{country.name}'"]))

        case 'Spanish':
            options['Hispania'] = []

            if any(region in country.regions for region in (1142, 1130)) and 'Catalan' not in cultures:
                options['Hispania'].append(('Release Catalonia', {'politicalPower': 25}, [f"independence('Catalonia', '{country.name}')"]))

            if 1079 in country.regions and 'Quebecois' not in cultures:
                options['Hispania'].append(('Release Basque', {'politicalPower': 25}, [f"independence('Basque_Country', '{country.name}')"]))

        case 'Italian':
            options['Italia'] = []

            if 1263 in country.regions and 'Sardinian' not in cultures:
                options['Italia'].append(('Release Sardinia', {'politicalPower': 25}, [f"independence('Sardinia', '{country.name}')"]))

            if 1422 in country.regions and 'Sicilian' not in cultures:
                options['Italia'].append(('Release Sicily', {'politicalPower': 25}, [f"independence('Sicily', '{country.name}')"]))

    options = {v: k for v, k in options.items() if k != []}

    return options


def getCountryOptions(countries, countryList, country, selectedCountry):
    options = {}

    options['Friendly Options'] = []

    options['Aggressive Options'] = []

    if selectedCountry.name not in country.atWarWith and \
       selectedCountry.name not in country.militaryAccess and \
       ('nonaligned' in (getIdeologyName(selectedCountry.ideology), getIdeologyName(country.ideology)) or \
        country.faction == selectedCountry.faction):

        cost = 50

        if country.faction == selectedCountry.faction:
            cost -= 25

        if getIdeologyName(selectedCountry.ideology) != getIdeologyName(country.ideology):
            cost += 25

        options['Friendly Options'].append(('Get Military Access', {'politicalPower': cost}, [f"{country.name}.militaryAccess.append('{selectedCountry.name}')"]))

    if selectedCountry.name not in country.atWarWith:
        options['Aggressive Options'].append(('Declare War', {'politicalPower': 25}, [f"{country.name}.declareWar('{selectedCountry.name}')"]))

    if selectedCountry.faction == country.faction and \
       not len(country.atWarWith) == 0 and \
       not set(country.atWarWith) == set(selectedCountry.atWarWith) and \
       not selectedCountry.faction == None and \
       not country.faction == None:
        options['Friendly Options'].append(('Call To Arms', {'politicalPower': 25}, [f"{selectedCountry.name}.callToArms('{country.name}')"]))

    if country.faction != None and \
       country.factionLeader and \
       selectedCountry.name not in country.atWarWith and \
       'nonaligned' in (getIdeologyName(selectedCountry.ideology), getIdeologyName(country.ideology)) and \
       (selectedCountry.name in country.bordering or country.expandedInvitations) and \
       selectedCountry.faction == None:
        options['Friendly Options'].append(('Invite To Faction', {'politicalPower': 25}, [f"globals()['{country.faction}'].addCountry('{selectedCountry.name}')"]))

    options = {v: k for v, k in options.items() if k != []}

    return options


def getDemands(countryList, country, selectedCountry, releasables=[]):
    options = {}

    worldCultures = list(set([countries.getCulture(c) for c in countryList]))

    options['General Demands'] = []
    options['Territorial Claims'] = []
    options['Releasables'] = []

    if selectedCountry.regions != []:

        for culture in releasables:
            if selectedCountry.cultures[culture] != []:
                options['Territorial Claims'].append((
                    f"{culture} Regions",
                    {'warScore': 0},
                    [
                        f"{country.name}.addRegions({selectedCountry.cultures[culture]})",
                        f"releasables.remove('{culture}')",
                        'updateTreatyOptions()',
                    ],
                ))

        if country.ideologyName not in (selectedCountry.ideologyName, 'nonaligned'):
            if countries.getCountryType(selectedCountry.culture, country.ideologyName) != None and \
               countries.getCountryType(selectedCountry.culture, country.ideologyName) not in countryList:
                options['General Demands'].append((
                    'Install Government',
                    {'warScore': 0},
                    [
                        f"{selectedCountry.name}.replaceCountry('{countries.getCountryType(selectedCountry.culture, country.ideologyName)}')",
                        f"changeClicked('{countries.getCountryType(selectedCountry.culture, country.ideologyName)}')",
                        f"combatants.remove('{selectedCountry.name}')",
                        f"combatants.insert(0, '{countries.getCountryType(selectedCountry.culture, country.ideologyName)}')",
                        'updateTreatyOptions()',
                    ],
                ))
        elif country.ideologyName not in (selectedCountry.ideologyName, 'nonaligned'):
            options['General Demands'].append((
                'Install Government',
                {'warScore': 0},
                [
                    f"{selectedCountry.name}.ideologyName = '{country.ideologyName}'",
                    f"{selectedCountry.name}.ideology = {country.ideology}",
                    'updateTreatyOptions()',
                ],
            ))

        if selectedCountry.ideologyName in (country.ideologyName, 'nonaligned') and \
           country.faction != None and \
           selectedCountry.faction != country.faction:
            options['General Demands'].append((
                'Invite to Faction',
                {'warScore': 0},
                [
                    f"{country.faction}.addCountry('{selectedCountry.name}', False, False)",
                    'updateTreatyOptions()',
                ],
            ))

        if selectedCountry.faction != None:
            options['General Demands'].append((
                'Remove From Faction',
                {'warScore': 0},
                [
                    f"{selectedCountry.faction}.removeCountry('{selectedCountry.name}', False, False)",
                    'updateTreatyOptions()',
                ],
            ))

        for culture in releasables:
            if culture not in worldCultures:
                if countries.getCountryType(culture, country.ideologyName) != None:
                    toSpawn = countries.getCountryType(culture, country.ideologyName)
                    options['Releasables'].append((
                        f"{toSpawn.replace('_', ' ')}",
                        {'warScore': 0},
                        [
                            f"spawnCountry('{toSpawn}', {selectedCountry.cultures[culture]})",
                            f"releasables.remove('{culture}')",
                            'updateTreatyOptions()',
                        ],
                    ))
                else:
                    toSpawn = countries.getCountryType(culture)
                    options['Releasables'].append((
                        f"{toSpawn.replace('_', ' ')}",
                        {'warScore': 0},
                        [
                            f"spawnCountry('{toSpawn}', {selectedCountry.cultures[culture]})",
                            f"releasables.remove('{culture}')",
                            'updateTreatyOptions()',
                        ],
                    ))

    options = {v: k for v, k in options.items() if k != []}

    return options
