/* Copyright 2011 ZAO "Begun".
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LIZARD_LOGGER_HPP__
#define __LIZARD_LOGGER_HPP__

#include <utils/error.hpp>
#include <string>

namespace lizard
{

enum log_level
{
    ACCESS,
    DEBUG,
    INFO,
    NOTICE,
    WARN,
    ERROR,
    CRIT,
    LOG_OFF
};

log_level str2lv(const std::string& aName);

class Logger
{
    int fd;
    log_level   m_Level;
    const char* m_SelfName;
    bool        m_PrintSelfName;
    std::string m_Filename;

    void initFrom(const Logger& aLogger, bool aForce = false);

    friend Logger& getLog(const std::string& aCatName);
    friend void setLogFile(const std::string& aCatName, const std::string& aFileName);
    friend void setLogLevel(const std::string& aCatName, log_level aLevel);
    friend void setLogPrintSelfName(const std::string& aCatName, bool aPrint);
    friend std::string getRegisteredLoggersVerbose();
    friend void initChildLoggers(Logger& aParent);

public:
    Logger() : fd(-1), m_Level(LOG_OFF), m_SelfName((const char *)0), m_PrintSelfName(true) {}

    log_level logLevel() const { return m_Level; }

    void log(log_level lv, const char* fmt, va_list ap);
    void log(log_level lv, const char* fmt, ...) LZ_FORMAT(printf, 3, 4);
    void access(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);
    void  debug(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);
    void   info(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);
    void notice(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);
    void   warn(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);
    void  error(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);
    void   crit(const char* fmt, ...) LZ_FORMAT(printf, 2, 3);

    void rotate();
};

Logger& getLog(const std::string& aName);

void setLogFile(const std::string& aCatName, const std::string& aFileName);
void setLogLevel(const std::string& aCatName, log_level aLevel);
void setLog(const std::string& aCatName, const std::string& aFileName, log_level aLevel);
template <class CFG>
void setLog(const std::string& aCatName, const CFG& cfg)
{
    setLog(aCatName, cfg.log_file_name, str2lv(cfg.log_level));
}
// Config format: file=<filename1>,level=<level1>,log=<log1>,inherit=false,log=<log2>,level=<level2>,log=<log3>,file=<filename2>,log=<log4>...
void configureLogger(const std::string& aConfigStr);
void setLogPrintSelfName(const std::string& aCatName, bool aPrint);

void rotateLogs();

void setInitChildLoggers(bool aInit);
bool getInitChildLoggers();
void initChildLoggers(Logger& aParent);

template <class CFG>
void configureLoggerFromConfig(const CFG& cfg)
{
    if (false == cfg.access_log_file_name.empty())
    {
        setLog("access", cfg.access_log_file_name, ACCESS);
        setLogPrintSelfName("access", false);
    }
    if (false == cfg.log_config_str.empty())
    {
        const std::string log_config_str = "file=" + cfg.log_file_name + ",level=" + cfg.log_level + "," + cfg.log_config_str;
        configureLogger(log_config_str);
    }
}

}// lizard

#endif
