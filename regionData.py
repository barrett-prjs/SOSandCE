from __future__ import annotations

from data_loader import get_region_payload


def _tuple_key(value):
    return tuple(value) if isinstance(value, (list, tuple)) else value


class Regions:
    def __init__(self):
        payload = get_region_payload()

        self.regionInfo = {}
        self.colorsAndIds = {}
        self.regionsAndOwners = {}
        for region_id, record in payload["regions"].items():
            numeric_id = int(region_id)
            color = list(record["color"])
            # The compiled renderer uses locations/connections in hashed lookups.
            # Keep them immutable even though the JSON payload stores them as arrays.
            location = tuple(record["location"])
            connections = tuple(record.get("connections", []))
            population = record.get("population", 0)
            self.regionInfo[numeric_id] = [color, location, connections, population]
            self.colorsAndIds[tuple(color)] = numeric_id
            self.regionsAndOwners[numeric_id] = record.get("owner")

        self.regionResources = {}
        for region_id, record in payload["regions"].items():
            self.regionResources[int(region_id)] = record.get("resources", {})

        self.biomeInfo = {
            _tuple_key(json_color): value for json_color, value in (
                ((tuple(map(int, key.split(","))) if isinstance(key, str) and "," in key else key), value)
                for key, value in payload["biomes"].items()
            )
        }

        self.worldRegionInfo = {
            _tuple_key(tuple(map(int, key.split(","))) if isinstance(key, str) else key): value
            for key, value in payload["world_regions"].items()
        }
        self.worldRegionInfoInverted = {value[0]: value[1:] for value in self.worldRegionInfo.values()}

        self.cities = {}
        self.locationAndCities = {}
        for city, record in payload["cities"].items():
            region = int(record["region"])
            culture = record.get("culture")
            self.cities[city] = [region, culture]
            self.locationAndCities[region] = city

    def updateOwner(self, region, name):
        self.regionsAndOwners[region] = name

    def getAllOwners(self):
        return self.regionsAndOwners

    def getAllWorldRegions(self):
        return [(name, data[0]) for name, data in self.worldRegionInfoInverted.items()]

    def getWorldRegionLocation(self, name):
        return self.worldRegionInfoInverted[name][1:4]

    def getAllWorldRegionNames(self):
        return [info[0] for info in self.worldRegionInfo.values()]

    def getWorldRegion(self, color):
        return self.worldRegionInfo.get(tuple(color), ["Invalid World Region", ""])[0]

    def getWorldAdjective(self, color):
        return self.worldRegionInfo.get(tuple(color), ["Invalid World Region", ""])[1]

    def getOwner(self, region):
        return self.regionsAndOwners[region]

    def getCities(self):
        return self.cities.keys()

    def getCityLocation(self, city):
        return self.getLocation(self.cities[city][0])

    def getCity(self, location):
        return self.locationAndCities.get(location, None)

    def getCityRegion(self, city):
        return self.cities.get(city)[0]

    def getCityCulture(self, city):
        return self.cities.get(city)[1]

    def getBiomeInfo(self, color):
        return self.biomeInfo.get(tuple(color), ["Invalid", 1, 1, 1])

    def getRegion(self, color):
        if tuple(color) == (0, 0, 0):
            return None
        return self.colorsAndIds.get(tuple(color))

    def getLocation(self, region_id):
        return self.regionInfo.get(region_id)[1]

    def getConnections(self, region_id):
        return self.regionInfo.get(region_id)[2]

    def getAllConnections(self):
        return {key: value[2] for key, value in self.regionInfo.items()}

    def getAllLocations(self):
        return {key: value[1] for key, value in self.regionInfo.items()}

    def getPopulation(self, region_id):
        return self.regionInfo.get(region_id)[3]

    def getInfo(self, region_id):
        return self.regionInfo.get(region_id)

    def getRegionAdjective(self, color):
        return self.worldRegionInfo.get(tuple(color), ["Error", "Error"])[1]

    def getRegionName(self, color):
        return self.worldRegionInfo.get(tuple(color), ["Error"])[0]

    def getResources(self, region_id):
        return self.regionResources.get(region_id, {})
