//
// ESP8266 Mobile Rick Roll Captive Portal
// - User connects thinking it is a free WiFi access point
// - After accepting Terms of Service they are Rick Roll'd
//
// References (I learned everything I needed to create this from these projects)
// PlatformIO - http://platformio.org/
//              http://docs.platformio.org/en/latest/platforms/espressif.html#uploading-files-to-file-system-spiffs
// ESP8266 Captive Portal - https://www.hackster.io/rayburne/esp8266-captive-portal-5798ff
// ESP-RADIO - https://github.com/Edzelf/Esp-radio
// ESPAsyncWebServer - https://github.com/me-no-dev/ESPAsyncWebServer
// SOFTAP - Get List of Connected Clients - http://www.esp8266.com/viewtopic.php?f=32&t=5669
// How to print free memory - http://forum.wemos.cc/topic/175/how-to-print-free-memory
// WebSocketToSerial - https://github.com/hallard/WebSocketToSerial.git
// JQueryTerminal - https://github.com/jcubic/jquery.terminal
// OTA Update - http://www.penninkhof.com/2015/12/1610-over-the-air-esp8266-programming-using-platformio/
// Piezo Beep - http://framistats.com/2015/06/07/esp8266-arduino-smtp-server-part-2/
// Simple Audio Board - http://bitcows.com/?p=19
// Tone Doesn't Work - https://www.reddit.com/r/esp8266/comments/46kw38/esp8266_calling_tone_function_does_not_work/
// ArduinoJson - https://github.com/bblanchon/ArduinoJson
// EEPROM - https://gist.github.com/dogrocker/f998dde4dbac923c47c1
// Exception Causes - https://github.com/esp8266/Arduino/blob/master/doc/exception_causes.md
// WiFi Scan- https://www.linuxpinguin.de/project/wifiscanner/
// SPIFFS - https://github.com/esp8266/Arduino/blob/master/doc/filesystem.md
//          http://blog.squix.org/2015/08/esp8266arduino-playing-around-with.html
// WiFiManager - https://github.com/tzapu/WiFiManager
// ESP-GDBStub - https://github.com/esp8266/Arduino/tree/master/libraries/GDBStub

#include <stdio.h>
#include <string.h>

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>

#include <Hash.h>
#include <Ticker.h>

#include "DNSServer.h"

#include "Display/DisplayController.h"
#include "Spiffs/SpiffsHandler.h"

#include "Helper.h"


extern "C"
{
#include "user_interface.h"
    void system_set_os_print ( uint8 onoff );
    void ets_install_putc1 ( void *routine );
}

// Set ADC for Voltage Monitoring
ADC_MODE ( ADC_VCC );

// #define OLED_RESET 0  // GPIO0
// Adafruit_SSD1306 display(OLED_RESET);
DisplayController display;
SpiffsHandler spiffs;

const int buttonPinA = 0;
const int buttonPinB = 2;
int buttonStateA = 0;
int buttonStateB = 0;
bool DEBUG = false;

// Use the internal hardware buffer
static void _u0_putc ( char c )
{
    while ( ( ( U0S >> USTXC ) & 0x7F ) == 0x7F ) ;
    U0F = c;
}

//
//******************************************************************************************
// Forward declaration of methods                                                          *
//******************************************************************************************
int setupAP ( int chan_selected );

//***************************************************************************
// Global data section.                                                     *
//***************************************************************************
float version               = 1.43;
const char *appid           = "mobile-rr";
char ssid[]                 = "FREE Highspeed WiFi";
int channel                 = 0;
char username[]             = "admin";
char password[]             = "";
bool SILENT                 = 0;
int interval                = 30; // 30 Minutes
bool READY                  = false;

#define CLIENT_NONE     0
#define CLIENT_ACTIVE   1

enum class statemachine
{
    none,
    scan_wifi,
    ap_change,
    redraw_display
};
statemachine state = statemachine::none;
int state_int;
String state_string;

IPAddress ip ( 10, 10, 10, 1 );      // Private network for httpd
DNSServer dnsd;                      // Create the DNS object
MDNSResponder mdns;

AsyncWebServer httpd ( 80 );         // Instance of embedded webserver
//AsyncWebServer  httpsd ( 443 );

Ticker timer;                        // Setup Auto Scan Timer


int clients = 0;
int rrsession;                       // Rick Roll Count Session
int rrsessiondebug = 0;
int rrtotal;                         // Rick Roll Count Total
int rrtotaldebug = 0;
char str_vcc[8];
int chan_selected;

//***************************************************************************
// End of global data section.                                              *
//***************************************************************************

// WiFi Encryption Types
String encryptionTypes ( int which )
{
    switch ( which )
    {
        case ENC_TYPE_WEP:
            return "WEP";
            break;

        case ENC_TYPE_TKIP:
            return "WPA/TKIP";
            break;

        case ENC_TYPE_CCMP:
            return "WPA2/CCMP";
            break;

        case ENC_TYPE_NONE:
            return "None";
            break;

        case ENC_TYPE_AUTO:
            return "Auto";
            break;

        default:
            return "Unknown";
            break;
    }
}



/** IP to String? */
String ipToString ( IPAddress ip )
{
    String res = "";

    for ( int i = 0; i < 3; i++ )
    {
        res += String ( ( ip >> ( 8 * i ) ) & 0xFF ) + ".";
    }

    res += String ( ( ( ip >> 8 * 3 ) ) & 0xFF );
    return res;
}



//***************************************************************************
//                    S E T U P                                             *
//***************************************************************************
void setup ( void )
{
    uint8_t mac[6];
    char mdnsDomain[] = "";

    ets_install_putc1 ( ( void * ) &_u0_putc );
    system_set_os_print ( 1 );
    // // Set CPU to 80/160 MHz
    // system_update_cpu_freq ( 160 );

    //Serial.begin ( 921600 );
    Serial.begin ( 9600 );
    Serial.println();

    // Setup display
    display.setupDisplay();

   // pinMode ( LED_BUILTIN, OUTPUT );    // initialize onboard LED as output
   // digitalWrite ( LED_BUILTIN, HIGH ); // Turn the LED off by making the voltage HIGH
    // initialize the pushbutton pin as an input:
    pinMode(buttonPinA, INPUT);
    pinMode(buttonPinB, INPUT);

    // Startup Banner
    dbg_printf (
        "--------------------\n"
        "ESP8266 Mobile Rick Roll Captive Portal\n"
    );

    // Load EEPROM Settings
    setupEEPROM();

    // Setup Access Point
    wifi_set_phy_mode ( PHY_MODE_11B );
    WiFi.mode ( WIFI_AP );
    WiFi.softAPConfig ( ip, ip, IPAddress ( 255, 255, 255, 0 ) );
    chan_selected = setupAP ( channel );
    WiFi.softAP ( ssid, NULL, chan_selected );
    WiFi.softAPmacAddress ( mac );

    // Show Soft AP Info
    dbg_printf (
        "SoftAP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n" \
        "SoftAP IP: %s\n",
        MAC2STR ( mac ),
        ipToString ( ip ).c_str()
    );

/**

    dbg_printf ( "SYSTEM ---" );
    dbg_printf ( "getSdkVersion:      %s", ESP.getSdkVersion() );
    dbg_printf ( "getBootVersion:     %08X", ESP.getBootVersion() );
    dbg_printf ( "getBootMode:        %08X", ESP.getBootMode() );
    dbg_printf ( "getChipId:          %08X", ESP.getChipId() );
    dbg_printf ( "getCpuFreqMHz:      %d Mhz", ESP.getCpuFreqMHz() );
    dbg_printf ( "getCycleCount:      %u\n", ESP.getCycleCount() );

    dbg_printf ( "POWER ---" );
    dbg_printf ( "Voltage:            %d.%d v\n ", ( ESP.getVcc() / 1000 ), ( ESP.getVcc() % 1000 ) );

    dbg_printf ( "MEMORY ---" );
    dbg_printf ( "getFreeHeap:        %d B", ESP.getFreeHeap() );
    dbg_printf ( "getSketchSize:      %s", formatBytes ( ESP.getSketchSize() ).c_str() );
    dbg_printf ( "getFreeSketchSpace: %s", formatBytes ( ESP.getFreeSketchSpace() ).c_str() );
    dbg_printf ( "getFlashChipSize:   %s", formatBytes ( ESP.getFlashChipRealSize() ).c_str() );
    dbg_printf ( "getFlashChipSpeed:  %d MHz\n", int ( ESP.getFlashChipSpeed() / 1000000 ) );


**/

    struct	rst_info	*rtc_info	=	system_get_rst_info();

dbg_printf("reset	reason:	%x\n",	rtc_info->reason);

	 if	(rtc_info->reason	==	REASON_WDT_RST	||

	 	 rtc_info->reason	==	REASON_EXCEPTION_RST	||

	 	 rtc_info->reason	==	REASON_SOFT_WDT_RST)	{

	 	 if	(rtc_info->reason	==	REASON_EXCEPTION_RST)	{

	 	 	 dbg_printf("Fatal	exception	(%d):\n",	rtc_info->exccause);

	 	 }

	 	 dbg_printf("epc1=0x%08x,	epc2=0x%08x,	epc3=0x%08x,	excvaddr=0x%08x,depc=0x%08x\n",

	 	 	 	 rtc_info->epc1,	rtc_info->epc2,	rtc_info->epc3,	rtc_info->excvaddr,	rtc_info->depc);

//The	address	of	the	last	crash	is	printed,	which	is	used	to	debug	garbled	output.
	 }




    // Start File System
    spiffs.setupSPIFFS();

    ESP.wdtFeed();

    setupDNSServer();

    #define DEBUG_ESP_MDNS_RX


    ESP.wdtFeed();
    sprintf ( mdnsDomain, "%s.local", appid );
    dbg_printf ( "Starting mDNS Responder" );

    if ( !mdns.begin ( mdnsDomain, ip ) )
    {
        Serial.println ( ">>>>> Error setting up mDNS responder!" );
    }
    else
    {
        dbg_printf ( ">>>>> mDNS Domain: %s", mdnsDomain );
    }

    dbg_printf("mDnshere");

    setupHTTPServer();
    setupOTAServer();

    // Setup Timer
    if ( interval )
    {
        dbg_printf ( "Starting Auto WiFi Scan Timer" );
        timer.attach_ms ( ( 1000 * 60 * interval ), onTimer );
    }

    // Setup wifi connection callbacks
    wifi_set_event_handler_cb ( wifi_handle_event_cb );

    dbg_printf ( "\nReady!\n--------------------" );
    READY = true;
    state = statemachine::redraw_display;
}

int setupAP ( int chan_selected )
{
    struct softap_config config;

    // Get config first.
    wifi_softap_get_config ( &config );

    if ( chan_selected == 0 )
    {
        chan_selected = scanWiFi();
    }

    //dbg_printf ( "WiFi \n\tSSID: %s\n\tPassword: %s\n\t Channel: %d", config.ssid, config.password, config.channel );
    //dbg_printf ( "Channel %d Selected!", chan_selected );
    //dbg_printf ( "SSID: %s", ssid );
    //dbg_printf ( "Compare: %d", strcmp ( (char*)config.ssid, ssid ) );

    if ( config.channel != chan_selected || strcmp ( (char*)config.ssid, ssid ) )
    {
        if ( config.channel != chan_selected )
          dbg_printf ( "Changing WiFi channel from %d to %d.", config.channel, chan_selected );

        if ( strcmp ( (char*)config.ssid, ssid ) )
          dbg_printf ( "Changing SSID from '%s' to '%s'.", (char*)config.ssid, ssid );

        WiFi.softAPdisconnect ( true );
        WiFi.mode ( WIFI_AP );
        wifi_set_phy_mode ( PHY_MODE_11B );
        WiFi.softAPConfig ( ip, ip, IPAddress ( 255, 255, 255, 0 ) );
        // WiFi.softAP ( ssid, NULL, chan_selected );
        WiFi.softAP("FREE Highspeed Wifi", NULL, chan_selected);


    }
    else
    {
        dbg_printf ( "No change." );
    }

    return chan_selected;
}


void setupEEPROM()
{
    dbg_printf ( "EEPROM - Checking" );
    EEPROM.begin ( 512 );
    eepromLoad();
    dbg_printf ( "" );
}

void setupSPIFFS()
{
    FSInfo fs_info;  // Info about SPIFFS
    Dir dir;         // Directory struct for SPIFFS
    File f;          // Filehandle
    String filename; // Name of file found in SPIFFS

    // Enable file system
    SPIFFS.begin();

    // Show some info about the SPIFFS
    uint16_t cnt = 0;
    SPIFFS.info ( fs_info );
    dbg_printf ( "SPIFFS Files\nName                           -      Size" );
    // Show files in FS
    dir = SPIFFS.openDir ( "/" );

    // All files
    while ( dir.next() )
    {
        f = dir.openFile ( "r" );
        filename = dir.fileName();
        // Show name and size
        dbg_printf ( "%-30s - %9s",
                     filename.c_str(),
                     formatBytes ( f.size() ).c_str()
                   );
        cnt++;
    }

    dbg_printf ( "%d Files, %s of %s Used",
                 cnt,
                 formatBytes ( fs_info.usedBytes ).c_str(),
                 formatBytes ( fs_info.totalBytes ).c_str()
               );
    dbg_printf ( "%s Free\n",
                 formatBytes ( fs_info.totalBytes - fs_info.usedBytes ).c_str()
               );
}

void setupDNSServer()
{
    // Setup DNS Server
    // if DNS Server is started with "*" for domain name,
    // it will reply with provided IP to all DNS request
    dbg_printf ( "Starting DNS Server" );
    dnsd.onQuery ( [] ( const IPAddress & remoteIP, const char *domain, const IPAddress & resolvedIP )
    {
        dbg_printf ( "DNS Query [%d]: %s -> %s", remoteIP[3], domain, ipToString ( resolvedIP ).c_str() );
    } );
    dnsd.onOverride ( [] ( const IPAddress & remoteIP, const char *domain, const IPAddress & overrideIP )
    {
        dbg_printf ( "DNS Override [%d]: %s -> %s", remoteIP[3], domain, ipToString ( overrideIP ).c_str() );
    } );
    //dnsd.setErrorReplyCode ( DNSReplyCode::NoError );
    //dnsd.setTTL(0);
    dnsd.start ( 53, "*", ip );
}


void setupHTTPServer()
{
    // Web Server Document Setup
    dbg_printf ( "Starting HTTP Captive Portal" );

    // Handle requests
    httpd.on ( "/generate_204", onRequest );  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
    httpd.on ( "/fwlink", onRequest ); //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
    httpd.on ( "/hotspot-detect.html", onRequest ); //Apple captive portal. Maybe not needed. Might be handled by notFound handler.
    httpd.onNotFound ( onRequest );

    httpd.on ( "/trigger", HTTP_GET, [] ( AsyncWebServerRequest * request )
    {
        if(DEBUG) {
          rrsessiondebug++;
          rrtotaldebug++;
        } else {
          rrsession++;
          rrtotal++;
        }

        String remoteIP = ipToString ( request->client()->remoteIP() );

        request->send ( 200, "text/html", String ( rrsession ) );
        eepromSave();
        display.redrawDisplay(rrtotal, rrsession, clients, READY);

        // Disconnect that station
        //wifi_softap_dhcps_client_leave(NULL, remoteIP, true);
    } );

    httpd.on ( "/info", HTTP_GET, [] ( AsyncWebServerRequest * request )
    {
        AsyncWebServerResponse *response = request->beginResponse ( 200, "text/html", getSystemInformation() );
        response->addHeader ( "Access-Control-Allow-Origin", "http://localhost" );
        request->send ( response );
    } );

    httpd.begin();
}

void setupOTAServer()
{
    dbg_printf ( "Starting OTA Update Server" );

    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname ( appid );

    // No authentication by default
    ArduinoOTA.setPassword ( password );

    // OTA callbacks
    ArduinoOTA.onStart ( []()
    {
        // Clean SPIFFS
        SPIFFS.end();

        // Let connected clients know what's going on
        dbg_printf ( "OTA Update Started" );
    } );
    ArduinoOTA.onEnd ( []()
    {
        dbg_printf ( "OTA Update Complete!\n" );
    } );
    ArduinoOTA.onProgress ( [] ( unsigned int progress, unsigned int total )
    {
        dbg_printf ( "Progress: %u%%\r", ( progress / ( total / 100 ) ) );
    } );
    ArduinoOTA.onError ( [] ( ota_error_t error )
    {
        dbg_printf ( "Error[%u]: ", error );

        if ( error == OTA_AUTH_ERROR ) dbg_printf ( "OTA Auth Failed" );
        else if ( error == OTA_BEGIN_ERROR ) dbg_printf ( "OTA Begin Failed" );
        else if ( error == OTA_CONNECT_ERROR ) dbg_printf ( "OTA Connect Failed" );
        else if ( error == OTA_RECEIVE_ERROR ) dbg_printf ( "OTA Receive Failed" );
        else if ( error == OTA_END_ERROR ) dbg_printf ( "OTA End Failed" );
    } );
    ArduinoOTA.begin();
}

int scanWiFi ()
{
    int channels[11];
    std::fill_n ( channels, 11, 0 );

    dbg_printf ( "Scanning WiFi Networks" );

    for ( int count = 1; count < 4; count++ )
    {
        int networks = WiFi.scanNetworks();
        dbg_printf ( "Scan %d, %d Networks Found", count, networks );
    //    dbg_printf ( "RSSI  CHANNEL  ENCRYPTION  BSSID              SSID" );

        for ( int network = 0; network < networks; network++ )
        {
            String ssid_scan;
            int32_t rssi_scan;
            uint8_t sec_scan;
            uint8_t *BSSID_scan;
            int32_t chan_scan;
            bool hidden_scan;
            WiFi.getNetworkInfo ( network, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan );

            // dbg_printf ( "%-6d%-9d%-12s%02X:%02X:%02X:%02X:%02X:%02X  %s",
            //              rssi_scan,
            //              chan_scan,
            //              encryptionTypes ( sec_scan ).c_str(),
            //              MAC2STR ( BSSID_scan ),
            //              ssid_scan.c_str()
            //            );

            channels[ chan_scan ]++;
        }

        WiFi.scanDelete();
    }

    // Find least used channel
    int lowest_count = 10000;

    for ( int channel = 1; channel <= 11; channel++ )
    {
        // Include side channels to account for signal overlap
        int current_count = 0;

        for ( int i = channel - 4; i <= ( channel + 4 ); i++ )
        {
            if ( i > 0 )
                current_count += channels[i];
        }

        if ( current_count < lowest_count )
        {
            lowest_count = current_count;
            chan_selected = channel;
        }

        //Serial.printf( "Channel %d = %d\n", channel, current_count);
    }

    dbg_printf ( "Channel %d is least used.", chan_selected );

    return chan_selected;
}

String getSystemInformation()
{
    String json;
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["sdk_version"] = ESP.getSdkVersion();
    root["boot_version"] = ESP.getBootVersion();
    root["boot_mode"] = ESP.getBootMode();
    root["chipid"] = ESP.getChipId();
    root["cpu_frequency"] = ESP.getCpuFreqMHz();

    root["cycle_count"] = ESP.getCycleCount();
    root["voltage"] = ( ESP.getVcc() / 1000 );

    root["free_memory"] = ESP.getFreeHeap();
    root["sketch_size"] = ESP.getSketchSize();
    root["sketch_free"] = ESP.getFreeSketchSpace();

    root["flash_size"] = ESP.getFlashChipRealSize();
    root["flash_speed"] = ( ESP.getFlashChipSpeed() / 1000000 );

    FSInfo fs_info;
    SPIFFS.info ( fs_info );
    root["spiffs_size"] = fs_info.totalBytes;
    root["spiffs_used"] = fs_info.usedBytes;
    root["spiffs_free"] = ( fs_info.totalBytes - fs_info.usedBytes );

    char apIP[16];
    sprintf ( apIP, "%s", ipToString ( WiFi.softAPIP() ).c_str() );
    root["softap_mac"] = WiFi.softAPmacAddress();
    root["softap_ip"] = apIP;

    char staIP[16];
    sprintf ( staIP, "%s", ipToString ( WiFi.localIP() ).c_str() );
    root["station_mac"] = WiFi.macAddress();
    root["station_ip"] = staIP;

    root.printTo ( json );
    return json;
}

String getApplicationSettings()
{
    String json;
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["version"] = version;
    root["appid"] = appid;
    root["ssid"] = ssid;
    root["channel"] = chan_selected;
    root["interval"] = ( interval );
    root["username"] = username;
    root["password"] = password;
    root["silent"] = SILENT;
    root["rrsession"] = rrsession;
    root["rrtotal"] = rrtotal;

    root.printTo ( json );
    return json;
}

void onTimer ()
{
    dbg_printf ( "Auto WiFi scan initiated!" );
    chan_selected = 0;
    state = statemachine::ap_change;
}

void eepromLoad()
{
    String json;
    StaticJsonBuffer<512> jsonBuffer;

    int i = 0;

    while ( EEPROM.read ( i ) != 0 )
    {
        json += char ( EEPROM.read ( i ) );
        i++;
    }

    //dbg_printf("EEPROM[%s]", json.c_str());

    JsonObject &root = jsonBuffer.parseObject ( json );

    // If Parsing failed EEPROM isn't initialized
    if ( !root.success() )
    {
        eepromInitialize();
        eepromSave();
    }
    else
    {
        // Load Settings from JSON
        sprintf ( ssid, "%s", root["ssid"].as<char*>() );
        channel = root["channel"];
        interval = root["interval"];
        sprintf ( username, "%s", root["username"].as<char*>() );
        sprintf ( password, "%s", root["password"].as<char*>() );

        SILENT = root["silent"];
        rrtotal = root["rrtotal"];

        // If the AppID doesn't match then initialize EEPROM
        if ( version != root.get<float> ( "version" ) )
        {
            dbg_printf ( "EEPROM - Version Changed" );
            eepromInitialize();
            eepromSave();
            dbg_printf ( "EEPROM - Upgraded" );
        }

        dbg_printf ( "EEPROM - Loaded" );
        root.prettyPrintTo ( Serial );
        Serial.println();
    }
}

void eepromSave()
{
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["version"] = version;
    root["appid"] = appid;
    root["ssid"] = ssid;
    root["channel"] = channel;
    root["interval"] = interval;
    root["username"] = username;
    root["password"] = password;
    root["silent"] = SILENT;
    root["rrtotal"] = rrtotal;

    char buffer[512];
    root.printTo ( buffer, sizeof ( buffer ) );

    int i = 0;

    while ( buffer[i] != 0 )
    {
        EEPROM.write ( i, buffer[i] );
        i++;
    }

    EEPROM.write ( i, buffer[i] );
    EEPROM.commit();

    dbg_printf ( "EEPROM - Saved" );
    root.prettyPrintTo ( Serial );
    Serial.println();
}

void eepromInitialize()
{
    dbg_printf ( "EEPROM - Initializing" );

    for ( int i = 0; i < 512; ++i )
    {
        EEPROM.write ( i, 0 );
    }

    EEPROM.commit();
}

String getEEPROM()
{
    String json;

    int i = 0;

    while ( EEPROM.read ( i ) != 0 )
    {
        json += char ( EEPROM.read ( i ) );
        i++;
    }

    return json;
}

bool disconnectStationByIP ( IPAddress station_ip )
{
    // Do ARP Query to get MAC address of station_ip
}

bool disconnectStationByMAC ( uint8_t *station_mac )
{
}

//***************************************************************************
//                    L O O P                                               *
//***************************************************************************
// Main program loop.                                                       *
//***************************************************************************
void loop ( void )
{
    dnsd.processNextRequest();
    ArduinoOTA.handle(); // Handle remote Wifi Updates

    buttonStateA = digitalRead(buttonPinA);
    buttonStateB = digitalRead(buttonPinB);

    if(buttonStateA == LOW || buttonStateB == LOW) {
      if(DEBUG) {
        DEBUG = false;
        dbg_printf("Setting DEBUG=false");
        state = statemachine::redraw_display;
      } else {
        DEBUG = true;
        dbg_printf("Setting DEBUG=true");
        state = statemachine::redraw_display;
      }
      delay(200);
    }

    switch ( state )
    {
        case statemachine::scan_wifi:
            scanWiFi();
            break;
        case statemachine::ap_change:
            chan_selected = setupAP ( chan_selected );
            break;
        case statemachine::redraw_display:
            display.redrawDisplay(rrtotal, rrsession, clients, READY);
            break;
    }

    state = statemachine::none;
    state_int = 0;
    state_string = "";
}

void wifi_handle_event_cb ( System_Event_t *evt )
{
//    printf ( "event %x\n", evt->event );

    switch ( evt->event )
    {
        case EVENT_STAMODE_CONNECTED:
            printf ( "connect to ssid %s, channel %d\n",
                     evt->event_info.connected.ssid,
                     evt->event_info.connected.channel );
            break;

        case EVENT_STAMODE_DISCONNECTED:
            printf ( "disconnect from ssid %s, reason %d\n",
                     evt->event_info.disconnected.ssid,
                     evt->event_info.disconnected.reason );
            break;

        case EVENT_STAMODE_AUTHMODE_CHANGE:
            printf ( "mode: %d -> %d\n",
                     evt->event_info.auth_change.old_mode,
                     evt->event_info.auth_change.new_mode );
            break;

        case EVENT_STAMODE_GOT_IP:
            printf ( "ip: " IPSTR " ,mask: " IPSTR " ,gw: " IPSTR,
                     IP2STR ( &evt->event_info.got_ip.ip ),
                     IP2STR ( &evt->event_info.got_ip.mask ),
                     IP2STR ( &evt->event_info.got_ip.gw )
                   );
            printf ( "\n" );
            break;

        case EVENT_SOFTAPMODE_STACONNECTED:  // 5
            clients = clients + 1;
            state = statemachine::redraw_display;
            printf ( "station connected: %02X:%02X:%02X:%02X:%02X:%02X, AID = %d\n",
                     MAC2STR ( evt->event_info.sta_connected.mac ),
                     evt->event_info.sta_connected.aid );                 
            break;

        case EVENT_SOFTAPMODE_STADISCONNECTED:  // 6
            clients = clients - 1;
            state = statemachine::redraw_display;
            printf ( "station disconnected: %02X:%02X:%02X:%02X:%02X:%02X, AID = %d\n",
                     MAC2STR ( evt->event_info.sta_disconnected.mac ),
                     evt->event_info.sta_disconnected.aid );
            break;

        default:
            break;
    }
}

//***************************************************************************
// HTTPD onRequest                                                          *
//***************************************************************************
void onRequest ( AsyncWebServerRequest *request )
{
    // Turn the LED on by making the voltage LOW
    digitalWrite ( LED_BUILTIN, LOW );

    IPAddress remoteIP = request->client()->remoteIP();
    dbg_printf (
        "HTTP[%d]: %s%s",
        remoteIP[3],
        request->host().c_str(),
        request->url().c_str()
    );

    String path = request->url();

    // dbg_printf ("Path:      %s", path.c_str());
    // dbg_printf ("Host:      %s", request->host().c_str());
    // dbg_printf ("File?:     %d", !SPIFFS.exists(path));
    // dbg_printf ("File(gz)?: %d", !SPIFFS.exists(path + ".gz"));
    // dbg_printf ("Host?:     %d", (request->host() != "10.10.10.1"));

    if ( ( !SPIFFS.exists ( path ) && !SPIFFS.exists ( path + ".gz" ) ) || ( request->host() != "10.10.10.1" ) )
    {
        AsyncWebServerResponse *response = request->beginResponse ( 302, "text/plain", "" );
        response->addHeader ( "Cache-Control", "no-cache, no-store, must-revalidate" );
        response->addHeader ( "Pragma", "no-cache" );
        response->addHeader ( "Expires", "-1" );
        dbg_printf ("[REDIRECT] %s -> http://10.10.10.1/index.htm", path.c_str());
        response->addHeader ( "Location", "http://10.10.10.1/index.htm" );
        request->send ( response );
    }
    else
    {
        char s_tmp[] = "";
        AsyncWebServerResponse *response;

        if ( !request->hasParam ( "download" ) && SPIFFS.exists ( path + ".gz" ) )
        {
            response = request->beginResponse ( SPIFFS, path, String(), request->hasParam ( "download" ) );
        }
        else
        {
            // Okay, send the file
            response = request->beginResponse ( SPIFFS, path );
        }

        response->addHeader ( "Cache-Control", "no-cache, no-store, must-revalidate" );
        response->addHeader ( "Pragma", "no-cache" );
        response->addHeader ( "Expires", "-1" );
        sprintf ( s_tmp, "%d", ESP.getFreeHeap() );
        response->addHeader ( "ESP-Memory", s_tmp );
        request->send ( response );
    }

    // Turn the LED off by making the voltage HIGH
    digitalWrite ( LED_BUILTIN, HIGH );
}
