#include "Logger.hpp"

#include <fstream>
#include <iostream>

void Logger::Log(const std::string& message) {
    std::ofstream logFile("application.log", std::ios::app); // Otwórz plik w trybie dołączania
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    } else {
        std::cerr << "Failed to open log file" << std::endl;
    }
}