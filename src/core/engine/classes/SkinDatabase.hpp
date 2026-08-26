#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct SkinEntry {
    int paint_kit;
    const char* name;
};

// Weapon item definition indices (from CS2 schema).
enum WeaponID : int {
    WEAPON_DEAGLE = 1,
    WEAPON_ELITE = 2,
    WEAPON_FIVESEVEN = 3,
    WEAPON_GLOCK = 4,
    WEAPON_AK47 = 7,
    WEAPON_AUG = 8,
    WEAPON_AWP = 9,
    WEAPON_FAMAS = 10,
    WEAPON_G3SG1 = 11,
    WEAPON_GALIL = 13,
    WEAPON_M249 = 14,
    WEAPON_M4A4 = 16,
    WEAPON_MAC10 = 17,
    WEAPON_P2000 = 32,
    WEAPON_MP5SD = 23,
    WEAPON_UMP45 = 24,
    WEAPON_XM1014 = 25,
    WEAPON_PPBIZON = 26,
    WEAPON_MAG7 = 27,
    WEAPON_NEGEV = 28,
    WEAPON_SAWEDOFF = 29,
    WEAPON_TEC9 = 30,
    WEAPON_P250 = 36,
    WEAPON_MP7 = 33,
    WEAPON_MP9 = 34,
    WEAPON_NOVA = 35,
    WEAPON_P90 = 19,
    WEAPON_SCAR20 = 38,
    WEAPON_SG553 = 39,
    WEAPON_SSG08 = 40,
    WEAPON_M4A1S = 60,
    WEAPON_USP = 61,
    WEAPON_CZ75 = 63,
    WEAPON_REVOLVER = 64,
    WEAPON_KNIFE_BAYONET = 500,
    WEAPON_KNIFE_FLIP = 505,
    WEAPON_KNIFE_GUT = 506,
    WEAPON_KNIFE_KARAMBIT = 507,
    WEAPON_KNIFE_M9 = 508,
    WEAPON_KNIFE_HUNTSMAN = 512,
    WEAPON_KNIFE_FALCHION = 514,
    WEAPON_KNIFE_BOWIE = 515,
    WEAPON_KNIFE_BUTTERFLY = 516,
    WEAPON_KNIFE_SHADOW = 517,
    WEAPON_KNIFE_STILETTO = 518,
    WEAPON_KNIFE_TALON = 519,
    WEAPON_KNIFE_SKELETON = 520,
    WEAPON_KNIFE_KUKRI = 521,
};

// Common CS2 paint kit IDs (skin names).  Expandable.
namespace SkinDB {

struct WeaponInfo {
    const char* display_name;
    int default_paint_kit;  // 0 = default skin
};

inline const std::unordered_map<int, WeaponInfo>& Weapons() {
    static const std::unordered_map<int, WeaponInfo> w = {
        { WEAPON_DEAGLE,      { "Desert Eagle", 0 } },
        { WEAPON_ELITE,       { "Dual Berettas", 0 } },
        { WEAPON_FIVESEVEN,   { "Five-SeveN", 0 } },
        { WEAPON_GLOCK,       { "Glock-18", 0 } },
        { WEAPON_AK47,        { "AK-47", 0 } },
        { WEAPON_AUG,         { "AUG", 0 } },
        { WEAPON_AWP,         { "AWP", 0 } },
        { WEAPON_FAMAS,       { "FAMAS", 0 } },
        { WEAPON_G3SG1,       { "G3SG1", 0 } },
        { WEAPON_GALIL,       { "Galil AR", 0 } },
        { WEAPON_M249,        { "M249", 0 } },
        { WEAPON_M4A4,        { "M4A4", 0 } },
        { WEAPON_MAC10,       { "MAC-10", 0 } },
        { WEAPON_P2000,       { "P2000", 0 } },
        { WEAPON_MP5SD,       { "MP5-SD", 0 } },
        { WEAPON_UMP45,       { "UMP-45", 0 } },
        { WEAPON_XM1014,      { "XM1014", 0 } },
        { WEAPON_PPBIZON,     { "PP-Bizon", 0 } },
        { WEAPON_MAG7,        { "MAG-7", 0 } },
        { WEAPON_NEGEV,       { "Negev", 0 } },
        { WEAPON_SAWEDOFF,    { "Sawed-Off", 0 } },
        { WEAPON_TEC9,        { "Tec-9", 0 } },
        { WEAPON_P250,        { "P250", 0 } },
        { WEAPON_MP7,         { "MP7", 0 } },
        { WEAPON_MP9,         { "MP9", 0 } },
        { WEAPON_NOVA,        { "Nova", 0 } },
        { WEAPON_P90,         { "P90", 0 } },
        { WEAPON_SCAR20,      { "SCAR-20", 0 } },
        { WEAPON_SG553,       { "SG 553", 0 } },
        { WEAPON_SSG08,       { "SSG 08", 0 } },
        { WEAPON_M4A1S,       { "M4A1-S", 0 } },
        { WEAPON_USP,         { "USP-S", 0 } },
        { WEAPON_CZ75,        { "CZ75-Auto", 0 } },
        { WEAPON_REVOLVER,    { "R8 Revolver", 0 } },
        { WEAPON_KNIFE_BAYONET,   { "Bayonet", 0 } },
        { WEAPON_KNIFE_FLIP,      { "Flip Knife", 0 } },
        { WEAPON_KNIFE_GUT,       { "Gut Knife", 0 } },
        { WEAPON_KNIFE_KARAMBIT,  { "Karambit", 0 } },
        { WEAPON_KNIFE_M9,        { "M9 Bayonet", 0 } },
        { WEAPON_KNIFE_BUTTERFLY, { "Butterfly Knife", 0 } },
        { WEAPON_KNIFE_SKELETON,  { "Skeleton Knife", 0 } },
        { WEAPON_KNIFE_KUKRI,     { "Kukri Knife", 0 } },
    };
    return w;
}

// Popular CS2 paint kit IDs.  These are the most commonly used skins.
inline const std::vector<SkinEntry>& PopularSkins() {
    static const std::vector<SkinEntry> s = {
        { 0,    "Default" },
        // AK-47
        { 180,  "Redline" },
        { 302,  "Vulcan" },
        { 44,   "Red Laminate" },
        { 144,  "Fire Serpent" },
        { 747,  "Neon Rider" },
        { 801,  "The Empress" },
        { 506,  "Cartel" },
        { 639,  "Frontside Misty" },
        { 878,  "Ice Coaled" },
        // AWP
        { 38,   "Dragon Lore" },
        { 51,   "Asiimov" },
        { 259,  "Medusa" },
        { 344,  "Hyper Beast" },
        { 475,  "Wildfire" },
        { 662,  "Fade" },
        { 837,  "Neo-Noir" },
        { 898,  "Containment Breach" },
        // M4A4
        { 309,  "Howl" },
        { 388,  "Desolate Space" },
        { 488,  "Buzz Kill" },
        { 730,  "Neo-Noir" },
        { 811,  "In Living Color" },
        // M4A1-S
        { 430,  "Hyper Beast" },
        { 587,  "Chantico's Fire" },
        { 757,  "Decimator" },
        { 897,  "Printstream" },
        // USP-S
        { 25,   "Kill Confirmed" },
        { 332,  "Cortex" },
        { 504,  "Neo-Noir" },
        { 658,  "Printstream" },
        // Glock-18
        { 38,   "Fade" },
        { 292,  "Wasteland Rebel" },
        { 586,  "Gamma Doppler" },
        { 799,  "Water Elemental" },
        // knives
        { 568,  "Fade" },
        { 569,  "Marble Fade" },
        { 570,  "Doppler" },
        { 571,  "Tiger Tooth" },
        { 572,  "Crimson Web" },
        { 578,  "Slaughter" },
        { 600,  "Lore" },
        { 752,  "Case Hardened" },
        { 1132, "Autotronic" },
        { 1133, "Blue Steel" },
        { 1134, "Rust Coat" },
        { 1135, "Stained" },
    };
    return s;
}

// Wear display names.
inline const char* WearName(float wear) {
    if (wear < 0.07f) return "Factory New";
    if (wear < 0.15f) return "Minimal Wear";
    if (wear < 0.38f) return "Field-Tested";
    if (wear < 0.45f) return "Well-Worn";
    return "Battle-Scarred";
}

} // namespace SkinDB
