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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/thread/mutex.hpp>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <pthread.h>
#include <string>
#include <time.h>
#include <utils/logger.hpp>
#include <vector>

using namespace lizard;

namespace
{
    boost::mutex loggers_lock;
    typedef std::map<std::string, Logger> loggers_t;
    loggers_t loggers;

    struct lev_data
    {
        const char *data;
        int len;
    };

    lev_data log_level_name [] =
    {
        { "access" , sizeof("access") - 1 },
        { "debug"  , sizeof("debug" ) - 1 },
        { "info"   , sizeof("info"  ) - 1 },
        { "notice" , sizeof("notice") - 1 },
        { "warn"   , sizeof("warn"  ) - 1 },
        { "error"  , sizeof("error" ) - 1 },
        { "crit"   , sizeof("crit"  ) - 1 },
        { "off"    , sizeof("off"   ) - 1 }
    };

    bool init_child_loggers = true;

#define MS_FMT  "%fms"

    size_t strfstime(char* s, size_t max, const char* format, const struct timeval* tv)
    {
        const char* f = strstr(format, MS_FMT);

        struct tm nt;
        localtime_r(&tv->tv_sec, &nt);

        if (NULL != f)
        {
            const size_t MS_FMT_SIZE = sizeof(MS_FMT) - 1;
            /* Заменяем в формате наши кастомные вхождения */
            char* new_format = (char*)alloca(strlen(format) - MS_FMT_SIZE + 3 + 1);
            const size_t off = f - format;
            memcpy(new_format, format, off);
            snprintf(new_format + off, 4, "%03ld", tv->tv_usec / 1000);
            strcat(new_format + off, f + MS_FMT_SIZE);
            return strftime(s, max, new_format, &nt);
        }

        return strftime(s, max, format, &nt);
    }
}// anonymous-namespace

log_level lizard::str2lv(const std::string& aName)
{
    for (log_level lv = ACCESS; lv <= LOG_OFF; lv = log_level(lv + 1))
    {
        if (0 == strcasecmp(aName.c_str(), log_level_name[lv].data))
        {
            return lv;
        }
    }

    throw error("Unknown log level: %s", aName.c_str());
}

Logger& lizard::getLog(const std::string& aName)
{
    boost::mutex::scoped_lock l(loggers_lock);
    loggers_t::iterator i = loggers.find(aName);
    if (loggers.end() == i)
    {
        i = loggers.insert(std::make_pair(aName, Logger())).first;
        i->second.m_SelfName = i->first.c_str();
    }
    return i->second;
}

void lizard::setLogFile(const std::string& aCatName, const std::string& aFileName)
{
    int fd = ::open(aFileName.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (-1 == fd)
        throw error("Cannot open file '%s' for writing: %s (%d)", aFileName.c_str(), strerror(errno), errno);
    Logger& logger = getLog(aCatName);
    if (-1 == logger.fd)
    {
        logger.fd = fd;
    }
    else
    {
        if (-1 == ::dup2(fd, logger.fd))
        {
            ::close(fd);
            throw error("Cannot do dup2 on file '%s': %s (%d)", aFileName.c_str(), strerror(errno), errno);
        }
        if (0 != ::close(fd))
            throw error("Cannot close file '%s': %s (%d)", aFileName.c_str(), strerror(errno), errno);
    }

    logger.m_Filename = aFileName;

    if (true == init_child_loggers)
    {
        boost::mutex::scoped_lock l(loggers_lock);
        initChildLoggers(logger);
    }
}

void lizard::setLogLevel(const std::string& aCatName, log_level aLevel)
{
    Logger& logger = getLog(aCatName);
    logger.m_Level = aLevel;

    if (true == init_child_loggers)
    {
        boost::mutex::scoped_lock l(loggers_lock);
        initChildLoggers(logger);
    }
}

void lizard::setLog(const std::string& aCatName, const std::string& aFileName, log_level aLevel)
{
    setLogFile(aCatName, aFileName);
    setLogLevel(aCatName, aLevel);
}

void lizard::configureLogger(const std::string& aConfigStr)
{
    std::vector<std::string> sCommands;
    static const std::string sDelimeters("=,");
    boost::algorithm::split(sCommands, aConfigStr, boost::algorithm::is_any_of(sDelimeters), boost::algorithm::token_compress_on);
    if (0 != sCommands.size() % 2)
        throw error("Invalid configure line: count of keys doesn't match count of values: %s", aConfigStr.c_str());

    std::string sFilename, sLevel;
    for (std::vector<std::string>::const_iterator it = sCommands.begin(); it != sCommands.end(); ++it)
    {
        const std::string& sCommand = *it++;
        const std::string& sValue = *it;
        if ("log" == sCommand )
        {
            setLog(sValue.c_str(), sFilename.c_str(), str2lv(sLevel.c_str()));
        }
        else if ("level" == sCommand)
        {
            sLevel = sValue;
        }
        else if ("file" == sCommand)
        {
            sFilename = sValue;
        }
        else if ("inherit" == sCommand)
        {
            if ("true" == sValue)
                setInitChildLoggers(true);
            else if ("false" == sValue)
                setInitChildLoggers(false);
            else throw error("Unknown boolean constant in %s command: %s", sCommand.c_str(), sValue.c_str());
        }
        else
        {
            throw error("Unknown command in log config: %s", sCommand.c_str());
        }
    }
}

void lizard::setLogPrintSelfName(const std::string& aCatName, bool aPrint)
{
    getLog(aCatName).m_PrintSelfName = aPrint;
}

void lizard::rotateLogs()
{
    loggers_t sLocalLoggers;
    {
        boost::mutex::scoped_lock l(loggers_lock);
        sLocalLoggers = loggers;
    }
    for (loggers_t::iterator i = sLocalLoggers.begin(); i != sLocalLoggers.end(); ++i)
    {
        i->second.rotate();
    }
}

void lizard::setInitChildLoggers(bool aInit)
{
    init_child_loggers = aInit;
}

bool lizard::getInitChildLoggers()
{
    return init_child_loggers;
}

void lizard::initChildLoggers(Logger& aParent)
{
    const std::string sParentName = std::string(aParent.m_SelfName) + ".";
    for (loggers_t::iterator i = loggers.upper_bound(sParentName); loggers.end() != i; ++i)
    {
        const std::string& sCatName = i->first;
        if (0 == sCatName.compare(0, sParentName.size(), sParentName))
        {
            // Нашлось дитя
            Logger& child = i->second;
            child.initFrom(aParent, true);
        }
        else
            break;
    }
}

void Logger::initFrom(const Logger& aLogger, bool aForce)
{
    if (-1 == fd)
    {
        fd = ::dup(aLogger.fd);
        m_Filename = aLogger.m_Filename;
        m_PrintSelfName = aLogger.m_PrintSelfName;
    }
    else if (true == aForce)
    {
        m_Filename = aLogger.m_Filename;
        m_PrintSelfName = aLogger.m_PrintSelfName;
        rotate();
    }

    if (LOG_OFF == m_Level || true == aForce)
    {
        m_Level = aLogger.m_Level;
        m_PrintSelfName = aLogger.m_PrintSelfName;
    }
}

#define CHECK_RC \
{ \
    if (rc < 0) goto write_failed; \
    if (rc >= size) goto write_truncated; \
    buf += rc; size -= rc; \
}

void Logger::log(log_level lv, const char* fmt, va_list ap)
{
    // Выводим только уровни, большие или равные нашему
    if (lv < m_Level) return;
    if (-1 == fd) return;

    const size_t BUFSZ = 4096;

    // log
    char wb [BUFSZ], *buf = wb;
    int rc, size = BUFSZ;

    // time
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        rc = strfstime(buf, size, "%d.%m %H:%M:%S:%fms", &tv);
        if (0 == rc) goto write_failed;
        if (rc >= size) goto write_truncated;
        buf += rc; size -= rc;
    }

    // all logger's: level, pthread_self, ': '
    rc = snprintf(buf, size, " %*.s[%s] *%06lu: ", 6 - log_level_name[lv].len, "", log_level_name[lv].data, pthread_self() % 1000000);
    CHECK_RC;

    if (true == m_PrintSelfName)
    {
        rc = snprintf(buf, size, "%s: ", m_SelfName);
        CHECK_RC;
    }

    // user text
    {
        // putting in brackets for workaround gcc-4.1.2 error message (like when declaring variable in case statement)
        rc = vsnprintf(buf, size, fmt, ap);
        CHECK_RC;
    }

    // Все записалось, пропускаем секцию write_truncated
    goto write;

write_truncated:
    buf = wb + BUFSZ;   // Указывает на первый байт ЗА буфером
    size = 0;           // Доступно 0 байт для записи

write_failed:
write:
    // '\n' надо записать в конец независимо от того, поместились ли другие данные
    // size должен быть равным минимум 2, чтобы записать туда '\n', т.к. при size=1
    // запишется только нулевой символ.
    if (size >= 2)
    {
        *buf = '\n';
        size -= 1;
    }
    else
    {
        // В буфере осталось или 1, или 0 байт. В любом случае, мы записываем в последний
        // доступный байт буфера символ перевод строки, а size приравниваем нулю. Строка
        // получается без завершающего нулевого символа
        *(buf + size - 1) = '\n';
        size = 0;
    }

    /* write */
    write(fd, wb, BUFSZ - size);
}

void Logger::log(log_level lv, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(lv, fmt, ap);
}

void Logger::access(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(ACCESS, fmt, ap);
}

void Logger::debug(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(DEBUG, fmt, ap);
}

void Logger::info(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(INFO, fmt, ap);
}

void Logger::notice(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(NOTICE, fmt, ap);
}

void Logger::warn(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(WARN, fmt, ap);
}

void Logger::error(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(ERROR, fmt, ap);
}

void Logger::crit(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    log(CRIT, fmt, ap);
}

void Logger::rotate()
{
//        if (0 != lizard::pmkdir(logger.m_Filename, 0755)) return;
    const int new_fd = ::open(m_Filename.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (-1 == new_fd) return;
    if (-1 == ::dup2(new_fd, fd)) { ::close(new_fd); return; }
    if (0 != ::close(new_fd)) return;
}
