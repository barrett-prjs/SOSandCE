#pragma once
#include "core/common.h"


std::string prefixNumber(float value);
std::string resourceValueText(float value);
float sigDigs(float value, int digits);


std::string getMonthName(int month);
int         getMonthLength(int month);


std::string getIdeologyName(float economic, float social);
std::string getIsmName(float economic, float social);
std::string getIsm(const std::string& ideologyName);
Color       getIdeologyColor(float economic, float social);


std::string getMilitarySizeName(int size);


float normalize(float value, float inMin, float inMax, float outMin = 0.0f, float outMax = 1.0f);
float wrap(float value, float lo, float hi);
float getCurrent();


namespace Helpers {
    using ::getIdeologyName;
    using ::getIsmName;
    using ::getIsm;
    using ::getIdeologyColor;
    using ::prefixNumber;
    using ::resourceValueText;
    using ::sigDigs;
    using ::getMonthName;
    using ::getMonthLength;
    using ::getMilitarySizeName;
    using ::normalize;
    using ::wrap;
    using ::getCurrent;
}

class GameState;
std::string getFactionName(const std::string& countryName, GameState& gs);
