#pragma once

#include "core/common.h"


namespace Theme {


constexpr Color bg           {  10,  10,  11 };
constexpr Color bg_dark      {   6,   6,   7 };


constexpr Color panel        {  16,  16,  18 };
constexpr Color panel_alt    {  24,  24,  27 };
constexpr Color header       {   9,   9,  10 };
constexpr Color panel_header {   9,   9,  10 };
constexpr Color bar          {  12,  12,  14 };


constexpr Color surface0     {  10,  10,  11 };
constexpr Color surface1     {  14,  14,  16 };
constexpr Color surface2     {  24,  24,  27 };


constexpr Color btn          {  24,  24,  27 };
constexpr Color btn_hover    {  40,  40,  45 };
constexpr Color btn_press    {  15,  15,  17 };
constexpr Color btn_disabled {  17,  17,  19 };
constexpr Color btn_danger   { 120,  40,  40 };
constexpr Color btn_confirm  {  40, 100,  55 };


constexpr Color slot          {  22,  22,  25 };
constexpr Color slot_hover    {  36,  36,  40 };
constexpr Color slot_selected {  40,  40,  45 };
constexpr Color slot_active   {  40,  40,  45 };


constexpr Color border       { 185, 185, 192 };
constexpr Color border_light { 215, 215, 220 };
constexpr Color border_hi    { 215, 215, 220 };


constexpr Color gold         { 228, 228, 232 };
constexpr Color gold_bright  { 255, 255, 255 };
constexpr Color gold_dim     { 150, 150, 157 };
constexpr Color gold_bg      {  44,  44,  49 };


constexpr Color accent_yellow { 233, 208, 120 };


constexpr Color cream        { 226, 226, 230 };
constexpr Color white        { 240, 240, 243 };
constexpr Color grey         { 152, 154, 160 };
constexpr Color dark_grey    {  98, 100, 106 };
constexpr Color text_shadow  {   0,   0,   0 };


constexpr Color red          { 185,  60,  60 };
constexpr Color red_light    { 220, 100, 100 };
constexpr Color green        {  60, 165,  80 };
constexpr Color green_light  { 100, 210, 120 };
constexpr Color blue         {  70, 130, 200 };
constexpr Color yellow       { 200, 185,  75 };
constexpr Color orange       { 210, 145,  55 };


constexpr Color democratic   {  60, 120, 200 };
constexpr Color communist    { 180,  40,  40 };
constexpr Color fascist      { 100,  70,  40 };
constexpr Color nonaligned   { 120, 120, 120 };


inline float uiScale = 24.0f;


inline float s(float v) { return v * uiScale / 16.0f; }
inline int si(float v) { return static_cast<int>(v * uiScale / 16.0f); }


inline float us(float v) { return v * uiScale; }
inline int usi(float v) { return static_cast<int>(v * uiScale); }


inline void setScale(float scale) { uiScale = scale; }


inline int fsTitle(float u)   { return std::max(15, static_cast<int>(u * 1.5f)); }
inline int fsSection(float u) { return std::max(12, static_cast<int>(u * 1.0f)); }
inline int fsBody(float u)    { return std::max(12, static_cast<int>(u * 1.05f)); }
inline int fsCaption(float u) { return std::max(10, static_cast<int>(u * 0.82f)); }

inline Color hairline()       { return {border.r, border.g, border.b, 70}; }
inline Color hairlineStrong() { return {border.r, border.g, border.b, 140}; }

}
