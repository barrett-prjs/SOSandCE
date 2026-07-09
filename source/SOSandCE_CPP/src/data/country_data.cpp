#include "data/country_data.h"
#include "data/data_loader.h"

CountryData& CountryData::instance() {
    static CountryData cd;
    return cd;
}

void CountryData::load() {
    auto raw = DataLoader::getCountryRecords();
    for (auto& [name, record] : raw.items()) {
        CountryRecord cr;
        if (record.contains("color") && record["color"].is_array() && record["color"].size() >= 3) {
            cr.color = {
                record["color"][0].get<uint8_t>(),
                record["color"][1].get<uint8_t>(),
                record["color"][2].get<uint8_t>()
            };
        }
        if (record.contains("claims")) {
            for (auto& c : record["claims"]) cr.claims.push_back(c.get<int>());
        }
        cr.culture = record.value("culture", "");
        cr.ideology = record.value("ideology", "nonaligned");
        cr.baseStability = record.value("base_stability", 60);

        records_[name] = cr;
        colorToCountry_[{cr.color.r, cr.color.g, cr.color.b}] = name;
    }
}

std::string CountryData::colorToCountry(Color color) const {
    auto it = colorToCountry_.find({color.r, color.g, color.b});
    return it != colorToCountry_.end() ? it->second : "";
}

const CountryRecord* CountryData::getRecord(const std::string& name) const {
    auto it = records_.find(name);
    return it != records_.end() ? &it->second : nullptr;
}

Color CountryData::getColor(const std::string& name) const {
    auto r = getRecord(name);
    return r ? r->color : Color{128, 128, 128};
}

std::vector<int> CountryData::getClaims(const std::string& name) const {
    auto r = getRecord(name);
    return r ? r->claims : std::vector<int>{};
}

std::string CountryData::getCulture(const std::string& name) const {
    auto r = getRecord(name);
    return r ? r->culture : "";
}

std::string CountryData::getIdeology(const std::string& name) const {
    auto r = getRecord(name);
    return r ? r->ideology : "nonaligned";
}

int CountryData::getBaseStability(const std::string& name) const {
    auto r = getRecord(name);
    return r ? r->baseStability : 60;
}

std::array<float, 2> CountryData::getIdeologyName(const std::string& name) const {
    std::string ideo = getIdeology(name);
    if (ideo == "liberal") return {-0.5f, 0.5f};
    if (ideo == "communist") return {-0.5f, -0.5f};
    if (ideo == "monarchist") return {0.5f, 0.5f};
    if (ideo == "nationalist") return {0.5f, -0.5f};
    return {0.0f, 0.0f};
}

std::string CountryData::getCountryType(const std::string& culture, const std::string& ideology) const {
    if (!ideology.empty()) {
        for (auto& [name, rec] : records_) {
            if (rec.culture == culture && rec.ideology == ideology) return name;
        }
        return "";
    }

    for (auto& [name, rec] : records_) {
        if (rec.culture == culture) return name;
    }
    return "";
}

std::vector<std::string> CountryData::getAllCountries(const std::string& culture) const {
    std::vector<std::string> result;
    for (auto& [name, rec] : records_) {
        if (rec.culture == culture) result.push_back(name);
    }
    return result;
}

std::vector<std::string> CountryData::getEveryCountry() const {
    std::vector<std::string> result;
    for (auto& [name, _] : records_) result.push_back(name);
    return result;
}

std::vector<std::string> CountryData::getCultures() const {
    std::set<std::string> seen;
    std::vector<std::string> result;
    for (auto& [_, rec] : records_) {
        if (seen.insert(rec.culture).second) result.push_back(rec.culture);
    }
    return result;
}
