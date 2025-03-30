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

// StatsTracker class to keep track of wins, losses, bets, and history.
class StatsTracker {
public:
    StatsTracker()
        : wins(0), losses(0), totalSpins(0), totalMoneyBet(0.0)
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

    void addOutcomeToHistory(const RouletteOutcome& outcome) {
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
        std::cout << "=========================\n\n";
    }

private:
    int wins;
    int losses;
    int totalSpins;
    double totalMoneyBet;
    std::deque<std::string> history; // store up to last 10 outcomes

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

    // Ask user how many spins to auto bet for; 0 indicates manual play.
    int getAutoSpinCount() const {
        int count;
        std::cout << "Enter number of spins to auto bet (0 for manual mode): ";
        std::cin >> count;
        return count;
    }

    bool askToContinue() const {
        char choice;
        std::cout << "You have just won after reaching the max bet. Do you want to continue playing? (y/n): ";
        std::cin >> choice;
        return (choice == 'y' || choice == 'Y');
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
    // If left empty, the bet will reset to $1 after each win (old behavior).
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
};

// New class to simulate the casino spin delay and track elapsed play time.
class CasinoTimer {
public:
    CasinoTimer() : elapsedSeconds(0) {}

    // This method simulates the delay between spins.
    // We use a short actual sleep (100 ms) to simulate delay without waiting full real time.
    void addSpin() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsedSeconds += averageDelay;
    }

    // Returns true if the total simulated play time has reached 8 hours.
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
    // Store the initial bankroll to later compare gains/losses.
    double startingBankroll = bankroll;
    int lossThreshold = ui.getLossThreshold();

    // Get loss multipliers.
    std::vector<double> lossMultipliers = ui.getBettingStrategyMultipliers();
    BettingStrategy lossStrategy(lossMultipliers);

    // Get winning multipliers (if any).
    std::vector<double> winMultipliers = ui.getWinningStrategyMultipliers();
    // If empty, then we will revert to resetting bet to $1 after each win.
    bool useWinMultipliers = !winMultipliers.empty();
    BettingStrategy winStrategy(winMultipliers);

    RouletteWheel wheel;
    StatsTracker stats;
    CasinoTimer timer;

    // Betting settings.
    double currentBet = 1.0;
    Color betColor = Color::BLACK; // start betting on black.
    int consecutiveLosses = 0;
    int consecutiveWins = 0;

    while (bankroll > 0 && !timer.isTimeUp()) {
        int autoSpinCount = ui.getAutoSpinCount();

        if (autoSpinCount > 0) {
            // Auto-play mode: perform autoSpinCount spins automatically.
            for (int i = 0; i < autoSpinCount && bankroll > 0 && !timer.isTimeUp(); ++i) {
                RouletteOutcome outcome = wheel.spin();

                std::cout << "Roulette Outcome: " << numberToString(outcome.number)
                    << " (" << colorToString(outcome.color)
                    << ", " << parityToString(outcome.parity) << ")\n";

                stats.addOutcomeToHistory(outcome);

                if (outcome.color == betColor) { // WIN
                    bankroll += currentBet;
                    std::cout << "You WIN! Gained $" << currentBet << "\n";
                    stats.recordWin(currentBet);
                    // Reset loss counter.
                    consecutiveLosses = 0;
                    // Update win streak.
                    consecutiveWins++;
                    if (currentBet == 200.0) {
                        if (!ui.askToContinue()) {
                            bankroll = 0;
                            break;
                        }
                    }
                    if (useWinMultipliers) {
                        double newBet = currentBet * winStrategy.getMultiplier(consecutiveWins);
                        if (newBet > 200.0) newBet = 200.0;
                        currentBet = newBet;
                    }
                    else {
                        currentBet = 1.0;
                        consecutiveWins = 0;
                    }
                }
                else { // LOSS
                    bankroll -= currentBet;
                    std::cout << "You lose $" << currentBet << "\n";
                    stats.recordLoss(currentBet);
                    consecutiveLosses++;
                    // Reset win streak.
                    consecutiveWins = 0;

                    if (consecutiveLosses >= lossThreshold) {
                        betColor = (betColor == Color::BLACK) ? Color::RED : Color::BLACK;
                        std::cout << "Consecutive losses reached " << lossThreshold
                            << ". Switching bet color to " << colorToString(betColor)
                            << " and resetting bet to $1.\n";
                        currentBet = 1.0;
                        consecutiveLosses = 0;
                    }
                    else {
                        double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                        double newBet = currentBet * multiplier;
                        if (newBet > 200.0) {
                            newBet = 200.0;
                        }
                        currentBet = newBet;
                    }
                }

                stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);
                timer.addSpin(); // Simulate delay for this spin

                if (bankroll <= 0 || timer.isTimeUp()) {
                    break;
                }
            }
        }
        else {
            // Manual mode: wait for user input to spin.
            std::cout << "Press Enter to spin the wheel...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cin.get();

            RouletteOutcome outcome = wheel.spin();

            std::cout << "Roulette Outcome: " << numberToString(outcome.number)
                << " (" << colorToString(outcome.color)
                << ", " << parityToString(outcome.parity) << ")\n";

            stats.addOutcomeToHistory(outcome);

            if (outcome.color == betColor) { // WIN
                bankroll += currentBet;
                std::cout << "You WIN! Gained $" << currentBet << "\n";
                stats.recordWin(currentBet);
                consecutiveLosses = 0;
                consecutiveWins++;
                if (currentBet == 200.0) {
                    if (!ui.askToContinue()) {
                        break;
                    }
                }
                if (useWinMultipliers) {
                    double newBet = currentBet * winStrategy.getMultiplier(consecutiveWins);
                    if (newBet > 200.0) newBet = 200.0;
                    currentBet = newBet;
                }
                else {
                    currentBet = 1.0;
                    consecutiveWins = 0;
                }
            }
            else { // LOSS
                bankroll -= currentBet;
                std::cout << "You lose $" << currentBet << "\n";
                stats.recordLoss(currentBet);
                consecutiveLosses++;
                consecutiveWins = 0;
                if (consecutiveLosses >= lossThreshold) {
                    betColor = (betColor == Color::BLACK) ? Color::RED : Color::BLACK;
                    std::cout << "Consecutive losses reached " << lossThreshold
                        << ". Switching bet color to " << colorToString(betColor)
                        << " and resetting bet to $1.\n";
                    currentBet = 1.0;
                    consecutiveLosses = 0;
                }
                else {
                    double multiplier = lossStrategy.getMultiplier(consecutiveLosses);
                    double newBet = currentBet * multiplier;
                    if (newBet > 200.0) {
                        newBet = 200.0;
                    }
                    currentBet = newBet;
                }
            }

            stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);
            timer.addSpin(); // Simulate delay for this spin

            if (bankroll <= 0) {
                std::cout << "Your bankroll is empty. Game over.\n";
                break;
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

    std::cout << "Thank you for playing!\n";
    return 0;
}
