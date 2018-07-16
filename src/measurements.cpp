#include "measurements.h"
#include "general.h"

Measurements::Measurements() {
	// ADC is apparently very noisey
	// analogSetWidth(10);                           // 10Bit resolution
	//analogSetAttenuation((adc_attenuation_t)2);   // -6dB range

	//An initial useless adc read is performed because when the ESP32 boots up its ADC its "busy".
	//Doing this at least once, "releases" the ADC for the first instance of this library to take ownership and start working
	adcAttachPin(VBAT_PIN);
	adcStart(VBAT_PIN);
}

void Measurements::analogRead(uint8_t pin, uint32_t NumOfSamples, uint16_t &adcRaw) {
	/*
	uint8_t pin = Valid GPIO pins on ADC1 (ADC2 not yet implemented).  Value pins are:  32, 33, 34, 35, 36, 37, 38, or 39
	uint16_t NumOfSamples = Number of samples to average together for better smoothing.
	&uint16_t adcRaw = your global variable that this routine will modify when complete
	I.E. analogRead(39,100,Pin39ADCRaw, ADC1ArbitrationToken).  This will take 100 samples ( min of ~1ms @ 10us per sample)
	*/
	newValueFlag = false;
	if (adcBusy(pin) || (_arbitrationToken != pin && _arbitrationToken > 0)) return; // no point in doing anything if the adc is already doing something or someone else has ownership

	if (_arbitrationToken == 0) { // adc isn't busy, nor is it arbitrated, now's your chance!
		adcAttachPin(pin);
		adcStart(pin);
		_arbitrationToken = pin; // take ownership of the token by setting it to your pin
		_adcSamplesTotal = 0;
		_adcCurrentSamples = 0;
		// DEBUG_MSG_P(PSTR("[ADC] Measurement started for %d at %lu\n"), pin, millis());
		return; // no reason to continue, check for completion on the next loop
	}
	else if (_arbitrationToken == pin) { // you must have ownership and the adc is done.  This really could have been just an else statement, but for completeness...
		
		_adcSamplesTotal += adcEnd(pin); // get the adc results and add them to a total for averaging
		_adcCurrentSamples++; // increment the number of samples taken for averaging
		// DEBUG_MSG_P(PSTR("[ADC] Got sample %d of %d as %d\n"), _adcCurrentSamples, NumOfSamples, _adcSamplesTotal);
		if (_adcCurrentSamples >= NumOfSamples) { // you have taken the requested number of samples, take the average and release arbitration
			adcRaw = _adcSamplesTotal / _adcCurrentSamples; // take the average
			_arbitrationToken = 0; // release arbitration
			newValueFlag = true;
		}
		else { // you have more samples to take, start the adc again
			adcStart(pin);
		}
	}
}

double Measurements::readLightSensor() {
    uint16_t rawValue = 0;
	newValueFlag = false;
    // 10 samples @ ~10us per sample gives a ~100us read time
    while(!newValueFlag) analogRead(LIGHT_SENSOR_PIN, 10, rawValue);
	double calibratedValue = analogCalibration((double)rawValue);
	
	// 12 bit adc converted to 10^5 lux at 3.3V
	double logLux = calibratedValue * 5.0 / 4095;
	return pow(10, logLux);
}

float Measurements::getBatteryVoltage() {
    uint16_t vbatRaw = 0;
	newValueFlag = false;
    // 10 samples @ ~10us per sample gives a ~100us read time
    while(!newValueFlag) analogRead(VBAT_PIN, 10, vbatRaw);

    double measuredvbat = analogCalibration((double)vbatRaw);
	// DEBUG_MSG_P(PSTR("[ADC] Raw sample: %d Calibrated: %f\n"), vbatRaw, measuredvbat);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    //measuredvbat *= 1.1;  // Multipy by ADC reference voltage
    measuredvbat /= 4095; // convert to voltage from 12 bit ADC

    return float(measuredvbat);
}