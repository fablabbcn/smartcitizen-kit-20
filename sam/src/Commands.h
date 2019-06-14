#pragma once

#include <Arduino.h>
#include <Wire.h>

extern TwoWire auxWire;

class SckBase;

// Define command type
enum CommandType {

	COM_RESET,
	COM_GET_VERSION,
	COM_RESET_CAUSE,
	COM_OUTLEVEL,
	COM_HELP,
	COM_PINMUX,
	COM_LIST_SENSOR,
	COM_READ_SENSOR,
	COM_CONTROL_SENSOR,
	COM_MONITOR_SENSOR,
	COM_READINGS,
	COM_GET_FREERAM,
	COM_I2C_DETECT,
	COM_CHARGER,
	COM_CONFIG,
	COM_ESP_CONTROL,
	COM_NETINFO,
	COM_TIME,
	COM_STATE,
	COM_HELLO,
	COM_DEBUG,
	COM_SHELL,
	COM_CUSTOM_MQTT,
	COM_DIMLED,

	COM_COUNT
};

// Declare command function
void reset_com(SckBase* base, String parameters);
void getVersion_com(SckBase* base, String parameters);
void resetCause_com(SckBase* base, String parameters);
void outlevel_com(SckBase* base, String parameters);
void help_com(SckBase* base, String parameters);
void pinmux_com(SckBase* base, String parameters);
void sensorConfig_com(SckBase* base, String parameters);
void readSensor_com(SckBase* base, String parameters);
void controlSensor_com(SckBase* base, String parameters);
void monitorSensor_com(SckBase* base, String parameters);
void readings_com(SckBase* base, String parameters);
void freeRAM_com(SckBase* base, String parameters);
void i2cDetect_com(SckBase* base, String parameters);
void charger_com(SckBase* base, String parameters);
void config_com(SckBase* base, String parameters);
void esp_com(SckBase* base, String parameters);
void netInfo_com(SckBase* base, String parameters);
void time_com(SckBase* base, String parameters);
void state_com(SckBase* base, String parameters);
void hello_com(SckBase* base, String parameters);
void debug_com(SckBase* base, String parameters);
void shell_com(SckBase* base, String parameters);
void custom_mqtt_com(SckBase* base, String parameters);
void ramGet_com(SckBase* base, String parameters);
void dimled_com(SckBase* base, String parameters);

typedef void (*com_function)(SckBase* , String);

class OneCom {
	public:
		uint16_t place;
		CommandType type;
		const char *title;
		const char *help;
		com_function function;

		OneCom(uint16_t nplace, CommandType ntype, const char *nTitle, const char *nhelp, com_function nfunction) {
			place = nplace;
			type = ntype;
			title = nTitle;
			help = nhelp;
			function = nfunction;
		}
};

class AllCommands {
	public:

		OneCom com_list[COM_COUNT] {

			//	place	type 			title 		help 																	function
			OneCom {10,	COM_RESET, 		"reset", 	"Resets the SCK", 															reset_com},
			OneCom {20,	COM_GET_VERSION, 	"version",	"Shows versions and Hardware ID",				 									getVersion_com},
			OneCom {30,	COM_RESET_CAUSE,	"rcause",	"Show last reset cause (debug)",													resetCause_com},
			OneCom {40,	COM_OUTLEVEL,		"outlevel",	"Shows/sets outlevel [0:silent, 1:normal, 2:verbose]",											outlevel_com},
			OneCom {50,	COM_HELP,		"help",		"Duhhhh!!",																help_com},
			OneCom {60,	COM_PINMUX,		"pinmux",	"Shows SAMD pin mapping status",													pinmux_com},
			OneCom {80,	COM_LIST_SENSOR,	"sensor",	"Shows/sets enabled/disabled sensor [-enable or -disable sensor-name] or [-interval sensor-name interval(seconds)]",			sensorConfig_com},
			OneCom {90,	COM_READ_SENSOR,	"read",		"Reads sensor [sensorName]",														readSensor_com},
			OneCom {90,	COM_CONTROL_SENSOR,	"control",	"Control sensor [sensorName] [command]",												controlSensor_com},
			OneCom {90,	COM_MONITOR_SENSOR,	"monitor",	"Continously read sensor [-sd] [-notime] [-noms] [sensorName[,sensorNameN]]",								monitorSensor_com},
			OneCom {90,	COM_READINGS,		"saved",	"Shows locally stored sensor readings [-details] [-publish]",											readings_com},
			OneCom {90,	COM_GET_FREERAM,	"free",		"Shows the amount of free RAM memory",													freeRAM_com},
			OneCom {90,	COM_I2C_DETECT,		"i2c",		"Search the I2C bus for devices",													i2cDetect_com},
			OneCom {90,	COM_CHARGER,		"charger",	"Controls or shows charger configuration [-otg on/off] [-charge on/off]",								charger_com},
			OneCom {90,	COM_CONFIG,		"config",	"Shows/sets configuration [-defaults] [-mode sdcard/network] [-pubint seconds] [-readint seconds] [-wifi \"ssid\" [\"pass\"]] [-token token]", config_com},
			OneCom {100,	COM_ESP_CONTROL,	"esp",		"Controls or shows info from ESP [-on -off -sleep -wake -reboot -flash]",								esp_com},
			OneCom {100,	COM_NETINFO,		"netinfo",	"Shows network information",														netInfo_com},
			OneCom {100,	COM_TIME,		"time",		"Shows/sets time [epoch time] [-sync]",													time_com},
			OneCom {100,	COM_STATE,		"state",	"Shows state flags",															state_com},
			OneCom {100,	COM_HELLO,		"hello",	"Sends MQTT hello to platform",														hello_com},
			OneCom {100,	COM_DEBUG, 		"debug", 	"Toggle debug messages [-sdcard] [-espcom] [-list]", 												debug_com},
			OneCom {100,	COM_SHELL, 		"shell", 	"Shows or sets shell mode [-on] [-off]",												shell_com},
			OneCom {100,	COM_CUSTOM_MQTT,	"mqtt", 	"Publish custom mqtt message ('topic' 'message')",											custom_mqtt_com},
			OneCom {100,	COM_DIMLED,		"dimled", 	"Show/set brightness of LED [float]",													dimled_com},
		};

		OneCom & operator[](CommandType type) {
			return com_list[type];
		};

		void in(SckBase* base, String strIn);
		void wildCard(SckBase* base, String strIn);

	private:

};
