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


//----------------------------------
// Static Declarations
//----------------------------------
static void UpdateMapArgumentsIfNotAlreadySet(std::string mapArgStr, std::string value);
static I2PDataFile *pFile_I2P_Object;

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
    cdsI2P << *(pFile_I2P_Object);

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
        // de-serialize address data into one I object
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
//    Return:       bool - False if a private key has not been defined
//                         True otherwise
//
//******************************************************************************
bool I2PManager::UpdateMapArguments(void)
{
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_ENABLED, pFile_I2P_Object->getEnableStatus() ? "1" : "0" );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_MYDESTINATION_STATIC, pFile_I2P_Object->getStatic() ? "1" : "0" );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_SAMHOST, pFile_I2P_Object->getSamHost() );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_SAMPORT, NumberToString(pFile_I2P_Object->getSamPort()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_SESSION, pFile_I2P_Object->getSessionName() );

    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_INBOUND_QUANTITY, NumberToString(pFile_I2P_Object->getInboundQuantity()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_INBOUND_LENGTH, NumberToString(pFile_I2P_Object->getInboundLength()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_INBOUND_LENGTHVARIANCE, NumberToString(pFile_I2P_Object->getInboundLengthVariance()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_INBOUND_BACKUPQUANTITY, NumberToString(pFile_I2P_Object->getInboundBackupQuantity()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_INBOUND_ALLOWZEROHOP, NumberToString(pFile_I2P_Object->getInboundAllowZeroHop()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_INBOUND_IPRESTRICTION, NumberToString(pFile_I2P_Object->getInboundIPRestriction()) );

    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_QUANTITY, NumberToString(pFile_I2P_Object->getOutboundQuantity()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_LENGTH, NumberToString(pFile_I2P_Object->getOutboundLength()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_LENGTHVARIANCE, NumberToString(pFile_I2P_Object->getOutboundLengthVariance()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_BACKUPQUANTITY, NumberToString(pFile_I2P_Object->getOutboundBackupQuantity()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_ALLOWZEROHOP, NumberToString(pFile_I2P_Object->getOutboundAllowZeroHop()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_IPRESTRICTION, NumberToString(pFile_I2P_Object->getOutboundIPRestriction()) );
    UpdateMapArgumentsIfNotAlreadySet( MAP_ARGS_I2P_OPTIONS_OUTBOUND_PRIORITY, NumberToString(pFile_I2P_Object->getOutboundPriority()) );

    std::string privateKey = GetArg(MAP_ARGS_I2P_MYDESTINATION_PRIVATEKEY, "");

    if (privateKey != "")
    {
        // Private key has been previously defined within config file
        pFile_I2P_Object->setPrivateKey(privateKey);
    }
    else if ( (privateKey == "") && (pFile_I2P_Object->getPrivateKey() != "") )
    {
        SoftSetArg(MAP_ARGS_I2P_MYDESTINATION_PRIVATEKEY,pFile_I2P_Object->getPrivateKey() );
    }
    else
    {
        // Neither have been set, disable I2P and inform the user
        HardSetArg(MAP_ARGS_I2P_OPTIONS_ENABLED, "0");
        return false;
    }
    return true;
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
    LogPrintf("\n========== I 2 P   D A T A   F I L E ==========\n");

    LogPrintf("Please note that the following settings will be applied only if\n");
    LogPrintf("they have not been previously defined with anoncoin.conf.\n");

    LogPrintf("\nEnabled: %*s",           SPACE, pFile_I2P_Object->getEnableStatus());
    LogPrintf("\nStatic: %*s",            SPACE, pFile_I2P_Object->getStatic());
    LogPrintf("\nSession Name: %*s",      SPACE, pFile_I2P_Object->getSessionName());
    LogPrintf("\nSam Host: %*s",          SPACE, pFile_I2P_Object->getSamHost());
    LogPrintf("\nSam Port: %*s",          SPACE, pFile_I2P_Object->getSamPort());
    LogPrintf("\nPrivateKey: %*s",        SPACE, pFile_I2P_Object->getPrivateKey());

    LogPrintf("\n[Inbound Settings]");
    LogPrintf("\nQuantity: %*s",          SPACE, pFile_I2P_Object->I2PData.fileData.inbound.quantity);
    LogPrintf("\nBackup Quanity: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.inbound.backupquantity);
    LogPrintf("\nLength: %*s",            SPACE, pFile_I2P_Object->I2PData.fileData.inbound.length);
    LogPrintf("\nLength Variance: %*s",   SPACE, pFile_I2P_Object->I2PData.fileData.inbound.lengthvariance);
    LogPrintf("\nAllow Zero Hop: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.inbound.allowzerohop);
    LogPrintf("\nIP Restriction: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.inbound.iprestriction);


    LogPrintf("\n[Outbound Settings]");
    LogPrintf("\nQuantity: %*s",          SPACE, pFile_I2P_Object->I2PData.fileData.outbound.quantity);
    LogPrintf("\nBackup Quanity: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.outbound.backupquantity);
    LogPrintf("\nLength: %*s",            SPACE, pFile_I2P_Object->I2PData.fileData.outbound.length);
    LogPrintf("\nLength Variance: %*s",   SPACE, pFile_I2P_Object->I2PData.fileData.outbound.lengthvariance);
    LogPrintf("\nAllow Zero Hop: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.outbound.allowzerohop);
    LogPrintf("\nIP Restriction: %*s",    SPACE, pFile_I2P_Object->I2PData.fileData.outbound.iprestriction);
    LogPrintf("\nPriority: %*s",          SPACE, pFile_I2P_Object->I2PData.fileData.outbound.priority);

    LogPrintf("\n========== I 2 P   D A T A   F I L E ==========\n\n");
#undef SPACE
}

//******************************************************************************
//
//    Name:         I2PManager::UpdateMapArgumentsIfNotAlreadySet
//
//    Parameters:   std::string mapArgStr, std::string value
//
//    Description:  If map element is not already set via anoncoin.conf, override
//                  it via the I2P settings file
//
//    Return:       None
//
//******************************************************************************
static void UpdateMapArgumentsIfNotAlreadySet(std::string mapArgStr, std::string value)
{
    LogPrintf("\n[I2P Settings Manager] %s", mapArgStr);
    if (!SoftSetArg(mapArgStr,value))
    {
        LogPrintf( " (=%s)\n\tIgnoring value stored within settings file [%s].", GetArg(mapArgStr, ""), value );
    }
    else
    {
        LogPrintf( " (=%s)\n\tUsing settings file value.", value );
    }
}
