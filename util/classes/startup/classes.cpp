#include "../classes.h"
#include <Windows.h>
#include <vector>
// #include "../../protection/hider.h"
#include <thread>
#include <chrono>
#include "../../globals.h"
#include "../../../drawing/overlay/overlay.h"
#include "../../../features/macros.h"

inline class Logger {
public:
    static void GetTime(char(&timeBuffer)[9])
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm localTime;
        localtime_s(&localTime, &now_time);
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);
    }

    static void Log(const std::string& message) {
        char timeBuffer[9];
        GetTime(timeBuffer);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "[" << timeBuffer << "] ";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << message << "\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void CustomLog(int color, const std::string& start, const std::string& message) {
        char timeBuffer[9];
        GetTime(timeBuffer);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, color | FOREGROUND_INTENSITY);
        std::cout << "[" << start << "] ";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << message << "\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Logf(const char* format, ...) {
        char buffer[1024];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Log(std::string(buffer));
    }

    static void LogfCustom(int color, const char* start, const char* format, ...) {
        char buffer[1024];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        CustomLog(color, start, std::string(buffer));
    }

    static void Banner() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "[ AKVARIUM SOLUTIONS ]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Separator() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "================================================================================\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Section(const std::string& title) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "\n[== " << title << " ==============================================================]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Success(const std::string& message) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "[== " << message << " COMPLETE ==]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

}logger;

bool engine::startup() {
    system("cls");
    FreeConsole();
    logger.Banner();

    logger.Section("CORE INITIALIZATION");
    logger.CustomLog(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "ENGINE", "Starting Core Systems...");

    logger.Section("THREAD INITIALIZATION");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "OVERLAY", "Launching overlay interface...");
    std::thread overlay(overlay::load_interface);
    overlay.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Overlay thread launched");


    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "MACROS", "Launching double click thread...");
    std::thread DClick(macros::doubleClick);
    DClick.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Double click thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "MACROS", "Launching attribute swap thread...");
    std::thread attSwap(macros::attributeSwap);
    attSwap.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Attribute swap thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "MACROS", "Launching spear swap thread...");
    std::thread spear(macros::spearSwap);
    spear.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Spear swap thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "MACROS", "Launching panic button thread...");
    std::thread panic(macros::panicButtonHandler);
    panic.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Panic button thread launched");


    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "RUNTIME", "Engine running... Press ENTER to terminate");
    logger.Separator();
    try {
        while (true) {
            if (globals::unattach) {
                logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "Unattach signal received");
                std::abort();
                break;
            }

            if (std::cin.peek() != EOF) {
                std::cin.clear();
                char c;
                if (std::cin.get(c)) {
                    if (c == '\n' || c == '\r') {
                        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "User requested termination");
                        std::abort();
                        break;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Exception in main loop");
    }

    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "Initiating cleanup sequence...");
    globals::unattach = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SHUTDOWN", "Engine terminated successfully");

    return true;
}