#ifndef WIFI_H
#define WIFI_H

#include <Arduino.h>
#include <DNSServer.h>
#include <memory>

class WifiManager
{

private:
    DNSServer *dnsServer;
    String ssid;
    String pass;
    IPAddress ip;
    IPAddress gw;
    IPAddress sub;
    IPAddress dns;
    bool reconnect = false;
    bool inCaptivePortal = false;
    bool captivePortalEnabled = false;
    char const *captivePortalName;
    unsigned long timeout = 60000;

    unsigned long STAStartInstant = 0;
    unsigned long connectNewWifiStartInstant = 0;
    uint8_t connectNewWifiLoopState = 0;
    uint8_t beginLoopState = 0;

    void startCaptivePortal(char const *apName, char const *apPass = "");
    void stopCaptivePortal();
    void connectNewWifi(String newSSID, String newPass);    
    void storeToEEPROM();
    int8_t waitForConnectResult(unsigned long timeoutLength);

    bool STAEnabled();
    void connectNewWifiLoop();
    void beginLoop();

    std::function<void()> _forgetwificallback;
    std::function<void()> _newwificallback;    

public :
    WifiManager();
    void begin(char const *apName, unsigned long newTimeout = 20000);
    void beginSTA(unsigned long newTimeout = 20000);
    void loop();
    void forget();
    bool isCaptivePortal();
    bool IsConnectionInProgress();
    bool IsConnectionTimedOut();
    bool IsConnectionSuccessful();
    String SSID();
    void setNewWifi(String newSSID, String newPass);
    void setNewWifi(String newSSID, String newPass, String newIp, String newSub, String newGw, String newDns);
    void forgetWiFiFunctionCallback( std::function<void()> func );
    void newWiFiFunctionCallback( std::function<void()> func );
};

extern WifiManager WiFiManager;

#endif


