package org.selfbus.updater;

/**
 * Basic information about the bootloader's MCU
 */
public final class Mcu {
    /** Maximum payload one APCI_USERMSG_MANUFACTURER_0/APCI_USERMSG_MANUFACTURER_6 can handle */
    public static final int MAX_PAYLOAD = 12;
    ///\todo get rid of this, the MCU should handle this by it self or report us the size
    /** Maximum length a asdu can be in a standard frame */
    public static final int MAX_ASDU_LENGTH = 14;
    /** Selfbus ARM controller flash page size */
    public static final int FLASH_PAGE_SIZE = 256;
    /** Vector table end of the mcu */
    public static final int VECTOR_TABLE_END = 0xC0;
    /** Length of bootloader identity string */
    public static final int BL_ID_STRING_LENGTH = 12;
    /** App Pointer follows this MAGIC word */
    public static final byte[] APP_VER_PTR_MAGIC = {'!','A','V','P','!','@',':'};

    /** Default restart time of the MCU in seconds */
    public static final int DEFAULT_RESTART_TIME_SECONDS = 6;
}
