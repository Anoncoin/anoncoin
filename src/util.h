// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_UTIL_H
#define ANONCOIN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "compat.h"
#include "serialize.h"
#include "sync.h"
#include "tinyformat.h"

#include <cstdio>
#include <exception>
#include <map>
#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#ifdef ENABLE_STEALTH
#include <openssl/rand.h>
#include "random.h"
#endif

#ifndef WIN32
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>

extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fDebug;
extern bool fPrintToConsole;
extern bool fPrintToDebugLog;
extern bool fServer;
extern std::string strMiscWarning;
extern bool fLogTimestamps;
extern bool fLogIPs;        // New v10 param
extern bool fLogI2Ps;       // Anoncoin specific
extern volatile bool fReopenDebugLog;

void SetupEnvironment();

/* Return true if log accepts specified category */
bool LogAcceptCategory(const char* category);
/* Send a string to the log output */
int LogPrintStr(const std::string &str);

class uint256;

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

//! This value is used all over the place, sometimes stored in data space for no good reason &
//! should have just been DEFINED for the compiler to place in code space as a constant
#define SECONDSPERDAY (60 * 60 * 24)

// Align by increasing pointer, must have extra space at end of buffer
template <size_t nBytes, typename T>
T* alignup(T* p)
{
    union
    {
        T* ptr;
        size_t n;
    } u;
    u.ptr = p;
    u.n = (u.n + (nBytes-1)) & ~(nBytes-1);
    return u.ptr;
}

#ifdef WIN32
#define MSG_DONTWAIT        0

#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#else
#define MAX_PATH            1024
#endif // WIN32
// As Solaris does not have the MSG_NOSIGNAL flag for send(2) syscall, it is defined as 0
#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

inline void MilliSleep(int64_t n)
{
// Boost's sleep_for was uninterruptable when backed by nanosleep from 1.50
// until fixed in 1.52. Use the deprecated sleep method for the broken case.
// See: https://svn.boost.org/trac/boost/ticket/7238
#if defined(HAVE_WORKING_BOOST_SLEEP_FOR)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#elif defined(HAVE_WORKING_BOOST_SLEEP)
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
#else
//should never get here
#error missing boost sleep implementation
#endif
}

extern const signed char p_util_hexdigit[];
inline signed char HexDigit(char c)
{
    return p_util_hexdigit[(unsigned char)c];
}

#define strprintf tfm::format
#define LogPrintf(...) LogPrint(NULL, __VA_ARGS__)

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_ERROR_AND_LOG_FUNC(n)                                        \
    /*   Print to debug.log if -debug=category switch is given OR category is NULL. */ \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline int LogPrint(const char* category, const char* format, TINYFORMAT_VARARGS(n))  \
    {                                                                         \
        if(!LogAcceptCategory(category)) return 0;                            \
        return LogPrintStr(tfm::format(format, TINYFORMAT_PASSARGS(n))); \
    }                                                                         \
    /*   Log error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool error(const char* format, TINYFORMAT_VARARGS(n))                     \
    {                                                                         \
        LogPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n"); \
        return false;                                                         \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_ERROR_AND_LOG_FUNC)

/* Zero-arg versions of logging and error, these are not covered by
 * TINYFORMAT_FOREACH_ARGNUM
 */
static inline int LogPrint(const char* category, const char* format)
{
    if(!LogAcceptCategory(category)) return 0;
    return LogPrintStr(format);
}
static inline bool error(const char* format)
{
    LogPrintStr(std::string("ERROR: ") + format + "\n");
    return false;
}


void LogException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(const std::exception* pex, const char* pszThread);
std::string SanitizeString(const std::string& str);
std::vector<unsigned char> ParseHex(const char* psz);
std::vector<unsigned char> ParseHex(const std::string& str);
bool IsHex(const std::string& str);
std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid = NULL);
std::string DecodeBase64(const std::string& str);
std::string EncodeBase64(const unsigned char* pch, size_t len);
std::string EncodeBase64(const std::string& str);
std::vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid = NULL);
std::string DecodeBase32(const std::string& str);
std::string EncodeBase32(const unsigned char* pch, size_t len);
std::string EncodeBase32(const std::string& str);
void ParseParameters(int argc, const char*const argv[]);
bool WildcardMatch(const char* psz, const char* mask);
bool WildcardMatch(const std::string& str, const std::string& mask);
void FileCommit(FILE *fileout);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest);
bool TryCreateDirectory(const boost::filesystem::path& p);
boost::filesystem::path GetDefaultDataDir();
const boost::filesystem::path &GetDataDir(bool fNetSpecific = true);
void ClearDatadirCache();
boost::filesystem::path GetConfigFile();
boost::filesystem::path GetQtStyleFile();
#ifndef WIN32
boost::filesystem::path GetPidFile();
void CreatePidFile(const boost::filesystem::path &path, pid_t pid);
#endif
void ReadConfigFile(std::map<std::string, std::string>& mapSettingsRet, std::map<std::string, std::vector<std::string> >& mapMultiSettingsRet);
#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
boost::filesystem::path GetTempPath();
void ShrinkDebugFile();
int64_t GetTime();
void SetMockTime(int64_t nMockTimeIn);
void runCommand(std::string strCommand);


inline std::string i64tostr(int64_t n)
{
    return strprintf("%d", n);
}

inline std::string itostr(int n)
{
    return strprintf("%d", n);
}

inline int64_t atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

inline int64_t atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

inline int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}

template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return rv;
}

template<typename T>
inline std::string HexStr(const T& vch, bool fSpaces=false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

template<typename T>
void PrintHex(const T pbegin, const T pend, const char* pszFormat="%s", bool fSpaces=true)
{
    LogPrintf(pszFormat, HexStr(pbegin, pend, fSpaces).c_str());
}

inline void PrintHex(const std::vector<unsigned char>& vch, const char* pszFormat="%s", bool fSpaces=true)
{
    LogPrintf(pszFormat, HexStr(vch, fSpaces).c_str());
}

inline int64_t GetTimeMillis()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
}

inline int64_t GetTimeMicros()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime);

template<typename T>
void skipspaces(T& it)
{
    while (isspace(*it))
        ++it;
}

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

/** \brief
 *
 * \param strArg const std::string&
 * \param strValue const std::string&
 * \return void
 *
 */
void HardSetArg(const std::string& strArg, const std::string& strValue);

/** \brief HardSet functions will guarantee that the parameter name exists and is the value required
 *
 * \param strArg const std::string& the parameter string, should always start with a '-'something
 * \param fValue bool the new value this parameter flag is to be set to.
 * \return none
 *
 */
void HardSetBoolArg(const std::string& strArg, bool fValue);

/**
 * Timing-attack-resistant comparison.
 * Takes time proportional to length
 * of first argument.
 */
template <typename T>
bool TimingResistantEqual(const T& a, const T& b)
{
    if (b.size() == 0) return a.size() == 0;
    size_t accumulator = a.size() ^ b.size();
    for (size_t i = 0; i < a.size(); i++)
        accumulator |= a[i] ^ b[i%b.size()];
    return accumulator == 0;
}


#ifdef WIN32
inline void SetThreadPriority(int nPriority)
{
    SetThreadPriority(GetCurrentThread(), nPriority);
}
#else

// PRIO_MAX is not defined on Solaris
#ifndef PRIO_MAX
#define PRIO_MAX 20
#endif
#define THREAD_PRIORITY_LOWEST          PRIO_MAX
#define THREAD_PRIORITY_BELOW_NORMAL    2
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_ABOVE_NORMAL    (-2)

inline void SetThreadPriority(int nPriority)
{
    // It's unclear if it's even possible to change thread priorities on Linux,
    // but we really and truly need it for the generation threads.
#ifdef PRIO_THREAD
    setpriority(PRIO_THREAD, 0, nPriority);
#else
    setpriority(PRIO_PROCESS, 0, nPriority);
#endif
}
#endif

void RenameThread(const char* name);

inline uint32_t ByteReverse(uint32_t value)
{
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return (value<<16) | (value>>16);
}

// Standard wrapper for do-something-forever thread functions.
// "Forever" really means until the thread is interrupted.
// Use it like:
//   new boost::thread(boost::bind(&LoopForever<void (*)()>, "dumpaddr", &DumpAddresses, 900000));
// or maybe:
//    boost::function<void()> f = boost::bind(&FunctionWithArg, argument);
//    threadGroup.create_thread(boost::bind(&LoopForever<boost::function<void()> >, "nothing", f, milliseconds));
template <typename Callable> void LoopForever(const char* name,  Callable func, int64_t msecs)
{
    std::string s = strprintf("anoncoin-%s", name);
    RenameThread(s.c_str());
    LogPrintf("%s thread start\n", name);
    try
    {
        while (1)
        {
            MilliSleep(msecs);
            func();
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("%s thread stop\n", name);
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(NULL, name);
        throw;
    }
}
// .. and a wrapper that just calls func once
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    std::string s = strprintf("anoncoin-%s", name);
    RenameThread(s.c_str());
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(NULL, name);
        throw;
    }
}

/**
 * Convert string to signed 32-bit integer with strict parse error feedback.
 * @returns true if the entire string could be parsed as valid integer,
 *   false if not the entire string could be parsed or when overflow or underflow occurred.
 */
bool ParseInt32(const std::string& str, int32_t *out);

/**
 * Format a paragraph of text to a fixed width, adding spaces for
 * indentation to any added line.
 */
std::string FormatParagraph(const std::string in, size_t width=79, size_t indent=0);

#endif // ANONCOIN_UTIL_H
