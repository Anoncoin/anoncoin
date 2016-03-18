/*
 * i2pdatafile.cpp
 *
 *  Created on: Mar 16, 2016
 *      Author: ssuag
 */

#include "util.h"

#include "chainparamsbase.h"
#include "netbase.h"
#include "random.h"
#include "uint256.h"
#include "version.h"
#include "i2pdata.h"

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
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>

//******************************************************************************
//
//    Name: I2PDataFile constructor
//
//    Parameters:       N/A
//
//    Description:      Unused
//
//    Return:           N/A
//
//******************************************************************************
I2PDataFile::I2PDataFile()
{

}

//******************************************************************************
//
//    Name: I2PDataFile destructor
//
//    Parameters:       N/A
//
//    Description:      Unused
//
//    Return:           N/A
//
//******************************************************************************
I2PDataFile::~I2PDataFile()
{
}

//******************************************************************************
//
//    Name: I2PDataFile::initHeader
//
//    Parameters: N/A
//
//    Description:
//      Initializes the I2P header to their current revision
//
//    Return:
//      N/A
//
//******************************************************************************
void I2PDataFile::initHeader(void)
{
    I2PData.fileHeader.file_header_version = FILE_HEADER_VERSION;
    I2PData.fileHeader.version             = FILE_I2P_VERSION;
    I2PData.fileHeader.file_size           = FILE_I2P_SIZE;
    I2PData.fileHeader.data_offset         = FILE_I2P_HEADER_SIZE;
}

//******************************************************************************
//
//    Name: I2PDataFile::initInbound
//
//    Parameters: N/A
//
//    Description:
//      Initializes the inbound I2P settings to their default values
//
//    Return:
//      N/A
//
//******************************************************************************
void I2PDataFile::initInbound(void)
{
    I2PData.fileData.inbound.allowzerohop    = I2P_DEFAULT_INBOUND_ALLOWZEROHOP;
    I2PData.fileData.inbound.backupquantity  = I2P_DEFAULT_INBOUND_BACKUPQUANTITY;
    I2PData.fileData.inbound.iprestriction   = I2P_DEFAULT_INBOUND_IPRESTRICTION;
    I2PData.fileData.inbound.length          = I2P_DEFAULT_INBOUND_LENGTH;
    I2PData.fileData.inbound.lengthvariance  = I2P_DEFAULT_INBOUND_LENGTHVARIANCE;
    I2PData.fileData.inbound.quantity        = I2P_DEFAULT_INBOUND_QUANITITY;
}

//******************************************************************************
//
//    Name: I2PDataFile::initOutbound
//
//    Parameters: N/A
//
//    Description:
//      Initializes the outbound I2P settings to their default values
//
//    Return:
//      N/A
//
//******************************************************************************
void I2PDataFile::initOutbound(void)
{
    I2PData.fileData.outbound.allowzerohop    = I2P_DEFAULT_OUTBOUND_ALLOWZEROHOP;
    I2PData.fileData.outbound.backupquantity  = I2P_DEFAULT_OUTBOUND_BACKUPQUANTITY;
    I2PData.fileData.outbound.iprestriction   = I2P_DEFAULT_OUTBOUND_IPRESTRICTION;
    I2PData.fileData.outbound.length          = I2P_DEFAULT_OUTBOUND_LENGTH;
    I2PData.fileData.outbound.lengthvariance  = I2P_DEFAULT_OUTBOUND_LENGTHVARIANCE;
    I2PData.fileData.outbound.quantity        = I2P_DEFAULT_OUTBOUND_QUANITITY;
    I2PData.fileData.outbound.priority        = I2P_DEFAULT_OUTBOUND_PRIORITY;
}

//******************************************************************************
//
//    Name: I2PDataFile::initDefaultValues
//
//    Parameters: N/A
//
//    Description:
//      Initializes the data portion of the I2P data file to its default values
//
//    Return:
//      N/A
//
//******************************************************************************
void I2PDataFile::initDefaultValues(void)
{

    I2PDataFile::initInbound();
    I2PDataFile::initOutbound();

    I2PDataFile::setSessionName(std::string(I2P_DEFAULT_SESSIONNAME));
    I2PDataFile::setSam(I2P_DEFAULT_SAMHOST, I2P_DEFAULT_SAMPORT);
    I2PDataFile::setStatic(true);
    I2PDataFile::setEnableStatus(true);

    // GENERATE PRIVATE KEY
}

//******************************************************************************
//
//                              Helper functions
//
//
//******************************************************************************

void I2PDataFile::setPrivateKey(const std::string privateKey)
{
    I2PData.fileData.privateKey = privateKey;
}

void I2PDataFile::setSessionName(const std::string sessionName)
{
    I2PData.fileData.sessionName = sessionName;
}

void I2PDataFile::setSam(const std::string samHost, const int32_t samPort)
{
    I2PData.fileData.samhost = samHost;
    I2PData.fileData.samport = samPort;
}

void I2PDataFile::setEnableStatus(const bool fStatus)
{
    I2PData.fileData.isEnabled = fStatus;
}

void I2PDataFile::setStatic(const bool fStatic)
{
    I2PData.fileData.isStatic = fStatic;
}

const std::string I2PDataFile::getPrivateKey(void) const
{
    return I2PData.fileData.privateKey;
}

const std::string I2PDataFile::getSessionName(void) const
{
    return I2PData.fileData.sessionName;
}

const std::string I2PDataFile::getSamHost(void) const
{
    return I2PData.fileData.samhost;
}

const int32_t I2PDataFile::getSamPort(void) const
{
    return I2PData.fileData.samport;
}


const bool I2PDataFile::getEnableStatus(void) const
{
    return I2PData.fileData.isEnabled;
}

const bool I2PDataFile::getStatic(void) const
{
    return I2PData.fileData.isStatic;
}

//-----------------------------------------------------------
//
//              GET INBOUND PROPERTIES
//
//-----------------------------------------------------------

const int32_t I2PDataFile::getInboundQuantity(void) const{
    return I2PData.fileData.inbound.quantity;
}

const int32_t I2PDataFile::getInboundBackupQuantity(void) const
{
    return I2PData.fileData.inbound.backupquantity;
}

const int32_t I2PDataFile::getInboundLength(void) const
{
    return I2PData.fileData.inbound.length;
}
const int32_t I2PDataFile::getInboundLengthVariance(void) const
{
    return I2PData.fileData.inbound.lengthvariance;
}

const int32_t I2PDataFile::getInboundIPRestriction(void) const
{
    return I2PData.fileData.inbound.iprestriction;
}

const int32_t I2PDataFile::getInboundAllowZeroHop(void) const
{
    return I2PData.fileData.inbound.allowzerohop;
}

const int32_t I2PDataFile::getOutboundQuantity(void) const
{
    return I2PData.fileData.outbound.quantity;
}

const int32_t I2PDataFile::getOutboundPriority(void) const
{
    return I2PData.fileData.outbound.priority;
}

//-----------------------------------------------------------
//
//              GET OUTBOUND PROPERTIES
//
//-----------------------------------------------------------

const int32_t I2PDataFile::getOutboundQuantity(void) const{
    return I2PData.fileData.outbound.quantity;
}

const int32_t I2PDataFile::getOutboundBackupQuantity(void) const
{
    return I2PData.fileData.outbound.backupquantity;
}

const int32_t I2PDataFile::getOutboundLength(void) const
{
    return I2PData.fileData.outbound.length;
}
const int32_t I2PDataFile::getOutboundLengthVariance(void) const
{
    return I2PData.fileData.outbound.lengthvariance;
}

const int32_t I2PDataFile::getOutboundIPRestriction(void) const
{
    return I2PData.fileData.outbound.iprestriction;
}

const int32_t I2PDataFile::getOutboundAllowZeroHop(void) const
{
    return I2PData.fileData.outbound.allowzerohop;
}

const int32_t I2PDataFile::getOutboundQuantity(void) const
{
    return I2PData.fileData.outbound.quantity;
}

//-----------------------------------------------------------
//
//              SET INBOUND PROPERTIES
//
//-----------------------------------------------------------

void I2PDataFile::setInboundQuantity(int32_t quantity) const{
    I2PData.fileData.inbound.quantity = quantity;
}

void I2PDataFile::setInboundBackupQuantity(int32_t backupQuantity) const
{
    I2PData.fileData.inbound.backupquantity = backupQuantity;
}

void I2PDataFile::setInboundLength(int32_t length) const
{
    I2PData.fileData.inbound.length = length;
}

void I2PDataFile::setInboundLengthVariance(int32_t lengthVariance) const
{
    I2PData.fileData.inbound.lengthvariance = lengthVariance;
}

void I2PDataFile::setInboundIPRestriction(int32_t iprestriction) const
{
    I2PData.fileData.inbound.iprestriction = iprestriction;
}

void I2PDataFile::setInboundAllowZeroHop(int32_t allowzerohop) const
{
    I2PData.fileData.inbound.allowzerohop = allowzerohop;
}

//-----------------------------------------------------------
//
//              SET OUTBOUND PROPERTIES
//
//-----------------------------------------------------------

void I2PDataFile::setOutboundQuantity(int32_t quantity) const
{
    I2PData.fileData.outbound.quantity = quantity;
}

void I2PDataFile::setOutboundBackupQuantity(int32_t backupQuantity) const
{
    I2PData.fileData.outbound.backupquantity = backupQuantity;
}

void I2PDataFile::setOutboundLength(int32_t length) const
{
    I2PData.fileData.outbound.length = length;
}

void I2PDataFile::setOutboundLengthVariance(int32_t lengthVariance) const
{
    I2PData.fileData.outbound.lengthvariance = lengthVariance;
}

void I2PDataFile::setOutboundIPRestriction(int32_t iprestriction) const
{
    I2PData.fileData.outbound.iprestriction = iprestriction;
}

void I2PDataFile::setOutboundAllowZeroHop(int32_t allowzerohop) const
{
    I2PData.fileData.outbound.allowzerohop = allowzerohop;
}

void I2PDataFile::setOutboundPriority(int32_t priority) const
{
    I2PData.fileData.outbound.priority = priority;
}
