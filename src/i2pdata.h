/*
 * i2pdata.h
 *
 *  Created on: Mar 16, 2016
 *      Author: ssuag
 */

#ifndef __I2P_DATA__
#define __I2P_DATA__


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
    std::string             sessionName[256];
    std::string             samhost[7];        // xxx.xxx.xxx.xxx
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

    void setInboundQuantity(int32_t) const;
    void setInboundBackupQuantity(int32_t) const;
    void setInboundLength(int32_t) const;
    void setInboundLengthVariance(int32_t) const;
    void setInboundIPRestriction(int32_t) const;
    void setInboundAllowZeroHop(int32_t) const;

    void setOutboundPriority(int32_t) const;
    void setOutboundBackupQuantity(int32_t) const;
    void setOutboundLength(int32_t) const;
    void setOutboundLengthVariance(int32_t) const;
    void setOutboundIPRestriction(int32_t) const;
    void setOutboundAllowZeroHop(int32_t) const;
    void setOutboundQuantity(int32_t) const;


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

};


#endif /* __I2P_DATA__ */
