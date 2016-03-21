#ifndef __I2P_MANAGER__
#define __I2P_MANAGER__

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <string>
#include "stdint.h"
#include "i2pdata.h"

class I2PManager
{
public:
    I2PManager();
    ~I2PManager();

    boost::filesystem::path GetI2PSettingsFilePath(void) const;
    bool WriteToI2PSettingsFile(void);
    bool ReadI2PSettingsFile(void);
    I2PDataFile* getFileI2PPtr(void);
    void LogDataFile(void);
    void UpdateMapArguments(void);
};

extern I2PManager *pI2PManager;

#endif // __I2P_MANAGER__
