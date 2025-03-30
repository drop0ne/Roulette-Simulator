#include <iostream>
#include <random>
#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <sstream>
#include <thread>

// Enums for color and parity.
enum class Color { RED, BLACK, GREEN };
enum class Parity { ODD, EVEN, NONE };

// Helper to convert number to string (handles "00").
std::string numberToString(int num) {
    return (num == 37) ? "00" : std::to_string(num);
}

// Class that encapsulates a high-quality random number generator.
class RandomNumberGenerator {
public:
    RandomNumberGenerator()
        : rng(std::random_device{}())
    {
    }

    // Returns a random integer between min and max inclusive.
    int getRandomNumber(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }
private:
    std::mt19937 rng;
};

// Structure for a roulette outcome.
struct RouletteOutcome {
    int number;    // 0, 1,2,...,36; 37 represents "00"
    Color color;
    Parity parity;
};

// RouletteWheel class containing the roulette logic.
class RouletteWheel {
public:
    RouletteWheel() : rng() {}

    RouletteOutcome spin() {
        // American roulette has 38 outcomes: 0, 00, and 1-36.
        // We represent "00" as 37.
        int outcome = rng.getRandomNumber(0, 37);
        RouletteOutcome result;
        result.number = outcome;
        if (outcome == 0 || outcome == 37) {
            result.color = Color::GREEN;
            result.parity = Parity::NONE;
        }
        else {
            // Typical American roulette red numbers.
            static const std::vector<int> redNumbers = { 1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36 };
            result.color = (std::find(redNumbers.begin(), redNumbers.end(), outcome) != redNumbers.end())
                ? Color::RED : Color::BLACK;
            result.parity = (outcome % 2 == 0) ? Parity::EVEN : Parity::ODD;
        }
        return result;
    }
private:
    RandomNumberGenerator rng;
};

// ExtraBetMode class encapsulates the optional extra bet logic.
// When enabled, it places a $1 bet on 0 and a $1 bet on 00 every spin.
class ExtraBetMode {
public:
    ExtraBetMode(bool enabled = false) : enabled(enabled) {}

    bool isEnabled() const { return enabled; }

    // Given the outcome number, returns the net result of the extra bets.
    // For a straight bet, a win pays 35:1. When betting on both 0 and 00:
    // - If outcome is 0 or 37 ("00"), one extra bet wins ($35 profit) and the other loses ($1 loss),
    //   for a net gain of $34.
    // - Otherwise, both extra bets lose ($2 total loss).
    double processOutcome(int outcome) const {
        if (!enabled) return 0.0;
        if (outcome == 0 || outcome == 37)
            return 34.0;
        return -2.0;
    }

    // Returns the extra money wagered per spin (always $2 if enabled).
    double extraBetAmount() const {
        return enabled ? 2.0 : 0.0;
    }
private:
    bool enabled;
};

// BettingStrategy class to encapsulate a multiplier strategy (for losses or wins).
class BettingStrategy {
public:
    // The constructor accepts a vector of multipliers.
    BettingStrategy(const std::vector<double>& multipliers) : multipliers_(multipliers) {
        if (multipliers_.empty()) {
            // Fallback default if empty.
            multipliers_ = { 3.0, 3.0, 2.0 };
        }
    }

    // Returns the multiplier for the current consecutive event.
    // If the count exceeds the number of multipliers provided, the last multiplier is used.
    double getMultiplier(int consecutiveCount) const {
        if (consecutiveCount <= static_cast<int>(multipliers_.size()))
            return multipliers_[consecutiveCount - 1];
        return multipliers_.back();
    }
private:
    std::vector<double> multipliers_;
};

// StatsTracker class to keep track of wins, losses, bets, history, and new green/zero stats.
class StatsTracker {
public:
    StatsTracker()
        : wins(0), losses(0), totalSpins(0), totalMoneyBet(0.0), count0(0), count00(0)
    {
    }

    void recordWin(double betAmount) {
        ++wins;
        ++totalSpins;
        totalMoneyBet += betAmount;
        addToHistory("Win: Bet $" + std::to_string(betAmount));
    }

    void recordLoss(double betAmount) {
        ++losses;
        ++totalSpins;
        totalMoneyBet += betAmount;
        addToHistory("Loss: Bet $" + std::to_string(betAmount));
    }

    // Updated to record extra stats for outcomes.
    void addOutcomeToHistory(const RouletteOutcome& outcome) {
        if (outcome.number == 0) {
            count0++;
        }
        else if (outcome.number == 37) {
            count00++;
        }
        std::string outcomeStr = "Spin: " + numberToString(outcome.number) +
            " (" + colorToString(outcome.color) + ", " + parityToString(outcome.parity) + ")";
        addToHistory(outcomeStr);
    }

    void printStats(double bankroll, double currentBet, int consecutiveLosses, Color betColor) const {
        std::cout << "\n===== Current Stats =====\n";
        std::cout << "Bankroll: $" << bankroll << "\n";
        std::cout << "Current Bet: $" << currentBet << "\n";
        std::cout << "Consecutive Losses: " << consecutiveLosses << "\n";
        std::cout << "Betting on: " << colorToString(betColor) << "\n";
        std::cout << "Total Spins: " << totalSpins << "\n";
        std::cout << "Wins: " << wins << "\n";
        std::cout << "Losses: " << losses << "\n";
        std::cout << "Total Money Bet: $" << totalMoneyBet << "\n";
        std::cout << "Last " << history.size() << " outcomes:\n";
        for (const auto& h : history) {
            std::cout << "  " << h << "\n";
        }
        // Print new stats.
        std::cout << "Total Green Outcomes (0 or 00): " << (count0 + count00) << "\n";
        std::cout << "   - 0 appeared: " << count0 << " times\n";
        std::cout << "   - 00 appeared: " << count00 << " times\n";
        std::cout << "=========================\n\n";
    }

private:
    int wins;
    int losses;
    int totalSpins;
    double totalMoneyBet;
    std::deque<std::string> history; // store up to last 10 outcomes

    // New counters for green outcomes.
    int count0;
    int count00;

    void addToHistory(const std::string& entry) {
        if (history.size() >= 10) {
            history.pop_front();
        }
        history.push_back(entry);
    }

    std::string colorToString(Color color) const {
        switch (color) {
        case Color::RED: return "Red";
        case Color::BLACK: return "Black";
        case Color::GREEN: return "Green";
        default: return "Unknown";
        }
    }

    std::string parityToString(Parity parity) const {
        switch (parity) {
        case Parity::ODD: return "Odd";
        case Parity::EVEN: return "Even";
        case Parity::NONE: return "None";
        default: return "Unknown";
        }
    }
};

// UserInterface class to manage I/O interactions.
class UserInterface {
public:
    double getInitialBankroll() const {
        double bankroll;
        std::cout << "Enter your initial bankroll: $";
        std::cin >> bankroll;
        return bankroll;
    }

    int getLossThreshold() const {
        int threshold;
        std::cout << "Enter the maximum consecutive losses allowed before switching bet color: ";
        std::cin >> threshold;
        return threshold;
    }

    // Updated prompt:
    // Enter a positive number for auto-play count,
    // 0 for manual mode,
    // or -1 for continuous mode (play without interruption until game ends).
    int getAutoSpinCount() const {
        int count;
        std::cout << "Enter number of spins to auto bet (0 for manual mode, -1 for continuous mode, positive number for auto-play count): ";
        std::cin >> count;
        return count;
    }

    // Ask the user for the betting multipliers for consecutive losses.
    std::vector<double> getBettingStrategyMultipliers() const {
        std::cout << "Enter the betting multipliers for each consecutive loss (e.g., \"3 3 2\" or \"4 3 3 2\").\n";
        std::cout << "Separate values with spaces and press Enter: ";
        std::string input;
        std::getline(std::cin >> std::ws, input);
        std::istringstream iss(input);
        std::vector<double> multipliers;
        double value;
        while (iss >> value) {
            multipliers.push_back(value);
        }
        if (multipliers.empty()) {
            std::cout << "No valid input provided. Using default loss multipliers: 3 3 2.\n";
            multipliers = { 3.0, 3.0, 2.0 };
        }
        return multipliers;
    }

    // Ask the user for the betting multipliers for consecutive wins.
    // If left empty, the bet will reset to $1 after each win.
    std::vector<double> getWinningStrategyMultipliers() const {
        std::cout << "Enter the betting multipliers for each consecutive win (e.g., \"2 2 1.5\").\n";
        std::cout << "Leave empty to reset bet to $1 after each win: ";
        std::string input;
        std::getline(std::cin >> std::ws, input);
        std::istringstream iss(input);
        std::vector<double> multipliers;
        double value;
        while (iss >> value) {
            multipliers.push_back(value);
        }
        return multipliers; // may be empty
    }

    // Ask the user if they want to enable Extra Bet mode.
    bool askExtraBetMode() const {
        char choice;
        std::cout << "Do you want to enable Extra Bet mode (place $1 bet on both 0 and 00 every spin)? (y/n): ";
        std::cin >> choice;
        return (choice == 'y' || choice == 'Y');
    }
};

// New class to simulate the casino spin delay and track elapsed play time.
class CasinoTimer {
public:
    CasinoTimer() : elapsedSeconds(0) {}

    // Simulate delay between spins (using a short sleep).
    void addSpin() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsedSeconds += averageDelay;
    }

    // Returns true if total simulated play time has reached 8 hours.
    bool isTimeUp() const {
        return elapsedSeconds >= maxTime;
    }

    // Report elapsed time (in seconds).
    int getElapsedSeconds() const {
        return elapsedSeconds;
    }

    static constexpr int averageDelay = 35;   // seconds per spin (average delay)
    static constexpr int maxTime = 8 * 3600;      // 8 hours in seconds (28800 seconds)
private:
    int elapsedSeconds;
};

// Helper functions to convert enums to strings for printing.
std::string colorToString(Color color) {
    switch (color) {
    case Color::RED: return "Red";
    case Color::BLACK: return "Black";
    case Color::GREEN: return "Green";
    default: return "Unknown";
    }
}

std::string parityToString(Parity parity) {
    switch (parity) {
    case Parity::ODD: return "Odd";
    case Parity::EVEN: return "Even";
    case Parity::NONE: return "None";
    default: return "Unknown";
    }
}

int main() {
    UserInterface ui;
    double bankroll = ui.getInitialBankroll();
    double startingBankroll = bankroll; // for profit calculations
    int lossThreshold = ui.getLossThreshold();

    // Get loss multipliers.
    std::vector<double> lossMultipliers = ui.getBettingStrategyMultipliers();
    BettingStrategy lossStrategy(lossMultipliers);

    // Get winning multipliers (if any).
    std::vector<double> winMultipliers = ui.getWinningStrategyMultipliers();
    bool useWinMultipliers = !winMultipliers.empty();
    BettingStrategy winStrategy(winMultipliers);

    // Ask if Extra Bet mode is enabled.
    bool extraModeEnabled = ui.askExtraBetMode();
    ExtraBetMode extraBetMode(extraModeEnabled);

    RouletteWheel wheel;
    StatsTracker stats;
    CasinoTimer timer;

    // Track the number of times the max bet ($200) is reached.
    int maxBetReachedCount = 0;

    // For profit threshold prompting.
    double nextProfitThreshold = startingBankroll;

    // Determine play mode.
    int autoSpinChoice = ui.getAutoSpinCount();
    bool continuousMode = (autoSpinChoice == -1);

    // Betting settings.
    double currentBet = 1.0;
    Color betColor = Color::BLACK; // start betting on black.
    int consecutiveLosses = 0;
    int consecutiveWins = 0;
    bool continuePlaying = true;

    // Main game loop.
    while (bankroll > 0 && !timer.isTimeUp() && continuePlaying) {
        if (continuousMode) {
            // Continuous mode: run one spin per iteration without prompting.
            RouletteOutcome outcome = wheel.spin();
            std::cout << "Roulette Outcome: " << numberToString(outcome.number)
                << " (" << colorToString(outcome.color)
                << ", " << parityToString(outcome.parity) << ")\n";
            stats.addOutcomeToHistory(outcome);

            double extraResult = extraBetMode.isEnabled() ? extraBetMode.processOutcome(outcome.number) : 0.0;
            double totalWager = currentBet + (extraBetMode.isEnabled() ? extraBetMode.extraBetAmount() : 0.0);

            if (outcome.color == betColor) { // main bet wins (red/black win)
                double netChange = currentBet + extraResult;
                bankroll += netChange;
                std::cout << "You WIN on your main bet! Gained $" << currentBet;
                if (extraBetMode.isEnabled())
                    std::cout << " but extra bets lose $2";
                std::cout << ".\n";
                stats.recordWin(currentBet);
                consecutiveLosses = 0;
                consecutiveWins++;
                if (useWinMultipliers) {
                    double newBet = currentBet * winStrategy.getMultiplier(consecutiveWins);
                    if (newBet >= 200.0) {
                        newBet = 200.0;
                        maxBetReachedCount++;
                    }
                    currentBet = newBet;
                }
                else {
                    currentBet = 1.0;
                    consecutiveWins = 0;
                }
            }
            else { // main bet loses
                if (outcome.number == 0 || outcome.number == 37) { // outcome is green
                    double netChange = -currentBet + extraResult;
                    bankroll += netChange;
                    std::cout << "Outcome is green (" << numberToString(outcome.number)
                        << "). Main bet lost $" << currentBet << ", extra bets net $" << netChange << ".\n";
                    stats.recordLoss(currentBet);
                    consecutiveLosses++;
                    consecutiveWins = 0;
                    double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                    double newBet = totalWager * multiplier;
                    if (newBet >= 200.0) {
                        newBet = 200.0;
                        maxBetReachedCount++;
                    }
                    currentBet = newBet;
                }
                else { // opposite color: full loss.
                    double netLoss = totalWager;
                    bankroll -= netLoss;
                    std::cout << "You lose. Lost $" << netLoss << " (main bet plus extra bets).\n";
                    stats.recordLoss(currentBet);
                    consecutiveLosses++;
                    consecutiveWins = 0;
                    double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                    double newBet = totalWager * multiplier;
                    if (newBet >= 200.0) {
                        newBet = 200.0;
                        maxBetReachedCount++;
                    }
                    currentBet = newBet;
                }
            }

            stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);
            timer.addSpin(); // simulate delay

            // In continuous mode, skip profit threshold prompt.
        }
        else {
            // Non-continuous modes: prompt for auto-play count or manual spins.
            int autoSpinCount = ui.getAutoSpinCount();
            if (autoSpinCount > 0) {
                for (int i = 0; i < autoSpinCount && bankroll > 0 && !timer.isTimeUp(); ++i) {
                    RouletteOutcome outcome = wheel.spin();
                    std::cout << "Roulette Outcome: " << numberToString(outcome.number)
                        << " (" << colorToString(outcome.color)
                        << ", " << parityToString(outcome.parity) << ")\n";
                    stats.addOutcomeToHistory(outcome);

                    double extraResult = extraBetMode.isEnabled() ? extraBetMode.processOutcome(outcome.number) : 0.0;
                    double totalWager = currentBet + (extraBetMode.isEnabled() ? extraBetMode.extraBetAmount() : 0.0);

                    if (outcome.color == betColor) {
                        double netChange = currentBet + extraResult;
                        bankroll += netChange;
                        std::cout << "You WIN on your main bet! Gained $" << currentBet;
                        if (extraBetMode.isEnabled())
                            std::cout << " but extra bets lose $2";
                        std::cout << ".\n";
                        stats.recordWin(currentBet);
                        consecutiveLosses = 0;
                        consecutiveWins++;
                        if (useWinMultipliers) {
                            double newBet = currentBet * winStrategy.getMultiplier(consecutiveWins);
                            if (newBet >= 200.0) {
                                newBet = 200.0;
                                maxBetReachedCount++;
                            }
                            currentBet = newBet;
                        }
                        else {
                            currentBet = 1.0;
                            consecutiveWins = 0;
                        }
                    }
                    else {
                        if (outcome.number == 0 || outcome.number == 37) {
                            double netChange = -currentBet + extraResult;
                            bankroll += netChange;
                            std::cout << "Outcome is green (" << numberToString(outcome.number)
                                << "). Main bet lost $" << currentBet << ", extra bets net $" << netChange << ".\n";
                            stats.recordLoss(currentBet);
                            consecutiveLosses++;
                            consecutiveWins = 0;
                            double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                            double newBet = totalWager * multiplier;
                            if (newBet >= 200.0) {
                                newBet = 200.0;
                                maxBetReachedCount++;
                            }
                            currentBet = newBet;
                        }
                        else {
                            double netLoss = totalWager;
                            bankroll -= netLoss;
                            std::cout << "You lose. Lost $" << netLoss << " (main bet plus extra bets).\n";
                            stats.recordLoss(currentBet);
                            consecutiveLosses++;
                            consecutiveWins = 0;
                            double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                            double newBet = totalWager * multiplier;
                            if (newBet >= 200.0) {
                                newBet = 200.0;
                                maxBetReachedCount++;
                            }
                            currentBet = newBet;
                        }
                    }

                    stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);
                    timer.addSpin(); // simulate delay

                    // Check profit threshold prompt.
                    if (bankroll > startingBankroll && (bankroll - startingBankroll >= nextProfitThreshold)) {
                        std::cout << "Your winnings are $" << (bankroll - startingBankroll)
                            << " (at least an increase of $" << nextProfitThreshold << " over your starting bankroll).\n";
                        std::cout << "Do you want to continue playing? (y/n): ";
                        char choice;
                        std::cin >> choice;
                        if (choice != 'y' && choice != 'Y') {
                            continuePlaying = false;
                            break;
                        }
                        else {
                            nextProfitThreshold += startingBankroll;
                        }
                    }
                }
            }
            else { // Manual mode (autoSpinCount == 0)
                std::cout << "Press Enter to spin the wheel...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cin.get();

                RouletteOutcome outcome = wheel.spin();
                std::cout << "Roulette Outcome: " << numberToString(outcome.number)
                    << " (" << colorToString(outcome.color)
                    << ", " << parityToString(outcome.parity) << ")\n";
                stats.addOutcomeToHistory(outcome);

                double extraResult = extraBetMode.isEnabled() ? extraBetMode.processOutcome(outcome.number) : 0.0;
                double totalWager = currentBet + (extraBetMode.isEnabled() ? extraBetMode.extraBetAmount() : 0.0);

                if (outcome.color == betColor) {
                    double netChange = currentBet + extraResult;
                    bankroll += netChange;
                    std::cout << "You WIN on your main bet! Gained $" << currentBet;
                    if (extraBetMode.isEnabled())
                        std::cout << " but extra bets lose $2";
                    std::cout << ".\n";
                    stats.recordWin(currentBet);
                    consecutiveLosses = 0;
                    consecutiveWins++;
                    if (useWinMultipliers) {
                        double newBet = currentBet * winStrategy.getMultiplier(consecutiveWins);
                        if (newBet >= 200.0) {
                            newBet = 200.0;
                            maxBetReachedCount++;
                        }
                        currentBet = newBet;
                    }
                    else {
                        currentBet = 1.0;
                        consecutiveWins = 0;
                    }
                }
                else {
                    if (outcome.number == 0 || outcome.number == 37) {
                        double netChange = -currentBet + extraResult;
                        bankroll += netChange;
                        std::cout << "Outcome is green (" << numberToString(outcome.number)
                            << "). Main bet lost $" << currentBet << ", extra bets net $" << netChange << ".\n";
                        stats.recordLoss(currentBet);
                        consecutiveLosses++;
                        consecutiveWins = 0;
                        double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                        double newBet = totalWager * multiplier;
                        if (newBet >= 200.0) {
                            newBet = 200.0;
                            maxBetReachedCount++;
                        }
                        currentBet = newBet;
                    }
                    else {
                        double netLoss = totalWager;
                        bankroll -= netLoss;
                        std::cout << "You lose. Lost $" << netLoss << " (main bet plus extra bets).\n";
                        stats.recordLoss(currentBet);
                        consecutiveLosses++;
                        consecutiveWins = 0;
                        double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                        double newBet = totalWager * multiplier;
                        if (newBet >= 200.0) {
                            newBet = 200.0;
                            maxBetReachedCount++;
                        }
                        currentBet = newBet;
                    }
                }

                stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);
                timer.addSpin(); // simulate delay

                if (bankroll > startingBankroll && (bankroll - startingBankroll >= nextProfitThreshold)) {
                    std::cout << "Your winnings are $" << (bankroll - startingBankroll)
                        << " (at least an increase of $" << nextProfitThreshold << " over your starting bankroll).\n";
                    std::cout << "Do you want to continue playing? (y/n): ";
                    char choice;
                    std::cin >> choice;
                    if (choice != 'y' && choice != 'Y') {
                        break;
                    }
                    else {
                        nextProfitThreshold += startingBankroll;
                    }
                }
            }
        }
    }

    std::cout << "\n=== Final Stats ===\n";
    stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);
    std::cout << "Total simulated play time: " << timer.getElapsedSeconds() << " seconds ("
        << timer.getElapsedSeconds() / 3600.0 << " hours)\n";
    std::cout << "Starting Bankroll: $" << startingBankroll << "\n";
    std::cout << "Final Bankroll:    $" << bankroll << "\n";
    double net = bankroll - startingBankroll;
    if (net >= 0)
        std::cout << "Net Profit:        $" << net << "\n";
    else
        std::cout << "Net Loss:          $" << -net << "\n";
    std::cout << "Max Bet Reached:   " << maxBetReachedCount << " times\n";

    std::cout << "Thank you for playing!\n";
    return 0;
}
