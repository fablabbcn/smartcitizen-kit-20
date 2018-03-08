#include <AlphaDelta.h>

bool AlphaDelta::begin() {

	if (!I2Cdetect(sht31Address)) return false;

	if (alreadyStarted) return true;
	alreadyStarted = true;

	sht31.begin();

	// Set all potentiometers to 0
	setPot(Slot1.electrode_A, 0);
	setPot(Slot1.electrode_W, 0);
	setPot(Slot2.electrode_A, 0);
	setPot(Slot2.electrode_W, 0);
	setPot(Slot3.electrode_A, 0);
	setPot(Slot3.electrode_W, 0);

	return true;
}

float AlphaDelta::getTemperature() {

	SerialUSB.print("UID: ");
	SerialUSB.println(getUID());

	SerialUSB.println("Write Test... ");
	uint8_t writed = 5;
    SerialUSB.println(writeByte(0x14, writed));
    uint8_t readed = readByte(0x14);
    SerialUSB.println(readed);
    if (writed == readed) SerialUSB.println("OK!");
    else SerialUSB.println("ERROR!!!");

	return sht31.readTemperature();
}

float AlphaDelta::getHumidity() {

	return sht31.readHumidity();
}

uint32_t AlphaDelta::getPot(Electrode wichElectrode) {

	return ((255 - readI2C(wichElectrode.resistor.address, wichElectrode.resistor.channel)) * ohmsPerStep);
}

void AlphaDelta::setPot(Electrode wichElectrode, uint32_t value) {

	int data=0x00;
	if (value>100000) value = 100000;
	data = 255 - (int)(value/ohmsPerStep);		// POT's are connected 'upside down' (255 - step)
	
	writeI2C(wichElectrode.resistor.address, 16, 192);        	// select WR (volatile) registers in POT
	writeI2C(wichElectrode.resistor.address, wichElectrode.resistor.channel, data);
}

uint8_t AlphaDelta::getPGAgain(MCP342X adc) {
	uint8_t gainPGA = adc.getConfigRegShdw() & 0x3;
	return pow(2, gainPGA);
}

float AlphaDelta::getElectrodeGain(Electrode wichElectrode) {

	return (((getPot(wichElectrode) + 85) / 10000.0f) + 1) * getPGAgain(wichElectrode.adc);
}

uint32_t AlphaDelta::getUID() {
    uint8_t UIDBytes[4];
    if(readConsecutive(UIDBytes, 0xFC, 4) != 4){
        return 0;
    }
    uint8_t pos;
    uint32_t UID = 0;
    for(pos = 0; pos < 4; pos++){
        UID <<= 8;
        UID |= UIDBytes[pos];
    }
    return UID;
}

// TEMP this is ugly!!
uint8_t AlphaDelta::readConsecutive(uint8_t * dataBuffer, uint8_t startAddress, uint8_t bytes){
    Wire.beginTransmission(eepromAddress);
    Wire.write(startAddress);
    Wire.endTransmission(false);
    if(Wire.requestFrom(eepromAddress, bytes) != bytes){
        return 0;
    } else {
        uint8_t pos;
        for(pos = 0; pos < bytes; pos++){
            dataBuffer[pos] = Wire.read();
        }
        return bytes;
    }
}
uint8_t AlphaDelta::writeByte(uint8_t dataAddress, uint8_t data){
    Wire.beginTransmission(eepromAddress);
    SerialUSB.println(Wire.write(dataAddress));
    SerialUSB.println(Wire.write(data));
    return Wire.endTransmission();
}
uint8_t AlphaDelta::readByte(uint8_t dataAddress){
    Wire.beginTransmission(eepromAddress);
    Wire.write(dataAddress);
    if(Wire.endTransmission(false)){
        // Handle error
        return 0; // May need value here <------
    }
    if(!Wire.requestFrom(eepromAddress, 1)){
        // Handle error
        return 0; // May need value here <------
    }
    return Wire.read();
}


// Returns electrode value in mV
double AlphaDelta::getElectrode(Electrode wichElectrode) {

	static int32_t result;

	// Gain can be changed before calling this funtion with: wichElectrode.gain = newGain (0->gain of 1, 1->gain of 2, 2->gain of 3 or 3->gain 0f 8)
	wichElectrode.adc.configure( MCP342X_MODE_ONESHOT | MCP342X_SIZE_18BIT | wichElectrode.gain);	
	wichElectrode.adc.startConversion(wichElectrode.channel);
	wichElectrode.adc.getResult(&result);

	return (result * 0.015625) / getPGAgain(wichElectrode.adc);
}