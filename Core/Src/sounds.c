/*
 * sounds.c
 *
 *  Created on: Dec 16, 2024
 *      Author: klim405
 */
#include "sounds.h"

#include <math.h>

#define TOTAL_SOUNDS 12.0

/**
 * @brief Вычисляет частоту ноты
 * @note Человеческое ухо способно воспринять 11 октав и частично 0 и 10.
 * @param octave номер октавы. Нумерация начинается с 0
 * @param note номер ноты в октаве
 * @return частоту ноты
 */
double Sounds_CalcFrequency(int8_t octave, int8_t note) {
    return SOUNDS_C0 * pow(2.0, (double) octave) * pow(2.0, note / TOTAL_SOUNDS);
}

/**
 * @brief Вычисляет период ноты
 * @param ocatave номер октавы
 * @param note номер ноты в октаве
 * @param unit множитель для периода. Например 1000 - мс.
 * @return Период математическое округление
 */
uint16_t Sounds_CalcPeriod(int8_t octave, int8_t note, int32_t unit) {
    double f = Sounds_CalcFrequency(octave, note);
    return (uint16_t) round(unit / f);
}

/**
 * @brief Вычисляет период в наносекундах ноты
 * @param octave номер октавы
 * @param note номер ноты в октаве
 * @return Период математическое округление
 */
uint16_t Sounds_CalcPeriodInNs(int8_t octave, int8_t note) {
    double f = Sounds_CalcFrequency(octave, note);
    return (uint16_t) round(10000000 / f);
}

/**
 * @brief Преобразует символ ноты в звук
 * @note Предпологается что ноты полутонов обозначаются маленькими буквами.
 *       Например "D" - ре; "d" - ре диез (D#).
 * @param note символ ноты.
 * @return Номер ноты. Если число отрицательное, значит не удалось определить ноту.
 */
int8_t Sounds_ParseNote(char note) {
    int8_t note_number = -2;

    if (note >= 'a') {  // Это полутон ?
        note -= 32;     // Приводим к нижнему регистру
        ++note_number;  // Увеличиваем номер ноты на 1
    }

    if (note == 'C') {
        note_number += 2;
    } else if (note == 'D') {
        note_number += 4;
    } else if (note == 'E') {
        note_number += 6;
    } else if (note == 'F') {
        note_number += 7;
    } else if (note == 'G') {
        note_number += 9;
    } else if (note == 'A') {
        note_number += 11;
    } else if (note == 'H') {
        note_number += 13;
    }

    return note_number;
}
