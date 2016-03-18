/*
 * i2pdata.cpp
 *
 *  Created on: Mar 11, 2016
 *      Author: ssuag
 */

#include "util.h"

#include "chainparamsbase.h"
#include "netbase.h"
#include "random.h"
#include "uint256.h"
#include "version.h"
#include "i2pmanager.h"

#include <stdarg.h>

#ifndef WIN32
// for posix_fallocate
#ifdef __linux_

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L
#include <sys/prctl.h>
#endif // __linux_

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else  // is WIN32

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif // _MSC_VER

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <io.h> /* for _commit */
#include <shlobj.h>
#endif // WIN32

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>

#include <openssl/crypto.h>
#include <openssl/rand.h>

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// See also: http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
namespace boost {
    namespace program_options {
        std::string to_internal(const std::string&);
    }
} // namespace boost

using namespace std;


I2PDataFile *pFile_I2P_Object;

//******************************************************************************
//
//    Name: I2PManager constructor
//
//    Parameters:   N/A
//
//    Description:
//      Allocates a new I2PDataFile object in memory
//
//    Return:       N/A
//
//******************************************************************************
void I2PManager::I2PManager(void)
{
    pFile_I2P_Object = new I2PDataFile();
    assert(pFile_I2P_Object);
    pFile_I2P_Object->initHeader();
}

//******************************************************************************
//
//    Name: I2PManager destructor
//
//    Parameters:   N/A
//
//    Description:
//      Destroys the current I2P object
//
//    Return:       N/A
//
//******************************************************************************
I2PManager::~I2PManager()
{
    delete pFile_I2P_Object;
}

//******************************************************************************
//
//    Name: I2PManager::getFileI2PPtr
//
//    Parameters:   N/A
//
//    Description:
//      Returns a pointer to the current I2P data file
//
//    Return:
//      const I2P_Data_File_t* - Ptr to data file
//
//******************************************************************************
const I2P_Data_File_t* I2PManager::getFileI2PPtr(void) const
{
    assert(pFile_I2P_Object);
    return pFile_I2P_Object;
}

//******************************************************************************
//
//    Name: I2PManager::GetI2PSettingsFilePath
//
//    Parameters:   N/A
//
//    Description:
//      Returns the path to the current I2P data file for usage when writing or
//      reading to the I2P settings data file
//
//    Return:
//      boost::filesystem::path
//
//******************************************************************************
boost::filesystem::path GetI2PSettingsFilePath(void)
{
    boost::filesystem::path pathConfigFile(GetArg("-conf", "i2p.dat"));
    if (!pathConfigFile.is_complete()) pathConfigFile = GetDataDir(false) / pathConfigFile;
    return pathConfigFile;
}

//******************************************************************************
//
//    Name: I2PManager::WriteToI2PSettingsFile
//
//    Parameters: void
//
//    Description:
//      Write I2P Data File settings to disk
//
//    Return:
//      Boolean - True if write was successful
//                False otherwise
//
//******************************************************************************
bool I2PManager::WriteToI2PSettingsFile(void)
{
    try {
        boost::filesystem::ofstream outputFile(GetI2PSettingsFilePath(), std::ios_base::out|std::ios_base::trunc|std::ios_base::binary);
        if (!outputFile.good())
        {
            LogPrintf("[I2P Data] Cannot open i2p data file for output!");
            return false;
        }

        outputFile << (*(this->getFileI2PPtr()));
        outputFile.close();

        return true;
    } catch (std::runtime_error &e) {
        cout << "Unable to write to i2p data file:" << e.what() << endl;
    }
    return false;
}

//******************************************************************************
//
//    Name: I2PManager::ReadI2PSettingsFile
//
//    Parameters: void
//
//    Description:
//      Read in the I2P settings file from disk into memory
//
//    Return:
//      Boolean - True if read was successful
//                False otherwise
//
//******************************************************************************
bool I2PManager::ReadI2PSettingsFile(void)
{
    try {
        I2P_Data_File_t localI2PObject;
        boost::filesystem::ifstream inputFile(GetI2PSettingsFilePath());
        if (!inputFile.good())
        {
            LogPrintf("[I2P Data] Cannot open i2p data file for input!");
            return false;
        }

        // Read file from disk into local object
        inputFile.read((char*)&localI2PObject, sizeof(I2P_Data_File_t));
        inputFile.close();

        // Copy into global for usage
        if (localI2PObject != NULL)
        {
            memcpy((void*)&localI2PObject, (void*)this->getFileI2PPtr(), sizeof(I2P_Data_File_t));
        }
        else
        {
            LogPrintf("[I2P Data] Unable to read i2p data file into memory.");
            return false;
        }
        return true;
    } catch (std::runtime_error &e) {
        cout << "[I2P Data] Unable to read i2p data file:" << e.what() << endl;
    }
    return false;
}

void I2PManager::UpdateMapArguments(void)
{
    mapArgs["-i2p.options.enabled"] = pFile_I2P_Object->getEnableStatus();
    mapArgs["-i2p.options.static"] = pFile_I2P_Object->getEnableStatus();
    mapArgs["-i2p.options.enabled"] = pFile_I2P_Object->getEnableStatus();
    mapArgs["-i2p.options.enabled"] = pFile_I2P_Object->getEnableStatus();
}

//******************************************************************************
//
//    Name: I2PManager::LogDataFile
//
//    Parameters:   N/A
//
//    Description:
//      Print data file contents to debug log
//
//    Return:       N/A
//
//******************************************************************************
void I2PManager::LogDataFile(void)
{
#define SPACE 20
    LogPrintf("========== I 2 P   D A T A   F I L E ==========");
    LogPrintf("Enabled: %*s",           SPACE, this->FileI2P.getEnableStatus());
    LogPrintf("Static: %*s",            SPACE, this->FileI2P.getStatic());
    LogPrintf("Session Name: %*s",      SPACE, this->FileI2P.getSessionName());
    LogPrintf("Sam Host: %*s",          SPACE, this->FileI2P.getSamHost());
    LogPrintf("Sam Port: %*s",          SPACE, this->FileI2P.getSamPort());
    LogPrintf("PrivateKey: %*s",        SPACE, this->FileI2P.getPrivateKey());

    LogPrintf("[Inbound Settings]");
    LogPrintf("Quantity: %*s",          SPACE, this->FileI2P.I2PData.fileData.inbound.quantity);
    LogPrintf("Backup Quanity: %*s",    SPACE, this->FileI2P.I2PData.fileData.inbound.backupquantity);
    LogPrintf("Length: %*s",            SPACE, this->FileI2P.I2PData.fileData.inbound.length);
    LogPrintf("Length Variance: %*s",   SPACE, this->FileI2P.I2PData.fileData.inbound.lengthvariance);
    LogPrintf("Allow Zero Hop: %*s",    SPACE, this->FileI2P.I2PData.fileData.inbound.allowzerohop);
    LogPrintf("IP Restriction: %*s",    SPACE, this->FileI2P.I2PData.fileData.inbound.iprestriction);


    LogPrintf("[Outbound Settings]");
    LogPrintf("Quantity: %*s",          SPACE, this->FileI2P.I2PData.fileData.outbound.quantity);
    LogPrintf("Backup Quanity: %*s",    SPACE, this->FileI2P.I2PData.fileData.outbound.backupquantity);
    LogPrintf("Length: %*s",            SPACE, this->FileI2P.I2PData.fileData.outbound.length);
    LogPrintf("Length Variance: %*s",   SPACE, this->FileI2P.I2PData.fileData.outbound.lengthvariance);
    LogPrintf("Allow Zero Hop: %*s",    SPACE, this->FileI2P.I2PData.fileData.outbound.allowzerohop);
    LogPrintf("IP Restriction: %*s",    SPACE, this->FileI2P.I2PData.fileData.outbound.iprestriction);
    LogPrintf("Priority: %*s",          SPACE, this->FileI2P.I2PData.fileData.outbound.priority);

    LogPrintf("========== I 2 P   D A T A   F I L E ==========");
#undef SPACE
}

