#pragma once

#include "core/common.h"


namespace Theme {


constexpr Color bg           {  24,  25,  26 };
constexpr Color bg_dark      {  16,  17,  18 };


constexpr Color panel        {  42,  43,  44 };
constexpr Color panel_alt    {  53,  54,  55 };
constexpr Color header       {  30,  31,  32 };
constexpr Color panel_header {  30,  31,  32 };
constexpr Color bar          {  27,  28,  29 };


constexpr Color surface0     {  33,  34,  35 };
constexpr Color surface1     {  42,  43,  44 };
constexpr Color surface2     {  53,  54,  55 };


constexpr Color btn          {  60,  61,  63 };
constexpr Color btn_hover    {  79,  80,  83 };
constexpr Color btn_press    {  45,  46,  48 };
constexpr Color btn_disabled {  38,  39,  40 };
constexpr Color btn_danger   { 122,  48,  44 };
constexpr Color btn_confirm  {  62, 104,  60 };


constexpr Color slot          {  46,  47,  48 };
constexpr Color slot_hover    {  62,  63,  65 };
constexpr Color slot_selected {  71,  71,  74 };
constexpr Color slot_active   {  71,  71,  74 };


constexpr Color border       {  95,  92,  72 };
constexpr Color border_light { 137, 131, 101 };
constexpr Color border_hi    { 137, 131, 101 };


constexpr Color gold         { 178, 160, 106 };
constexpr Color gold_bright  { 216, 195, 134 };
constexpr Color gold_dim     { 118, 108,  76 };
constexpr Color gold_bg      {  54,  51,  41 };


constexpr Color accent_yellow { 212, 184, 110 };


constexpr Color cream        { 214, 208, 186 };
constexpr Color white        { 235, 230, 214 };
constexpr Color grey         { 152, 148, 134 };
constexpr Color dark_grey    { 102, 100,  90 };
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
