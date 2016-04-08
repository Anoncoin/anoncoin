/*
 * i2pdata.cpp
 *
 *  Created on: Mar 11, 2016
 *      Author: ssuag
 */

#include "clientversion.h"
#include "util.h"
#include "random.h"
#include "uint256.h"
#include "version.h"
#include "streams.h"
#include "serialize.h"
#include "crypto/sha256.h"
#include "hash.h"
#include "chainparams.h"
#include "netbase.h"
#include "protocol.h"
#include "sync.h"
#include "timedata.h"
#include "i2pmanager.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>
#include <sstream>

template <typename T>
  string NumberToString ( T Number )
  {
     ostringstream ss;
     ss << Number;
     return ss.str();
  }

#include <openssl/rand.h>
#include <stdarg.h>
#include <stdio.h>

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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <fstream>

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
I2PManager::I2PManager()
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
I2PDataFile* I2PManager::getFileI2PPtr(void)
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
boost::filesystem::path I2PManager::GetI2PSettingsFilePath(void)
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
    // Generate random temporary filename
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("i2p.dat.%04x", randv);
    
    // serialize addresses, checksum data up to that point, then append checksum
    CDataStream cdsI2P(SER_DISK, CLIENT_VERSION);
    cdsI2P << FLATDATA(*pFile_I2P_Object);

    uint256 hash = Hash(cdsI2P.begin(), cdsI2P.end());
    cdsI2P << hash;
    
    // Open output file and associate with CAutoFile
    boost::filesystem::path path = GetDataDir() / tmpfn;
    FILE *file = fopen(path.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
    {
        return error("%s : Failed to open file %s", __func__, path);
    }
    
    // Write and commit header, data
    try {
        fileout << cdsI2P;
    }
    catch (std::exception &e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    FileCommit(fileout.Get());
    fileout.fclose();

    // replace existing i2p.dat, if any, with new i2p.dat.XXXX
    if (!RenameOver(path, GetI2PSettingsFilePath()))
        return error("%s : Rename-into-place failed", __func__);
    
    return true;
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
    // open input file, and associate with CAutoFile
    FILE *file = fopen(GetI2PSettingsFilePath().string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s : Failed to open file %s", __func__, GetI2PSettingsFilePath().string());

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(GetI2PSettingsFilePath());
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;
    
    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }
    filein.fclose();
    
    CDataStream cdsI2P(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(cdsI2P.begin(), cdsI2P.end());
    if (hashIn != hashTmp)
        return error("%s : Checksum mismatch, data corrupted", __func__);

    try {
        // de-serialize address data into one CAddrMan object
        cdsI2P >> *(pFile_I2P_Object);
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }
    
    return true;
}

//******************************************************************************
//
//    Name:         I2PManager::UpdateMapArguments
//
//    Parameters:   void
//
//    Description:  Update the map of arguments with what is stored within
//                  our settings file
//
//    Return:       None
//
//******************************************************************************
void I2PManager::UpdateMapArguments(void)
{
    mapArgs["-i2p.options.enabled"]                 = pFile_I2P_Object->getEnableStatus() ? "1" : "0";
    mapArgs["-i2p.mydestination.static"]            = pFile_I2P_Object->getEnableStatus() ? "1" : "0";
    mapArgs["-i2p.options.i2p.options.samhost"]     = pFile_I2P_Object->getSamHost();
    mapArgs["-i2p.options.i2p.options.samport"]     = NumberToString(pFile_I2P_Object->getSamPort());
    mapArgs["-i2p.options.sessionname"]             = pFile_I2P_Object->getSessionName();

    mapArgs["-i2p.options.inbound.quantity"]        = NumberToString(pFile_I2P_Object->getInboundQuantity());
    mapArgs["-i2p.options.inbound.length"]          = NumberToString(pFile_I2P_Object->getInboundLength());
    mapArgs["-i2p.options.inbound.lengthvariance"]  = NumberToString(pFile_I2P_Object->getInboundLengthVariance());
    mapArgs["-i2p.options.inbound.backupquantity"]  = NumberToString(pFile_I2P_Object->getInboundBackupQuantity());
    mapArgs["-i2p.options.inbound.allowzerohop"]    = NumberToString(pFile_I2P_Object->getInboundAllowZeroHop());
    mapArgs["-i2p.options.inbound.iprestriction"]   = NumberToString(pFile_I2P_Object->getInboundIPRestriction());

    mapArgs["-i2p.options.outbound.quantity"]        = NumberToString(pFile_I2P_Object->getOutboundQuantity());
    mapArgs["-i2p.options.outbound.length"]          = NumberToString(pFile_I2P_Object->getOutboundLength());
    mapArgs["-i2p.options.outbound.lengthvariance"]  = NumberToString(pFile_I2P_Object->getOutboundLengthVariance());
    mapArgs["-i2p.options.outbound.backupquantity"]  = NumberToString(pFile_I2P_Object->getOutboundBackupQuantity());
    mapArgs["-i2p.options.outbound.allowzerohop"]    = NumberToString(pFile_I2P_Object->getOutboundAllowZeroHop());
    mapArgs["-i2p.options.outbound.iprestriction"]   = NumberToString(pFile_I2P_Object->getOutboundIPRestriction());
    mapArgs["-i2p.options.outbound.priority"]        = NumberToString(pFile_I2P_Object->getOutboundPriority());
    
    if ( (mapArgs["-i2p.mydestination.privatekey"] != "") && (pFile_I2P_Object->getPrivateKey() != ""))
    {
        mapArgs["-i2p.mydestination.privatekey"] = pFile_I2P_Object->getPrivateKey();
    }
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
    
    LogPrintf("Enabled: %*s",           SPACE, pFile_I2P_Object->getEnableStatus());
    LogPrintf("Static: %*s",            SPACE, pFile_I2P_Object->getStatic());
    LogPrintf("Session Name: %*s",      SPACE, pFile_I2P_Object->getSessionName());
    LogPrintf("Sam Host: %*s",          SPACE, pFile_I2P_Object->getSamHost());
    LogPrintf("Sam Port: %*s",          SPACE, pFile_I2P_Object->getSamPort());
    LogPrintf("PrivateKey: %*s",        SPACE, pFile_I2P_Object->getPrivateKey());

    LogPrintf("[Inbound Settings]");
    LogPrintf("Quantity: %*s",          SPACE, pFile_I2P_Object->I2PData.fileData.inbound.quantity);
    LogPrintf("Backup Quanity: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.inbound.backupquantity);
    LogPrintf("Length: %*s",            SPACE, pFile_I2P_Object->I2PData.fileData.inbound.length);
    LogPrintf("Length Variance: %*s",   SPACE, pFile_I2P_Object->I2PData.fileData.inbound.lengthvariance);
    LogPrintf("Allow Zero Hop: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.inbound.allowzerohop);
    LogPrintf("IP Restriction: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.inbound.iprestriction);


    LogPrintf("[Outbound Settings]");
    LogPrintf("Quantity: %*s",          SPACE, pFile_I2P_Object->I2PData.fileData.outbound.quantity);
    LogPrintf("Backup Quanity: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.outbound.backupquantity);
    LogPrintf("Length: %*s",            SPACE, pFile_I2P_Object->I2PData.fileData.outbound.length);
    LogPrintf("Length Variance: %*s",   SPACE, pFile_I2P_Object->I2PData.fileData.outbound.lengthvariance);
    LogPrintf("Allow Zero Hop: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.outbound.allowzerohop);
    LogPrintf("IP Restriction: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.outbound.iprestriction);
    LogPrintf("Priority: %*s",          SPACE, pFile_I2P_Object->I2PData.fileData.outbound.priority);

    LogPrintf("========== I 2 P   D A T A   F I L E ==========");
#undef SPACE
}

