/*
 * i2pdata.h
 *
 *  Created on: Mar 16, 2016
 *      Author: ssuag
 */

#ifndef __I2P_DATA__
#define __I2P_DATA__

#include "serialize.h"

#define FILE_HEADER_VERSION     0x53554147
#define FILE_I2P_VERSION        0x0001
#define FILE_I2P_SIZE           sizeof(I2P_Data_File_t)
#define FILE_I2P_HEADER_SIZE    sizeof(I2P_File_Header_t)

#define I2P_DEFAULT_INBOUND_QUANITITY        3
#define I2P_DEFAULT_INBOUND_LENGTH           3
#define I2P_DEFAULT_INBOUND_LENGTHVARIANCE   0
#define I2P_DEFAULT_INBOUND_BACKUPQUANTITY   1
#define I2P_DEFAULT_INBOUND_ALLOWZEROHOP     0
#define I2P_DEFAULT_INBOUND_IPRESTRICTION    2

#define I2P_DEFAULT_OUTBOUND_QUANITITY       3
#define I2P_DEFAULT_OUTBOUND_LENGTH          3
#define I2P_DEFAULT_OUTBOUND_LENGTHVARIANCE  0
#define I2P_DEFAULT_OUTBOUND_BACKUPQUANTITY  1
#define I2P_DEFAULT_OUTBOUND_ALLOWZEROHOP    0
#define I2P_DEFAULT_OUTBOUND_IPRESTRICTION   2
#define I2P_DEFAULT_OUTBOUND_PRIORITY        0

#define I2P_DEFAULT_ENABLED                  1
#define I2P_DEFAULT_SAMHOST                  "127.0.0.1"
#define I2P_DEFAULT_SAMPORT                  7656
#define I2P_DEFAULT_SESSIONNAME              "ANONCOIN-CLIENT"

typedef struct I2P_File_Header_t{
    uint32_t      file_header_version;         // [Bytes 00:03] Hardcoded to 0x53554147
    uint16_t      data_offset;                 // [Bytes 06:07] Offset to data in File, skip over the file header.
    uint16_t      file_size;                   // [Bytes 08:09] Size in bytes
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
}I2P_InboundSettings_t;

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

    I2P_Data_File_t I2PData;
    void initInbound(void);
    void initOutbound(void);
    
 /*      ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(I2PData);
    }
*/
   /**
     * serialized format:
     * * version byte (currently 1)
     * * 0x20 + nKey (serialized as if it were a vector, for backward compatibility)
     * * nNew
     * * nTried
     * * number of "new" buckets XOR 2**30
     * * all nNew addrinfos in vvNew
     * * all nTried addrinfos in vvTried
     * * for each bucket:
     *   * number of elements
     *   * for each element: index
     *
     * 2**30 is xorred with the number of buckets to make addrman deserializer v0 detect it
     * as incompatible. This is necessary because it did not check the version number on
     * deserialization.
     *
     * Notice that vvTried, mapAddr and vVector are never encoded explicitly;
     * they are instead reconstructed from the other information.
     *
     * vvNew is serialized, but only used if ADDRMAN_UNKOWN_BUCKET_COUNT didn't change,
     * otherwise it is reconstructed as well.
     *
     * This format is more complex, but significantly smaller (at most 1.5 MiB), and supports
     * changes to the ADDRMAN_ parameters without breaking the on-disk structure.
     *
     * We don't use ADD_SERIALIZE_METHODS since the serialization and deserialization code has
     * very little in common.
     *
     */
    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersionDummy) const
    {
        LOCK(cs);
   
        s << I2PData.fileHeader.file_header_version;
        s << I2PData.fileHeader.data_offset;
        s << I2PData.fileHeader.file_size;
        s << I2PData.fileHeader.version;
               
        s << I2PData.fileData.isEnabled;
        s << I2PData.fileData.isStatic;
        s << I2PData.fileData.privateKey.c_str();
        s << I2PData.fileData.sessionName.c_str();
        s << I2PData.fileData.samhost.c_str();
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

    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersionDummy)
    {
        LOCK(cs);
        
        // need to start importing data on offset 25 of serial stream
        // TODO
        
        s >> I2PData.fileHeader.file_header_version;
        s >> I2PData.fileHeader.data_offset;
        s >> I2PData.fileHeader.file_size;
        s >> I2PData.fileHeader.version;
               
        s >> I2PData.fileData.isEnabled;
        s >> I2PData.fileData.isStatic;
        
        s >> I2PData.fileData.privateKey;
        s >> I2PData.fileData.sessionName;
        s >> I2PData.fileData.samhost;
        
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
    }
    
};


#endif /* __I2P_DATA__ */
