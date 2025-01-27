/**************************************************************************//**
 * @addtogroup SBLIB_MAIN_GROUP Selfbus KNX-Library
 * @defgroup SBLIB_SUB_GROUP_KNX KNX Transport layer 4
 * @ingroup SBLIB_MAIN_GROUP
 * @brief   
 * @details 
 *
 *
 * @{
 *
 * @file   knx_tlayer4.h
 * @author Darthyson <darth@maptrack.de> Copyright (c) 2022
 * @bug No known bugs.
 ******************************************************************************/

/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 3 as
 published by the Free Software Foundation.
 ---------------------------------------------------------------------------*/

#ifndef TLAYER4_H_
#define TLAYER4_H_

#include <stdint.h>
#include <sblib/eib/knx_tpdu.h>
#include <sblib/eib/apci.h>


#define TL4_CONNECTION_TIMEOUT_MS (6000) //!< Transport layer 4 connection timeout in milliseconds
#define TL4_T_ACK_TIMEOUT_MS      (3000) //!< Transport layer 4 T_ACK/T_NACK timeout in milliseconds
#define TL4_MAX_REPETITION_COUNT  (3)    //!< Maximum number of repetitions

#ifdef DEBUG
#   define LONG_PAUSE_THRESHOLD_MS (500)
#endif

extern uint16_t telegramCount;   //!< number of telegrams since system reset
extern uint16_t disconnectCount; //!< number of disconnects since system reset
extern uint16_t repeatedT_ACKcount; //!< number of repeated @ref T_ACK_PDU in @ref actionA03sendAckPduAgain

/**
 * Implementation of the KNX transportation layer 4 Style 3
 */
class TLayer4
{
public:
    /** The states of the transport layer 4 state machine Style 3 */
    enum TL4State ///\todo make it private after refactoring of TL4 dumping
    {
        CLOSED,
        OPEN_IDLE,
        OPEN_WAIT
    };
    TLayer4(uint8_t maxTelegramLength);
    TLayer4() = delete;
    virtual ~TLayer4() = default;

    /**
     * Pre-processes the received telegram from bus.telegram.
     *
     * @param telegram  The telegram to process
     * @param telLength Length of the telegram
     */
    void processTelegram(unsigned char *telegram, uint8_t telLength);

    /**
     * Test if a connection-oriented connection is open.
     *
     * @return True if a connection is open, otherwise false.
     */
    bool directConnection();

    /**
     * The transport layer 4 processing loop. This is like the application's loop() function,
     * and is called automatically by the libraries main() when the BCU is activated with bcu.begin().
     */
    virtual void loop();

    /**
     * Get our own physical address.
     *
     * @return Physical KNX address.
     */
    uint16_t ownAddress();

    /**
     * Set our own physical address. Normally the physical address is set by ETS when
     * programming the device.
     *
     * @param addr The new physical address
     */
    virtual void setOwnAddress(uint16_t addr);

    /**
     * Returns the physical knx-address of the connected partner.
     *
     * @return Remote's physical KNX address, 0 in case of no connection
     */
    uint16_t connectedTo();

    /**
     * Wait for @ref sendTelegram to be free and acquire it.
     */
    uint8_t * acquireSendBuffer();

    /**
     * Sends the telegram that was prepared in @ref sendTelegram.
     */
    void sendPreparedTelegram();

    /**
     * Notification that the last telegram transmission has ended.
     *
     * @param successful Whether the telegram was transmitted successfully (received an LL_ACK)
     *                   or not (not even after repeating it a few times).
     */
    void finishedSendingTelegram(bool successful);
protected:
    /**
     * Special initialization for the transport layer.
     */
    virtual void _begin();

    /**
     * Process a group address (T_Data_Group) telegram.
     */
    virtual bool processGroupAddressTelegram(ApciCommand apciCmd, uint16_t groupAddress, unsigned char *telegram, uint8_t telLength) = 0;

    /**
     * Process a broadcast telegram.
     */
    virtual bool processBroadCastTelegram(ApciCommand apciCmd, unsigned char *telegram, uint8_t telLength) = 0;

    /**
     * Reset TL4 connection by setting
     * @ref sendConnectedTelegramBufferState and @ref sendConnectedTelegramBuffer2State to
     * @ref CONNECTED_TELEGRAM_FREE
     */
    virtual void resetConnection();

    /**
     * Send a connection control telegram @ref TPDU.
     *
     * @param cmd           The transport command @ref TPDU
     * @param address       The KNX address to send the telegram to
     * @param senderSeqNo   The sequence number of the sender, 0 if not required
     */
    void sendConControlTelegram(TPDU cmd, uint16_t address, int8_t senderSeqNo);

    /**
     * Processes a APCI telegram
     *
     * @param apciCmd       @ref ApciCommand of the telegram
     * @param telegram      The APCI-telegram
     * @param telLength     Telegram length
     * @param sendBuffer    Pointer to the buffer for a potential response telegram
     * @return Always false
     */
    virtual bool processApci(ApciCommand apciCmd, unsigned char * telegram, uint8_t telLength, uint8_t * sendBuffer);

    bool enabled = false; //!< The BCU is enabled. Set by bcu.begin().


    virtual void discardReceivedTelegram() = 0;
    virtual void send(unsigned char* telegram, unsigned short length) = 0;

private:
    /**
     * Internal processing of the received telegram from bus.telegram. Called by processTelegram
     */
    void processTelegramInternal(unsigned char *telegram, uint8_t telLength);

    /** Sets a new state @ref TL4State for the transport layer state machine
     *
     * @param  newState new @ref TL4State to set
     * @return always true
     */
    bool setTL4State(TLayer4::TL4State newState);

    /**
     * Process a unicast connection control telegram with our physical address as
     * destination address. The telegram is stored in sbRecvTelegram[].
     *
     * When this function is called, the sender address is != 0 (not a broadcast).
     *
     * @param tpci - the transport control field
     */
    void processConControlTelegram(const uint16_t& senderAddr, const TPDU& tpci, unsigned char *telegram, const uint8_t& telLength);

    /**
     * @brief Process a TP Layer 4 @ref T_CONNECT_PDU
     * @details T_Connect.req see KNX Spec. 2.1 3/3/4 page 13
     *
     * @param senderAddr physical KNX address of the sender
     * @return true if successful, otherwise false
     */
    bool processConControlConnectPDU(uint16_t senderAddr);

    /**
     * @brief Process a TP Layer 4 @ref T_DISCONNECT_PDU
     * @details T_Disconnect.req see KNX Spec. 2.1 3/3/4 page 14
     *
     * @param senderAddr physical KNX address of the sender
     * @return true if successful, otherwise false
     */
    bool processConControlDisconnectPDU(uint16_t senderAddr);

    /**
     * @brief Process a TP Layer 4 @ref T_ACK_PDU and @ref T_NACK_PDU
     * @details T_ACK-PDU & T_NAK-PDU see KNX Spec. 2.1 3/3/4 page 6
     *
     * @param senderAddr physical KNX address of the sender
     * @param tpci the TPCI to process
     * @return true if successful, otherwise false
     */
    bool processConControlAcknowledgmentPDU(uint16_t senderAddr, const TPDU& tpci, unsigned char *telegram, uint8_t telLength);

    /**
     * Process a unicast telegram with our physical address as destination address.
     * The telegram is stored in sbRecvTelegram[].
     *
     * When this function is called, the sender address is != 0 (not a broadcast).
     *
     * @param apciCmd - The application control field command (APCI)
     */
    void processDirectTelegram(ApciCommand apciCmd, unsigned char *telegram, uint8_t telLength);

    /**
     * Forward the connection-oriented telegram in @ref sendConnectedTelegram to @ref sendTelegram
     * and send it.
     */
    void sendPreparedConnectedTelegram();

    void actionA00Nothing();
    void actionA01Connect(uint16_t address);
    void actionA02sendAckPduAndProcessApci(ApciCommand apciCmd, const int8_t seqNo, unsigned char *telegram, uint8_t telLength);

    /**
     * Performs action A3 as described in the KNX Spec. 2.1 3/3/4 5.3 p.19
     * @param seqNo Sequence number the T_ACK should be send with
     */
    void actionA03sendAckPduAgain(const int8_t seqNo);

    /**
     * @brief Performs action A4 as described in the KNX Spec.
     *        Send a @ref T_NACK_PDU to @ref connectedAddr with system priority
     *
     * @param seqNo Received (incorrect) sequence number
     */
    void actionA04SendNAck(const uint8_t seqNo);

    /**
     * @brief Performs action A5 as described in the KNX Spec.
     */
    void actionA05DisconnectUser();

    /**
     * @brief Performs action A6 as described in the KNX Spec.
     *        Send a @ref T_DISCONNECT_PDU to @ref connectedAddr with system priority and Sequence# = 0
     */
    void actionA06DisconnectAndClose();

    /**
     * @brief Sends the direct telegram which is provided in buffer @ref sendConnectedTelegram
     */
    void actionA07SendDirectTelegram();
    void actionA08IncrementSequenceNumber();

    /**
     * @brief Repeats the last direct telegram in buffer @ref sendConnectedTelegram
     */
    void actionA09RepeatMessage();

    /**
     * @brief Performs action A10 as described in the KNX Spec.
     *        Send a @ref T_DISCONNECT_PDU to @ref address with system priority and Sequence# = 0
     *
     * @param address KNX address to send the @ref T_DISCONNECT_PDU
     */
    void actionA10Disconnect(uint16_t address);

    TLayer4::TL4State state = TLayer4::CLOSED;  //!< Current state of the TL4 state machine
    uint16_t connectedAddr = 0;                 //!< Remote address of the connected partner
    int8_t seqNoSend = -1;                      //!< Sequence number for the next telegram we send
    int8_t seqNoRcv = -1;                       //!< Sequence number of the last telegram received from connected partner
    int8_t repCount = 0;                        //!< Telegram repetition count
    uint32_t connectedTime = 0;                 //!< System time of the last connection oriented telegram
    uint32_t sentTelegramTime = 0;              //!< System time of the last sent telegram

    volatile uint16_t ownAddr;                  //!< Our own physical address on the bus

    /**
     * A buffer for the telegram to send.
     */
    byte *sendTelegram;

    /**
     * Two buffers for connection-oriented telegrams to send. Separate from @ref sendTelegram as repeated sending
     * can be necessary after seconds, while other telegrams can be received and transmitted.
     */
    byte *sendConnectedTelegram;
    byte *sendConnectedTelegram2;

    enum SendTelegramBufferState
    {
        TELEGRAM_FREE,
        TELEGRAM_ACQUIRED,
        TELEGRAM_SENDING
    };
    enum SendConnectedTelegramBufferState
    {
        CONNECTED_TELEGRAM_FREE,
        CONNECTED_TELEGRAM_WAIT_T_ACK_SENT,
        CONNECTED_TELEGRAM_WAIT_LOOP,
        CONNECTED_TELEGRAM_SENDING
    };

    volatile SendTelegramBufferState sendTelegramBufferState;
    volatile SendConnectedTelegramBufferState sendConnectedTelegramBufferState;
    volatile SendConnectedTelegramBufferState sendConnectedTelegramBuffer2State;
};

inline bool TLayer4::directConnection()
{
    return (state != TLayer4::CLOSED);
}

inline uint16_t TLayer4::ownAddress()
{
    ///\todo bus.ownAddress should also only return uint16_t
    return (ownAddr);
}

inline void TLayer4::setOwnAddress(uint16_t addr)
{
    ownAddr = addr;
}

inline uint16_t TLayer4::connectedTo()
{
    if (directConnection())
    {
        return (connectedAddr);
    }
    else
    {
        return (0);
    }
}

#endif /* TLAYER4_H_ */
/** @}*/
