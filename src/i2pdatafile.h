/*
 * i2pdata.h
 *
 *  Created on: Mar 16, 2016
 *      Author: ssuag
 */

#ifndef __I2P_DATA__
#define __I2P_DATA__

#include "serialize.h"
#include "i2psam.h"

#define FILE_HEADER_VERSION     (0x53554147)
#define FILE_I2P_VERSION        (0x0001)
#define FILE_I2P_HEADER_SIZE    (sizeof(I2P_File_Header_t))

#define I2P_DEFAULT_INBOUND_QUANITITY        (SAM_DEFAULT_INBOUND_QUANTITY)
#define I2P_DEFAULT_INBOUND_LENGTH           (SAM_DEFAULT_INBOUND_LENGTH)
#define I2P_DEFAULT_INBOUND_LENGTHVARIANCE   (SAM_DEFAULT_INBOUND_LENGTHVARIANCE)
#define I2P_DEFAULT_INBOUND_BACKUPQUANTITY   (0)
#define I2P_DEFAULT_INBOUND_ALLOWZEROHOP     (0)
#define I2P_DEFAULT_INBOUND_IPRESTRICTION    (SAM_DEFAULT_INBOUND_IPRESTRICTION)

#define I2P_DEFAULT_OUTBOUND_QUANITITY       (SAM_DEFAULT_OUTBOUND_QUANTITY)
#define I2P_DEFAULT_OUTBOUND_LENGTH          (SAM_DEFAULT_OUTBOUND_LENGTH)
#define I2P_DEFAULT_OUTBOUND_LENGTHVARIANCE  (SAM_DEFAULT_OUTBOUND_LENGTHVARIANCE)
#define I2P_DEFAULT_OUTBOUND_BACKUPQUANTITY  (0)
#define I2P_DEFAULT_OUTBOUND_ALLOWZEROHOP    (0)
#define I2P_DEFAULT_OUTBOUND_IPRESTRICTION   (SAM_DEFAULT_OUTBOUND_IPRESTRICTION)
#define I2P_DEFAULT_OUTBOUND_PRIORITY        (SAM_DEFAULT_OUTBOUND_PRIORITY)

#define I2P_DEFAULT_ENABLED                  (1)
#define I2P_DEFAULT_SAMHOST                  (SAM_DEFAULT_ADDRESS)
#define I2P_DEFAULT_SAMPORT                  (SAM_DEFAULT_PORT)
#define I2P_DEFAULT_SESSIONNAME              "Anoncoin-client"

#define MAP_ARGS_I2P_MYDESTINATION_B32KEY           "-i2p.mydestination.base32key"
#define MAP_ARGS_I2P_MYDESTINATION_PUBLICKEY        "-i2p.mydestination.publickey"
#define MAP_ARGS_I2P_MYDESTINATION_PRIVATEKEY       "-i2p.mydestination.privatekey"
#define MAP_ARGS_I2P_OPTIONS_ENABLED                "-i2p.options.enabled"
#define MAP_ARGS_I2P_MYDESTINATION_STATIC           "-i2p.mydestination.static"
#define MAP_ARGS_I2P_OPTIONS_SAMHOST                "-i2p.options.samhost"
#define MAP_ARGS_I2P_OPTIONS_SAMPORT                "-i2p.options.samport"
#define MAP_ARGS_I2P_OPTIONS_SESSION                "-i2p.options.sessionname"

#define MAP_ARGS_I2P_OPTIONS_INBOUND_QUANTITY       "-i2p.options.inbound.quantity"
#define MAP_ARGS_I2P_OPTIONS_INBOUND_LENGTH         "-i2p.options.inbound.length"
#define MAP_ARGS_I2P_OPTIONS_INBOUND_LENGTHVARIANCE "-i2p.options.inbound.lengthvariance"
#define MAP_ARGS_I2P_OPTIONS_INBOUND_BACKUPQUANTITY "-i2p.options.inbound.backupquantity"
#define MAP_ARGS_I2P_OPTIONS_INBOUND_ALLOWZEROHOP   "-i2p.options.inbound.allowzerohop"
#define MAP_ARGS_I2P_OPTIONS_INBOUND_IPRESTRICTION  "-i2p.options.inbound.iprestriction"

#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_QUANTITY       "-i2p.options.outbound.quantity"
#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_LENGTH         "-i2p.options.outbound.length"
#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_LENGTHVARIANCE "-i2p.options.outbound.lengthvariance"
#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_BACKUPQUANTITY "-i2p.options.outbound.backupquantity"
#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_ALLOWZEROHOP   "-i2p.options.outbound.allowzerohop"
#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_IPRESTRICTION  "-i2p.options.outbound.iprestriction"
#define MAP_ARGS_I2P_OPTIONS_OUTBOUND_PRIORITY       "-i2p.options.outbound.priority"

typedef struct I2P_File_Header_t{
    uint32_t      file_header_version;         // [Bytes 00:03] Hardcoded to 0x53554147
    uint16_t      data_offset;                 // [Bytes 06:07] Offset to data in File, skip over the file header.
    uint16_t      version;                     // [Bytes 14:15] File version
} I2P_File_Header_t;

typedef struct i2pInboundSettings_t
{
    int32_t     quantity;
    int32_t     length;
    int32_t     lengthvariance;
    int32_t     backupquantity;
    int32_t     iprestriction;
    bool        allowzerohop;
} I2P_InboundSettings_t;

typedef struct i2pOutboundSettings_t
{
    int32_t     quantity;
    int32_t     length;
    int32_t     lengthvariance;
    int32_t     backupquantity;
    int32_t     iprestriction;
    int32_t     priority;
    bool        allowzerohop;
} I2P_OutboundSettings_t;

typedef struct i2pfiledata_t
{
    bool                    isEnabled;            // Whether or not I2P is enabled
    bool                    isStatic;            // Must be set if a private key is specified
    std::string             privateKey;
    std::string             sessionName;
    std::string             samhost;        // xxx.xxx.xxx.xxx
    int32_t                 samport;
    I2P_InboundSettings_t   inbound;
    I2P_OutboundSettings_t  outbound;
} I2P_File_Data_t;

typedef struct i2psettings_t
{
    I2P_File_Header_t       fileHeader;
    I2P_File_Data_t         fileData;
} I2P_Data_File_t;

class I2PDataFile
{

private:
    //! critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    I2PDataFile();
    ~I2PDataFile();

    void initHeader(void);
    void initDefaultValues(void);

    void setPrivateKey(const std::string);
    void setSessionName(const std::string);
    void setSam(const std::string, const int32_t);
    void setEnableStatus(const bool);
    void setStatic(const bool);

    void setInboundQuantity(int32_t);
    void setInboundBackupQuantity(int32_t);
    void setInboundLength(int32_t);
    void setInboundLengthVariance(int32_t);
    void setInboundIPRestriction(int32_t);
    void setInboundAllowZeroHop(int32_t);

    void setOutboundPriority(int32_t);
    void setOutboundBackupQuantity(int32_t);
    void setOutboundLength(int32_t);
    void setOutboundLengthVariance(int32_t);
    void setOutboundIPRestriction(int32_t);
    void setOutboundAllowZeroHop(int32_t);
    void setOutboundQuantity(int32_t);


    const std::string getPrivateKey(void) const;
    const std::string getSessionName(void) const;
    const std::string getSamHost(void) const;
    const int32_t getSamPort(void) const;
    const bool getEnableStatus(void) const;
    const bool getStatic(void) const;

    const int32_t getInboundQuantity(void) const;
    const int32_t getInboundBackupQuantity(void) const;
    const int32_t getInboundLength(void) const;
    const int32_t getInboundLengthVariance(void) const;
    const int32_t getInboundIPRestriction(void) const;
    const int32_t getInboundAllowZeroHop(void) const;

    const int32_t getOutboundPriority(void) const;
    const int32_t getOutboundBackupQuantity(void) const;
    const int32_t getOutboundLength(void) const;
    const int32_t getOutboundLengthVariance(void) const;
    const int32_t getOutboundIPRestriction(void) const;
    const int32_t getOutboundAllowZeroHop(void) const;
    const int32_t getOutboundQuantity(void) const;

private:
    I2P_Data_File_t I2PData;
    void initInbound(void);
    void initOutbound(void);

public:
   /**
     * serialized format:
     * * file header
     *   *
     *   *
     *   *
     * * file data
     *   *
     *   *
     *
     * Serialize the header then data portions. Data order was chosen arbitrarily
     * but a few bytes can be saved if packing was optimized. 
     *
     */
    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersionDummy) const
    {
        LOCK(cs);

        uint32_t privateKeyLength   = (uint32_t)I2PData.fileData.privateKey.length();
        uint32_t sessionNameLength  = (uint32_t)I2PData.fileData.sessionName.length();
        uint32_t samHostLength      = (uint32_t)I2PData.fileData.samhost.length();

        const char *privateKeyCStr  = I2PData.fileData.privateKey.c_str();
        const char *sessionNameCStr = I2PData.fileData.sessionName.c_str();
        const char *samHostCStr     = I2PData.fileData.samhost.c_str();

        s << I2PData.fileHeader.file_header_version;
        s << I2PData.fileHeader.data_offset;
        s << I2PData.fileHeader.version;

        s << (char)I2PData.fileData.isEnabled;
        s << (char)I2PData.fileData.isStatic;

        SerializeCStr(s, (char*)sessionNameCStr, sessionNameLength);
        SerializeCStr(s, (char*)samHostCStr, samHostLength);

        s << I2PData.fileData.samport;

        s << I2PData.fileData.inbound.quantity;
        s << I2PData.fileData.inbound.length;
        s << I2PData.fileData.inbound.lengthvariance;
        s << I2PData.fileData.inbound.backupquantity;
        s << I2PData.fileData.inbound.iprestriction;
        s << I2PData.fileData.inbound.allowzerohop;
        s << I2PData.fileData.outbound.quantity;
        s << I2PData.fileData.outbound.length;
        s << I2PData.fileData.outbound.lengthvariance;
        s << I2PData.fileData.outbound.backupquantity;
        s << I2PData.fileData.outbound.iprestriction;
        s << I2PData.fileData.outbound.priority;
        s << I2PData.fileData.outbound.allowzerohop;

        SerializeCStr(s, (char*)privateKeyCStr, privateKeyLength);
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersionDummy)
    {
        LOCK(cs);

        char tempChar;

        s >> I2PData.fileHeader.file_header_version;
        s >> I2PData.fileHeader.data_offset;
        s >> I2PData.fileHeader.version;

        s >> tempChar;
        I2PData.fileData.isEnabled = (bool)tempChar;

        s >> tempChar;
        I2PData.fileData.isStatic = (bool)tempChar;


        UnserializeIntoString(s, I2PData.fileData.sessionName);
        UnserializeIntoString(s, I2PData.fileData.samhost);

        s >> I2PData.fileData.samport;

        s >> I2PData.fileData.inbound.quantity;
        s >> I2PData.fileData.inbound.length;
        s >> I2PData.fileData.inbound.lengthvariance;
        s >> I2PData.fileData.inbound.backupquantity;
        s >> I2PData.fileData.inbound.iprestriction;
        s >> I2PData.fileData.inbound.allowzerohop;
        s >> I2PData.fileData.outbound.quantity;
        s >> I2PData.fileData.outbound.length;
        s >> I2PData.fileData.outbound.lengthvariance;
        s >> I2PData.fileData.outbound.backupquantity;
        s >> I2PData.fileData.outbound.iprestriction;
        s >> I2PData.fileData.outbound.priority;
        s >> I2PData.fileData.outbound.allowzerohop;

        UnserializeIntoString(s, I2PData.fileData.privateKey);
    }

};

// Helper function to Serialize a C-string into the Stream object.
template<typename Stream>
void SerializeCStr(Stream& s, const char* pCStr, const uint32_t len)
{
    uint32_t index;

    // Prefix string with length so it can be properly de-seralized
    s << len;

    if ((len != 0) && (pCStr != 0))
    {
        for (index = 0; index<len; index++)
        {
            s << pCStr[index];
        }
    }
}

template<typename Stream>
void UnserializeIntoString(Stream& s, std::string& dataStr)
{
    // This assumes to follow the format:
    // Length (4 bytes) -> xLen
    // Character stream of length xLen

    uint32_t index = 0;
    unsigned long len = 0;

    //Character stream length
    s >> len;

    if (len !=0)
    {
        char* pCStr = new char[len+1];

        assert(pCStr);

        std::fill (pCStr, pCStr + len + 1, 0);

        for (index = 0; index<len; index++)
        {
            s >> pCStr[index];
        }

        dataStr = std::string(pCStr);

        delete [] pCStr;
    }
    else
    {
        dataStr = std::string("");
    }
}

#endif /* __I2P_DATA__ */
