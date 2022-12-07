#ifndef CONFIGMGR_H
#define CONFIGMGR_H

#include "IPAddress.h"
#include "generated/config.h"

//data that needs to be persisted for other parts of the framework

#define SIZE_INTERNAL 32 //allocate 32 bytes to have room for future expansion
//#define SIZE_INTERNAL 96 //allocate 32 bytes to have room for future expansion

//#define MAX_SIZE_PERSISTED_STRING 32

struct internalData
{
    uint32_t ip;
    uint32_t gw;
    uint32_t sub;
    uint32_t dns;
//    char SSID[MAX_SIZE_PERSISTED_STRING];
//    char password[MAX_SIZE_PERSISTED_STRING];
};

class config
{

public:
    configData data;
    internalData internal;
    bool begin(int numBytes = 512);
    void saveRaw(uint8_t bytes[]);
    void saveExternal(configData *extData);
    void save();
    void reset();
    void loop();
    void setConfigSaveCallback( std::function<void()> func );

private:
    uint8_t checksum(uint8_t *byteArray, unsigned long length);
  	std::function<void()> _configsavecallback;
    bool requestSave = false;
};

extern config configManager;

#endif