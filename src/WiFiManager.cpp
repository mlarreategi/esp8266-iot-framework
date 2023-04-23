//inspired by https://github.com/tzapu/WiFiManager but 
//- with more flexibility to add your own web server setup
//= state machine for changing wifi settings on the fly

#include <ESP8266WiFi.h>

#include "WiFiManager.h"
#include "configManager.h"
#include "helpers.h"

// State machine
#define SM_IDLE                 0
#define SM_START                1
#define SM_WAIT_1000            2
#define SM_WAIT_2000            3
#define SM_CONNECT_OR_TIME_OUT  4

//create global object
WifiManager WiFiManager;

WifiManager::WifiManager()
{
    captivePortalEnabled = false;
    beginLoopState = SM_IDLE;
    connectNewWifiLoopState = SM_IDLE;
}

//function to call in setup
void WifiManager::begin(char const *apName, unsigned long newTimeout)
{
    captivePortalName = apName;
    captivePortalEnabled = true;
    timeout = newTimeout;

    beginLoopState = SM_START;
    connectNewWifiLoopState = SM_IDLE;
}

void WifiManager::beginSTA(unsigned long newTimeout)
{
    timeout = newTimeout;
    captivePortalEnabled = false;
    
    beginLoopState = SM_START;
    connectNewWifiLoopState = SM_IDLE;
}

//
bool WifiManager::STAEnabled() {
    //1 and 3 have STA enabled
    return (wifi_get_opmode() & 1) != 0;
}

bool WifiManager::IsConnectionTimedOut() {

    //1 and 3 have STA enabled
    if(!STAEnabled())
        return true;

    return millis() - STAStartInstant >= timeout;
}

bool WifiManager::IsConnectionInProgress() {

    if(!STAEnabled())
        return false;

    if(IsConnectionTimedOut())
        return false;

    int8_t result = WiFi.status();

    return result == WL_DISCONNECTED || result == WL_NO_SSID_AVAIL;
}

bool WifiManager::IsConnectionSuccessful() {

    if(!STAEnabled())
        return false;

    if(IsConnectionTimedOut())
        return false;

    return WiFi.status() == WL_CONNECTED;
}

//Upgraded default waitForConnectResult function to incorporate WL_NO_SSID_AVAIL, fixes issue #122
int8_t WifiManager::waitForConnectResult(unsigned long timeoutLength) {
    //1 and 3 have STA enabled
    if((wifi_get_opmode() & 1) == 0) {
        return WL_DISCONNECTED;
    }
    using esp8266::polledTimeout::oneShot;
    oneShot timeout(timeoutLength); // number of milliseconds to wait before returning timeout error
    while(!timeout) {
        yield();
        if(WiFi.status() != WL_DISCONNECTED && WiFi.status() != WL_NO_SSID_AVAIL) {
            return WiFi.status();
        }
    }
    return -1; // -1 indicates timeout
}

//function to forget current WiFi details and start a captive portal
void WifiManager::forget()
{ 
    WiFi.disconnect();
    startCaptivePortal(captivePortalName);

    //remove IP address from EEPROM
    ip = IPAddress();
    sub = IPAddress();
    gw = IPAddress();
    dns = IPAddress();

    //make EEPROM empty
    storeToEEPROM();

    if ( _forgetwificallback != NULL) {
        _forgetwificallback();
    } 

    Serial.println(PSTR("Requested to forget WiFi. Started Captive portal."));
}

//function to request a connection to new WiFi credentials
void WifiManager::setNewWifi(String newSSID, String newPass)
{
    ssid = newSSID;
    pass = newPass;
    ip = IPAddress();
    sub = IPAddress();
    gw = IPAddress();
    dns = IPAddress();

    connectNewWifiLoopState = SM_START;
}

//function to request a connection to new WiFi credentials
void WifiManager::setNewWifi(String newSSID, String newPass, String newIp, String newSub, String newGw, String newDns)
{
    ssid = newSSID;
    pass = newPass;
    ip.fromString(newIp);
    sub.fromString(newSub);
    gw.fromString(newGw);
    dns.fromString(newDns);

    connectNewWifiLoopState = SM_START;
}

//function to connect to new WiFi credentials
void WifiManager::connectNewWifi(String newSSID, String newPass)
{
    delay(1000);

    //set static IP or zeros if undefined    
    WiFi.config(ip, gw, sub, dns);

    //fix for auto connect racing issue
    if (!(WiFi.status() == WL_CONNECTED && (WiFi.SSID() == newSSID)) || ip.v4() != configManager.internal.ip  || dns.v4() != configManager.internal.dns)
    {          
        //trying to fix connection in progress hanging
        ETS_UART_INTR_DISABLE();
        wifi_station_disconnect();
        ETS_UART_INTR_ENABLE();

        //store old data in case new network is wrong
        String oldSSID = WiFi.SSID();
        String oldPSK = WiFi.psk();

        WiFi.begin(newSSID.c_str(), newPass.c_str(), 0, NULL, true);
        delay(2000);

        if (WiFi.waitForConnectResult(timeout) != WL_CONNECTED)
        {
            
            Serial.println(PSTR("New connection unsuccessful"));
            if (!inCaptivePortal)
            {
                WiFi.begin(oldSSID, oldPSK, 0, NULL, true);
                if (WiFi.waitForConnectResult(timeout) != WL_CONNECTED)
                {
                    Serial.println(PSTR("Reconnection failed too"));
                    startCaptivePortal(captivePortalName);
                }
                else 
                {
                    Serial.println(PSTR("Reconnection successful"));
                    Serial.println(WiFi.localIP());
                }
            }
        }
        else
        {
            if (inCaptivePortal)
            {
                stopCaptivePortal();
            }

            Serial.println(PSTR("New connection successful"));
            Serial.println(WiFi.localIP());

            //store IP address in EEProm
            storeToEEPROM();

            if ( _newwificallback != NULL) {
                _newwificallback();
            }

        }
    }
}

//function to connect to new WiFi credentials
void WifiManager::connectNewWifiLoop()
{
    switch(connectNewWifiLoopState){

        case SM_IDLE://Do nothing
            break;
            
        case SM_START: 
            connectNewWifiStartInstant = millis();
            connectNewWifiLoopState = SM_WAIT_1000;
            break;

        case SM_WAIT_1000: 
            if(millis() - connectNewWifiStartInstant < 1000)
                break;

            //set static IP or zeros if undefined    
            WiFi.config(ip, gw, sub, dns);

            //fix for auto connect racing issue
            if((WiFi.status() == WL_CONNECTED && (WiFi.SSID() == ssid) && (ip.v4() == configManager.internal.ip) && (dns.v4() == configManager.internal.dns)))
            {
                connectNewWifiLoopState = SM_IDLE;
                break;
            }
            
            //trying to fix connection in progress hanging
            ETS_UART_INTR_DISABLE();
            wifi_station_disconnect();
            ETS_UART_INTR_ENABLE();

            WiFi.begin(ssid.c_str(), pass.c_str(), 0, NULL, true);
            connectNewWifiStartInstant = millis();
            connectNewWifiLoopState = SM_WAIT_2000;
            break;

        case SM_WAIT_2000: 
            if(millis() - connectNewWifiStartInstant < 2000)
                break;
            connectNewWifiLoopState = SM_CONNECT_OR_TIME_OUT;
            break;

        case SM_CONNECT_OR_TIME_OUT:
            
            if(IsConnectionInProgress())
                break;

            connectNewWifiLoopState = SM_IDLE;

            if(IsConnectionSuccessful())
            {
                if (inCaptivePortal)
                    stopCaptivePortal();
                
                _PL(F("New connection successful"));
                _PL(WiFi.localIP());

                //store IP address in EEProm
                storeToEEPROM();

                if (_newwificallback != NULL)
                    _newwificallback();
            }
            else
                _PL(F("New connection unsuccessful"));

            break;

        default: 
            _PL("Unknown state in 'connectNewWifiLoop': " + String(connectNewWifiLoopState));
    }
}

//function to connect to new WiFi credentials
void WifiManager::beginLoop()
{
    switch(beginLoopState)
    {
        case SM_IDLE://Do nothing
            break;
            
        case SM_START:

            STAStartInstant = millis();
            WiFi.mode(WIFI_STA);
            WiFi.persistent(true);
            
            //set static IP if entered
            ip = IPAddress(configManager.internal.ip);
            gw = IPAddress(configManager.internal.gw);
            sub = IPAddress(configManager.internal.sub);
            dns = IPAddress(configManager.internal.dns);

            _PL(F("Static IP data"));
            _PP(F("IP: ")); _PL(ip.toString());
            _PP(F("Gateway: ")); _PL(gw.toString());
            _PP(F("Sub: ")); _PL(sub.toString());
            _PP(F("DNS: ")); _PL(dns.toString());

            if (ip.isSet() || gw.isSet() || sub.isSet() || dns.isSet())
            {
                _PL(F("Using static IP"));
                WiFi.config(ip, gw, sub, dns);
            }

            if (WiFi.SSID() != "")
            {
                //trying to fix connection in progress hanging
                ETS_UART_INTR_DISABLE();
                wifi_station_disconnect();
                ETS_UART_INTR_ENABLE();
                WiFi.begin();
            }

            _PP(F("Trying to connect WiFi: ")); _PL(WiFi.SSID());
            
            beginLoopState = SM_CONNECT_OR_TIME_OUT;
            break;

        case SM_CONNECT_OR_TIME_OUT:
            
            if(IsConnectionInProgress())
                break;

            beginLoopState = SM_IDLE;

            if(IsConnectionSuccessful())
            {
                _PL(F("Connected to stored WiFi details"));
                _PL(WiFi.localIP());

                if (_newwificallback != NULL)
                    _newwificallback();
            }
            else
            {
                //captive portal
                if(captivePortalEnabled)
                    startCaptivePortal(captivePortalName);
            }

            break;

        default: 
            _PL("Unknown state in 'beginLoop': " + String(beginLoopState));
    }
}

//function to start the captive portal
void WifiManager::startCaptivePortal(char const *apName, char const *apPass)
{
    WiFi.persistent(false);
    // disconnect sta, start ap
    WiFi.disconnect(); //  this alone is not enough to stop the autoconnecter
    WiFi.mode(WIFI_AP);
    WiFi.persistent(true);

    WiFi.softAP(apName, apPass);

    dnsServer = new DNSServer();

    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, "*", WiFi.softAPIP());

    _PL(F("Opened a captive portal"));
    _PL(F("192.168.4.1"));
    inCaptivePortal = true;
}

//function to stop the captive portal
void WifiManager::stopCaptivePortal()
{    
    WiFi.mode(WIFI_STA);
    delete dnsServer;

    inCaptivePortal = false;
}

void  WifiManager::forgetWiFiFunctionCallback( std::function<void()> func ) {
  _forgetwificallback = func;
}

void WifiManager::newWiFiFunctionCallback( std::function<void()> func ) {
  _newwificallback = func;
}

//return captive portal state
bool WifiManager::isCaptivePortal()
{
    return inCaptivePortal;
}

//return current SSID
String WifiManager::SSID()
{    
    return WiFi.SSID();
}

//captive portal loop
void WifiManager::loop()
{
    if (inCaptivePortal)
    {
        //captive portal loop
        dnsServer->processNextRequest();
    }

    beginLoop();
    connectNewWifiLoop();
}

//update IP address in EEPROM
void WifiManager::storeToEEPROM()
{
    configManager.internal.ip = ip.v4();
    configManager.internal.gw = gw.v4();
    configManager.internal.sub = sub.v4();
    configManager.internal.dns = dns.v4();
    configManager.save();
}

