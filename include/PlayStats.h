// PlayStats.h - running totals for the stats page (time played, vegetables
// picked, coins earned and spent...). Kept per save slot, in the save file as
// "stat <key> <value>" lines.
#pragma once

#include <map>
#include <sstream>
#include <string>

struct PlayStats {
    double secondsPlayed = 0.0;   // farming, the day summary and the barn (not paused, not menus)
    double secondsFarming = 0.0;  // just the days themselves
    long long picked = 0;         // every vegetable, whoever picked it
    long long pickedByHand = 0;
    long long pickedByHelper = 0; // Helping Hand auto-picks
    long long pickedByFarmers = 0;
    std::map<std::string, long long> pickedByCrop; // crop id -> count
    double coinsSpent = 0.0;      // in the barn
    double bestDayCoins = 0.0;
    long long bestDayPicked = 0;
    double biggestSale = 0.0;     // the most one vegetable has sold for

    void serialize(std::ostream& out) const {
        out << "stat played " << secondsPlayed << "\n";
        out << "stat farming " << secondsFarming << "\n";
        out << "stat picked " << picked << "\n";
        out << "stat by_hand " << pickedByHand << "\n";
        out << "stat by_helper " << pickedByHelper << "\n";
        out << "stat by_farmers " << pickedByFarmers << "\n";
        for (const auto& [id, n] : pickedByCrop) out << "stat crop_" << id << " " << n << "\n";
        out << "stat spent " << coinsSpent << "\n";
        out << "stat best_day_coins " << bestDayCoins << "\n";
        out << "stat best_day_picked " << bestDayPicked << "\n";
        out << "stat biggest_sale " << biggestSale << "\n";
    }

    // One "stat" line, after the word "stat". Unknown keys are ignored.
    void parse(std::istringstream& ss) {
        std::string key;
        ss >> key;
        if (key == "played") ss >> secondsPlayed;
        else if (key == "farming") ss >> secondsFarming;
        else if (key == "picked") ss >> picked;
        else if (key == "by_hand") ss >> pickedByHand;
        else if (key == "by_helper") ss >> pickedByHelper;
        else if (key == "by_farmers") ss >> pickedByFarmers;
        else if (key == "spent") ss >> coinsSpent;
        else if (key == "best_day_coins") ss >> bestDayCoins;
        else if (key == "best_day_picked") ss >> bestDayPicked;
        else if (key == "biggest_sale") ss >> biggestSale;
        else if (key.rfind("crop_", 0) == 0) ss >> pickedByCrop[key.substr(5)];
    }
};
