#pragma once
#include "core/common.h"

struct CountryRecord {
    Color color;
    std::vector<int> claims;
    std::string culture;
    std::string ideology;
    int baseStability = 60;
};

class CountryData {
public:
    static CountryData& instance();
    void load();

    std::string colorToCountry(Color color) const;
    const CountryRecord* getRecord(const std::string& name) const;
    Color getColor(const std::string& name) const;
    std::vector<int> getClaims(const std::string& name) const;
    std::string getCulture(const std::string& name) const;
    std::string getIdeology(const std::string& name) const;
    int getBaseStability(const std::string& name) const;
    std::array<float, 2> getIdeologyName(const std::string& name) const;

    std::string getCountryType(const std::string& culture, const std::string& ideology = "") const;
    std::vector<std::string> getAllCountries(const std::string& culture) const;
    std::vector<std::string> getEveryCountry() const;
    std::vector<std::string> getCultures() const;

private:
    CountryData() = default;
    std::unordered_map<std::string, CountryRecord> records_;
    std::map<std::tuple<uint8_t,uint8_t,uint8_t>, std::string> colorToCountry_;
};
