#pragma once

#include "core/common.h"


namespace Theme {


constexpr Color bg           {  15,  17,  20 };
constexpr Color bg_dark      {  10,  12,  14 };


constexpr Color panel        {  28,  30,  35 };
constexpr Color panel_alt    {  34,  37,  43 };
constexpr Color header       {  22,  24,  30 };
constexpr Color panel_header {  22,  24,  30 };
constexpr Color bar          {  18,  20,  25 };


constexpr Color btn          {  42,  45,  52 };
constexpr Color btn_hover    {  58,  62,  72 };
constexpr Color btn_press    {  35,  38,  45 };
constexpr Color btn_disabled {  28,  30,  34 };
constexpr Color btn_danger   { 120,  40,  40 };
constexpr Color btn_confirm  {  40, 100,  55 };


constexpr Color slot          {  36,  40,  48 };
constexpr Color slot_hover    {  48,  54,  64 };
constexpr Color slot_selected {  52,  58,  70 };
constexpr Color slot_active   {  52,  58,  70 };


constexpr Color border       {  60,  64,  72 };
constexpr Color border_light {  88,  94, 104 };
constexpr Color border_hi    {  88,  94, 104 };


constexpr Color gold         { 192, 168, 104 };
constexpr Color gold_bright  { 220, 196, 130 };
constexpr Color gold_dim     { 130, 114,  72 };
constexpr Color gold_bg      {  48,  44,  32 };


constexpr Color cream        { 210, 200, 172 };
constexpr Color white        { 222, 222, 225 };
constexpr Color grey         { 140, 144, 152 };
constexpr Color dark_grey    {  85,  88,  95 };
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

}
