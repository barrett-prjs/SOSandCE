#include "game/helpers.h"
#include "game/game_state.h"
#include "game/country.h"

#include <cmath>
#include <sstream>
#include <iomanip>


float sigDigs(float value, int digits) {
    if (value == 0.0f) return 0.0f;

    float d = std::ceil(std::log10(std::fabs(value)));
    float power = digits - d;
    float magnitude = std::pow(10.0f, power);
    float shifted = std::round(value * magnitude);
    return shifted / magnitude;
}


static std::string sigDigsStr(float value, int sig) {
    float rounded = sigDigs(value, sig);

    std::ostringstream oss;
    oss << rounded;
    std::string s = oss.str();


    std::string digits_only;
    for (char c : s) {
        if (c != '.' && c != '-') digits_only += c;
    }


    if (digits_only.size() < static_cast<size_t>(sig)) {
        if (s.find('.') == std::string::npos) {
            s += ".0";
            digits_only += "0";
        }
        while (digits_only.size() < static_cast<size_t>(sig)) {
            s += "0";
            digits_only += "0";
        }
    }

    return s;
}


std::string prefixNumber(float value) {
    if (value >= 1e12f)
        return sigDigsStr(value / 1e12f, 3) + "T";
    if (value >= 1e9f)
        return sigDigsStr(value / 1e9f, 3) + "B";
    if (value >= 1e6f)
        return sigDigsStr(value / 1e6f, 3) + "M";
    if (value >= 1e3f)
        return sigDigsStr(value / 1e3f, 3) + "k";


    std::ostringstream oss;
    oss << static_cast<long long>(std::round(value));
    return oss.str();
}

std::string resourceValueText(float value) {
    float absValue = std::fabs(value);
    if (absValue >= 1000.0f) {
        return prefixNumber(value);
    }

    std::ostringstream oss;
    oss << std::fixed;
    if (absValue >= 100.0f) {
        oss << std::setprecision(0);
    } else if (absValue >= 1.0f) {
        oss << std::setprecision(1);
    } else {
        oss << std::setprecision(2);
    }
    oss << value;
    return oss.str();
}


std::string getMonthName(int month) {
    static const char* names[] = {
        "Invalid",
        "January", "February", "March", "April",
        "May", "June", "July", "August",
        "September", "October", "November", "December"
    };
    if (month >= 1 && month <= 12) return names[month];
    return "Invalid Month";
}

int getMonthLength(int month) {
    static const int lengths[] = {
        0,
        31, 28, 31, 30,
        31, 30, 31, 31,
        30, 31, 30, 31
    };
    if (month >= 1 && month <= 12) return lengths[month];
    return 30;
}


std::string getIdeologyName(float economic, float social) {
    float dist = std::sqrt(economic * economic + social * social);
    if (dist < 0.4f) return "nonaligned";

    if (economic <= 0.0f && social <= 0.0f) return "communist";
    if (economic >= 0.0f && social <= 0.0f) return "nationalist";
    if (economic <= 0.0f && social >= 0.0f) return "liberal";
    if (economic >= 0.0f && social >= 0.0f) return "monarchist";

    return "nonaligned";
}

std::string getIsmName(float economic, float social) {
    float dist = std::sqrt(economic * economic + social * social);
    if (dist < 0.4f) return "nonalignment";

    if (economic <= 0.0f && social <= 0.0f) return "communism";
    if (economic >= 0.0f && social <= 0.0f) return "nationalism";
    if (economic <= 0.0f && social >= 0.0f) return "liberalism";
    if (economic >= 0.0f && social >= 0.0f) return "monarchism";

    return "nonalignment";
}

std::string getIsm(const std::string& ideologyName) {
    if (ideologyName == "nonaligned")   return "nonalignment";
    if (ideologyName == "communist")    return "communism";
    if (ideologyName == "nationalist")  return "nationalism";
    if (ideologyName == "liberal")      return "liberalism";
    if (ideologyName == "monarchist")   return "monarchism";
    return "nonalignment";
}

Color getIdeologyColor(float economic, float social) {
    float dist = std::sqrt(economic * economic + social * social);
    if (dist < 0.4f) return {255, 255, 255};

    if (economic <= 0.0f && social <= 0.0f) return {255, 117, 117};
    if (economic >= 0.0f && social <= 0.0f) return {66, 170, 255};
    if (economic <= 0.0f && social >= 0.0f) return {154, 237, 151};
    if (economic >= 0.0f && social >= 0.0f) return {192, 154, 236};

    return {255, 255, 255};
}


std::string getMilitarySizeName(int size) {
    switch (size) {
        case 0: return "Disbanded Military";
        case 1: return "Reservist Force";
        case 2: return "Volunteer Force";
        case 3: return "Mandatory Service";
        case 4: return "Conscripted Army";
        default: return "Invalid Military Size (" + std::to_string(size) + ")";
    }
}


float normalize(float value, float inMin, float inMax, float outMin, float outMax) {
    if (std::fabs(inMax - inMin) < 1e-9f) return outMin;
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}

float wrap(float value, float lo, float hi) {
    float range = hi - lo;
    if (range <= 0.0f) return value;

    while (value < lo) value += range;
    while (value >= hi) value -= range;
    return value;
}

float getCurrent() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return static_cast<float>(millis) / 1000.0f;
}

std::string getFactionName(const std::string& countryName, GameState& gs) {
    static const char* suffixes[] = {
        "Alliance", "Confederation", "Dominion", "Federation",
        "League", "Coalition", "Brotherhood", "Commonwealth",
        "Syndicate", "Authority", "Order", "Consortium",
        "Assembly", "Collective", "Bloc", "Front",
        "Pact", "Accord", "Community", "Compact"
    };

    auto* c = gs.getCountry(countryName);
    if (!c) return countryName + "_Alliance";

    std::vector<std::string> descriptors;
    descriptors.push_back(c->ideologyName);
    if (!descriptors.back().empty()) descriptors.back()[0] = std::toupper(descriptors.back()[0]);
    descriptors.push_back(c->culture);
    if (!c->capital.empty()) descriptors.push_back(c->capital);

    std::string desc = descriptors[randInt(0, (int)descriptors.size() - 1)];
    std::string suf = suffixes[randInt(0, 19)];
    return desc + "_" + suf;
}
