#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Pins.h>
#include "wiring_private.h"

const float workingVoltage = 3.3;

class SckBase;

class SckCharger
{
private:
	uint8_t address = 0x6B;
	uint8_t timeout = 10; // ms

	// Registers
	byte INPUT_SOURCE_CONTROL = 0;
	byte INPUT_CURR_LIMIT = 0;		// Bits 0:2 - > 000 – 100 mA, 001 – 150 mA, 010 – 500 mA, 011 – 900 mA, 100 – 1 A, 01 – 1.5 A, 110 - 2 A 111 - 3 A

	byte POWER_ON_CONF_REG = 1;
	byte BOOST_LIM = 0;			// Limit for boost mode. 0 – 1 A, 1 – 1.5 A
	byte VSYS_MIN = 1; 			// (Three bits 1:3 -- bit 1 - 0.1v, bit 2 - 0.2v, bit 3 - 0.4v) Minimum System Voltage Limit -> Offset:3.0 V, Range3.0 V – 3.7 VDefault:3.5 V (101)
	byte CHG_CONFIG = 4;			// Charger Configuration. 0 - Charge Disable; 1- Charge Enable
	byte OTG_CONFIG = 5;			// Charger Configuration. 0 – OTG Disable; 1 – OTG Enable. OTG_CONFIG would over-ride Charge Enable Function in CHG_CONFIG
	byte I2C_WATCHDOG_TIMER_RESET = 6;	// I2C Watchdog Timer Reset. 0 – Normal; 1 – Reset
	byte RESET_DEFAULT_CONFIG = 7;		// 0 – Keep current register setting, 1 – Reset to default

	byte CHARGE_CURRENT_CONTROL = 2;
	byte CHARGE_CURRENT_LIMIT = 2;		// Limit charge current.(bits 2:6) Range: 512 – 2048mA (00000–11000), bit6 - 1024mA, bit5 - 512mA, bit4 - 256mA, bit3 - 128mA, bit2 - 64mA, (ej. 00000 -> 512mA, 00100 -> 768mA)

	byte CHARGE_VOLT_CONTROL = 4;
	byte BATLOWV = 1; 			// 0 – 2.8V, 1 – 3V Default:3.0 V

	byte CHG_TERM_TIMER_CTRL = 5;
	byte CHG_TIMER = 1;			// Fast Charge Timer Setting. (Two bits 1:2) 00 – 5 hrs, 01 – 8 hrs, 10 – 12 hrs, 11 – 20 hrs
	byte CHG_TIMER_ENABLE = 3;		// Charging Safety Timer Enable. 0 - Disable, 1 - Enable.
	byte I2C_WATCHDOG = 4;			// I2C Watchdog Timer Setting. (Two bits 4:5) 00 – Disable timer, 01 – 40 s, 10 – 80 s, 11 – 160 s
	byte CHG_TERM_ENABLE = 7;		// Charging Termination Enable. 0 - Disable, 1 - Enable.

	byte MISC_REG = 7;
	byte INT_MASK_BAT_FAULT = 0;		// Battery fault interrupt mask. 0 – No INT during CHRG_FAULT, 1 – INT on CHRG_FAULT
	byte INT_MASK_CHRG_FAULT = 1;		// CHarge fault interrupt mask. 0 – No INT during BAT_FAULT, 1 – INT on BAT_FAULT
	byte BATFET_DISABLE = 5; 		// Force BATFET Off. 0 - Allow BATFET (Q4) turn on, 1 - Turn off BATFET (Q4)
	byte DPDM_EN = 7;			// Force DPDM detection. 0 – Not in Force detection; 1 – Force detection when VBUS power is present

	byte SYS_STATUS_REG = 8;		// System Status Register
	byte VSYS_STAT = 0; 			// 0 – Not in VSYSMIN regulation (BAT > VSYSMIN), 1 – In VSYSMIN regulation (BAT < VSYSMIN)
	byte PG_STAT = 2;			// Power Good status. 0 – Not Power Good, 1 – Power Good
	byte DPM_STAT = 3;			// DPM (detection) 0 - Not in DPM, 1 - On DPM
	byte CHRG_STAT = 4;			// (Two bits 4:5) 00 – Not Charging, 01 – Pre-charge (<VBATLOWV), 10 – Fast Charging, 11 – Charge Termination Done
	byte VBUS_STAT = 6;			// (Two bits 6:7) 00 – Unknown (no input, or DPDM detection incomplete), 01 – USB host, 10 – Adapter port, 11 – OTG

	byte NEW_FAULT_REGISTER = 9;
	byte BAT_FAULT = 3;			// 0 – Normal, 1 – Battery OVP
	byte CHRG_FAULT = 4;			// (Two bits 4:5) 00 – Normal, 01 – Input fault (OVP or bad source), 10 - Thermal shutdown, 11 – Charge Timer Expiration
	byte OTG_FAULT = 6;			// 0 – Normal, 1 – VBUS overloaded in OTG, or VBUS OVP, or battery is too low (any conditions that cannot start boost function)
	byte WATCHDOG_FAULT = 7;		// 0 – Normal, 1- Watchdog timer expiration

	float batLow = 3.0; 			// If batt < batLow the charger will start with precharging cycle.
	byte readREG(byte wichRegister);
	bool writeREG(byte wichRegister, byte data);

public:

	enum ChargeStatus {
		CHRG_NOT_CHARGING,
		CHRG_PRE_CHARGE,
		CHRG_FAST_CHARGING,
		CHRG_CHARGE_TERM_DONE,

		CHRG_STATE_COUNT
	};

	enum VBUSstatus {
		VBUS_UNKNOWN,
		VBUS_USB_HOST,
		VBUS_ADAPTER_PORT,
		VBUS_OTG,

		VBUS_STATUS_COUNT
	};

	const char *chargeStatusTitles[CHRG_STATE_COUNT] PROGMEM = {
		"Not charging",
		"Pre charging",
		"Fast charging",
		"Charge finished",
	};

	const char *VBUSStatusTitles[VBUS_STATUS_COUNT] PROGMEM = {
		"Unknown",
		"USB host connected",
		"Adapter connected",
		"USB OTG connected",
	};

	const char *enTitles[2] PROGMEM = {
		"disabled",
		"enabled",
	};

	void setup();
	bool resetConfig();
	uint16_t inputCurrentLimit(int16_t current=-1);
	uint16_t chargerCurrentLimit(int16_t current=-1);
	bool chargeState(int8_t enable=-1);
	bool batfetState(int8_t enable=-1);
	bool OTG(int8_t enable=-1);
	uint8_t I2Cwatchdog(int16_t seconds=-1);
	uint8_t chargeTimer(int8_t hours=-1);
	bool getPowerGoodStatus();
	bool getDPMstatus();
	void forceInputCurrentLimitDetection();
	void detectUSB(SckBase *base);
	float getSysMinVolt();
	bool getBatLowerSysMin();
	ChargeStatus getChargeStatus();
	VBUSstatus getVBUSstatus();
	byte getNewFault();		// TODO
	bool onUSB = true;
	bool batfetON = false;
	ChargeStatus chargeStatus = CHRG_NOT_CHARGING;

};

class SckBatt
{
	private:
		const float nominalVoltage = 3.7;
		const uint16_t analogResolution = 4096;

		const uint16_t batTable[100] PROGMEM = {3324,3326,3335,3357,3376,3394,3411,3428,3436,3447,3460,3467,3471,3482,3495,3502,3511,3516,3524,3525,3533,3533,3544,3550,3554,3561,3565,3571,3571,3577,3583,3593,3594,3594,3603,3605,3605,3612,3617,3619,3622,3625,3629,3634,3641,3641,3647,3651,3655,3659,3665,3672,3679,3686,3693,3696,3700,3708,3717,3723,3731,3740,3748,3753,3766,3771,3778,3784,3791,3799,3809,3819,3826,3832,3843,3852,3858,3867,3880,3892,3901,3911,3917,3926,3942,3950,3959,3968,3975,3986,4001,4011,4025,4042,4055,4069,4100,4117,4152,4201};

	public:
		bool present = false;

		float maxVolt = 4.1; 	// This should be updated when dynamic lookup table is implemented

		const uint8_t threshold_low = 10;
		const uint8_t threshold_emergency = 2;

		uint8_t lowBatCounter = 0;
		uint8_t emergencyLowBatCounter = 0;

		bool setup(SckCharger charger);
		float voltage();
		int8_t percent();
};

void ISR_charger();
