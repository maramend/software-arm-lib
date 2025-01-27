/*
 *  bcu_base.cpp
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/io_pin_names.h>
#include <sblib/eib/knx_lpdu.h>
#include <sblib/eib/bcu_base.h>
#include <sblib/eib/addr_tables.h>
#include <sblib/internal/variables.h>
#include <sblib/internal/iap.h>
#include <sblib/eib/userEeprom.h>
#include <sblib/eib/userRam.h>
#include <sblib/eib/apci.h>
#include <sblib/utils.h>
#include <string.h>
#include <sblib/eib/bus.h>

static Bus* timerBusObj;
// The interrupt handler for the EIB bus access object
BUS_TIMER_INTERRUPT_HANDLER(TIMER16_1_IRQHandler, (*timerBusObj))

#if defined(INCLUDE_SERIAL)
#   include <sblib/serial.h>
#endif

extern volatile unsigned int systemTime;

BcuBase::BcuBase(UserRam* userRam, AddrTables* addrTables) :
        TLayer4(maxTelegramSize()),
        bus(new Bus(this, timer16_1, PIN_EIB_RX, PIN_EIB_TX, CAP0, MAT0)),
        progPin(PIN_PROG),
        progPinInv(true),
        userRam(userRam),
        addrTables(addrTables),
        comObjects(nullptr),
        progButtonDebouncer(),
        requestedRestartType(NO_RESTART)
{
    timerBusObj = bus;
    setFatalErrorPin(progPin);
    setKNX_TX_Pin(bus->txPin);
}

void BcuBase::_begin()
{
    TLayer4::_begin();
    bus->begin();
    progButtonDebouncer.init(1);
}

void BcuBase::loop()
{
    bus->loop();
    TLayer4::loop();
    bool telegramInQueu = bus->telegramReceived();
    telegramInQueu &= (!bus->sendingTelegram());

    bool busOK = (bus->state == Bus::IDLE) || (bus->state == Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE);
    if (telegramInQueu && busOK && (userRam->status() & BCU_STATUS_TRANSPORT_LAYER))
	{
        processTelegram(bus->telegram, (uint8_t)bus->telegramLen); // if processed successfully, received telegram will be discarded by processTelegram()
	}

	if (progPin)
	{
		// Detect the falling edge of pressing the prog button
		pinMode(progPin, INPUT|PULL_UP);
		int oldValue = progButtonDebouncer.value();
		if (!progButtonDebouncer.debounce(digitalRead(progPin), 50) && oldValue)
		{
			userRam->status() ^= 0x81;  // toggle programming mode and checksum bit
		}
		pinMode(progPin, OUTPUT);
		digitalWrite(progPin, (userRam->status() & BCU_STATUS_PROGRAMMING_MODE) ^ progPinInv);
	}

    // Rest of this function is only relevant if currently able to send another telegram.
    if (bus->sendingTelegram())
    {
        return;
    }

    if (requestedRestartType != NO_RESTART)
    {
        if (connectedTo() != 0)
        {
            // send disconnect
            sendConControlTelegram(T_DISCONNECT_PDU, connectedTo(), 0);
        }
        softSystemReset();
    }
}

bool BcuBase::setProgrammingMode(bool newMode)
{
    if (!progPin)
    {
        return false;
    }

    if (newMode)
    {
        userRam->status() |= 0x81;  // set programming mode and checksum bit
    }
    else
    {
        userRam->status() &= 0x81;  // clear programming mode and checksum bit
    }
    pinMode(progPin, OUTPUT);
    digitalWrite(progPin, (userRam->status() & BCU_STATUS_PROGRAMMING_MODE) ^ progPinInv);
    return true;
}

bool BcuBase::processApci(ApciCommand apciCmd, unsigned char * telegram, uint8_t telLength, uint8_t * sendBuffer)
{
    switch (apciCmd)
    {
        case APCI_BASIC_RESTART_PDU:
            requestedRestartType = RESTART_BASIC;
            return (false);
        default:
            return (TLayer4::processApci(apciCmd, telegram, telLength, sendBuffer));
    }
    return (false);
}

void BcuBase::sendApciIndividualAddressReadResponse()
{
    auto sendBuffer = acquireSendBuffer();
    initLpdu(sendBuffer, PRIORITY_SYSTEM, false, FRAME_STANDARD);
    // 1+2 contain the sender address, which is set by bus.sendTelegram()
    setDestinationAddress(sendBuffer, 0x0000); // Zero target address, it's a broadcast
    sendBuffer[5] = 0xe0 + 1; // address type & routing count in high nibble + response length in low nibble
    setApciCommand(sendBuffer, APCI_INDIVIDUAL_ADDRESS_RESPONSE_PDU, 0);
    sendPreparedTelegram();
}

void BcuBase::end()
{
    enabled = false;
    bus->end();
}

bool BcuBase::programmingMode() const
{
    return (userRam->status() & BCU_STATUS_PROGRAMMING_MODE) == BCU_STATUS_PROGRAMMING_MODE;
}

int BcuBase::maxTelegramSize()
{
    return 23;
}

void BcuBase::discardReceivedTelegram()
{
    bus->discardReceivedTelegram();
}

void BcuBase::send(unsigned char* telegram, unsigned short length)
{
    bus->sendTelegram(telegram, length);
}

void BcuBase::softSystemReset()
{
    NVIC_SystemReset();
}

void BcuBase::setProgPin(int prgPin) {
    progPin=prgPin;
    setFatalErrorPin(progPin);
}

void BcuBase::setProgPinInverted(int prgPinInv) {
    progPinInv=prgPinInv;
}

