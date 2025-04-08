
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>

/*
	DEBUG: Detailed information, typically of interest only when diagnosing problems.
	INFO: Confirmation that things are working as expected.
	WARN: An indication that something unexpected happened, or indicative of some problem in the near future (e.g., 'disk space low'). The software is still working as expected.
	ERROR: Due to a more serious problem, the software has not been able to perform some function.
	FATAL: A very severe error event that will presumably lead the application to abort.
*/

class Logger
{
	public:
		enum Level { DEBUG, INFO, WARN, ERROR, FATAL };


		static std::string intToString(int number);

		static void	setLevel(Level level);
		static void	setOutput(std::ostream& outputStream);
		static void	setFormat(const std::string& formatString);

		static void	init(Logger::Level logLevel, const std::string& logFilePath = "");
		static void cleanup();

		static void log(Level level, const std::string& message, const std::string& source);

	private:
		static Level			currentLevel;
		static std::ostream		*output;
		static std::string		format;
		static std::ofstream	logFile;
		static bool				isStandardOutput;
		

		static std::string levelToString(Level level);
		static std::string getCurrentTimeFormatted();
};

#endif