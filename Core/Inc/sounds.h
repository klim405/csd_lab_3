/*
 * sounds.h
 *
 *  Created on: Dec 16, 2024
 *      Author: klim405
 */

#ifndef INC_SOUNDS_H_
#define INC_SOUNDS_H_

#include <stdint.h>

#define SOUNDS_C0 16.352

double Sounds_CalcFrequency(int8_t ocatave, int8_t note);

uint16_t Sounds_CalcPeriod(int8_t ocatave, int8_t note, int32_t unit);

uint16_t Sounds_CalcPeriodInNs(int8_t octave, int8_t note);

int8_t Sounds_ParseNote(char note);

#endif /* INC_SOUNDS_H_ */
