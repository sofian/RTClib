/*
 * RTCLib.h
 *
 * Original code by JeeLabs http://news.jeelabs.org/code/
 * Based on LadyAda's version https://github.com/adafruit/RTClib
 * Millisecond hack by Sofian Audry -- info(@)sofianaudry(.)com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "RTClib.h"
#include <Wire.h>
#include <avr/pgmspace.h>

#define DS1307_ADDRESS 0x68
#define SECONDS_PER_DAY 86400L

#define SECONDS_FROM_1970_TO_2000 946684800

#ifndef WIRE_WRITE
#if ARDUINO >= 100
#define WIRE_WRITE write
#else
#define WIRE_WRITE send
#endif
#endif

#ifndef WIRE_READ
#if ARDUINO >= 100
#define WIRE_READ read
#else
#define WIRE_READ receive
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

static const uint8_t daysInMonth [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += pgm_read_byte(daysInMonth + i - 1);
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000;    // bring to 2000 timestamp from 1970

    ss = t % 60;
    t /= 60;
    mm = t % 60;
    t /= 60;
    hh = t % 24;
    uint16_t days = t / 24;
    uint8_t leap;
    for (yOff = 0; ; ++yOff) {
        leap = yOff % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }
    for (m = 1; ; ++m) {
        uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
    
    ms = 0;
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec, uint16_t millisec) {
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = day;
    hh = hour;
    mm = min;
    ss = sec;
    ms = millisec;
}

static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using PSTR would further reduce the RAM footprint
DateTime::DateTime (const char* date, const char* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    yOff = conv2d(date + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec 
    switch (date[0]) {
        case 'J': m = date[1] == 'a' ? 1 : m = date[2] == 'n' ? 6 : 7; break;
        case 'F': m = 2; break;
        case 'A': m = date[2] == 'r' ? 4 : 8; break;
        case 'M': m = date[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(date + 4);
    hh = conv2d(time);
    mm = conv2d(time + 3);
    ss = conv2d(time + 6);
    ms = 0;
}

uint8_t DateTime::dayOfWeek() const {    
    uint16_t day = date2days(yOff, m, d);
    return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::unixtime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2long(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000;  // seconds from 1970 to 2000

  return t;
}

////////////////////////////////////////////////////////////////////////////////
// RTC_DS1307 implementation

static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

uint8_t RTC_DS1307::begin(void) {
  return 1;
}

uint8_t RTC_DS1307::isrunning(void) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.WIRE_WRITE(0);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 1);
  uint8_t ss = Wire.WIRE_READ();
  return !(ss>>7);
}

void RTC_DS1307::adjust(const DateTime& dt) {
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire.WIRE_WRITE(0);
    Wire.WIRE_WRITE(bin2bcd(dt.second()));
    Wire.WIRE_WRITE(bin2bcd(dt.minute()));
    Wire.WIRE_WRITE(bin2bcd(dt.hour()));
    Wire.WIRE_WRITE(bin2bcd(0));
    Wire.WIRE_WRITE(bin2bcd(dt.day()));
    Wire.WIRE_WRITE(bin2bcd(dt.month()));
    Wire.WIRE_WRITE(bin2bcd(dt.year() - 2000));
    Wire.WIRE_WRITE(0);
    Wire.endTransmission();
    
    prevUnixtime = dt.unixtime();
    prevMillis = millis();
}

DateTime RTC_DS1307::now() {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.WIRE_WRITE(0);
  Wire.endTransmission();
  
  Wire.requestFrom(DS1307_ADDRESS, 7);
  uint8_t ss = bcd2bin(Wire.WIRE_READ() & 0x7F);
  uint8_t mm = bcd2bin(Wire.WIRE_READ());
  uint8_t hh = bcd2bin(Wire.WIRE_READ());
  Wire.WIRE_READ();
  uint8_t d = bcd2bin(Wire.WIRE_READ());
  uint8_t m = bcd2bin(Wire.WIRE_READ());
  uint16_t y = bcd2bin(Wire.WIRE_READ()) + 2000;
  
  DateTime dt (y, m, d, hh, mm, ss);

  unsigned long curms = millis();
  long ms = (curms - prevMillis);
  ms -= (dt.unixtime() - prevUnixtime) * 1000;
  ms = constrain(ms, 0, 999);

  dt.ms = ms;  
  prevUnixtime = dt.unixtime();
  prevMillis = curms;
    
  return dt;
}

unsigned long RTC_DS1307:: prevMillis = 0;
uint32_t RTC_DS1307:: prevUnixtime = 0;

////////////////////////////////////////////////////////////////////////////////
// RTC_Millis implementation

long RTC_Millis::offset = 0;

void RTC_Millis::adjust(const DateTime& dt) {
  offsetms = millis();
    offset = dt.unixtime() - offsetms / 1000;
    offsetms %= 1000; // just keep the milliseconds part
}

DateTime RTC_Millis::now() {
  unsigned long time = millis();
  DateTime dt((uint32_t)(offset + time / 1000));
  dt.ms = time;
  return dt;
}

////////////////////////////////////////////////////////////////////////////////
