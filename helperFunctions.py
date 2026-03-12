import math

from countryData import *

countries = Countries()


def prefixNumber(number):
    stringNumber = str(round(number))

    if number >= 1000000000000:
        stringNumber = f"{str(sigDigs(number / 1000000000000, 3))}T"
    elif number >= 1000000000:
        stringNumber = f"{str(sigDigs(number / 1000000000, 3))}B"
    elif number >= 1000000:
        stringNumber = f"{str(sigDigs(number / 1000000, 3))}M"
    elif number >= 1000:
        stringNumber = f"{str(sigDigs(number / 1000, 3))}k"

    return stringNumber


def sigDigs(number, sig_figs):
    format_string = "{:." + str(sig_figs) + "g}"
    formatted_number = format_string.format(number)

    if len(formatted_number.replace(".", "")) != 3 and "." not in formatted_number:
        formatted_number = formatted_number + ".0"
        if len(formatted_number.replace(".", "")) != 3:
            formatted_number = formatted_number + "0"
    else:
        if len(formatted_number.replace(".", "")) != 3:
            formatted_number = formatted_number + "0"

    return formatted_number


def getMonthName(month: int):
    months = {
        1: "January",
        2: "February",
        3: "March",
        4: "April",
        5: "May",
        6: "June",
        7: "July",
        8: "August",
        9: "September",
        10: "October",
        11: "November",
        12: "December",
    }

    return months.get(month, "Invalid Month")


def getMonthLength(month: int):
    months = {
        1: 31,
        2: 28,
        3: 31,
        4: 30,
        5: 31,
        6: 30,
        7: 31,
        8: 31,
        9: 30,
        10: 31,
        11: 30,
        12: 31,
    }

    return months.get(month, "Invalid Month")


def getIdeologyName(ideology):
    economic, social = ideology

    if math.sqrt(economic**2 + social**2) < 0.4:
        return "nonaligned"

    if economic <= 0 and social <= 0:
        return "communist"
    if economic >= 0 and social <= 0:
        return "nationalist"
    if economic <= 0 and social >= 0:
        return "liberal"
    if economic >= 0 and social >= 0:
        return "monarchist"


def getIsmName(ideology):
    economic, social = ideology

    if math.sqrt(economic**2 + social**2) < 0.4:
        return "nonalignment"

    if economic <= 0 and social <= 0:
        return "communism"
    if economic >= 0 and social <= 0:
        return "nationalism"
    if economic <= 0 and social >= 0:
        return "liberalism"
    if economic >= 0 and social >= 0:
        return "monarchism"


def getIsm(ideology: str):
    match ideology:
        case "nonaligned":
            return "nonalignment"
        case "communist":
            return "communism"
        case "nationalist":
            return "nationalism"
        case "liberal":
            return "liberalism"
        case "monarchist":
            return "monarchism"


def getIdeologyColor(ideology):
    economic, social = ideology

    if math.sqrt(economic**2 + social**2) < 0.4:
        return (255, 255, 255)

    if economic <= 0 and social <= 0:
        return (255, 117, 117)
    if economic >= 0 and social <= 0:
        return (66, 170, 255)
    if economic <= 0 and social >= 0:
        return (154, 237, 151)
    if economic >= 0 and social >= 0:
        return (192, 154, 236)


def getMilitarySizeName(num):
    names = {
        0: "Disbanded Military",
        1: "Reservist Force",
        2: "Volunteer Force",
        3: "Mandatory Service",
        4: "Conscripted Army",
    }

    return names.get(num, f"Invalid Military Size ({num})")


def normalize(position, mapSize, cam):
    if cam + position < -mapSize / 2:
        position += mapSize
    if cam + position > mapSize / 2:
        position -= mapSize
    return position


def wrap(position, finalPosition, mapSize):
    if abs(position) - abs(finalPosition) > mapSize / 2:
        position = min(position + mapSize, position - mapSize)
    return position


def getCurrent(culture, countryList=[]):
    for country in countryList:
        if countries.getCulture(country) == culture:
            return country
    return None
