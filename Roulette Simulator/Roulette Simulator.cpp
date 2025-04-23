// RouletteSimulator.cpp – Concurrent Roulette Simulator with ThreadPool and CNG RNG
// Compliance: MN Gaming Control Board & US Standards (CSPRNG via Windows CNG)
// Build:  C++20, Visual Studio 2022 (x64, Windows)

// ----- Standard C++ headers -------------------------------------------------
#include <iostream>
#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <sstream>
#include <thread>
#include <cmath>
#include <stdexcept>
#include <future>
#include <atomic>

// ----- Win32 & CNG headers ---------------------------------------------------
#ifdef _WIN32
#include <windows.h>
#undef max
#undef min
#include <bcrypt.h>
#pragma comment(lib, "bcrypt")
#endif

#include "ThreadPool.h"

// ============================================================================
//  Helper: CNG-based RNG
// ============================================================================
static void getRandomBytes(BYTE* buffer, ULONG length) {
    if (BCryptGenRandom(nullptr, buffer, length, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("CNG random generation failed");
    }
}

class RandomNumberGenerator { // CNG-backed RNG
public:
    int getRandomNumber(int min, int max) {
        uint32_t r;
        getRandomBytes(reinterpret_cast<BYTE*>(&r), sizeof(r));
        return min + static_cast<int>(r % (static_cast<uint32_t>(max - min + 1)));
    }
};

// ============================================================================
//  Core game enums & helpers
// ============================================================================
enum class Color { RED, BLACK, GREEN };
enum class Parity { ODD, EVEN, NONE };
enum class PlayMode { MANUAL, AUTOPLAY, CONTINUOUS };

static std::string numberToString(int num) {
    return (num == 37) ? "00" : std::to_string(num);
}

static std::string colorToString(Color c) {
    switch (c) {
    case Color::RED:   return "Red";
    case Color::BLACK: return "Black";
    case Color::GREEN: return "Green";
    }
    return "Unknown";
}

static std::string parityToString(Parity p) {
    switch (p) {
    case Parity::ODD:  return "Odd";
    case Parity::EVEN: return "Even";
    default:           return "None";
    }
}

// Outcome struct
struct RouletteOutcome {
    int number;
    Color color;
    Parity parity;
};

// Wheel spinner
class RouletteWheel {
public:
    RouletteWheel() : rng() {}
    RouletteOutcome spin() {
        int outcome = rng.getRandomNumber(0, 37);
        RouletteOutcome res{ outcome, Color::GREEN, Parity::NONE };
        if (outcome > 0 && outcome < 37) {
            static const std::vector<int> redNums = {
                1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36
            };
            res.color = (std::find(redNums.begin(), redNums.end(), outcome) != redNums.end())
                ? Color::RED : Color::BLACK;
            res.parity = (outcome % 2 == 0) ? Parity::EVEN : Parity::ODD;
        }
        return res;
    }
private:
    RandomNumberGenerator rng;
};

// Extra-bet mode
class ExtraBetMode {
public:
    explicit ExtraBetMode(bool en = false) : enabled(en) {}
    double extraBetAmount() const { return enabled ? 2.0 : 0.0; }
    double processOutcome(int outcome) const {
        if (!enabled) return 0.0;
        return (outcome == 0 || outcome == 37) ? 34.0 : -2.0;
    }
private:
    bool enabled;
};

// Betting strategy
class BettingStrategy {
public:
    explicit BettingStrategy(const std::vector<double>& m) : multipliers(m) {
        if (multipliers.empty()) multipliers = { 3.0,3.0,2.0 };
    }
    double getMultiplier(int n) const {
        if (n <= static_cast<int>(multipliers.size()))
            return multipliers[n - 1];
        return multipliers.back();
    }
private:
    std::vector<double> multipliers;
};

// Timer for simulation speed (headless)
class CasinoTimer {
public:
    void addSpin() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        elapsed += 35;
    }
    bool isTimeUp() const { return elapsed >= 8 * 3600; }
private:
    int elapsed = 0;
};

// ============================================================================
//  Batch simulation structures
// ============================================================================
struct SimulationSettings {
    double initialBankroll;
    int lossThreshold;
    std::vector<double> lossMultipliers;
    std::vector<double> winMultipliers;
    bool extraBet;
    PlayMode playMode;
    int autoSpins;
    double startingBet;
};

struct SimulationResult {
    double finalBankroll;
};

// Single-instance simulation
static SimulationResult simulateOne(const SimulationSettings& s) {
    const double maxBet = 10000.0;
    double bankroll = s.initialBankroll;
    RouletteWheel wheel;
    ExtraBetMode extra(s.extraBet);
    BettingStrategy lossStrat(s.lossMultipliers);
    BettingStrategy winStrat(s.winMultipliers);
    bool useWinMult = !s.winMultipliers.empty();
    CasinoTimer timer;

    double currentBet = s.startingBet;
    Color betColor = Color::BLACK;
    int consecutiveLosses = 0, consecutiveWins = 0;

    while (bankroll > 0 && !timer.isTimeUp() && currentBet <= bankroll) {
        auto res = wheel.spin();
        double ext = extra.processOutcome(res.number);

        if (res.color == betColor) {
            bankroll += currentBet + ext;
            ++consecutiveWins; consecutiveLosses = 0;
            if (useWinMult) {
                double nb = s.startingBet * winStrat.getMultiplier(consecutiveWins);
                currentBet = (nb >= maxBet ? maxBet : nb);
            }
            else {
                currentBet = s.startingBet;
            }
        }
        else {
            bankroll += -currentBet + ext;
            ++consecutiveLosses; consecutiveWins = 0;
            double nb = currentBet * lossStrat.getMultiplier(consecutiveLosses);
            currentBet = (nb >= maxBet ? maxBet : nb);
            if (consecutiveLosses >= s.lossThreshold) {
                betColor = (betColor == Color::BLACK ? Color::RED : Color::BLACK);
                consecutiveLosses = 0;
            }
        }
        timer.addSpin();
    }

    return { bankroll };
}

// ============================================================================
//  main()
// ============================================================================
int main() {
    // 1) Gather settings
    std::cout << "Enter initial bankroll: $";
    double initBank;
    while (!(std::cin >> initBank) || initBank <= 0) {
        std::cin.clear(); std::cin.ignore(INT_MAX, '\n');
        std::cout << "Positive number please: $";
    }

    std::cout << "Enter loss threshold: ";
    int lossTh;
    while (!(std::cin >> lossTh) || lossTh < 0) {
        std::cin.clear(); std::cin.ignore(INT_MAX, '\n');
        std::cout << "Non-negative integer please: ";
    }
    std::cin.ignore(INT_MAX, '\n');

    std::cout << "Enter loss multipliers (e.g. 3 3 2): ";
    std::vector<double> lossMult;
    {
        std::string line; std::getline(std::cin, line);
        std::istringstream iss(line); double x;
        while (iss >> x) lossMult.push_back(x);
        if (lossMult.empty()) lossMult = { 3,3,2 };
    }

    std::cout << "Enter win multipliers (empty = reset to $1): ";
    std::vector<double> winMult;
    {
        std::string line; std::getline(std::cin, line);
        std::istringstream iss(line); double x;
        while (iss >> x) winMult.push_back(x);
    }

    std::cout << "Enable extra-bet ($1 on 0/00)? (y/n): ";
    char c; std::cin >> c; bool extra = (c == 'y' || c == 'Y');

    std::cout << "Play mode (0=manual, -1=continuous, >0=auto spins): ";
    int pm; std::cin >> pm;
    PlayMode mode = (pm == 0 ? PlayMode::MANUAL : (pm < 0 ? PlayMode::CONTINUOUS : PlayMode::AUTOPLAY));
    int autoSpins = (pm > 0 ? pm : 0);

    std::cout << "Enter starting bet: $";
    double startBet;
    while (!(std::cin >> startBet) || startBet <= 0) {
        std::cin.clear(); std::cin.ignore(INT_MAX, '\n');
        std::cout << "Positive number please: $";
    }

    std::cout << "How many simulation instances?: ";
    size_t numSims;
    while (!(std::cin >> numSims) || numSims == 0) {
        std::cin.clear(); std::cin.ignore(INT_MAX, '\n');
        std::cout << "Enter a positive integer: ";
    }

    SimulationSettings settings{
        initBank, lossTh, lossMult, winMult,
        extra, mode, autoSpins, startBet
    };

    // 2) ThreadPool & enqueue
    unsigned int poolSize = std::thread::hardware_concurrency();
    if (poolSize == 0) poolSize = 1;
    ThreadPool pool(poolSize);

    std::atomic<size_t> done{ 0 };
    std::vector<std::future<SimulationResult>> futures;
    futures.reserve(numSims);

    // 3) Progress reporter
    std::jthread prog([&]() {
        while (done < numSims) {
            std::cout << "\rProgress: " << done << " / " << numSims << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "\rProgress: " << done << " / " << numSims << " - Complete!\n";
        });

    // 4) Launch all jobs
    for (size_t i = 0; i < numSims; ++i) {
        futures.emplace_back(
            pool.enqueue([&settings, &done]() {
                auto r = simulateOne(settings);
                ++done;
                return r;
                })
        );
    }

    // 5) Collect & report
    std::vector<SimulationResult> results;
    results.reserve(numSims);
    for (auto& f : futures) results.push_back(f.get());

    std::cout << "\nAll simulations finished.\n";
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "Sim " << (i + 1)
            << " final bankroll: $" << results[i].finalBankroll << "\n";
    }

    return 0;
}
