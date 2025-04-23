// ============================================================================
//  main.cpp – Roulette simulator with 720 × 480-pixel console window (updated)
//  Build:  C++20, Visual Studio 2022 (x64, Windows)
// ============================================================================

// ----- Standard C++ headers -------------------------------------------------
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
#include <cmath>          // for std::ceil
#include <stdexcept>      // for std::runtime_error
#include <tuple>          // for std::tie

// ----- Win32 headers (console window control) -------------------------------
#ifdef _WIN32
#include <windows.h>
#undef max
#undef min
#endif

// ============================================================================
//  ConsoleControl – cursor-safe, enlarge-only buffer, 1080 p ready
// ============================================================================
class ConsoleControl { // Console window control class
public:
    static void setWindowSize(int widthPx, int heightPx,
        int x = CW_USEDEFAULT, int y = CW_USEDEFAULT)
    {
#ifdef _WIN32 // Windows only
		HWND   hWnd = ::GetConsoleWindow(); // Get console window handle
		HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE); // Get console output handle
        if (!hWnd || hOut == INVALID_HANDLE_VALUE)
            throw std::runtime_error("No console window/handle");

		::SetConsoleCursorPosition(hOut, { 0, 0 }); // Move cursor to top-left

		CONSOLE_FONT_INFO font{}; // Get current font info
        if (!::GetCurrentConsoleFont(hOut, FALSE, &font))
            throw std::runtime_error("GetCurrentConsoleFont failed");

		// Calculate target columns and rows based on font size
        const SHORT targetCols = static_cast<SHORT>(
            std::ceil(widthPx / static_cast<float>(font.dwFontSize.X)));
        const SHORT targetRows = static_cast<SHORT>(
            std::ceil(heightPx / static_cast<float>(font.dwFontSize.Y)));

		// Set the console buffer size to the target size
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!::GetConsoleScreenBufferInfo(hOut, &csbi))
            throw std::runtime_error("GetConsoleScreenBufferInfo failed");

		// Adjust the buffer size if necessary
        COORD newBuf = { std::max(targetCols, csbi.dwSize.X),
                         std::max(targetRows, csbi.dwSize.Y) };

		// Set the buffer size to the new size
        if (newBuf.X != csbi.dwSize.X || newBuf.Y != csbi.dwSize.Y) {
            if (!::SetConsoleScreenBufferSize(hOut, newBuf))
                throw std::runtime_error("SetConsoleScreenBufferSize failed");
        }

		// Set the console window size to the target size
        SMALL_RECT newWin{ 0, 0,
                            static_cast<SHORT>(targetCols - 1),
                            static_cast<SHORT>(targetRows - 1) };
        if (!::SetConsoleWindowInfo(hOut, TRUE, &newWin))
            throw std::runtime_error("SetConsoleWindowInfo failed");

		// Set the console font size to the target size
        DWORD style = ::GetWindowLong(hWnd, GWL_STYLE);
        DWORD exStyle = ::GetWindowLong(hWnd, GWL_EXSTYLE);

		// Adjust the window rectangle to fit the new size
        RECT rc{ 0, 0, widthPx, heightPx };
        if (!::AdjustWindowRectEx(&rc, style, FALSE, exStyle))
            throw std::runtime_error("AdjustWindowRectEx failed");

		// Move the window to the specified position
        if (!::MoveWindow(hWnd, x, y,
            rc.right - rc.left,
            rc.bottom - rc.top, TRUE))
            throw std::runtime_error("MoveWindow failed");
#else
		(void)widthPx; (void)heightPx; (void)x; (void)y; // No-op for non-Windows
#endif
    }
};

// ============================================================================
//  Enums, Helpers, and Classes (unchanged)
// ============================================================================
enum class Color { RED, BLACK, GREEN };
enum class Parity { ODD, EVEN, NONE };
enum class PlayMode { MANUAL, AUTOPLAY, CONTINUOUS };

std::string numberToString(int num) { // Convert number to string
    return (num == 37) ? "00" : std::to_string(num);
}
std::string colorToString(Color c) { // Convert color to string
    switch (c) { case Color::RED: return "Red"; case Color::BLACK: return "Black"; case Color::GREEN: return "Green"; }
                                return "Unknown";
}
std::string parityToString(Parity p) { // Convert parity to string
    switch (p) { case Parity::ODD: return "Odd"; case Parity::EVEN: return "Even"; case Parity::NONE: return "None"; }
                                 return "Unknown";
}

class RandomNumberGenerator { // Random number generator class
public:
    RandomNumberGenerator() : rng(std::random_device{}()) {}
    int getRandomNumber(int min, int max) { std::uniform_int_distribution<int> dist(min, max); return dist(rng); }
private:
    std::mt19937 rng;
};

struct RouletteOutcome { // Roulette outcome structure
    int number;
    Color color;
    Parity parity;
};

class RouletteWheel { // Roulette wheel class
public:
    RouletteWheel() : rng() {}
    RouletteOutcome spin() {
        int outcome = rng.getRandomNumber(0, 37);
        RouletteOutcome res{ outcome, Color::GREEN, Parity::NONE };
        if (outcome > 0 && outcome < 37) {
            static const std::vector<int> redNums = { 1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36 };
            res.color = (std::find(redNums.begin(), redNums.end(), outcome) != redNums.end()) ? Color::RED : Color::BLACK;
            res.parity = (outcome % 2 == 0) ? Parity::EVEN : Parity::ODD;
        }
        return res;
    }
private:
    RandomNumberGenerator rng;
};

class ExtraBetMode { // Extra-bet mode class
public:
    explicit ExtraBetMode(bool en = false) : enabled(en) {}
    bool isEnabled() const { return enabled; }
    double extraBetAmount() const { return enabled ? 2.0 : 0.0; }
    double processOutcome(int outcome) const {
        if (!enabled) return 0.0;
        return (outcome == 0 || outcome == 37) ? 34.0 : -2.0;
    }
private:
    bool enabled;
};

class BettingStrategy { // Betting strategy class
public:
    explicit BettingStrategy(std::vector<double> m) : multipliers(std::move(m)) {
        if (multipliers.empty()) multipliers = { 3.0,3.0,2.0 };
    }
    double getMultiplier(int n) const {
        if (n <= static_cast<int>(multipliers.size())) return multipliers[n - 1];
        return multipliers.back();
    }
private:
    std::vector<double> multipliers;
};

class StatsTracker { // Stats tracker class
public:
    void recordWin(double b) { ++wins; ++spins; moneyBet += b; add("Win:  $" + std::to_string(b)); }
    void recordLoss(double b) { ++loss; ++spins; moneyBet += b; add("Loss: $" + std::to_string(b)); }
    void addOutcomeToHistory(const RouletteOutcome& o) {
        if (o.number == 0) ++count0;
        else if (o.number == 37) ++count00;
        add("Spin: " + numberToString(o.number) + " (" + colorToString(o.color) + ", " + parityToString(o.parity) + ")");
    }
	void print(double bankroll, double currBet, int consLoss, Color betColor) const { // Print stats
        std::cout << "\n===== Stats =====\n"
            << "Bankroll: $" << bankroll << "\n"
            << "Current Bet: $" << currBet << "\n"
            << "Consecutive Losses: " << consLoss << "\n"
            << "Betting on: " << colorToString(betColor) << "\n"
            << "Spins: " << spins << " Wins: " << wins << " Losses: " << loss << " Total Bet: $" << moneyBet << "\n"
            << "Recent (" << history.size() << "):\n";
        for (const auto& h : history) std::cout << "  " << h << "\n";
        std::cout << "Greens: " << (count0 + count00) << " (0: " << count0 << ", 00: " << count00 << ")\n"
            << "=================\n";
    }
private:
    int wins = 0, loss = 0, spins = 0;
    double moneyBet = 0;
    int count0 = 0, count00 = 0;
    std::deque<std::string> history;
    void add(const std::string& s) { if (history.size() >= 10) history.pop_front(); history.push_back(s); }
};

class UserInterface { // User interface class
public:
    template<typename T>
    T getValidated(const std::string& prompt) const {
        T v;
		while (true) { // Loop until valid input
            std::cout << prompt;
            if (std::cin >> v) { std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); return v; }
            std::cout << "Invalid input, try again.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
    double getInitialBankroll() const { return getValidated<double>("Enter your initial bankroll: $"); }
    int getLossThreshold() const { return getValidated<int>("Enter max consecutive losses before switching: "); }
	std::pair<PlayMode, int> getPlayMode() const { // Get play mode
        int m = getValidated<int>("Enter play mode (0=manual, -1=continuous, >0=auto spins): ");
        if (m == 0) return { PlayMode::MANUAL,0 };
        if (m == -1) return { PlayMode::CONTINUOUS,0 };
        return { PlayMode::AUTOPLAY,m };
    }
	std::vector<double> getMultipliers(const std::string& prompt, const std::string& emptyMsg) const { // Get multipliers
        std::cout << prompt;
        std::string line; std::getline(std::cin, line);
        std::istringstream iss(line);
        std::vector<double> m; double x;
        while (iss >> x) m.push_back(x);
        if (m.empty()) std::cout << emptyMsg << "\n";
        return m;
    }
	bool askExtraBet() const { // Ask for extra-bet mode
        char c;
        std::cout << "Enable Extra-Bet mode ($1 on 0 and 00)? (y/n): ";
        std::cin >> c; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return (c == 'y' || c == 'Y');
    }
};

class CasinoTimer { // Casino timer class
public:
    void addSpin() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); elapsed += 35; }
    bool isTimeUp() const { return elapsed >= 8 * 3600; }
    int seconds() const { return elapsed; }
private:
    int elapsed = 0;
};

// ============================================================================
//  main()
// ============================================================================
int main() { // Main function
	/*
    try { // Resize console window to 1920x1080
        ConsoleControl::setWindowSize(1920, 1080);
    }
	catch (const std::exception& ex) { // Handle console resize error
        std::cerr << "[Console resize failed] " << ex.what() << "\n";
    }
    */

    UserInterface ui;
    bool playAgain = false;
    bool hasExistingBankroll = false;
    const double maxBet = 10000.0;  // maximum bet cap
    double bankroll = 0.0, startingBankroll = 0.0;
    int lossThreshold = 0;

	do { // Main game loop
        // -- (Re)gather settings
		if (!hasExistingBankroll) { // Ask for initial bankroll
            bankroll = ui.getInitialBankroll();
        }
        startingBankroll = bankroll;
		lossThreshold = ui.getLossThreshold(); // Ask for loss threshold

        auto lossMult = ui.getMultipliers(
            "Enter loss multipliers (e.g. \"3 3 2\"): ",
            "No multipliers entered – using default 3 3 2.");
        auto winMult = ui.getMultipliers(
            "Enter win multipliers (empty = reset to $1): ",
            "No win multipliers – bet resets to $1 after a win.");

		ExtraBetMode extra(ui.askExtraBet()); // Ask for extra-bet mode
		auto [playMode, autoSpins] = ui.getPlayMode(); // Ask for play mode

		BettingStrategy lossStrat(lossMult); // Loss multipliers
		BettingStrategy winStrat(winMult); // Win multipliers
		bool useWinMult = !winMult.empty(); // Use win multipliers

		RouletteWheel wheel; // Roulette wheel
		StatsTracker stats; // Stats tracker
		CasinoTimer timer; // Casino timer

		double currentBet = 1.0; // Current bet amount
		Color betColor = Color::BLACK; // Initial bet color // Black // Add method to get color from user
        int consecutiveLosses = 0, consecutiveWins = 0, maxBetHits = 0;
        double nextProfitThresh = startingBankroll;
        bool keepPlaying = true;

        // Lambda for one spin
		auto spinOnce = [&]() -> bool { // Spin once and update bankroll
			if (playMode == PlayMode::MANUAL) { // Manual play
                std::cout << "Press <Enter> to spin...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
			auto spin_result = wheel.spin(); // Spin the wheel
            std::cout << "\nOutcome: " << numberToString(spin_result.number) 
                << " (" << colorToString(spin_result.color)
                << ", " << parityToString(spin_result.parity) << ")\n";
            stats.addOutcomeToHistory(spin_result); 

            double extraResult = extra.processOutcome(spin_result.number);
            double totalWager = currentBet + extra.extraBetAmount();

			if (spin_result.color == betColor) { // Win
                double net = currentBet + extraResult;
                bankroll += net;
                stats.recordWin(currentBet);
                std::cout << "You WIN! Net change: $" << net << "\n";
                ++consecutiveWins; consecutiveLosses = 0;
				if (useWinMult) { // Use win multipliers
                    double newBet = currentBet * winStrat.getMultiplier(consecutiveWins);
                    if (newBet >= maxBet) { newBet = maxBet; ++maxBetHits; }
                    currentBet = newBet;
                }
				else { // Reset to $1
                    currentBet = 1.0; consecutiveWins = 0;
                }
            }
			else { // Lose
                double net = -currentBet + extraResult;
                bankroll += net;
                stats.recordLoss(currentBet);
                std::cout << "You lose. Net change: $" << net << "\n";
                ++consecutiveLosses; consecutiveWins = 0;
                double newBet = currentBet * lossStrat.getMultiplier(consecutiveLosses);
                if (newBet >= maxBet) { newBet = maxBet; ++maxBetHits; }
                currentBet = newBet;
            }
			if (consecutiveLosses >= lossThreshold) { // Switch color
                betColor = (betColor == Color::BLACK) ? Color::RED : Color::BLACK;
                std::cout << "Reached " << lossThreshold << " losses – switching to "
                    << colorToString(betColor) << ".\n";
                consecutiveLosses = 0;
            }
            stats.print(bankroll, currentBet, consecutiveLosses, betColor);
            timer.addSpin();

            if (playMode != PlayMode::CONTINUOUS &&
				bankroll - startingBankroll >= nextProfitThresh) { // Check profit threshold
                std::cout << "You are up by $" << bankroll - startingBankroll << ". Continue? (y/n): ";
                char c; std::cin >> c; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (c != 'y' && c != 'Y') return false;
                nextProfitThresh += startingBankroll;
            }
            return true;
            };

        // -- Main game loop
        while (bankroll > 0 && !timer.isTimeUp() && keepPlaying && currentBet <= bankroll) {
			if (playMode == PlayMode::CONTINUOUS) { // Continuous play
                if (!spinOnce()) break;
            }
			else if (playMode == PlayMode::AUTOPLAY) { // Auto-play
                for (int i = 0; i < autoSpins && bankroll > 0 && !timer.isTimeUp(); ++i) {
                    if (!spinOnce()) { keepPlaying = false; break; }
                }
				if (keepPlaying) { // Ask to continue after auto-spins
                    std::cout << "Auto-spin block done. Continue? (y/n): ";
                    char c; std::cin >> c; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    if (c != 'y' && c != 'Y') break;
                }
            }
			else { // Manual play
                if (!spinOnce()) break;
            }
        }

        // -- Final summary
        std::cout << "\n=== Final Stats ===\n";
        stats.print(bankroll, currentBet, consecutiveLosses, betColor);
        std::cout << "Simulated play time: " << (timer.seconds() / 3600.0) << " hours\n"
            << "Starting bankroll: $" << startingBankroll << "\n"
            << "Final bankroll:   $" << bankroll << "\n"
            << "Net " << (bankroll >= startingBankroll ? "profit: $" : "loss: $")
            << std::abs(bankroll - startingBankroll) << "\n"
            << "Max-bet cap hit: " << maxBetHits << " times\n";

        // -- Game over options
        std::cout << "\nGame over. Choose an option:\n"
            << "  1) Reset and start over\n"
            << "  2) Restart with same bankroll and reenter questions\n"
            << "  3) Quit\n"
            << "Enter choice (1-3): ";
        int choice = ui.getValidated<int>("");
		if (choice == 1) { // Reset and start over
            hasExistingBankroll = false;
            playAgain = true;
        }
		else if (choice == 2) { // Restart with same bankroll
            hasExistingBankroll = true;
            playAgain = true;
        }
		else { // Quit
            playAgain = false;
        }
		system("cls");
    } while (playAgain);

    return 0;
}
