#include "SckBase.h"
#include "Commands.h"

// Hardware Auxiliary I2C bus
TwoWire auxWire(&sercom1, pinAUX_WIRE_SDA, pinAUX_WIRE_SCL);
void SERCOM1_Handler(void) {

	auxWire.onService();
}

// ESP communication
RH_Serial driver(SerialESP);
RHReliableDatagram manager(driver, SAM_ADDRESS);

// Auxiliary I2C devices
AuxBoards auxBoards;

// Eeprom flash emulation to store persistent variables
FlashStorage(eepromConfig, Configuration);

void SckBase::setup()
{
	// Led
	led.setup();

	// ESP Configuration
	pinMode(pinPOWER_ESP, OUTPUT);
	pinMode(pinESP_CH_PD, OUTPUT);
	pinMode(pinESP_GPIO0, OUTPUT);
	SerialESP.begin(serialBaudrate);
	manager.init();
	manager.setTimeout(30);
	manager.setRetries(16);
	ESPcontrol(ESP_ON);

	// Internal I2C bus setup
	Wire.begin();

	// Auxiliary I2C bus
	pinMode(pinPOWER_AUX_WIRE, OUTPUT);
	digitalWrite(pinPOWER_AUX_WIRE, LOW);	// LOW -> ON , HIGH -> OFF
	pinPeripheral(pinAUX_WIRE_SDA, PIO_SERCOM);
	pinPeripheral(pinAUX_WIRE_SCL, PIO_SERCOM);
	auxWire.begin();
	delay(2000); 				// Give some time for external boards to boot

	// Button
	pinMode(pinBUTTON, INPUT_PULLUP);
	LowPower.attachInterruptWakeup(pinBUTTON, ISR_button, CHANGE);

	// Power management configuration
	charger.setup();
	battery.setup();

	// RTC setup
	rtc.begin();
	if (rtc.isConfigured() && (rtc.getEpoch() > 1514764800)) st.timeStat.setOk();	// If greater than 01/01/2018
	else {
		rtc.setTime(0, 0, 0);
		rtc.setDate(1, 1, 15);
	}
	espStarted = rtc.getEpoch();

	// Sanity cyclic reset: If the clock is synced the reset will happen 3 hour after midnight (UTC) otherwise the reset will happen 3 hour after booting
	rtc.setAlarmTime(wakeUP_H, wakeUP_M, wakeUP_S);
	rtc.enableAlarm(rtc.MATCH_HHMMSS);
	rtc.attachInterrupt(ext_reset);

	// SDcard and flash select pins
	pinMode(pinCS_SDCARD, OUTPUT);
	pinMode(pinCS_FLASH, OUTPUT);
	digitalWrite(pinCS_SDCARD, HIGH);
	digitalWrite(pinCS_FLASH, HIGH);
	pinMode(pinCARD_DETECT, INPUT_PULLUP);

	// SD card
	SerialUSB.println("Setting up SDcard interrupt");
	attachInterrupt(pinCARD_DETECT, ISR_sdDetect, CHANGE);
	sdDetect();

	// Flash storage
	/* readingsList.flashStart(); */

/* #define autoTest  // Uncomment for doing Gases autotest, you also need to uncomment  TODO complete this */

#ifdef autoTest
	// TODO verify led blinking...
	ESPcontrol(ESP_OFF);
	sckOut("Starting Gases Pro Board automated test...", PRIO_HIGH);
	led.off();
	led.update(led.BLUE, led.PULSE_STATIC);
	String testResult = auxBoards.control(SENSOR_GASESBOARD_SLOT_1W, "autotest");
	SerialUSB.println(testResult);
	if (testResult.startsWith("1")) {
		sckOut("Test finished OK!!");
		led.update(led.GREEN, led.PULSE_STATIC);
	} else {
		sckOut("ERROR Test failed, please check your connections");
		led.update(led.RED, led.PULSE_STATIC);
	}
	while(true);
#endif

	// Configuration
	ESPcontrol(ESP_REBOOT);
	loadConfig();
	if (st.mode == MODE_NOT_CONFIGURED) writeHeader = true;

	bool saveNeeded = false;

	// Urban board
	analogReadResolution(12);
	if (urban.setup()) {
		sckOut("Urban board detected");
		urbanPresent = true;

		// Find out if urban was reinstalled just now
		bool justInstalled = true;
		for (uint8_t i=0; i<SENSOR_COUNT; i++) {
			OneSensor *wichSensor = &sensors[static_cast<SensorType>(i)];
			if (wichSensor->location == BOARD_URBAN && wichSensor->enabled) {
				justInstalled = false;
			}
		}

		if (justInstalled) {
			sckOut("Enabling default sensors...");
			for (uint8_t i=0; i<SENSOR_COUNT; i++) {
				OneSensor *wichSensor = &sensors[static_cast<SensorType>(i)];
				if (wichSensor->location == BOARD_URBAN) wichSensor->enabled = wichSensor->defaultEnabled;
			}
			saveConfig();
		}

	} else {
		sckOut("No urban board detected!!");
		urbanPresent = false;

		// Find out if urban was removed just now
		bool justRemoved = false;
		for (uint8_t i=0; i<SENSOR_COUNT; i++) {
			OneSensor *wichSensor = &sensors[static_cast<SensorType>(i)];
			if (wichSensor->location == BOARD_URBAN && wichSensor->enabled) {
				justRemoved = true;
			}
		}

		if (justRemoved) {
			sckOut("Disabling sensors...");
			for (uint8_t i=0; i<SENSOR_COUNT; i++) {
				OneSensor *wichSensor = &sensors[static_cast<SensorType>(i)];
				if (wichSensor->location == BOARD_URBAN && wichSensor->enabled) disableSensor(wichSensor->type);
			}
			saveConfig();
		}
	}

	// Detect and enable auxiliary boards
	for (uint8_t i=0; i<SENSOR_COUNT; i++) {

		OneSensor *wichSensor = &sensors[sensors.sensorsPriorized(i)];

		if (wichSensor->location == BOARD_AUX) {
			if (enableSensor(wichSensor->type)) {
				wichSensor->enabled = true;
				saveNeeded = true;
			} else if (wichSensor->enabled)  {
				disableSensor(wichSensor->type);
				sprintf(outBuff, "Removed: %s... ", wichSensor->title);
				sckOut();
				wichSensor->enabled = false;
				saveNeeded = true;
			}
		}
	}

	// Update battery parcent for power management stuff
	battery.percent(&charger);

	if (saveNeeded) saveConfig();
}
void SckBase::update()
{
	if (millis() - reviewStateMillis > 500) {
		reviewStateMillis = millis();
		reviewState();
	}

	if (millis() - updatePowerMillis > 1000) {
		updatePowerMillis = millis();
		updatePower();
	}

	if (butState != butOldState) {
		buttonEvent();
		butOldState = butState;
		while(!butState) buttonStillDown();
	}
}

// **** Mode Control
void SckBase::reviewState()
{

	// Avoid ESP hangs
	if (st.espBooting) {
		if (rtc.getEpoch() - espStarted > 3) ESPcontrol(ESP_REBOOT);
	}

	if (pendingSyncConfig) {
		if (millis() - sendConfigTimer > 1000) {
			sendConfigTimer = millis();
			if (sendConfigCounter > 3) {
				ESPcontrol(ESP_REBOOT);
				sendConfigCounter = 0;
			} else if (st.espON) {
				if (!st.espBooting) sendConfig();
				sendConfigCounter++;
			} else {
				ESPcontrol(ESP_ON);
			}
		}
	}

	if (sdInitPending) sdInit();

	// SD card debug check file size and backup big files.
	if (config.sdDebug) {
		// Just do this every hour
		if (rtc.getEpoch() % 3600 == 0) {
			if (sdSelect()) {
				debugFile.file = sd.open(debugFile.name, FILE_WRITE);
				if (debugFile.file) {

					uint32_t debugSize = debugFile.file.size();

					// If file is bigger than 50mb rename the file.
					if (debugSize >= 52428800) debugFile.file.rename(sd.vwd(), "DEBUG01.TXT");
					debugFile.file.close();

				} else {
					st.cardPresent = false;
					st.cardPresentError = false;
				}

			}
		}
	}


	/* struct SckState { */
	/* bool onSetup --  in from enterSetup() and out from saveConfig()*/
	/* bool espON */
	/* bool wifiSet */
	/* bool tokenSet */
	/* bool helloPending */
	/* SCKmodes mode */
	/* bool cardPresent */
	/* bool sleeping */
	/* }; */

	/* state can be changed by: */
	/* loadConfig() */
	/* receiveMessage() */
	/* sdDetect() */
	/* buttonEvent(); */

	if (st.onShell) {


	} else if (st.onSetup) {


	} else if (sckOFF) {


	} else if (st.mode == MODE_NOT_CONFIGURED) {

		if (!st.onSetup) enterSetup();

	} else if (st.mode == MODE_NET) {

		if (!st.wifiSet) {
			if (!st.wifiStat.error) {
				sckOut("ERROR wifi is not configured!!!");
				ESPcontrol(ESP_OFF);
				led.update(led.BLUE, led.PULSE_HARD_FAST);
				st.wifiStat.error = true;
			}
			return;
		}

		if (!st.tokenSet) {
			if (!st.tokenError) {
				sckOut("ERROR token is not configured!!!");
				ESPcontrol(ESP_OFF);
				led.update(led.BLUE, led.PULSE_HARD_FAST);
				st.tokenError = true;
			}
			return;
		}

		if (st.helloPending || !st.timeStat.ok || (timeToPublish  && readingsList.countGroups() > 0) || !infoPublished) {

			if (!st.wifiStat.ok) {

				st.wifiStat.retry();

				if (!st.espON) ESPcontrol(ESP_ON);
				else if (st.wifiStat.error) {

					sckOut("ERROR Can't publish without wifi!!!");

					// Publish to sd card
					sdPublish();

					ESPcontrol(ESP_OFF); 		// Hard off not sleep to be sure the ESP state is reset
					led.update(led.BLUE, led.PULSE_HARD_FAST);

					lastPublishTime = rtc.getEpoch();
					st.wifiStat.reset(); 		// Restart wifi retry count
				}

			} else {

				led.update(led.BLUE, led.PULSE_SOFT);

				if (st.helloPending) {

					if (st.helloStat.retry()) {

						if (sendMessage(ESPMES_MQTT_HELLO, ""))	sckOut("Hello sent!");

					} else if (st.helloStat.error) {

						sckOut("ERROR sending hello!!!");

						ESPcontrol(ESP_REBOOT); 		// Try reseting ESP
						led.update(led.BLUE, led.PULSE_HARD_FAST);

						st.helloStat.reset();
					}

				} else if (!st.timeStat.ok) {

					if (st.timeStat.retry()) {

						if (sendMessage(ESPMES_GET_TIME, "")) sckOut("Asking time to ESP...");

					} else if (st.timeStat.error) {

						sckOut("ERROR getting time from the network!!!");

						ESPcontrol(ESP_REBOOT);
						led.update(led.BLUE, led.PULSE_HARD_FAST);

						st.timeStat.reset();
					}

				} else if (!infoPublished) {

					if (st.infoStat.retry()) {

						if (publishInfo()) sckOut("Info sent!");

					} else if (st.infoStat.error){

						sckOut("ERROR sending kit info to platform!!!");
						st.infoStat.reset();

					}

				} else if (timeToPublish) {

					if (st.publishStat.ok) {

						lastPublishTime = rtc.getEpoch();
						st.publishStat.reset(); 		// Restart publish error counter

						// Publish to sdcard
						sdPublish();

						epoch2iso(readingsList.getTime(0), ISOtimeBuff);
						sprintf(outBuff, "(%s) Published OK, erasing from memory", ISOtimeBuff);
						sckOut();
						readingsList.delLastGroup();

						// Continue as fast as posible with remaining readings, or go to sleep
						if (readingsList.countGroups() > 0) {

							if (st.publishStat.retry()) netPublish();
						} else {

							timeToPublish = false;
						}


					} else if (st.publishStat.error) {

						sckOut("Will retry on next publish interval!!!");

						// Publish to sd card
						sdPublish();

						led.update(led.BLUE, led.PULSE_HARD_FAST);

						ESPcontrol(ESP_OFF);
						timeToPublish = false;
						lastPublishTime = rtc.getEpoch();
						st.publishStat.reset(); 		// Restart publish error counter

					} else if (readingsList.countGroups() > 0) {

						if (st.publishStat.retry()) netPublish();
					}
				}
			}
		} else {


			while ( 	!charger.onUSB && 					// No USB connected
					pendingSensors <= 0 && 					// No sensor to wait to
					millis() - lastUserEvent > waitAfterLastEvent) { 	// No recent user interaction (button, sdcard or USB events)


				goToSleep();

				// Let the led be visible for one instant (and start breathing if we need to read sensors)
				led.update(led.BLUE2, led.PULSE_STATIC, true);
				delay(10);
				led.update(led.BLUE, led.PULSE_SOFT, true);

				updateSensors();
				updatePower();
			}

			updateSensors();
			if (readingsList.countGroups() > 0) sdPublish();

		}


	} else if  (st.mode == MODE_SD) {

		if (!st.cardPresent) {
			if (!st.cardPresentError) {
				sckOut("ERROR can't find SD card!!!");
				if (st.espON) ESPcontrol(ESP_OFF);
				led.update(led.PINK, led.PULSE_HARD_FAST);
				st.cardPresentError = true;
			}
			return;

		} else if (!st.timeStat.ok) {

			if (!st.wifiSet)  {
				if (!st.wifiStat.error) {
					sckOut("ERROR time is not synced and no wifi set!!!");
					ESPcontrol(ESP_OFF);
					led.update(led.PINK, led.PULSE_HARD_FAST);
					st.wifiStat.error = true;
				}
			} else {

				if (!st.wifiStat.ok) {

					st.wifiStat.retry();

					if (!st.espON) ESPcontrol(ESP_ON);
					else if (st.wifiStat.error) {

						sckOut("ERROR time is not synced!!!");

						ESPcontrol(ESP_OFF);
						led.update(led.PINK, led.PULSE_HARD_FAST);
						st.wifiStat.reset();
					}


				} else {

					if (st.timeStat.retry()) {

						if (sendMessage(ESPMES_GET_TIME, "")) sckOut("Asking time to ESP...");

					} else if (st.timeStat.error) {

						sckOut("ERROR time sync failed!!!");
						st.timeStat.reset();
						ESPcontrol(ESP_OFF);
						led.update(led.PINK, led.PULSE_HARD_FAST);
					}
				}
			}

		} else {

												// Conditions to go to sleep
			while ( 	!charger.onUSB && 					// No USB connected
					!timeToPublish && 					// No need to publish
					pendingSensors <= 0 && 					// No sensor to wait to
					millis() - lastUserEvent > waitAfterLastEvent) { 	// No recent user interaction (button, sdcard or USB events)


				goToSleep();

				// Let the led be visible for one instant (and start breathing if we need to read sensors)
				led.update(led.PINK2, led.PULSE_STATIC, true);
				delay(10);
				led.update(led.PINK, led.PULSE_SOFT, true);

				updateSensors();
				updatePower();
			}

			updateSensors();

			led.update(led.PINK, led.PULSE_SOFT);
			if (st.espON) ESPcontrol(ESP_OFF);

			if (readingsList.countGroups() > 0) {

				if (!sdPublish()) {
					sckOut("ERROR failed publishing to SD card");
					led.update(led.PINK, led.PULSE_HARD_FAST);
				} else {
					timeToPublish = false;
					lastPublishTime = rtc.getEpoch();
				}
			}
		}
	}
}
void SckBase::enterSetup()
{
	sckOut("Entering setup mode", PRIO_LOW);
	st.onSetup = true;

	// Update led
	led.update(led.RED, led.PULSE_SOFT);

	// Clear errors from other modes
	st.tokenError = false;
	st.cardPresentError = false;

	// Reboot ESP to have a clean start
	ESPcontrol(ESP_REBOOT);
}
void SckBase::printState()
{
	char t[] = "true";
	char f[] = "false";

	sprintf(outBuff, "%sonSetup: %s\r\n", outBuff, st.onSetup  ? t : f);
	sprintf(outBuff, "%stokenSet: %s\r\n", outBuff, st.tokenSet  ? t : f);
	sprintf(outBuff, "%shelloPending: %s\r\n", outBuff, st.helloPending  ? t : f);
	sprintf(outBuff, "%smode: %s\r\n", outBuff, modeTitles[st.mode]);
	sprintf(outBuff, "%scardPresent: %s\r\n", outBuff, st.cardPresent  ? t : f);
	sprintf(outBuff, "%ssleeping: %s\r\n", outBuff, st.sleeping  ? t : f);

	sprintf(outBuff, "%s\r\nespON: %s\r\n", outBuff, st.espON  ? t : f);
	sprintf(outBuff, "%s\r\nespBooting: %s\r\n", outBuff, st.espBooting  ? t : f);
	sprintf(outBuff, "%swifiSet: %s\r\n", outBuff, st.wifiSet  ? t : f);
	sprintf(outBuff, "%swifiOK: %s\r\n", outBuff, st.wifiStat.ok ? t : f);
	sprintf(outBuff, "%swifiError: %s\r\n", outBuff, st.wifiStat.error ? t : f);
	sckOut(PRIO_HIGH, false);

	sprintf(outBuff, "\r\ntimeOK: %s\r\n", st.timeStat.ok ? t : f);
	sprintf(outBuff, "%stimeError: %s\r\n", outBuff, st.timeStat.error ? t : f);

	sprintf(outBuff, "%s\r\npublishOK: %s\r\n", outBuff, st.publishStat.ok ? t : f);
	sprintf(outBuff, "%spublishError: %s\r\n", outBuff, st.publishStat.error ? t : f);
	sprintf(outBuff, "%s\r\ntime to next publish: %li\r\n", outBuff, config.publishInterval - (rtc.getEpoch() - lastPublishTime));

	sckOut(PRIO_HIGH, false);
}

// **** Input
void SckBase::inputUpdate()
{

	if (SerialUSB.available()) {

		char buff = SerialUSB.read();
		uint16_t blen = serialBuff.length();

		// New line
		if (buff == 13 || buff == 10) {

			SerialUSB.println();				// Newline

			serialBuff.replace("\n", "");		// Clean input
			serialBuff.replace("\r", "");
			serialBuff.trim();

			commands.in(this, serialBuff);		// Process input
			if (blen > 0) previousCommand = serialBuff;
			serialBuff = "";
			prompt();

			// Backspace
		} else if (buff == 127) {

			if (blen > 0) SerialUSB.print("\b \b");
			serialBuff.remove(blen-1);

			// Up arrow (previous command)
		} else if (buff == 27) {

			delayMicroseconds(200);
			SerialUSB.read();				// drop next char (always 91)
			delayMicroseconds(200);
			char tmp = SerialUSB.read();
			if (tmp != 65) tmp = SerialUSB.read(); // detect up arrow
			else {
				for (uint8_t i=0; i<blen; i++) SerialUSB.print("\b \b");	// clean previous command
				SerialUSB.print(previousCommand);
				serialBuff = previousCommand;
			}

			// Normal char
		} else {

			serialBuff += buff;
			SerialUSB.print(buff);				// Echo

		}
	}

	ESPbusUpdate();
}

// **** Output
void SckBase::sckOut(String strOut, PrioLevels priority, bool newLine)
{
	if (strOut.equals(outBuff)) {
		outRepetitions++;
		if (outRepetitions >= 10) {
			sckOut("Last message repeated 10 times");
			outRepetitions = 0;
		}
		return;
	}
	outRepetitions = 0;
	strOut.toCharArray(outBuff, strOut.length()+1);
	sckOut(priority, newLine);
}
void SckBase::sckOut(const char *strOut, PrioLevels priority, bool newLine)
{
	if (strncmp(strOut, outBuff, strlen(strOut)) == 0) {
		outRepetitions++;
		if (outRepetitions >= 10) {
			sckOut("Last message repeated 10 times");
			outRepetitions = 0;
		}
		return;
	}
	outRepetitions = 0;
	strncpy(outBuff, strOut, 240);
	sckOut(priority, newLine);
}
void SckBase::sckOut(PrioLevels priority, bool newLine)
{
	// Output via USB console
	if (charger.onUSB) {
		if (outputLevel + priority > 1) {
			if (newLine) SerialUSB.println(outBuff);
			else SerialUSB.print(outBuff);
		}
	} else  {
		digitalWrite(pinLED_USB, HIGH);
	}

	// Debug output to sdcard
	if (config.sdDebug) {
		if (!sdSelect()) return;
		debugFile.file = sd.open(debugFile.name, FILE_WRITE);
		if (debugFile.file) {
			ISOtime();
			debugFile.file.print(ISOtimeBuff);
			debugFile.file.print("-->");
			debugFile.file.println(outBuff);
			debugFile.file.close();
		} else st.cardPresent = false;
	}
}
void SckBase::prompt()
{
	sprintf(outBuff, "%s", "SCK > ");
	sckOut(PRIO_MED, false);
}

// **** Config
void SckBase::loadConfig()
{

	sckOut("Loading configuration from eeprom...");

	Configuration savedConf = eepromConfig.read();

	if (savedConf.valid) config = savedConf;
	else {
		sckOut("Can't find valid configuration!!! loading defaults...");
		saveConfig(true);
	}

	for (uint8_t i=0; i<SENSOR_COUNT; i++) {
		OneSensor *wichSensor = &sensors[static_cast<SensorType>(i)];
		wichSensor->enabled = config.sensors[i].enabled;
		wichSensor->everyNint = config.sensors[i].everyNint;
	}

	st.wifiSet = config.credentials.set;
	st.tokenSet = config.token.set;
	st.tokenError = false;
	st.mode = config.mode;
	led.dim = config.dimled;

	// CSS vocs sensor baseline loading
	if (config.extra.ccsBaselineValid && I2Cdetect(&Wire, urban.sck_ccs811.address)) {
		sprintf(outBuff, "Updating CCS sensor baseline: %u", config.extra.ccsBaseline);
		sckOut();
		urban.sck_ccs811.setBaseline(config.extra.ccsBaseline);
	}
}
void SckBase::saveConfig(bool defaults)
{
	// Save to eeprom
	if (defaults) {
		Configuration defaultConfig;

		if (config.mac.valid) macAddress = String(config.mac.address); 	// If we already have a mac address keep it

		config = defaultConfig;

		if (macAddress.length() > 0) {
			sprintf(config.mac.address, "%s", macAddress.c_str());
			config.mac.valid = true;
		} else {
			config.mac.valid = false;
		}

		for (uint8_t i=0; i<SENSOR_COUNT; i++) {
			config.sensors[i].enabled = sensors[static_cast<SensorType>(i)].defaultEnabled;
			config.sensors[i].everyNint = 1;
		}
		config.dimled = 1.0;
		pendingSyncConfig = true;
	} else {
		for (uint8_t i=0; i<SENSOR_COUNT; i++) {
			OneSensor *wichSensor = &sensors[static_cast<SensorType>(i)];
			config.sensors[i].enabled = wichSensor->enabled;
			config.sensors[i].everyNint = wichSensor->everyNint;
		}
	}
	eepromConfig.write(config);
	sckOut("Saved configuration on eeprom!!", PRIO_LOW);

	// Update state
	st.mode = config.mode;
	st.wifiSet = config.credentials.set;
	st.tokenSet = config.token.set;
	st.tokenError = false;
	st.wifiStat.reset();
	lastPublishTime = rtc.getEpoch() - config.publishInterval;
	lastSensorUpdate = rtc.getEpoch() - config.readInterval;
	led.dim = config.dimled;

	if (st.wifiSet || st.tokenSet) pendingSyncConfig = true;

	// Decide if new mode its valid
	if (st.mode == MODE_NET) {

		if (st.wifiSet && st.tokenSet) {

			infoPublished = false;
			st.helloPending = true;
			st.onSetup = false;
			led.update(led.BLUE, led.PULSE_SOFT);
			sendMessage(ESPMES_STOP_AP, "");

		} else {

			if (!st.wifiSet) sckOut("ERROR Wifi not configured: can't set Network Mode!!!");
			if (!st.tokenSet) sckOut("ERROR Token not configured: can't set Network Mode!!!");
			ESPcontrol(ESP_OFF);
			led.update(led.BLUE, led.PULSE_HARD_FAST);
		}

	} else if (st.mode == MODE_SD) {

		st.helloPending = false;
		st.onSetup = false;
		led.update(led.PINK, led.PULSE_SOFT);
		sendMessage(ESPMES_STOP_AP, "");

	}

	if (pendingSyncConfig && !st.espON) ESPcontrol(ESP_ON);
}
Configuration SckBase::getConfig()
{

	return config;
}
bool SckBase::sendConfig()
{
	if (!st.espON) {
		ESPcontrol(ESP_ON);
		return false;
	}
	if (st.espBooting) return false;

	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["cs"] = (uint8_t)config.credentials.set;
	json["ss"] = config.credentials.ssid;
	json["pa"] = config.credentials.pass;
	json["ts"] = (uint8_t)config.token.set;
	json["to"] = config.token.token;
	json["ver"] = SAMversion;
	json["bd"] = SAMbuildDate;

	if (!st.onSetup && ((st.mode == MODE_NET && st.wifiSet && st.tokenSet) || (st.mode == MODE_SD && st.wifiSet))) json["ac"] = (uint8_t)ESPMES_CONNECT;
	else json["ac"] = (uint8_t)ESPMES_START_AP;

	sprintf(netBuff, "%c", ESPMES_SET_CONFIG);
	json.printTo(&netBuff[1], json.measureLength() + 1);

	if (sendMessage()) {
		pendingSyncConfig = false;
		sendConfigCounter = 0;
		sckOut("Synced config with ESP!!", PRIO_LOW);
		return true;
	}

	return false;
}
bool SckBase::publishInfo()
{
	// Info file
	if (!espInfoUpdated) sendMessage(ESPMES_GET_NETINFO);
	else {
		// Publish info to platform

		/* { */
		/* 	"time":"2018-07-17T06:55:06Z", */
		/* 	"hw_ver":"2.0", */
		/* 	"id":"6C4C1AF4504E4B4B372E314AFF031619", */
		/* 	"sam_ver":"0.3.0-ce87e64", */
		/* 	"sam_bd":"2018-07-17T06:55:06Z", */
		/* 	"mac":"AB:45:2D:33:98", */
		/* 	"esp_ver":"0.3.0-ce87e64", */
		/* 	"esp_bd":"2018-07-17T06:55:06Z" */
		/* } */

		if (!st.espON) {
			ESPcontrol(ESP_ON);
			return false;
		}

		getUniqueID();

		StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();

		json["time"] = ISOtimeBuff;
		json["hw_ver"] = hardwareVer.c_str();
		json["id"] = uniqueID_str;
		json["sam_ver"] = SAMversion.c_str();
		json["sam_bd"] = SAMbuildDate.c_str();
		json["mac"] = config.mac.address;
		json["esp_ver"] = ESPversion.c_str();
		json["esp_bd"] = ESPbuildDate.c_str();

		sprintf(netBuff, "%c", ESPMES_MQTT_INFO);
		json.printTo(&netBuff[1], json.measureLength() + 1);
		if (sendMessage()) return true;
	}
	return false;
}

// **** ESP
void SckBase::ESPcontrol(ESPcontrols controlCommand)
{
	switch(controlCommand){
		case ESP_OFF:
		{
				sckOut("ESP off...", PRIO_LOW);
				st.espON = false;
				st.espBooting = false;
				digitalWrite(pinESP_CH_PD, LOW);
				digitalWrite(pinPOWER_ESP, HIGH);
				digitalWrite(pinESP_GPIO0, LOW);
				sprintf(outBuff, "Esp was on for %lu seconds", (rtc.getEpoch() - espStarted));
				espStarted = 0;
				break;
		}
		case ESP_FLASH:
		{
				led.update(led.WHITE, led.PULSE_STATIC);

				SerialESP.begin(espFlashSpeed);
				delay(100);

				digitalWrite(pinESP_CH_PD, LOW);
				digitalWrite(pinPOWER_ESP, HIGH);
				digitalWrite(pinESP_GPIO0, LOW);	// LOW for flash mode
				delay(100);

				digitalWrite(pinESP_CH_PD, HIGH);
				digitalWrite(pinPOWER_ESP, LOW);

				uint32_t flashTimeout = millis();
				uint32_t startTimeout = millis();
				while(1) {
					if (SerialUSB.available()) {
						SerialESP.write(SerialUSB.read());
						flashTimeout = millis();
					}
					if (SerialESP.available()) {
						SerialUSB.write(SerialESP.read());
					}
					if (millis() - flashTimeout > 1000) {
						if (millis() - startTimeout > 10000) sck_reset();  // Initial 10 seconds for the flashing to start
					}
				}
				break;
		}
		case ESP_ON:
		{
				if (st.espBooting || st.espON) return;
				sckOut("ESP on...", PRIO_LOW);
				digitalWrite(pinESP_CH_PD, HIGH);
				digitalWrite(pinESP_GPIO0, HIGH);		// HIGH for normal mode
				digitalWrite(pinPOWER_ESP, LOW);
				st.wifiStat.reset();
				st.espON = true;
				st.espBooting = true;
				espStarted = rtc.getEpoch();
				break;

		}
		case ESP_REBOOT:
		{
				sckOut("Restarting ESP...", PRIO_LOW);
				ESPcontrol(ESP_OFF);
				delay(50);
				ESPcontrol(ESP_ON);
				break;
		}
		case ESP_WAKEUP:
		{
				sckOut("ESP wake up...");
				digitalWrite(pinESP_CH_PD, HIGH);
				st.espON = true;
				espStarted = rtc.getEpoch();
				break;
		}
		case ESP_SLEEP:
		{
				sckOut("ESP deep sleep...", PRIO_LOW);
				sendMessage(ESPMES_LED_OFF);
				st.espON = false;
				st.espBooting = false;
				digitalWrite(pinESP_CH_PD, LOW);
				sprintf(outBuff, "Esp was awake for %lu seconds", (rtc.getEpoch() - espStarted));
				sckOut(PRIO_LOW);
				espStarted = 0;
				break;
		}
	}
}
void SckBase::ESPbusUpdate()
{
	if (manager.available()) {

		uint8_t len = NETPACK_TOTAL_SIZE;

		if (manager.recvfromAck(netPack, &len)) {

			if (debugESPcom) {
				sprintf(outBuff, "Receiving msg from ESP in %i parts", netPack[0]);
				sckOut();
			}

			// Identify received command
			uint8_t pre = netPack[1];
			SAMMessage wichMessage = static_cast<SAMMessage>(pre);

			// Get content from first package (1 byte less than the rest)
			memcpy(netBuff, &netPack[2], NETPACK_CONTENT_SIZE - 1);

			// Get the rest of the packages (if they exist)
			for (uint8_t i=0; i<netPack[0]-1; i++) {
				if (manager.recvfromAckTimeout(netPack, &len, 500))	{
					memcpy(&netBuff[(i * NETPACK_CONTENT_SIZE) + (NETPACK_CONTENT_SIZE - 1)], &netPack[1], NETPACK_CONTENT_SIZE);
				}
				else return;
			}

			if (debugESPcom) sckOut(netBuff);

			// Process message
			receiveMessage(wichMessage);
		}
	}
}
bool SckBase::sendMessage(ESPMessage wichMessage)
{
	sprintf(netBuff, "%c", wichMessage);
	return sendMessage();
}
bool SckBase::sendMessage(ESPMessage wichMessage, const char *content)
{
	sprintf(netBuff, "%c%s", wichMessage, content);
	return sendMessage();
}
bool SckBase::sendMessage()
{

	// This function is used when netbuff is already filled with command and content

	if (!st.espON || st.espBooting) {
		if (debugESPcom) sckOut("Can't send message, ESP is off or still booting...");
		return false;
	}

	uint16_t totalSize = strlen(netBuff);
	uint8_t totalParts = (totalSize + NETPACK_CONTENT_SIZE - 1)  / NETPACK_CONTENT_SIZE;

	if (debugESPcom) {
		sprintf(outBuff, "Sending msg to ESP with %i parts and %i bytes", totalParts, totalSize);
		sckOut();
		SerialUSB.println(netBuff);
	}

	for (uint8_t i=0; i<totalParts; i++) {
		netPack[0] = totalParts;
		memcpy(&netPack[1], &netBuff[(i * NETPACK_CONTENT_SIZE)], NETPACK_CONTENT_SIZE);
		if (!manager.sendtoWait(netPack, NETPACK_TOTAL_SIZE, ESP_ADDRESS)) {
			sckOut("ERROR sending mesg to ESP!!!");
			return false;
		}
		if (debugESPcom) {
			sprintf(outBuff, "Sent part num %i", i);
			sckOut();
		}
	}
	return true;
}
void SckBase::receiveMessage(SAMMessage wichMessage)
{
	switch(wichMessage) {
		case SAMMES_SET_CONFIG:
		{

				lastUserEvent = millis();
				sckOut("Received new config from ESP");
				StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(netBuff);

				if (json.containsKey("mo")) {
					String stringMode = json["mo"];
					if (stringMode.startsWith("net")) config.mode = MODE_NET;
					else if (stringMode.startsWith("sd")) config.mode = MODE_SD;
				} else config.mode = MODE_NOT_CONFIGURED;

				if (json.containsKey("pi")) {
					if (json["pi"] > minimal_publish_interval && json["pi"] < max_publish_interval)	config.publishInterval = json["pi"];
				} else config.publishInterval = default_publish_interval;

				if (json.containsKey("ss")) {
					config.credentials.set = true;
					strcpy(config.credentials.ssid, json["ss"]);
					if (json.containsKey("pa")) strcpy(config.credentials.pass, json["pa"]);
				} else config.credentials.set = false;


				if (json.containsKey("to")) {
					config.token.set = true;
					strcpy(config.token.token, json["to"]);
				} else config.token.set = false;

				st.helloPending = true;
				saveConfig();
				break;

		}
		case SAMMES_DEBUG:
		{

				sckOut("ESP --> ", PRIO_HIGH, false);
				sckOut(netBuff);
				break;

		}
		case SAMMES_NETINFO:
		{
				StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(netBuff);
				ipAddress = json["ip"].as<String>();
				hostname = json["hn"].as<String>();

				sprintf(outBuff, "\r\nHostname: %s\r\nIP address: %s\r\nMAC address: %s", hostname.c_str(), ipAddress.c_str(), macAddress.c_str());
				sckOut();
				sprintf(outBuff, "ESP version: %s\r\nESP build date: %s", ESPversion.c_str(), ESPbuildDate.c_str());
				sckOut();

				break;
		}
		case SAMMES_WIFI_CONNECTED:

			sckOut("Connected to wifi!!", PRIO_LOW);
			st.wifiStat.setOk();
			if (!timeSyncAfterBoot) {
				if (sendMessage(ESPMES_GET_TIME, "")) sckOut("Asked new time sync to ESP...");
			}
			break;

		case SAMMES_SSID_ERROR:

			sckOut("ERROR Access point not found!!"); st.wifiStat.error = true; break;

		case SAMMES_PASS_ERROR:

			sckOut("ERROR wrong wifi password!!"); st.wifiStat.error = true; break;

		case SAMMES_WIFI_UNKNOWN_ERROR:

			sckOut("ERROR unknown wifi error!!"); st.wifiStat.error = true; break;

		case SAMMES_TIME:
		{
				String strTime = String(netBuff);
				setTime(strTime);
				break;
		}
		case SAMMES_MQTT_HELLO_OK:
		{
				st.helloPending = false;
				st.helloStat.setOk();
				sckOut("Hello OK!!");
				break;
		}
		case SAMMES_MQTT_PUBLISH_OK:

			st.publishStat.setOk();
			break;

		case SAMMES_MQTT_PUBLISH_ERROR:

			sckOut("ERROR on MQTT publish");
			st.publishStat.error = true;
			break;

		case SAMMES_MQTT_INFO_OK:

			st.infoStat.setOk();
			infoPublished = true;
			sckOut("Info publish OK!!");
			break;

		case SAMMES_MQTT_INFO_ERROR:

			st.infoStat.error = true;
			sckOut("ERROR on Info publish!!");
			break;

		case SAMMES_MQTT_CUSTOM_OK:

			sckOut("Custom MQTT publish OK!!");
			break;

		case SAMMES_MQTT_CUSTOM_ERROR:

			sckOut("ERROR on custom MQTT publish");
			break;

		case SAMMES_BOOTED:
		{
			sckOut("ESP finished booting");

			st.espBooting = false;

			StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
			JsonObject& json = jsonBuffer.parseObject(netBuff);
			macAddress = json["mac"].as<String>();
			ESPversion = json["ver"].as<String>();
			ESPbuildDate = json["bd"].as<String>();

			// Udate mac address if we haven't yet
			if (!config.mac.valid) {
				sckOut("Updated MAC address");
				sprintf(config.mac.address, "%s", macAddress.c_str());
				config.mac.valid = true;
				saveConfig();
			}

			if (!espInfoUpdated) {
				espInfoUpdated = true;
				saveInfo();
			}

			pendingSyncConfig = true;
			break;
		}
		default: break;
	}
}
void SckBase::mqttCustom(const char *topic, const char *payload)
{
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["to"] = topic;
	json["pl"] = payload;

	sprintf(netBuff, "%c", ESPMES_MQTT_CUSTOM);
	json.printTo(&netBuff[1], json.measureLength() + 1);

	if (sendMessage()) sckOut("MQTT message sent to ESP...", PRIO_LOW);
}

// **** SD card
bool SckBase::sdInit()
{
	sdInitPending = false;

	if (sd.begin(pinCS_SDCARD, SPI_HALF_SPEED)) {
		sckOut("Sd card ready to use");
		st.cardPresent = true;
		st.cardPresentError = false;

		// Check if there is a info file on sdcard 
		if (!sd.exists(infoFile.name)) {
			infoSaved = false;
			saveInfo();
		}
		return true;
	}
	sckOut("ERROR on Sd card Init!!!");
	st.cardPresent = false; 	// If we cant initialize sdcard, don't use it!
	st.cardPresentError = false;
	return false;
}
bool SckBase::sdDetect()
{
	lastUserEvent = millis();
	st.cardPresent = !digitalRead(pinCARD_DETECT);
	st.cardPresentError = false;

	if (!digitalRead(pinCARD_DETECT)) {
		sckOut("Sdcard inserted");
		sdInitPending = true;
	} else sckOut("No Sdcard found!!");
	return false;
}
bool SckBase::sdSelect()
{

	if (!st.cardPresent) return false;

	digitalWrite(pinCS_FLASH, HIGH);	// disables Flash
	digitalWrite(pinCS_SDCARD, LOW);

	return true;
}
bool SckBase::saveInfo()
{
	// Save info to sdcard
	if (!infoSaved) {
		if (sdSelect()) {
			if (sd.exists(infoFile.name)) sd.remove(infoFile.name);
			infoFile.file = sd.open(infoFile.name, FILE_WRITE);
			getUniqueID();
			sprintf(outBuff, "Hardware Version: %s\r\nSAM Hardware ID: %s\r\nSAM version: %s\r\nSAM build date: %s", hardwareVer.c_str(), uniqueID_str, SAMversion.c_str(), SAMbuildDate.c_str());
			infoFile.file.println(outBuff);
			sprintf(outBuff, "ESP MAC address: %s\r\nESP version: %s\r\nESP build date: %s\r\n", config.mac.address, ESPversion.c_str(), ESPbuildDate.c_str());
			infoFile.file.println(outBuff);
			infoFile.file.close();
			infoSaved = true;
			sckOut("Saved INFO.TXT file!!");
			return true;
		}
	}
	return false;
}


// **** Power
void SckBase::sck_reset()
{
	// Save updated CCS sensor baseline
	if (I2Cdetect(&Wire, urban.sck_ccs811.address)) {
		uint16_t savedBaseLine = urban.sck_ccs811.getBaseline();
		if (savedBaseLine != 0)	{
			sprintf(outBuff, "Saved CCS baseline on eeprom: %u", savedBaseLine);
			sckOut();
			config.extra.ccsBaseline = savedBaseLine;
			config.extra.ccsBaselineValid = true;
			eepromConfig.write(config);
		}
	}

	sckOut("Bye!!");
	NVIC_SystemReset();
}
void SckBase::goToSleep()
{
	led.off();
	if (st.espON) ESPcontrol(ESP_OFF);

	// ESP control pins savings
	digitalWrite(pinESP_CH_PD, LOW);
	digitalWrite(pinESP_GPIO0, LOW);
	digitalWrite(pinESP_RX_WIFI, LOW);
	digitalWrite(pinESP_TX_WIFI, LOW);

	// Stop PM sensor
	if (urban.sck_pm.started) urban.sck_pm.stop();

	// Turn off USB led
	digitalWrite(pinLED_USB, HIGH);

	if (sckOFF) {

		sprintf(outBuff, "Sleeping forever!!! (until a button click)");
		sckOut();

		// Stop CCS811 VOCS sensor
		urban.stop(SENSOR_CCS811_VOCS);

		// Disable the Sanity cyclic reset so it doesn't wake us up
		rtc.disableAlarm();
		rtc.detachInterrupt();

		// Detach sdcard interrupt to avoid spurious wakeup
		detachInterrupt(pinCARD_DETECT);

		// Atach button wake Up interrupt
		pinMode(pinBUTTON, INPUT_PULLUP);
		LowPower.attachInterruptWakeup(pinBUTTON, ISR_button, CHANGE);

		LowPower.deepSleep();
	} else {

		sprintf(outBuff, "Sleeping for %.2f seconds", (sleepTime) / 1000.0);
		sckOut();

		st.sleeping = true;

		LowPower.deepSleep(sleepTime);
	}

	// Re enable Sanity cyclic reset
	rtc.setAlarmTime(wakeUP_H, wakeUP_M, wakeUP_S);
	rtc.enableAlarm(rtc.MATCH_HHMMSS);
	rtc.attachInterrupt(ext_reset);

	// Recover Noise sensor timer
	REG_GCLK_GENCTRL = GCLK_GENCTRL_ID(4);  // Select GCLK4
	while (GCLK->STATUS.bit.SYNCBUSY);
}
void SckBase::updatePower()
{
	charger.detectUSB(this);

	if (charger.onUSB) {

		// Reset lowBatt counters
		battery.lowBatCounter = 0;
		battery.emergencyLowBatCounter = 0;

		switch(charger.getChargeStatus()) {

			case charger.CHRG_NOT_CHARGING:
				// If voltage is too low we asume we don't have battery.
				if (charger.getBatLowerSysMin()) {
					if (battery.present) sckOut("Battery removed!!");
					battery.present = false;
					led.chargeStatus = led.CHARGE_NULL;
				} else {
					if (!battery.present) sckOut("Battery connected!!");
					battery.present = true;
					if (battery.voltage() < battery.maxVolt) charger.chargeState(true);
					else led.chargeStatus = led.CHARGE_FINISHED;
				}
				break;
			case charger.CHRG_CHARGE_TERM_DONE:

				// To be sure the batt is present, turn off charging and check voltages on next cycle
				if (charger.chargeState()) charger.chargeState(false);
				break;
			default:
				battery.present = true;
				led.chargeStatus = led.CHARGE_CHARGING;
				break;
		}
	} else {

		battery.present = true;

		// Emergency lowBatt
		if (battery.last_percent < battery.threshold_emergency) {
			if (battery.emergencyLowBatCounter < 5) battery.emergencyLowBatCounter++;
			else {

				for (uint8_t i=0; i<5; i++) {
					led.off();
					delay(200);
					led.update(led.ORANGE, led.PULSE_STATIC, true);
					delay(200);
				}

				// Ignore last user event and go to sleep
				lastUserEvent = millis() - waitAfterLastEvent;

				sleepTime = 60000; 			
				// Wake up every minute to check if USB power is back
				while (!charger.onUSB) {
					goToSleep();
					charger.detectUSB(this); 	// When USB is detecteed the kit should reset to start on clean state
					if (millis() - lastUserEvent < waitAfterLastEvent) break;  // Wakeup on user interaction (will go to sleep again after sone blinks)
				}
				sleepTime = 2500; 			// Return to runtime default sleep period (in theory this is not needed, but just in case)
			}

		// Low Batt
		} else if (battery.last_percent < battery.threshold_low) {

			battery.emergencyLowBatCounter = 0;
			if (battery.lowBatCounter < 5) battery.lowBatCounter++;
			else led.chargeStatus = led.CHARGE_LOW;

		} else {
			battery.emergencyLowBatCounter = 0;
			battery.lowBatCounter = 0;
			led.chargeStatus = led.CHARGE_NULL;
		}
	}
}

// **** Sensors
void SckBase::updateSensors()
{
	if (!rtc.isConfigured() || rtc.getEpoch() < 1514764800) st.timeStat.reset();
	if (!st.timeStat.ok || st.helloPending) return;
	if (st.onSetup) return;
	if (st.mode == MODE_SD && !st.cardPresent) return;

	// Main reading loop
	if (rtc.getEpoch() - lastSensorUpdate >= config.readInterval) {

		ISOtime();
		lastSensorUpdate = rtc.getEpoch();
		pendingSensors = 0;

		sckOut("\r\n-----------", PRIO_LOW);
		sckOut(ISOtimeBuff, PRIO_LOW);

		// Create new RAM group with this timestamp
		if (!readingsList.createGroup(lastSensorUpdate)) {
			sckOut("RAM full: Error creating new group of readings!!!");
			sck_reset(); // TODO this is temporal until flash support is ready
			return;
		};

		for (uint8_t i=0; i<SENSOR_COUNT; i++) {

			// Get next sensor based on priority
			OneSensor wichSensor = sensors[sensors.sensorsPriorized(i)];

			// Check if it is enabled
			if (wichSensor.enabled) {

				// Is time to read it?
				if ((lastSensorUpdate - wichSensor.lastReadingTime) >= (wichSensor.everyNint * config.readInterval)) {

					if (!getReading(&wichSensor)) {

						pendingSensorsList[pendingSensors] = wichSensor.type;
						pendingSensors++;

					} else {
						// Save reading
						if (!readingsList.appendReading(wichSensor.type, wichSensor.reading)) sckOut("Failed saving reading!!!");
						wichSensor.lastReadingTime = lastSensorUpdate;
						sprintf(outBuff, "%s: %s %s", wichSensor.title, wichSensor.reading.c_str(), wichSensor.unit);
						sckOut();
					}
				}
			}
		}
		if (pendingSensors == 0) {
			if (!readingsList.saveLastGroup()) sckOut("Failed saving reading Group!!");
			sckOut("-----------", PRIO_LOW);
		}

	} else if (pendingSensors > 0) {

		SensorType tmpPendingSensorList[pendingSensors];
		uint8_t tmpPendingSensors = 0;

		for (uint8_t i=0; i<pendingSensors; i++) {

			OneSensor wichSensor = sensors[pendingSensorsList[i]];

			if (!getReading(&wichSensor)) {

				// Reappend the sensor to the pending list
				tmpPendingSensorList[i] = wichSensor.type;
				tmpPendingSensors ++;

			} else  {
				// Save reading
				if (!readingsList.appendReading(wichSensor.type, wichSensor.reading)) sckOut("Failed saving reading!!!");
				wichSensor.lastReadingTime = lastSensorUpdate;
				sprintf(outBuff, "%s: %s %s", wichSensor.title, wichSensor.reading.c_str(), wichSensor.unit);
				sckOut();
			}
		}

		pendingSensors = tmpPendingSensors;
		if (pendingSensors <= 0) {
			if (!readingsList.saveLastGroup()) sckOut("Failed saving reading Group!!");
			sckOut("-----------", PRIO_LOW);
		} else {
			for (uint8_t i=0; i<pendingSensors; i++) pendingSensorsList[i] = tmpPendingSensorList[i];
		}
	}


	if (rtc.getEpoch() - lastPublishTime >= config.publishInterval) {
		timeToPublish = true;
	}
}
bool SckBase::enableSensor(SensorType wichSensor)
{
	bool result = false;
	switch (sensors[wichSensor].location) {
		case BOARD_BASE:
		{
			switch (wichSensor) {
				case SENSOR_BATT_VOLTAGE:
				case SENSOR_BATT_PERCENT:
					// Allow enabling battery even if its not present so it can be posted to platform (reading will return -1 if the batery is not present)
					result = true;
					break;
				case SENSOR_SDCARD:
					result = true;
					break;
				default: break;
			}
		}
		case BOARD_URBAN: if (urban.start(wichSensor)) result = true; break;
		case BOARD_AUX:	{
					if (auxBoards.start(wichSensor)) result = true;
					break;
				}
		default: break;
	}

	if (result) {
		sprintf(outBuff, "Enabling %s", sensors[wichSensor].title);
		sensors[wichSensor].enabled = true;
		sckOut();
		writeHeader = true;
		return true;
	}

	return false;
}
bool SckBase::disableSensor(SensorType wichSensor)
{
	bool result = false;
	switch (sensors[wichSensor].location) {
		case BOARD_BASE:
		{
			switch (wichSensor) {
				case SENSOR_BATT_PERCENT:
				case SENSOR_BATT_VOLTAGE:
				case SENSOR_SDCARD:
					result = true;
					break;
				default: break;
			}
		}
		case BOARD_URBAN: if (urban.stop(wichSensor)) result = true; break;
		case BOARD_AUX: if (auxBoards.stop(wichSensor)) result = true; break;
		default: break;
	}

	if (result) {
		sprintf(outBuff, "Disabling %s", sensors[wichSensor].title);
		sensors[wichSensor].enabled = false;
		sckOut();
		writeHeader = true;
		return true;
	}

	return false;
}
bool SckBase::getReading(OneSensor *wichSensor)
{
	switch (wichSensor->location) {
		case BOARD_BASE:
		{
				switch (wichSensor->type) {
					case SENSOR_BATT_PERCENT:
						if (!battery.present) wichSensor->reading = String("-1");
						else wichSensor->reading = String(battery.percent(&charger));
						break;
					case SENSOR_BATT_VOLTAGE:
						if (!battery.present) wichSensor->reading = String("-1");
						else wichSensor->reading = String(battery.voltage());
						break;
					case SENSOR_SDCARD:
						if (st.cardPresent) wichSensor->reading = String("1");
						else wichSensor->reading = String("0");
						break;
					default: break;
				}
				wichSensor->state = 0;
				break;
		}
		case BOARD_URBAN:
		{
				urban.getReading(this, wichSensor);
				break;
		}
		case BOARD_AUX:
		{
				auxBoards.getReading(wichSensor, this);
				break;
		}
	}

	// Reading is not yet ready
	if (wichSensor->state > 0) return false;

	// Sensor reading ERROR, save null value
	if (wichSensor->state == -1) wichSensor->reading == "null";

	// Temperature / Humidity temporary Correction
	// TODO remove this when calibration routine is ready
	if (wichSensor->type == SENSOR_TEMPERATURE) {
		float aux_temp = wichSensor->reading.toFloat();

		// Correct depending on battery/USB and network/sd card status
		if (charger.onUSB) {
			if (st.mode == MODE_NET) wichSensor->reading = String(aux_temp - 2.6);
			else wichSensor->reading = String(aux_temp - 1.6);
		} else {
			wichSensor->reading = String(aux_temp - 1.3);
		}

	} else if(wichSensor->type == SENSOR_HUMIDITY) {
		float aux_hum = wichSensor->reading.toFloat();
		wichSensor->reading = String(aux_hum + 10);
	}

	return true;
}
bool SckBase::controlSensor(SensorType wichSensorType, String wichCommand)
{
	if (sensors[wichSensorType].controllable)  {
		sprintf(outBuff, "%s: %s", sensors[wichSensorType].title, wichCommand.c_str());
		sckOut();
		switch (sensors[wichSensorType].location) {
				case BOARD_URBAN: urban.control(this, wichSensorType, wichCommand); break;
				case BOARD_AUX: sckOut(auxBoards.control(wichSensorType, wichCommand)); break;
				default: break;
			}

	} else {
		sprintf(outBuff, "No configured command found for %s sensor!!!", sensors[wichSensorType].title);
		sckOut();
		return false;
	}
	return true;
}
bool SckBase::netPublish()
{
	if (!st.espON) {
		ESPcontrol(ESP_ON);
		return false;
	}

	/* if (ramGroupsIndex < 0) return false; */

	// /* Example
	// {	t:2017-03-24T13:35:14Z,
	// 		29:48.45,
	// 		13:66,
	// 		12:28,
	// 		10:4.45
	// }
	// 	*/


	bool result = false;
	if (readingsList.countGroups() > 0) {
		uint32_t thisGroup = 0;
		if (readingsList.getFlag(thisGroup, readingsList.NET_PUBLISHED) == 0) {

			memset(netBuff, 0, sizeof(netBuff));
			uint16_t publishedReadings = 0;
			sprintf(netBuff, "%c", ESPMES_MQTT_PUBLISH);

			// Save time
			epoch2iso(readingsList.getTime(thisGroup), ISOtimeBuff);
			sprintf(netBuff, "%s{t:%s", netBuff, ISOtimeBuff);

			uint16_t readingsOnThisGroup = readingsList.countReadings(thisGroup);
			for (uint8_t i=0; i<readingsOnThisGroup; i++) {

				OneReading thisReading = readingsList.readReading(thisGroup, i);
				if (sensors[thisReading.type].id > 0 && !thisReading.value.startsWith("null")) {

					sprintf(netBuff, "%s,%u:%s", netBuff, sensors[thisReading.type].id, thisReading.value.c_str());;
					publishedReadings ++;
				}
			}

			sprintf(netBuff, "%s%s", netBuff, "}");

			sprintf(outBuff, "(%s) Sent %i readings to platform.", ISOtimeBuff, publishedReadings);
			sckOut();

			result = sendMessage();

		} else {

			// If the group is already published delete it from saved ones
			epoch2iso(readingsList.getTime(thisGroup), ISOtimeBuff);
			sprintf(outBuff, "(%s) Published OK, erasing from memory", ISOtimeBuff);
			sckOut();
			readingsList.delLastGroup();
		}

	}
	return result;
}
bool SckBase::sdPublish()
{
	if (!sdSelect()) return false;

	sprintf(postFile.name, "%02d-%02d-%02d.CSV", rtc.getYear(), rtc.getMonth(), rtc.getDay());
	if (!sd.exists(postFile.name)) writeHeader = true;
	else {
		if (writeHeader) {
			// This means actual enabled/disabled sensors are different from saved ones
			// So we rename original file and start a new one
			char newName[13];
			char prefix[13];
			sprintf(prefix, "%02d-%02d-%02d.", rtc.getYear(), rtc.getMonth(), rtc.getDay());
			bool fileExists = true;
			uint16_t fileNumber = 1;
			while (fileExists) {
				sprintf(newName, "%s%02u", prefix, fileNumber);
				fileNumber++;
				if (!sd.exists(newName)) fileExists = false;
			}
			sd.rename(postFile.name, newName);
		}
	}

	postFile.file = sd.open(postFile.name, FILE_WRITE);

	if (postFile.file) {

		// Write headers
		if (writeHeader) {
			postFile.file.print("TIME");
			for (uint8_t i=0; i<SENSOR_COUNT; i++) {
				SensorType wichSensor = sensors.sensorsPriorized(i);
				if (sensors[wichSensor].enabled) {
					postFile.file.print(",");
					postFile.file.print(sensors[wichSensor].shortTitle);
				}
			}
			postFile.file.println("");
			postFile.file.print("ISO 8601");
			for (uint8_t i=0; i<SENSOR_COUNT; i++) {
				SensorType wichSensor = sensors.sensorsPriorized(i);
				if (sensors[wichSensor].enabled) {
					postFile.file.print(",");
					if (String(sensors[wichSensor].unit).length() > 0) {
						postFile.file.print(sensors[wichSensor].unit);
					}
				}
			}
			postFile.file.println("");
			postFile.file.print("Time");
			for (uint8_t i=0; i<SENSOR_COUNT; i++) {
				SensorType wichSensor = sensors.sensorsPriorized(i);
				if (sensors[wichSensor].enabled) {
					postFile.file.print(",");
					postFile.file.print(sensors[wichSensor].title);
				}
			}
			postFile.file.println("");
			for (uint8_t i=0; i<SENSOR_COUNT; i++) {
				SensorType wichSensor = sensors.sensorsPriorized(i);
				if (sensors[wichSensor].enabled) {
					postFile.file.print(",");
					postFile.file.print(sensors[wichSensor].id);
				}
			}
			postFile.file.println("");
			writeHeader = false;
		}

		// From the saved groups check wich one's need to be published to sdcard
		uint32_t savedGroups = readingsList.countGroups();
		uint8_t counter = 0;
		for (uint32_t thisGroup=0; thisGroup<savedGroups; thisGroup++) {
			if (readingsList.getFlag(thisGroup, readingsList.SD_PUBLISHED) == 0) {

				uint16_t readingsOnThisGroup = readingsList.countReadings(thisGroup);

				// Save time
				epoch2iso(readingsList.getTime(thisGroup), ISOtimeBuff);
				postFile.file.print(ISOtimeBuff);


				// Go through all the enabled sensors
				for (uint8_t i=0; i<SENSOR_COUNT; i++) {
					SensorType wichSensor = sensors.sensorsPriorized(i);
					if (sensors[wichSensor].enabled) {

						bool founded = false;
						// Find sensor inside group readings
						// TODO this can be optimized
						for (uint16_t re=0; re<readingsOnThisGroup; re++) {
							OneReading thisReading = readingsList.readReading(thisGroup, re);
							if (thisReading.type == wichSensor) {

								// Save reading
								founded = true;
								postFile.file.print(",");
								postFile.file.print(thisReading.value);
							}
						}

						if (!founded) {
							postFile.file.print(",");
							postFile.file.print("null");
						}
					}
				}

				// Set SD_PUBLISHED flag for this group
				readingsList.setFlag(thisGroup, readingsList.SD_PUBLISHED, true);

				// newLine
				postFile.file.println("");

				counter++;

				epoch2iso(readingsList.getTime(thisGroup), ISOtimeBuff);
				sprintf(outBuff, "(%s) Readings saved to sdcard.", ISOtimeBuff);
				sckOut();
			}
		}

		postFile.file.close();

		if (counter > 0) {

			// If we are on MODE_SD we can delete the published groups
			if (st.mode == MODE_SD) {
				for (uint8_t i=0; i<counter; i++) readingsList.delLastGroup();
			}
			return true;
		}
	} else  {
		st.cardPresent = false;
		st.cardPresentError = false;
	}
	return false;
}

// **** Time
bool SckBase::setTime(String epoch)
{
	uint32_t wasOn = rtc.getEpoch() - espStarted;
	rtc.setEpoch(epoch.toInt());
	if (abs(rtc.getEpoch() - epoch.toInt()) < 2) {
		timeSyncAfterBoot = true;
		st.timeStat.setOk();
		espStarted = rtc.getEpoch() - wasOn;
		ISOtime();
		sprintf(outBuff, "RTC updated: %s", ISOtimeBuff);
		sckOut();
		return true;
	} else {
		sckOut("RTC update failed!!");
	}
	return false;
}
bool SckBase::ISOtime()
{
	if (st.timeStat.ok) {
		epoch2iso(rtc.getEpoch(), ISOtimeBuff);
		return true;
	} else {
		sprintf(ISOtimeBuff, "0");
		return false;
	}
}
void SckBase::epoch2iso(uint32_t toConvert, char* isoTime)
{

	time_t tc = toConvert;
	struct tm* tmp = gmtime(&tc);

	sprintf(isoTime, "20%02d-%02d-%02dT%02d:%02d:%02dZ",
			tmp->tm_year - 100,
			tmp->tm_mon + 1,
			tmp->tm_mday,
			tmp->tm_hour,
			tmp->tm_min,
			tmp->tm_sec);
}


void SckBase::getUniqueID()
{
	volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
	uniqueID[0] = *ptr1;

	volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
	uniqueID[1] = *ptr;
	ptr++;
	uniqueID[2] = *ptr;
	ptr++;
	uniqueID[3] = *ptr;

	sprintf(uniqueID_str,  "%lX%lX%lX%lX", uniqueID[0], uniqueID[1], uniqueID[2], uniqueID[3]);
}
bool I2Cdetect(TwoWire *_Wire, byte address)
{
	_Wire->beginTransmission(address);
	byte error = _Wire->endTransmission();

	if (error == 0) return true;
	else return false;
}

void Status::setOk()
{
	ok = true;
	error = false;
	retrys = 0;
	_lastTryMillis = 0;
}
bool Status::retry()
{
	if (error) return false;
	if (millis() - _lastTryMillis > _timeout || _lastTryMillis == 0) {
		if (retrys == _maxRetrys) {
			error = true;
			return false;
		}
		_lastTryMillis = millis();
		retrys++;
		return true;
	}

	return false;
}
void Status::reset()
{
	ok = false;
	error = false;
	retrys = 0;
	_lastTryMillis = 0;
}
