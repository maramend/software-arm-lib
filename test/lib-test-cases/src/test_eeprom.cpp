/*
 *  eeprom.cpp - Tests for the EEPROM handling
 *
 *  Copyright (c) 2014 Martin Glueck <martin@mangari.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include "catch.hpp"

#include <string.h>
#include <stdio.h>

#include <sblib/eib/user_memory.h>
#include <sblib/eib/bcu.h>
#include <sblib/internal/iap.h>
#include <sblib/platform.h>
#include <sblib/eib/sblib_default_objects.h>
#include <iap_emu.h>
#include <math.h>

static const unsigned char pattern[] = {0xCA, 0xFF, 0xEE, 0xAF, 0xFE, 0xDE, 0xAD};
extern unsigned char FLASH[];
#define EEPROM_PAGE_SIZE FLASH_PAGE_SIZE

static int getCallsToIapProgram(unsigned int eepromSize)
{
    if (eepromSize < 1024)
    {
        return 1;
    }
    else
    {
        return ceil(eepromSize / 1024);
    }
}

static void checkFlash(byte* address)
{
    for (int i = 0; i < EEPROM_PAGE_SIZE; ++i)
    {
        char c1[4];
        char c2[4];

        sprintf(c1, "%02X", userEeprom[USER_EEPROM_START + i]);
        sprintf(c2, "%02X", address[i]);
        INFO("Byte mismatch at byte position " << i << ": 0x" << c1 << " != 0x" << c2);
        REQUIRE(userEeprom[USER_EEPROM_START + i] == address[i]);
    }
}

TEST_CASE("Test of the basic RAM functions","[RAM][SBLIB]")
{
    SECTION("Test RAM configuration")
    {
        REQUIRE(sizeof(userRamData) == USER_RAM_SIZE + USER_RAM_SHADOW_SIZE); // user ram matches the defined size
        CHECK(sizeof(userRamData) >= sizeof(UserRam)); // userRam fields allocated in userRamData
    }
}

TEST_CASE("Test of the basic EEPROM functions","[EEPROM][SBLIB]")
{
    SECTION("Test EEPROM configuration")
    {
        REQUIRE(iapFlashSize() == FLASH_SIZE); // testing correct flash size reported
        REQUIRE(sizeof(userEepromData) == USER_EEPROM_SIZE); // userEeprom matches the defined size
        CHECK(sizeof(userEepromData) >= sizeof(UserEeprom)); // userEeprom fields allocated in userEepromData
    }

    int iap_save [5] ;
    SECTION("Test bcu.begin()")  // checks that the eeprom stays "untouched" on bcu.begin()
    {
        IAP_Init_Flash(0xFF);
        memcpy(iap_save, iap_calls, sizeof (iap_calls));
        bcu.begin(0, 0, 0);
        REQUIRE(iap_calls [I_PREPARE]     == (iap_save [I_PREPARE]     + 0));
        REQUIRE(iap_calls [I_ERASE]       == (iap_save [I_ERASE]       + 0));
        REQUIRE(iap_calls [I_BLANK_CHECK] == (iap_save [I_BLANK_CHECK] + 0));
        REQUIRE(iap_calls [I_RAM2FLASH]   == (iap_save [I_RAM2FLASH]   + 0));
        REQUIRE(iap_calls [I_COMPARE]     == (iap_save [I_COMPARE]     + 0));
    }

    SECTION("Test bcu.end()")
    {
        memcpy(iap_save, iap_calls, sizeof (iap_calls));
        userEeprom.modified();
        int callsToIapProgram = getCallsToIapProgram(USER_EEPROM_SIZE);

        bcu.end();
        REQUIRE(iap_calls [I_PREPARE]     == (iap_save [I_PREPARE]     + callsToIapProgram));
        REQUIRE(iap_calls [I_ERASE]       == (iap_save [I_ERASE]       + 0));
        REQUIRE(iap_calls [I_BLANK_CHECK] == (iap_save [I_BLANK_CHECK] + 0));
        REQUIRE(iap_calls [I_RAM2FLASH]   == (iap_save [I_RAM2FLASH]   + callsToIapProgram));
        REQUIRE(iap_calls [I_COMPARE]     == (iap_save [I_COMPARE]     + callsToIapProgram));
    }
}

TEST_CASE("Enhanced EEPROM tests","[EEPROM][SBLIB][ERASE]")
{
    int iap_save [5] ;
    unsigned int i;
    unsigned int ps = sizeof(pattern);
    int callsToIapProgram = getCallsToIapProgram(USER_EEPROM_SIZE);
    int userEepromStartInFlash = FLASH_SIZE - (trunc(USER_EEPROM_SIZE/FLASH_SECTOR_SIZE) + 1) * FLASH_SECTOR_SIZE; // userEepromData is flash sector aligned at the end of the real flash
    int additionalCalls = 1;

#ifdef BCU1

#endif

#if (!defined(BIM112))
    // BCU 1 & BCU 2 is shifted to flash end by USER_EEPROM_START
    userEepromStartInFlash += USER_EEPROM_START;
    additionalCalls = 0; // no additional calls to iapProgram
#endif

    SECTION("Start with empty FLASH")
    {
        IAP_Init_Flash(0xFF);
        iapFlashSize();
        bcu.begin(0, 0, 0);
        memcpy(iap_save, iap_calls, sizeof (iap_calls));
        for(i=0;i < ps;i++) userEeprom[USER_EEPROM_START + i] = pattern[i];
        userEeprom.modified();
        bcu.end();
        REQUIRE(iap_calls [I_PREPARE]     == (iap_save [I_PREPARE]     + callsToIapProgram));
        REQUIRE(iap_calls [I_ERASE]       == (iap_save [I_ERASE]       + 0));
        REQUIRE(iap_calls [I_BLANK_CHECK] == (iap_save [I_BLANK_CHECK] + 0));
        REQUIRE(iap_calls [I_RAM2FLASH]   == (iap_save [I_RAM2FLASH]   + callsToIapProgram));
        REQUIRE(iap_calls [I_COMPARE]     == (iap_save [I_COMPARE]     + callsToIapProgram));
        checkFlash(FLASH + FLASH_SIZE - SECTOR_SIZE);
    }

    SECTION("Test when first page is valid")
    {
        bcu.begin(0, 0, 0);
        memcpy(iap_save, iap_calls, sizeof (iap_calls));
        for(i=0;i < ps;i++) userEeprom[USER_EEPROM_START + i] = pattern[ps - i];
        userEeprom.modified();
        bcu.end();
        CHECK(iap_calls [I_PREPARE]     == (iap_save [I_PREPARE]     + callsToIapProgram + additionalCalls));
        CHECK(iap_calls [I_ERASE]       == (iap_save [I_ERASE]       + additionalCalls));
        CHECK(iap_calls [I_BLANK_CHECK] == (iap_save [I_BLANK_CHECK] + additionalCalls));
        CHECK(iap_calls [I_RAM2FLASH]   == (iap_save [I_RAM2FLASH]   + callsToIapProgram));
        CHECK(iap_calls [I_COMPARE]     == (iap_save [I_COMPARE]     + callsToIapProgram));
        checkFlash(FLASH + userEepromStartInFlash);
    }

    SECTION("Test an overrun of the FLASH area")
    {
        int i;
        for(i = 0; i < 14; i++)
        {
            memcpy(iap_save, iap_calls, sizeof (iap_calls));
            bcu.begin(0, 0, 0);
            userEeprom.modified();
            bcu.end();
            CHECK(iap_calls [I_PREPARE]     == (iap_save [I_PREPARE]     + callsToIapProgram + additionalCalls));
            CHECK(iap_calls [I_ERASE]       == (iap_save [I_ERASE]       + additionalCalls));
            CHECK(iap_calls [I_BLANK_CHECK] == (iap_save [I_BLANK_CHECK] + additionalCalls));
            CHECK(iap_calls [I_RAM2FLASH]   == (iap_save [I_RAM2FLASH]   + callsToIapProgram));
            CHECK(iap_calls [I_COMPARE]     == (iap_save [I_COMPARE]     + callsToIapProgram));
        }
        memcpy(iap_save, iap_calls, sizeof (iap_calls));
        bcu.begin(0, 0, 0);
        userEeprom.modified();
        bcu.end();
        CHECK(iap_calls [I_PREPARE]     == (iap_save [I_PREPARE]     + 2 * additionalCalls + 2));
        CHECK(iap_calls [I_ERASE]       == (iap_save [I_ERASE]       + 1));
        CHECK(iap_calls [I_BLANK_CHECK] == (iap_save [I_BLANK_CHECK] + 1));
        CHECK(iap_calls [I_RAM2FLASH]   == (iap_save [I_RAM2FLASH]   + 2 * additionalCalls + 1));
        CHECK(iap_calls [I_COMPARE]     == (iap_save [I_COMPARE]     + 2 * additionalCalls + 1));
    }
}