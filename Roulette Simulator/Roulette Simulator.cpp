#include <iostream>
#include <random>
#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <limits>

// Enums for color and parity
enum class Color { RED, BLACK, GREEN };
enum class Parity { ODD, EVEN, NONE };

// Helper to convert number to string (handles "00")
std::string numberToString(int num) {
    return (num == 37) ? "00" : std::to_string(num);
}

// Class that encapsulates a high-quality random number generator
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

// Structure for a roulette outcome
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
            // Set red numbers (typical American roulette red numbers)
            static const std::vector<int> redNumbers = { 1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36 };
            if (std::find(redNumbers.begin(), redNumbers.end(), outcome) != redNumbers.end()) {
                result.color = Color::RED;
            }
            else {
                result.color = Color::BLACK;
            }
            result.parity = (outcome % 2 == 0) ? Parity::EVEN : Parity::ODD;
        }
        return result;
    }
private:
    RandomNumberGenerator rng;
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
    int lossThreshold = ui.getLossThreshold();

    RouletteWheel wheel;
    StatsTracker stats;

    // Betting settings
    double currentBet = 1.0;
    Color betColor = Color::BLACK; // start betting on black
    int consecutiveLosses = 0;

    while (bankroll > 0) {
        int autoSpinCount = ui.getAutoSpinCount();

        if (autoSpinCount > 0) {
            // Auto-play mode: perform autoSpinCount spins automatically.
            for (int i = 0; i < autoSpinCount && bankroll > 0; ++i) {
                RouletteOutcome outcome = wheel.spin();

                std::cout << "Roulette Outcome: " << numberToString(outcome.number)
                    << " (" << colorToString(outcome.color)
                    << ", " << parityToString(outcome.parity) << ")\n";

                stats.addOutcomeToHistory(outcome);

                if (outcome.color == betColor) {
                    bankroll += currentBet;
                    std::cout << "You WIN! Gained $" << currentBet << "\n";
                    stats.recordWin(currentBet);
                    if (currentBet == 200.0) {
                        if (!ui.askToContinue()) {
                            bankroll = 0; // exit outer loop
                            break;
                        }
                    }
                    // Reset bet to $1 after a win.
                    currentBet = 1.0;
                    consecutiveLosses = 0;
                }
                else {
                    bankroll -= currentBet;
                    std::cout << "You lose $" << currentBet << "\n";
                    stats.recordLoss(currentBet);
                    ++consecutiveLosses;

                    if (consecutiveLosses >= lossThreshold) {
                        betColor = (betColor == Color::BLACK) ? Color::RED : Color::BLACK;
                        std::cout << "Consecutive losses reached " << lossThreshold
                            << ". Switching bet color to " << colorToString(betColor)
                            << " and resetting bet to $1.\n";
                        currentBet = 1.0;
                        consecutiveLosses = 0;
                    }
                    else {
                        double newBet;
                        if (consecutiveLosses <= 3) {
                            newBet = currentBet * 3;
                        }
                        else {
                            newBet = currentBet * 2;
                        }
                        if (newBet > 200.0) {
                            newBet = 200.0;
                        }
                        currentBet = newBet;
                    }
                }

                stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);

                if (bankroll <= 0) {
                    std::cout << "Your bankroll is empty. Game over.\n";
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

            if (outcome.color == betColor) {
                bankroll += currentBet;
                std::cout << "You WIN! Gained $" << currentBet << "\n";
                stats.recordWin(currentBet);
                if (currentBet == 200.0) {
                    if (!ui.askToContinue()) {
                        break;
                    }
                }
                currentBet = 1.0;
                consecutiveLosses = 0;
            }
            else {
                bankroll -= currentBet;
                std::cout << "You lose $" << currentBet << "\n";
                stats.recordLoss(currentBet);
                ++consecutiveLosses;

                if (consecutiveLosses >= lossThreshold) {
                    betColor = (betColor == Color::BLACK) ? Color::RED : Color::BLACK;
                    std::cout << "Consecutive losses reached " << lossThreshold
                        << ". Switching bet color to " << colorToString(betColor)
                        << " and resetting bet to $1.\n";
                    currentBet = 1.0;
                    consecutiveLosses = 0;
                }
                else {
                    double newBet;
                    if (consecutiveLosses <= 3) {
                        newBet = currentBet * 3;
                    }
                    else {
                        newBet = currentBet * 2;
                    }
                    if (newBet > 200.0) {
                        newBet = 200.0;
                    }
                    currentBet = newBet;
                }
            }

            stats.printStats(bankroll, currentBet, consecutiveLosses, betColor);

            if (bankroll <= 0) {
                std::cout << "Your bankroll is empty. Game over.\n";
                break;
            }
        }
    }

    std::cout << "Thank you for playing!\n";
    return 0;
}
