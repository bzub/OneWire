/*
Copyright (c) 2007, Jim Studt  (original old version - many contributors since)

The latest version of this library may be found at:
  http://www.pjrc.com/teensy/td_libs_OneWire.html

OneWire has been maintained by Paul Stoffregen (paul@pjrc.com) since
January 2010.

DO NOT EMAIL for technical support, especially not for ESP chips!
All project support questions must be posted on public forums
relevant to the board or chips used.  If using Arduino, post on
Arduino's forum.  If using ESP, post on the ESP community forums.
There is ABSOLUTELY NO TECH SUPPORT BY PRIVATE EMAIL!

Github's issue tracker for OneWire should be used only to report
specific bugs.  DO NOT request project support via Github.  All
project and tech support questions must be posted on forums, not
github issues.  If you experience a problem and you are not
absolutely sure it's an issue with the library, ask on a forum
first.  Only use github to report issues after experts have
confirmed the issue is with OneWire rather than your project.

Back in 2010, OneWire was in need of many bug fixes, but had
been abandoned the original author (Jim Studt).  None of the known
contributors were interested in maintaining OneWire.  Paul typically
works on OneWire every 6 to 12 months.  Patches usually wait that
long.  If anyone is interested in more actively maintaining OneWire,
please contact Paul (this is pretty much the only reason to use
private email about OneWire).

OneWire is now very mature code.  No changes other than adding
definitions for newer hardware support are anticipated.

Version 2.3:
  Unknown chip fallback mode, Roger Clark
  Teensy-LC compatibility, Paul Stoffregen
  Search bug fix, Love Nystrom

Version 2.2:
  Teensy 3.0 compatibility, Paul Stoffregen, paul@pjrc.com
  Arduino Due compatibility, http://arduino.cc/forum/index.php?topic=141030
  Fix DS18B20 example negative temperature
  Fix DS18B20 example's low res modes, Ken Butcher
  Improve reset timing, Mark Tillotson
  Add const qualifiers, Bertrik Sikken
  Add initial value input to crc16, Bertrik Sikken
  Add target_search() function, Scott Roberts

Version 2.1:
  Arduino 1.0 compatibility, Paul Stoffregen
  Improve temperature example, Paul Stoffregen
  DS250x_PROM example, Guillermo Lovato
  PIC32 (chipKit) compatibility, Jason Dangel, dangel.jason AT gmail.com
  Improvements from Glenn Trewitt:
  - crc16() now works
  - check_crc16() does all of calculation/checking work.
  - Added read_bytes() and write_bytes(), to reduce tedious loops.
  - Added ds2408 example.
  Delete very old, out-of-date readme file (info is here)

Version 2.0: Modifications by Paul Stoffregen, January 2010:
http://www.pjrc.com/teensy/td_libs_OneWire.html
  Search fix from Robin James
    http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295/27#27
  Use direct optimized I/O in all cases
  Disable interrupts during timing critical sections
    (this solves many random communication errors)
  Disable interrupts during read-modify-write I/O
  Reduce RAM consumption by eliminating unnecessary
    variables and trimming many to 8 bits
  Optimize both crc8 - table version moved to flash

Modified to work with larger numbers of devices - avoids loop.
Tested in Arduino 11 alpha with 12 sensors.
26 Sept 2008 -- Robin James
http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295/27#27

Updated to work with arduino-0008 and to include skip() as of
2007/07/06. --RJL20

Modified to calculate the 8-bit CRC directly, avoiding the need for
the 256-byte lookup table to be loaded in RAM.  Tested in arduino-0010
-- Tom Pollard, Jan 23, 2008

Jim Studt's original library was modified by Josh Larios.

Tom Pollard, pollard@alum.mit.edu, contributed around May 20, 2008

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
    (copyleft) 2006 by Derek Yerger - Free to distribute freely.

The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//--------------------------------------------------------------------------
*/

#include "OneWire.h"

OneWire::OneWire(const uint8_t pin)
{
    // prepare pin
    pinMode(pin, INPUT);
    pin_bitMask = PIN_TO_BITMASK(pin);
    pin_baseReg = PIN_TO_BASEREG(pin);
    DIRECT_WRITE_HIGH(pin_baseReg, pin_bitMask);    // on avr: act as pullup in input-mode

    reset_search(); // really needed?
}


// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
bool OneWire::reset()
{
    const io_reg_t _bitMask=pin_bitMask; // local copies save pgm-space when called >1
    volatile io_reg_t *_baseReg = pin_baseReg;

    uint8_t retries = 125;

    DIRECT_MODE_INPUT(_baseReg, _bitMask);
    DIRECT_WRITE_HIGH(_baseReg, _bitMask);    // on avr: act as pullup in input-mode
    // wait until the wire is high... just in case
    do
    {
        if (--retries == 0) return false;
        delayMicroseconds(2);
    } while (!DIRECT_READ(_baseReg, _bitMask));

    DIRECT_WRITE_LOW(_baseReg, _bitMask);
    DIRECT_MODE_OUTPUT(_baseReg, _bitMask);    // drive output low
    delayMicroseconds(480);
    noInterrupts();
    DIRECT_MODE_INPUT(_baseReg, _bitMask);    // allow it to float
    DIRECT_WRITE_HIGH(_baseReg, _bitMask);    // on avr: act as pullup in input-mode
    delayMicroseconds(70);
    const bool success = !DIRECT_READ(_baseReg, _bitMask);
    interrupts();
    delayMicroseconds(410);
    return success;
}

//
// Write a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
void OneWire::write_bit(const bool value, const bool power)
{
    const uint8_t time_high = uint8_t(value ? 10 : 60);
    const uint8_t time_low  = uint8_t(value ? 55 : 5);
    const io_reg_t _bitMask=pin_bitMask; // local copies save pgm-space when called >1
    volatile io_reg_t *_baseReg = pin_baseReg;

    noInterrupts();
    DIRECT_WRITE_LOW(_baseReg, _bitMask);
    DIRECT_MODE_OUTPUT(_baseReg, _bitMask);    // drive output low
    delayMicroseconds(time_high);
    if (!power) DIRECT_MODE_INPUT(_baseReg, _bitMask);    // allow it to float
    DIRECT_WRITE_HIGH(_baseReg, _bitMask);    // drive output high
    interrupts();
    delayMicroseconds(time_low);
}

//
// Read a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
bool OneWire::read_bit()
{
    const io_reg_t _bitMask=pin_bitMask; // local copies save pgm-space when called >1
    volatile io_reg_t *_baseReg = pin_baseReg;

    noInterrupts();
    DIRECT_WRITE_LOW(_baseReg, _bitMask);
    DIRECT_MODE_OUTPUT(_baseReg, _bitMask);
    delayMicroseconds(3);
    DIRECT_MODE_INPUT(_baseReg, _bitMask);    // let pin float, pull up will raise
    DIRECT_WRITE_HIGH(_baseReg, _bitMask);    // on avr: act as pullup in input-mode
    delayMicroseconds(10);
    const bool value = DIRECT_READ(_baseReg, _bitMask);
    interrupts();
    delayMicroseconds(53);
    return value;
}

//
// Write a byte. The writing code uses the active drivers to raise the
// pin high, if you need power after the write (e.g. DS18S20 in
// parasite power mode) then set 'power' to 1, otherwise the pin will
// go tri-state at the end of the write to avoid heating in a short or
// other mishap.
//
void OneWire::write(uint8_t value, const bool power)
{
    for (uint8_t index = 0; index < 8; index++)
    {
        write_bit((value & 0x01) != 0, power); // shifting value is faster than clean solution with bitmask (~18 byte on arduino)
        value >>= 1;
    }
}

void OneWire::write_bytes(const uint8_t data_array[], const uint16_t data_size, const bool power)
{
    for (uint16_t index = 0; index < data_size; index++) // not slower than solution without index
    {
        write(data_array[index], power);
    }
}

//
// Read a byte
//
uint8_t OneWire::read()
{
    uint8_t value = 0;

    for (uint8_t bitMask = 0x01; bitMask != 0; bitMask <<= 1)
    {
        if (read_bit()) value |= bitMask;
    }
    return value;
}

void OneWire::read_bytes(uint8_t data_array[], const uint16_t data_size)
{
    for (uint16_t index = 0; index < data_size; index++) // not slower than solution without index
    {
        data_array[index] = read();
    }
}

//
// Do a ROM select
//
void OneWire::select(const uint8_t rom_array[8])
{
    write(0x55);           // Choose ROM

    uint8_t rom_size = 8; // clean solution with for(index) seems to be 6 byte larger
    while (rom_size-- > 0)
    {
        write(*rom_array++);
    }
}

//
// Do a ROM skip
//
void OneWire::skip()
{
    write(0xCC);           // Skip ROM
}

void OneWire::depower()
{
    DIRECT_MODE_INPUT(pin_baseReg, pin_bitMask);
}

//
// You need to use this function to start a search again from the beginning.
// You do not need to do it for the first search, though you could.
//
void OneWire::reset_search()
{
    // reset the search state
    search_last_discrepancy = 0;
    search_last_device_flag = false;
    search_last_family_discrepancy = 0;

    memset(search_rom_array, 0, 8);
}

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
//
void OneWire::target_search(const uint8_t family_code)
{
    // set the search state to find SearchFamily type devices
    search_rom_array[0] = family_code;
    memset(&search_rom_array[1], 0, 7);
    search_last_discrepancy = 64;
    search_last_family_discrepancy = 0;
    search_last_device_flag = false;
}

//
// Perform a search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use OneWire::reset_search() to
// start over.
//
// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in search_rom_array buffer
//        FALSE : device not found, end of search
//
bool OneWire::search(uint8_t new_rom_array[], const bool search_mode)
{
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    bool search_result = false;

    uint8_t rom_byte_mask = 1;


    // if the last call was not the last one
    if (!search_last_device_flag)
    {
        // 1-Wire reset
        if (!reset())
        {
            // reset the search
            search_last_discrepancy = 0;
            search_last_device_flag = false;
            search_last_family_discrepancy = 0;
            return false;
        }

        // issue the search command
        if (search_mode)
        {
            write(0xF0);   // NORMAL SEARCH
        }
        else
        {
            write(0xEC);   // CONDITIONAL SEARCH
        }

        // loop to do the search
        do
        {
            // read a bit and its complement
            const bool id_bit = read_bit();
            const bool cmp_id_bit = read_bit();

            // check for no devices on 1-wire
            if (id_bit && cmp_id_bit)
            {
                break;
            }
            else
            {
                bool search_direction;
                // all devices coupled have 0 or 1
                if (id_bit != cmp_id_bit)
                {
                    search_direction = id_bit;  // bit write value for search
                }
                else
                {
                    // if this discrepancy if before the Last Discrepancy
                    // on a previous next then pick the same as last time
                    if (id_bit_number < search_last_discrepancy)
                    {
                        search_direction = ((search_rom_array[rom_byte_number] & rom_byte_mask) > 0);
                    }
                    else
                    {
                        // if equal to last pick 1, if not then pick 0
                        search_direction = (id_bit_number == search_last_discrepancy);
                    }
                    // if 0 was picked then record its position in LastZero
                    if (!search_direction)
                    {
                        last_zero = id_bit_number;

                        // check for Last discrepancy in family
                        if (last_zero < 9) search_last_family_discrepancy = last_zero;
                    }
                }

                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction)
                {
                    search_rom_array[rom_byte_number] |= rom_byte_mask;
                }
                else
                {
                    search_rom_array[rom_byte_number] &= ~rom_byte_mask;
                }
                // serial number search direction write bit
                write_bit(search_direction);

                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;

                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0)
                {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

        // if the search was successful then
        if (id_bit_number >= 65)
        {
            // search successful so set search_last_discrepancy,search_last_device_flag,search_result
            search_last_discrepancy = last_zero;

            // check for last device
            if (search_last_discrepancy == 0)
                search_last_device_flag = true;

            search_result = true;
        }
    }

    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || (search_rom_array[0] == 0))
    {
        search_last_discrepancy = 0;
        search_last_device_flag = false;
        search_last_family_discrepancy = 0;
        search_result = false;
    }
    else
    {
        memcpy(new_rom_array, search_rom_array, 8);
    }
    return search_result;
}



// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
//

#if ONEWIRE_CRC8_TABLE
// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
static const uint8_t PROGMEM crc_table[] = {
        0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
        157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
        35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
        190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
        70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
        219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
        101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
        248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
        140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
        17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
        175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
        50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
        202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
        87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
        233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
        116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53};

//
// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (note: this might better be done without to
// table, it would probably be smaller and certainly fast enough
// compared to all those delayMicrosecond() calls.  But I got
// confused, so I use this table from the examples.)
//
uint8_t OneWire::crc8(const uint8_t data_array[], uint8_t data_size, const uint8_t crc_init)
{
    uint8_t crc = crc_init;

    for (uint8_t index = 0; index < data_size; ++index)
    {
        crc = pgm_read_byte(crc_table + (crc ^ data_array[index]));
    }
    return crc;
}

#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but much smaller, than the lookup table.
//
uint8_t OneWire::crc8(const uint8_t data_array[], uint8_t data_size, const uint8_t crc_init)
{
    uint8_t crc = crc_init;

    while (data_size-- > 0) // clean solution with for(index) seems to be 6 byte larger
    {
#if defined(__AVR__)
        crc = _crc_ibutton_update(crc, *data_array++);
#else
        uint8_t inByte = *data_array++;
        for (uint8_t bitPosition = 0; bitPosition < 8; bitPosition++)
        {
            const uint8_t mix = (crc ^ inByte) & static_cast<uint8_t>(0x01);
            crc >>= 1;
            if (mix != 0) crc ^= 0x8C;
            inByte >>= 1;
        }
#endif
    }
    return crc;
}
#endif

bool OneWire::check_crc16(const uint8_t data_array[], const uint16_t data_size, const uint8_t *inverted_crc, uint16_t crc)
{
    crc = ~crc16(data_array, data_size, crc);
    return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
}

uint16_t OneWire::crc16(const uint8_t data_array[], uint16_t data_size, const uint16_t crc_init)
{
    uint16_t crc = crc_init; // init value

#if defined(__AVR__)
    while (data_size-- > 0)
    {
        crc = _crc16_update(crc, *data_array++);
    }
#else
    static const uint8_t oddParity[16] =
            {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};

    while (data_size-- > 0) // clean solution with for(index) seems to be 6 byte larger
    {
        // Even though we're just copying a byte from the data_array,
        // we'll be doing 16-bit computation with it.
        uint16_t cdata = *data_array++;
        cdata = (cdata ^ crc) & static_cast<uint16_t>(0xff);
        crc >>= 8;

        if ((oddParity[cdata & 0x0F] ^ oddParity[cdata >> 4]) != 0)
        {
            crc ^= 0xC001;
        }

        cdata <<= 6;
        crc ^= cdata;
        cdata <<= 1;
        crc ^= cdata;
    }
#endif
    return crc;
}
