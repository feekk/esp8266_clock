#include <U8g2lib.h>
#include <Ticker.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>

// 
// utils function
// 
// 时钟前置时间加0
String twoDigits(int digits) {
    if (digits < 10) {
        String i = '0' + String(digits);
        return i;
    }
    else {
        return String(digits);
    }
}

// variable config

// ap config
String apSSID = "esp8266_Clock";
String apPassword = "12345678";
IPAddress apIP(192, 168, 1, 1); 
IPAddress apGateway(192, 168, 0, 1); 
IPAddress apSubnet(255, 255, 255, 0); 
unsigned char apStatus = 0; // 0未启动 1已启动

// connect wifi
// String wifiSSID = "HUAWEI Mate 20 Pro (UD)";
// String wifiPassword ="12345678";
String wifiSSID = "Tenda_2C8500";
String wifiPassword ="password_0438";
unsigned char wifiStatus = 0; // 0未连接 1已连接
// 检查icon状态
void wifiIconStatusSync(void) {
    if(wifiIsConnected()){
        wifiStatus = 1;
    }else{
        wifiStatus = 0;
    }
}
// 检查wifi是否已连接
bool wifiIsConnected(void){
    return (WiFi.status() == WL_CONNECTED);
}

// 
// OTA
// 
// 固件版本
#define FIRMWARE_VERSION "202307071112"

// 0 none 1 start; 2 progress ;3 finish ;4 error
unsigned char otaProgressStatus = 0;

// ota bin url; when response http status code == 304 then means NO_UPDATES
void otaStart(String url){
    if(!wifiIsConnected() && url != ""){
        return;
    }
    WiFiClient UpdateClient;
    // Add optional callback notifiers
    ESPhttpUpdate.onStart(otaStarted);
    ESPhttpUpdate.onEnd(otaFinished);
    ESPhttpUpdate.onProgress(otaProgress);
    ESPhttpUpdate.onError(otaError);

    if(url.indexOf("?")>0){
        url = url + "&version="+FIRMWARE_VERSION;
    }else{
        url = url+"?version="+FIRMWARE_VERSION;
    }
    
    t_httpUpdate_return ret = ESPhttpUpdate.update(UpdateClient, url);
    switch (ret) {
        case HTTP_UPDATE_FAILED: 
            Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()); 
            break;
        case HTTP_UPDATE_NO_UPDATES: 
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;
        case HTTP_UPDATE_OK: 
            Serial.println("HTTP_UPDATE_OK"); 
            break;
    }
}
// ota Progress callback
void otaStarted() {
    Serial.println("CALLBACK:  HTTP update process started");
    otaProgressStatus = 1;
}
void otaProgress(int cur, int total) {
    Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
    otaProgressStatus = 2;
}
void otaFinished() {
    Serial.println("CALLBACK:  HTTP update process finished");
    otaProgressStatus = 3;
}
void otaError(int err) {
    Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
    otaProgressStatus = 4;
}


// 
// Clock Config
// 
// 开启ntp自动同步
unsigned char clockNtpSyncSwitch = 1; 
unsigned long clockTimestamp = 946656000;
int timeZone = 8;
// ntp server
static char ntpServerName[] = "ntp.ntsc.ac.cn";
// ntp server sync Interval / second
unsigned long ntpSyncInterval = 300;
// 第一次快速获取标志
unsigned long ntpSyncIntervalFastStatus = 0;
// 第一次快速获取时间
unsigned long ntpSyncIntervalFast = 3;

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);
// 设置时间，加上时区
void setColockTimestamp(unsigned long ts) {
    clockTimestamp = ts;
    setTime((time_t)(clockTimestamp + timeZone*SECS_PER_HOUR));
}
// 设置时间，加上时区
void setColockDateTime(int yr, int month, int day, int hr, int min, int sec) {
    setTime(hr, min, sec, day, month, yr);
    unsigned long nowTimestamp = (unsigned long)now() - timeZone*SECS_PER_HOUR;
    setColockTimestamp(nowTimestamp);
}
// 是否开启ntp同步
bool colockSyncNtpIsOpen(void) {
    return clockNtpSyncSwitch > 0;
}
// 计数，时间到进行ntp同步
unsigned char ntpSyncCounter = 0;
void colockSyncNtp(void) {
    if(ntpSyncIntervalFastStatus == 0 && ntpSyncCounter%ntpSyncIntervalFast == 0){
        // 每10秒获取一次
        if(colockSyncNtpIsOpen() && wifiIsConnected() && ntpClient.forceUpdate()){
            Serial.println();    
            Serial.print("ntpSyncIntervalFast success:");
            Serial.print(ntpClient.getEpochTime());
            setColockTimestamp(ntpClient.getEpochTime());
            ntpSyncIntervalFastStatus = 1;
            return;
        }
    }
    if(ntpSyncCounter > ntpSyncInterval){
        ntpSyncCounter = 0;
        if(colockSyncNtpIsOpen() && wifiIsConnected() && ntpClient.forceUpdate()) {
            Serial.println();    
            Serial.print("colockSyncNtp success:");
            Serial.print(ntpClient.getEpochTime());
            setColockTimestamp(ntpClient.getEpochTime());
        }
    }
}

// 
// webserver
// 
ESP8266WebServer server(80);
String configHtml(){
    String datetime = String(year()) + "-" + twoDigits(month()) + "-" +twoDigits(day()) +"T" + twoDigits(hour()) + ":" + twoDigits(minute());
    String clock_ntp_start = "";
    String clock_ntp_stop = "";
    if(colockSyncNtpIsOpen()){
        clock_ntp_start = "checked";
    }else{
        clock_ntp_stop = "checked";
    }
    String screenFlip = "";
    String screenNotFlip = "";
    if(isScreenFlipVertical()){
        screenFlip = "checked";
    }else{
        screenNotFlip = "checked";
    }

    return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>配置页面</title></head><body><div><form action=\"\"><h3>系统配置</h3><label>AP_SSID</label><input type=\"text\"name=\"ap_ssid\"value=\""+apSSID+"\"disabled/><br><label>AP_密码</label><input type=\"text\"name=\"ap_pwd\"value=\""+apPassword+"\"disabled/><br><label>屏幕翻转</label><input type=\"radio\"name=\"flip_vertical\"value=0 "+screenNotFlip+"/><label for=\"clock_sync_start\">正常</label><input type=\"radio\"name=\"flip_vertical\"value=1 "+screenFlip+"/><label for=\"flip_vertical\">翻转</label><br><button>保存</button></form><form action=\"\"><h3>WIFI配置</h3><label for=\"age\">WIFI_SSID</label><input type=\"text\"name=\"wifi_ssid\"value=\""+wifiSSID+"\"/><br><label for=\"age\">WIFI_密码</label><input type=\"text\"name=\"wifi_pwd\"value=\""+wifiPassword+"\"/><br><button>保存</button></form><form action=\"\"><h3>时间配置</h3><label for=\"clock_config\">时钟设置</label><input type=\"datetime-local\"name=\"clock_time\"value=\""+datetime+"\"><br><br><label>时区（时）</label><input type=\"text\"name=\"clock_time_zone\"value=\""+String(timeZone)+"\"disabled/><br><br><label>NTP同步</label><input type=\"radio\"name=\"clock_ntp\"value=1 "+clock_ntp_start+"/><label for=\"clock_sync_start\">启动</label><input type=\"radio\"name=\"clock_ntp\"value=0 "+clock_ntp_stop+"/><label for=\"clock_sync_stop\">禁止</label><br><br><label>NTP同步间隔(秒)</label><input type=\"text\"name=\"clock_ntp_interval\"value=\""+String(ntpSyncInterval)+"\"/><br><br><label>NTP地址</label><input type=\"text\"name=\"clock_ntp_addr\"value=\""+(String)ntpServerName+"\"disabled/><br><button>保存</button></form><form action=\"\"></form><h3>固件信息</h3><label for=\"firmware\">固件版本</label><input type=\"text\"name=\"firmware_version\"value=\""+FIRMWARE_VERSION+"\"disabled><br><br><label>OTA bin文件地址</label><input type=\"text\"name=\"firmware_url\"value=\"\"/><br><button>保存</button></from></div></body></html>";
}

// 配置页面
void config(void) {
    if(server.args() > 0) {
        // 有更新
        Serial.println();    
        Serial.print("rev param");
        if (server.hasArg("flip_vertical")){
            // 屏幕翻转
            setScreenFlipVertical((char)server.arg("flip_vertical").toInt(), false);
        }
        if (server.hasArg("clock_time")){
            // 时钟时间 2023-03-04T14:08
            String clockTime = server.arg("clock_time");
            Serial.println();
            Serial.println(clockTime);
            int year = (clockTime.substring(0,4)).toInt();
            int month = (clockTime.substring(5,7)).toInt();
            int day = (clockTime.substring(8,10)).toInt();
            int hour = (clockTime.substring(11,13)).toInt();
            int min = (clockTime.substring(14,16)).toInt();
            setColockDateTime(year, month, day, hour, min, 0);
        }
        if (server.hasArg("clock_ntp")){
            // ntp状态
            if(server.arg("clock_ntp").toInt() > 0){
                clockNtpSyncSwitch = 1;
            }else{
                clockNtpSyncSwitch = 0;
            }
        }
        if (server.hasArg("clock_ntp_interval")){
            // ntp更新间隔
            ntpSyncInterval = server.arg("clock_ntp_interval").toInt();
        }
        if (server.hasArg("wifi_ssid") && server.hasArg("wifi_pwd")){
            // wifi
            Serial.println();    
            Serial.print("rev param wifi");
            wifiSSID = server.arg("wifi_ssid");
            wifiPassword = server.arg("wifi_pwd");
            WiFi.disconnect();
            WiFi.begin(wifiSSID, wifiPassword);
            WiFi.setAutoConnect(true);
        }
        // OTA升级
        if (server.hasArg("firmware_url")){
            otaStart(server.arg("firmware_url"));
        }

        server.sendHeader("Location", "/", true);
        server.send(302);
        return;
    }
    server.send(200, "text/html", configHtml());
}


// Initialize the OLED display using Wire library
// 128*32

// GPIO define SCK = D1 SDA = D2
#define OLED_SCK 5  
#define OLED_SDA 4
#define OLED_ADDR 0x78
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C display(U8G2_R0, OLED_SCK, OLED_SDA, U8X8_PIN_NONE);

// Turn the display upside down.
// 0 normal ; 1 upside down
unsigned char flipVertical = 1;
bool isScreenFlipVertical(void){
    return flipVertical==1;
}
// 设置翻转
void setScreenFlipVertical(char mode, bool force){
    if(mode != flipVertical || force){
        flipVertical = mode;
        display.setFlipMode(flipVertical);
    }
}

// wifi图标
void showIcon_Wifi(void) {
    if(wifiStatus > 0){
        display.setFont(u8g2_font_siji_t_6x10); 
        display.drawGlyph(116, 8, 0xe21a);
    }
}
// AP热点图标
void showIcon_AP(void) {
    if(apStatus > 0){
        display.setFont(u8g2_font_siji_t_6x10); 
        display.drawGlyph(116, 20, 0xe02d);
    }
}
// 时分
void showTime(void) {
    display.setFont(u8g2_font_VCR_OSD_mn);
    // hour
    display.setCursor(48, 24);
    display.print(twoDigits(hour()));
    // minute
    display.setCursor(80, 24);
    display.print(twoDigits(minute()));
}
// 时分间隔符闪烁 0不显示 1显示
unsigned char ttsStatus = 0;
void triggerTimeSeparator(void){
    if (ttsStatus > 0) {
        ttsStatus = 0;
    } else {
        ttsStatus = 1;
    }
}
void showTimeSeparator(void){
    display.setFont(u8g2_font_9x15_t_symbols);
    if(ttsStatus > 0) {
        display.drawGlyph(70, 20, 0x003a);
    }else{
        display.drawGlyph(70, 20, 0x0020);
    }
}
// 年月日
void showDate(void) {
    display.setFont(u8g2_font_crox2tb_tn);
    // month
    display.setCursor(0, 14);
    display.print(twoDigits(month()));
    // day
    display.setCursor(22, 14);
    display.print(twoDigits(day()));
    // year
    display.setCursor(4, 28);
    display.print(year());
    // separator
    display.setFont(u8g2_font_7x13_t_symbols);
    display.drawGlyph(15, 14, 0x002f);
}
// ota进度
void showOta(void) {
    if(otaProgressStatus == 0){
        return;
    }
    display.setFont(u8g2_font_m2icon_9_tf); 
    if(otaProgressStatus == 1 || otaProgressStatus == 2){
        display.drawGlyph(115, 31, 0x0042); //开始升级 & 升级中
    }
    if(otaProgressStatus == 3){
        display.drawGlyph(115, 31, 0x0044); //升级成功
    }
    if(otaProgressStatus == 4){
        display.drawGlyph(115, 31, 0x0043); //升级失败
    }
}
// 
// ticker
// 
Ticker tickerPerSecond;
// 每秒定时器
void tickerPerSecondCallback(void) {
    clockTimestamp++;
    ntpSyncCounter++;
    triggerTimeSeparator();
    wifiIconStatusSync();
}

// 
// 
// start
// 
// 
void setup() {
    delay(1000);
    // 设置i2c地址，默认是0x78 
    display.setI2CAddress(OLED_ADDR);

    // 串口
    Serial.begin(115200);

    // 时钟初始化
    setColockTimestamp(clockTimestamp);

    // 初始化显示器, 清屏, 唤醒屏幕
    display.begin();
    // 开启Arduino平台下支持输出UTF8字符集
    // display.enableUTF8Print();

    // 屏幕翻转设置
    setScreenFlipVertical(flipVertical, true);
    // display.drawLine(0, 0, 127, 0);
    // display.drawLine(0, 31, 127, 31);
    // display.drawLine(0, 0, 0, 31);
    // display.drawLine(127, 0, 127, 31);
    // display.drawLine(0, 0, 127, 31);
    // display.sendBuffer();

    // 开启web服务，提供配置能力
    server.on("/", config);
    server.begin();

    // 开启AP
    WiFi.softAPConfig(apIP, apGateway, apSubnet);     
    if ( WiFi.softAP(apSSID, apPassword)) {
        apStatus = 1;
        Serial.println();    
        Serial.println("start ap:");    
        Serial.print("SSID:");          
        Serial.println(apSSID);        
        Serial.print("PWD:");          
        Serial.println(apPassword); 
        Serial.print("IP:");          
        Serial.println(apIP); 
        Serial.println(); 

    } else {
        Serial.println();    
        Serial.println("start ap fail");
    }

    // 连接wifi
    WiFi.begin(wifiSSID, wifiPassword);
    WiFi.setAutoConnect(true);
    Serial.println();    
    Serial.print("start wifi:");
    Serial.print(wifiIsConnected());

    // 启动udp
    ntpClient.setPoolServerName(ntpServerName);
    ntpClient.begin();

    // 每秒定时器
    tickerPerSecond.attach(1, tickerPerSecondCallback);
    Serial.println();    
    Serial.print("start tickerPerSecond.");
}

void loop() {
    colockSyncNtp();

    server.handleClient();

    display.clearBuffer();
    showIcon_AP();
    showIcon_Wifi();
    showDate();
    showTime();
    showTimeSeparator();
    showOta();
    // 发送缓冲区的内容到显示器
    display.sendBuffer();
    delay(1000);
}