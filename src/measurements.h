#pragma once

#include <Arduino.h>

#define VBAT_PIN            A13
#define LIGHT_SENSOR_PIN    A2

class Measurements
{
    public:
        Measurements();
        float getBatteryVoltage();
		double readLightSensor();

		void analogRead(uint8_t pin, uint32_t NumOfSamples, uint16_t &adcRaw); //call this every loop
		bool newValueFlag; //Set true for one loop when the adcRaw value is updated.  Can be used to update scaling only when nessesary

		double analogCalibration(double adcRaw) {// Performs a polynomial fit to linearize the awful ADC
			double adcCalibrated = 0;
			if (X6 != 0) adcCalibrated += X6 * pow(adcRaw, 6);
			if (X5 != 0) adcCalibrated += X5 * pow(adcRaw, 5);
			if (X4 != 0) adcCalibrated += X4 * pow(adcRaw, 4);
			if (X3 != 0) adcCalibrated += X3 * pow(adcRaw, 3);
			if (X2 != 0) adcCalibrated += X2 * pow(adcRaw, 2);
			if (X != 0) adcCalibrated += X * adcRaw;
			if (b != 0) adcCalibrated += b;
			return adcCalibrated;
		};

    private:
        uint32_t _adcSamplesTotal;
		uint32_t _adcCurrentSamples;
        uint8_t _arbitrationToken;

        //Note that these default values were the average of a couple ESP32's I had and linearized the 0-4095 12-bit raw counts.
		//They will most likely give you a useable linear response accurate to 100mV (over a 0-3.3V range, or 11dB attenuation)
		//However you can perform your own calibration with excel and a trendline to obtain the coefficients and therefore characterize each individual ESP32.  See spreadsheet provided in the library
		double X6 = 3.07344E-18;
		double X5 = -3.28432E-14;
		double X4 = 1.16038E-10;
		double X3 = -1.531075E-07;
		double X2 = 0.0000357848;
		double X = 1.06235;
		double b = 180.189;
};