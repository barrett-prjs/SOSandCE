from __future__ import annotations

import random

from data_loader import get_country_records


def _record_to_legacy_list(record):
    return [
        list(record.get("color", [0, 0, 0])),
        list(record.get("claims", [])),
        record.get("culture"),
        record.get("ideology", "nonaligned"),
        record.get("base_stability", 60),
    ]


class Countries:
    def __init__(self):
        raw_records = get_country_records()
        self.records = {country: record for country, record in raw_records.items()}
        self.countryData = {country: _record_to_legacy_list(record) for country, record in raw_records.items()}
        self.colorsToCountries = {tuple(values[0]): name for name, values in self.countryData.items()}

    def getCountryType(self, culture, ideology=None):
        if ideology is not None:
            for country in self.countryData.keys():
                if self.getCulture(country) == culture and self.getIdeology(country) == ideology:
                    return country
            return None

        countries = list(self.countryData.keys())
        random.shuffle(countries)
        for country in countries:
            if self.getCulture(country) == culture:
                return country
        return None

    def getAllCountries(self, culture):
        return [country for country in self.countryData.keys() if self.countryData[country][2] == culture]

    def getEveryCountry(self):
        return [country for country in self.countryData.keys()]

    def colorToCountry(self, color):
        return self.colorsToCountries.get(tuple(color))

    def getCountryData(self, country):
        return self.countryData.get(country)

    def getColor(self, country):
        data = self.countryData.get(country)
        return data[0] if data is not None else [128, 128, 128]

    def getClaims(self, country):
        data = self.countryData.get(country)
        return data[1] if data is not None else []

    def getCulture(self, country):
        data = self.countryData.get(country)
        return data[2] if data is not None else None

    def getIdeology(self, country):
        data = self.countryData.get(country)
        return data[3] if data is not None else "nonaligned"

    def getBaseStability(self, country):
        value = self.countryData.get(country)[-1]
        return 60 if isinstance(value, str) else value

    def getIdeologyName(self, country):
        ideologies = {
            "liberal": [-0.5, 0.5],
            "communist": [-0.5, -0.5],
            "monarchist": [0.5, 0.5],
            "nationalist": [0.5, -0.5],
        }
        return ideologies.get(str(self.countryData.get(country)[3]), [0, 0])

    def getCultures(self):
        culture_list = []
        for data in self.countryData.values():
            if data[2] not in culture_list:
                culture_list.append(data[2])
        return culture_list
