#include <Arduino.h>
#include "USB.h"
#include "USBHID.h"
#include <Wire.h>

// ============================================================
// CP2112 EMULATOR
// ESP32-S3
// Arduino ESP32 Core 2.0.17
// ============================================================

// ------------------------------------------------------------
// USB identification
// ------------------------------------------------------------

#define CP2112_VID 0x10C4
#define CP2112_PID 0xEA90

// ------------------------------------------------------------
// I2C / SMBus
// ------------------------------------------------------------

#define I2C_SDA 8
#define I2C_SCL 9

#define DEFAULT_I2C_SPEED 100000

// DJI BMS normally uses 0x0B
// This is the SMBus slave address.
// ------------------------------------------------------------

#define DEBUG_SERIAL 1

// ============================================================
// CP2112 REPORT IDS
// ============================================================

#define REPORT_RESET                 0x01
#define REPORT_GPIO_CONFIG           0x02
#define REPORT_GPIO_GET              0x03
#define REPORT_GPIO_SET              0x04
#define REPORT_GET_VERSION           0x05
#define REPORT_SMBUS_CONFIG          0x06

#define REPORT_DATA_READ_REQUEST     0x10
#define REPORT_DATA_WRITE_READ       0x11
#define REPORT_DATA_READ_FORCE       0x12
#define REPORT_DATA_READ_RESPONSE    0x13
#define REPORT_DATA_WRITE             0x14
#define REPORT_TRANSFER_STATUS_REQ   0x15
#define REPORT_TRANSFER_STATUS_RESP  0x16
#define REPORT_CANCEL_TRANSFER       0x17

#define REPORT_LOCK_BYTE             0x20
#define REPORT_USB_CONFIG             0x21
#define REPORT_MANUFACTURER           0x22
#define REPORT_PRODUCT                0x23
#define REPORT_SERIAL                 0x24

// ============================================================
// CP2112 STATUS
// ============================================================

#define STATUS_IDLE       0x00
#define STATUS_BUSY       0x01
#define STATUS_COMPLETE   0x02
#define STATUS_ERROR      0x03

#define STATUS_SUCCESS            0x05
#define STATUS_TIMEOUT_NACK       0x00
#define STATUS_TIMEOUT_BUS        0x01
#define STATUS_ARBITRATION_LOST   0x02
#define STATUS_READ_INCOMPLETE    0x03
#define STATUS_WRITE_INCOMPLETE   0x04

// ============================================================
// HID
// ============================================================

USBHID HID;

// ============================================================
// CP2112 state
// ============================================================

uint32_t smbusClock = DEFAULT_I2C_SPEED;

uint8_t smbusDeviceAddress = 0x02;
uint8_t autoSendRead = 0x00;

uint16_t writeTimeout = 0;
uint16_t readTimeout = 0;

uint8_t sclLowTimeout = 0;
uint16_t retryTime = 0;

uint8_t gpioLatch = 0x00;

// ------------------------------------------------------------
// Last transfer state
// ------------------------------------------------------------

volatile uint8_t transferStatus0 = STATUS_IDLE;
volatile uint8_t transferStatus1 = STATUS_SUCCESS;

volatile uint16_t transferRetries = 0;
volatile uint16_t transferLength = 0;

// ------------------------------------------------------------
// Read buffer
// ------------------------------------------------------------

uint8_t readBuffer[512];

uint16_t readBufferLength = 0;

bool readBufferValid = false;

// ============================================================
// DEBUG
// ============================================================

void dumpHex(
    const char *prefix,
    const uint8_t *data,
    uint16_t len)
{
#if DEBUG_SERIAL

    Serial.print(prefix);

    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] < 0x10)
            Serial.print("0");

        Serial.print(data[i], HEX);
        Serial.print(" ");
    }

    Serial.println();

#endif
}

// ============================================================
// HID REPORT DESCRIPTOR
// ============================================================

static const uint8_t cp2112_report_descriptor[] =
{
    // --------------------------------------------------------
    // Vendor defined usage page
    // --------------------------------------------------------

    0x06, 0x00, 0xFF,
    0x09, 0x01,
    0xA1, 0x01,

    // ========================================================
    // FEATURE 0x01
    // ========================================================

    0x85, REPORT_RESET,
    0x09, 0x01,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x01,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x02
    // ========================================================

    0x85, REPORT_GPIO_CONFIG,
    0x09, 0x02,
    0x75, 0x08,
    0x95, 0x04,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x03
    // ========================================================

    0x85, REPORT_GPIO_GET,
    0x09, 0x03,
    0x75, 0x08,
    0x95, 0x02,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x04
    // ========================================================

    0x85, REPORT_GPIO_SET,
    0x09, 0x04,
    0x75, 0x08,
    0x95, 0x02,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x05
    // ========================================================

    0x85, REPORT_GET_VERSION,
    0x09, 0x05,
    0x75, 0x08,
    0x95, 0x02,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x06
    // ========================================================

    0x85, REPORT_SMBUS_CONFIG,
    0x09, 0x06,
    0x75, 0x08,
    0x95, 0x0D,
    0xB1, 0x02,

    // ========================================================
    // OUT 0x10
    // ========================================================

    0x85, REPORT_DATA_READ_REQUEST,
    0x09, 0x10,
    0x75, 0x08,
    0x95, 0x03,
    0x91, 0x02,

    // ========================================================
    // OUT 0x11
    // ========================================================

    0x85, REPORT_DATA_WRITE_READ,
    0x09, 0x11,
    0x75, 0x08,
    0x95, 0x14,
    0x91, 0x02,

    // ========================================================
    // OUT 0x12
    // ========================================================

    0x85, REPORT_DATA_READ_FORCE,
    0x09, 0x12,
    0x75, 0x08,
    0x95, 0x02,
    0x91, 0x02,

    // ========================================================
    // IN 0x13
    // ========================================================

    0x85, REPORT_DATA_READ_RESPONSE,
    0x09, 0x13,
    0x75, 0x08,
    0x95, 0x3F,
    0x81, 0x02,

    // ========================================================
    // OUT 0x14
    // ========================================================

    0x85, REPORT_DATA_WRITE,
    0x09, 0x14,
    0x75, 0x08,
    0x95, 0x3F,
    0x91, 0x02,

    // ========================================================
    // OUT 0x15
    // ========================================================

    0x85, REPORT_TRANSFER_STATUS_REQ,
    0x09, 0x15,
    0x75, 0x08,
    0x95, 0x01,
    0x91, 0x02,

    // ========================================================
    // IN 0x16
    //
    // IMPORTANT:
    // 6 payload bytes
    // ========================================================

    0x85, REPORT_TRANSFER_STATUS_RESP,
    0x09, 0x16,
    0x75, 0x08,
    0x95, 0x06,
    0x81, 0x02,

    // ========================================================
    // OUT 0x17
    // ========================================================

    0x85, REPORT_CANCEL_TRANSFER,
    0x09, 0x17,
    0x75, 0x08,
    0x95, 0x01,
    0x91, 0x02,

    // ========================================================
    // FEATURE 0x20
    // ========================================================

    0x85, REPORT_LOCK_BYTE,
    0x09, 0x20,
    0x75, 0x08,
    0x95, 0x01,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x21
    // ========================================================

    0x85, REPORT_USB_CONFIG,
    0x09, 0x21,
    0x75, 0x08,
    0x95, 0x08,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x22
    // ========================================================

    0x85, REPORT_MANUFACTURER,
    0x09, 0x22,
    0x75, 0x08,
    0x95, 0x3F,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x23
    // ========================================================

    0x85, REPORT_PRODUCT,
    0x09, 0x23,
    0x75, 0x08,
    0x95, 0x3F,
    0xB1, 0x02,

    // ========================================================
    // FEATURE 0x24
    // ========================================================

    0x85, REPORT_SERIAL,
    0x09, 0x24,
    0x75, 0x08,
    0x95, 0x3F,
    0xB1, 0x02,

    0xC0
};

// ============================================================
// CP2112 DEVICE
// ============================================================

class CP2112Device : public USBHIDDevice
{
public:

    CP2112Device()
    {
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;

            HID.addDevice(
                this,
                sizeof(cp2112_report_descriptor)
            );
        }
    }

    void begin()
    {
        HID.begin();
    }

    uint16_t _onGetDescriptor(uint8_t *buffer)
    {
        memcpy(
            buffer,
            cp2112_report_descriptor,
            sizeof(cp2112_report_descriptor)
        );

        return sizeof(cp2112_report_descriptor);
    }

    // ========================================================
    // SET FEATURE
    // ========================================================

    void _onSetFeature(
        uint8_t report_id,
        const uint8_t *buffer,
        uint16_t len)
    {
#if DEBUG_SERIAL
        Serial.print("SET FEATURE 0x");
        Serial.println(report_id, HEX);

        dumpHex("  DATA: ", buffer, len);
#endif

        switch (report_id)
        {
            case REPORT_RESET:

                Serial.println("CP2112 RESET");

                transferStatus0 = STATUS_IDLE;
                transferStatus1 = STATUS_SUCCESS;

                transferRetries = 0;
                transferLength = 0;

                readBufferLength = 0;
                readBufferValid = false;

                break;

            case REPORT_GPIO_CONFIG:

                if (len >= 4)
                {
                    Serial.println(
                        "GPIO configuration received"
                    );
                }

                break;

            case REPORT_GPIO_SET:

                if (len >= 2)
                {
                    gpioLatch =
                        (gpioLatch & ~buffer[1]) |
                        (buffer[0] & buffer[1]);
                }

                break;

            case REPORT_SMBUS_CONFIG:

                handleSMBusConfig(buffer, len);

                break;

            case REPORT_LOCK_BYTE:
                break;

            case REPORT_USB_CONFIG:
                break;

            case REPORT_MANUFACTURER:
                break;

            case REPORT_PRODUCT:
                break;

            case REPORT_SERIAL:
                break;

            default:
                break;
        }
    }

    // ========================================================
    // GET FEATURE
    // ========================================================

    uint16_t _onGetFeature(
        uint8_t report_id,
        uint8_t *buffer,
        uint16_t len)
    {
        memset(buffer, 0, len);

        switch (report_id)
        {
            case REPORT_GPIO_CONFIG:

                if (len >= 4)
                {
                    buffer[0] = 0x00;
                    buffer[1] = 0x00;
                    buffer[2] = 0x00;
                    buffer[3] = 0x00;
                }

                break;

            case REPORT_GPIO_GET:

                if (len >= 2)
                {
                    buffer[0] = gpioLatch;
                    buffer[1] = gpioLatch;
                }

                break;

            case REPORT_GET_VERSION:

                if (len >= 2)
                {
                    buffer[0] = 0x0C;
                    buffer[1] = 0x01;
                }

                break;

            case REPORT_SMBUS_CONFIG:

                makeSMBusConfig(buffer, len);

                break;

            case REPORT_LOCK_BYTE:

                if (len >= 1)
                    buffer[0] = 0xFF;

                break;

            case REPORT_USB_CONFIG:

                makeUSBConfig(buffer, len);

                break;

            case REPORT_MANUFACTURER:

                makeStringReport(
                    buffer,
                    len,
                    "Silicon Laboratories"
                );

                break;

            case REPORT_PRODUCT:

                makeStringReport(
                    buffer,
                    len,
                    "CP2112 HID USB-to-SMBus Bridge"
                );

                break;

            case REPORT_SERIAL:

                makeStringReport(
                    buffer,
                    len,
                    "0001"
                );

                break;

            default:
                break;
        }

#if DEBUG_SERIAL
        dumpHex(
            "GET FEATURE response: ",
            buffer,
            len
        );
#endif

        return len;
    }

    // ========================================================
    // OUTPUT REPORT
    // ========================================================

    void _onOutput(
        uint8_t report_id,
        const uint8_t *buffer,
        uint16_t len)
    {
#if DEBUG_SERIAL

        Serial.print("OUTPUT 0x");
        Serial.println(report_id, HEX);

        dumpHex(
            "  DATA: ",
            buffer,
            len
        );

#endif

        switch (report_id)
        {
            case REPORT_DATA_READ_REQUEST:

                handleReadRequest(
                    buffer,
                    len
                );

                break;

            case REPORT_DATA_WRITE_READ:

                handleWriteRead(
                    buffer,
                    len
                );

                break;

            case REPORT_DATA_READ_FORCE:

                handleForceRead(
                    buffer,
                    len
                );

                break;

            case REPORT_DATA_WRITE:

                handleWrite(
                    buffer,
                    len
                );

                break;

            case REPORT_TRANSFER_STATUS_REQ:

                sendTransferStatus();

                break;

            case REPORT_CANCEL_TRANSFER:

                Serial.println(
                    "CANCEL TRANSFER"
                );

                transferStatus0 =
                    STATUS_ERROR;

                transferStatus1 =
                    STATUS_TIMEOUT_NACK;

                break;

            default:
                break;
        }
    }

private:

    // ========================================================
    // START NEW TRANSFER
    // ========================================================

    void beginTransfer()
    {
        transferStatus0 = STATUS_BUSY;
        transferStatus1 = STATUS_SUCCESS;

        transferRetries = 0;
        transferLength = 0;
    }

    // ========================================================
    // SMBUS CONFIG
    // ========================================================

    void handleSMBusConfig(
        const uint8_t *data,
        uint16_t len)
    {
        if (len < 13)
            return;

        smbusClock =
            ((uint32_t)data[0] << 24) |
            ((uint32_t)data[1] << 16) |
            ((uint32_t)data[2] << 8) |
            data[3];

        smbusDeviceAddress = data[4];

        autoSendRead = data[5];

        writeTimeout =
            ((uint16_t)data[6] << 8) |
            data[7];

        readTimeout =
            ((uint16_t)data[8] << 8) |
            data[9];

        sclLowTimeout = data[10];

        retryTime =
            ((uint16_t)data[11] << 8) |
            data[12];

        if (smbusClock == 0)
            smbusClock = DEFAULT_I2C_SPEED;

        Wire.setClock(smbusClock);

#if DEBUG_SERIAL

        Serial.println(
            "---- SMBUS CONFIG ----"
        );

        Serial.print("Clock: ");
        Serial.println(smbusClock);

        Serial.print("Device address: 0x");
        Serial.println(
            smbusDeviceAddress,
            HEX
        );

        Serial.print("Auto read: ");
        Serial.println(autoSendRead);

        Serial.print("Write timeout: ");
        Serial.println(writeTimeout);

        Serial.print("Read timeout: ");
        Serial.println(readTimeout);

        Serial.print("SCL low timeout: ");
        Serial.println(sclLowTimeout);

        Serial.print("Retry time: ");
        Serial.println(retryTime);

        Serial.println(
            "-----------------------"
        );

#endif
    }

    // ========================================================
    // SMBUS CONFIG RESPONSE
    // ========================================================

    void makeSMBusConfig(
        uint8_t *buffer,
        uint16_t len)
    {
        if (len < 13)
            return;

        buffer[0] =
            (smbusClock >> 24) & 0xFF;

        buffer[1] =
            (smbusClock >> 16) & 0xFF;

        buffer[2] =
            (smbusClock >> 8) & 0xFF;

        buffer[3] =
            smbusClock & 0xFF;

        buffer[4] =
            smbusDeviceAddress;

        buffer[5] =
            autoSendRead;

        buffer[6] =
            (writeTimeout >> 8) & 0xFF;

        buffer[7] =
            writeTimeout & 0xFF;

        buffer[8] =
            (readTimeout >> 8) & 0xFF;

        buffer[9] =
            readTimeout & 0xFF;

        buffer[10] =
            sclLowTimeout;

        buffer[11] =
            (retryTime >> 8) & 0xFF;

        buffer[12] =
            retryTime & 0xFF;
    }

    // ========================================================
    // USB CONFIG
    // ========================================================

    void makeUSBConfig(
        uint8_t *buffer,
        uint16_t len)
    {
        if (len < 8)
            return;

        buffer[0] =
            CP2112_VID & 0xFF;

        buffer[1] =
            CP2112_VID >> 8;

        buffer[2] =
            CP2112_PID & 0xFF;

        buffer[3] =
            CP2112_PID >> 8;

        buffer[4] = 0x32;
        buffer[5] = 0x00;

        buffer[6] = 0x01;
        buffer[7] = 0x00;
    }

    // ========================================================
    // STRING REPORT
    // ========================================================

    void makeStringReport(
        uint8_t *buffer,
        uint16_t len,
        const char *text)
    {
        if (len < 2)
            return;

        uint16_t chars = strlen(text);

        if (chars > 30)
            chars = 30;

        uint16_t byteLength =
            2 + chars * 2;

        buffer[0] =
            byteLength;

        buffer[1] =
            0x03;

        for (uint16_t i = 0; i < chars; i++)
        {
            buffer[2 + i * 2] =
                text[i];

            buffer[3 + i * 2] =
                0x00;
        }
    }

    // ========================================================
    // READ REQUEST 0x10
    // ========================================================

    void handleReadRequest(
        const uint8_t *data,
        uint16_t len)
    {
        if (len < 3)
            return;

        beginTransfer();

        uint8_t slave =
            data[0];

        uint16_t count =
            ((uint16_t)data[1] << 8) |
            data[2];

        if (count > sizeof(readBuffer))
            count = sizeof(readBuffer);

        uint8_t addr7 =
            slave >> 1;

#if DEBUG_SERIAL

        Serial.print(
            "READ slave=0x"
        );

        Serial.print(
            slave,
            HEX
        );

        Serial.print(
            " addr7=0x"
        );

        Serial.print(
            addr7,
            HEX
        );

        Serial.print(
            " length="
        );

        Serial.println(
            count
        );

#endif

        uint16_t received = 0;

        unsigned long start =
            millis();

        // ----------------------------------------------------
        // requestFrom
        // ----------------------------------------------------

        Wire.requestFrom(
            (int)addr7,
            (int)count
        );

        // ----------------------------------------------------
        // Wait for data
        // ----------------------------------------------------

        while (
            received < count &&
            millis() - start < 100
        )
        {
            while (
                Wire.available() &&
                received < count
            )
            {
                readBuffer[received++] =
                    Wire.read();
            }

            if (received < count)
                delayMicroseconds(100);
        }

        readBufferLength =
            received;

        readBufferValid =
            true;

        transferLength =
            received;

        // ----------------------------------------------------
        // Status
        // ----------------------------------------------------

        if (received == count)
        {
            transferStatus0 =
                STATUS_COMPLETE;

            transferStatus1 =
                STATUS_SUCCESS;
        }
        else
        {
            transferStatus0 =
                STATUS_ERROR;

            transferStatus1 =
                STATUS_READ_INCOMPLETE;
        }

        // ----------------------------------------------------
        // Send response
        // ----------------------------------------------------

        sendReadResponse();
    }

    // ========================================================
    // WRITE 0x14
    // ========================================================

    void handleWrite(
        const uint8_t *data,
        uint16_t len)
    {
        if (len < 2)
            return;

        beginTransfer();

        uint8_t slave =
            data[0];

        uint8_t count =
            data[1];

        if (count > 61)
            count = 61;

        uint8_t addr7 =
            slave >> 1;

#if DEBUG_SERIAL

        Serial.print(
            "WRITE slave=0x"
        );

        Serial.print(
            slave,
            HEX
        );

        Serial.print(
            " length="
        );

        Serial.println(
            count
        );

#endif

        Wire.beginTransmission(
            addr7
        );

        for (
            uint8_t i = 0;
            i < count;
            i++
        )
        {
            Wire.write(
                data[2 + i]
            );
        }

        uint8_t result =
            Wire.endTransmission();

        transferLength = 0;

        if (result == 0)
        {
            transferStatus0 =
                STATUS_COMPLETE;

            transferStatus1 =
                STATUS_SUCCESS;
        }
        else
        {
            transferStatus0 =
                STATUS_ERROR;

            transferStatus1 =
                STATUS_TIMEOUT_NACK;
        }

        // ----------------------------------------------------
        // CP2112 does not need a 0x13 read response for WRITE.
        // The host can request status using 0x15.
        // ----------------------------------------------------

#if DEBUG_SERIAL

        Serial.print(
            "WRITE result="
        );

        Serial.println(
            result
        );

#endif
    }

    // ========================================================
    // WRITE / READ 0x11
    // ========================================================

    void handleWriteRead(
        const uint8_t *data,
        uint16_t len)
    {
        if (len < 4)
            return;

        beginTransfer();

        uint8_t slave =
            data[0];

        uint16_t readLen =
            ((uint16_t)data[1] << 8) |
            data[2];

        uint8_t targetLen =
            data[3];

        if (targetLen > 16)
            targetLen = 16;

        if (readLen > sizeof(readBuffer))
            readLen = sizeof(readBuffer);

        uint8_t addr7 =
            slave >> 1;

#if DEBUG_SERIAL

        Serial.print(
            "WRITE/READ slave=0x"
        );

        Serial.print(
            slave,
            HEX
        );

        Serial.print(
            " addr7=0x"
        );

        Serial.print(
            addr7,
            HEX
        );

        Serial.print(
            " targetLen="
        );

        Serial.print(
            targetLen
        );

        Serial.print(
            " readLen="
        );

        Serial.println(
            readLen
        );

#endif

        // ----------------------------------------------------
        // WRITE command
        //
        // false = don't generate STOP
        // ----------------------------------------------------

        Wire.beginTransmission(
            addr7
        );

        for (
            uint8_t i = 0;
            i < targetLen;
            i++
        )
        {
            Wire.write(
                data[4 + i]
            );
        }

        uint8_t result =
            Wire.endTransmission(false);

        if (result != 0)
        {
            transferStatus0 =
                STATUS_ERROR;

            transferStatus1 =
                STATUS_TIMEOUT_NACK;

            transferLength = 0;

            readBufferLength = 0;
            readBufferValid = false;

#if DEBUG_SERIAL

            Serial.print(
                "WRITE/READ command failed: "
            );

            Serial.println(
                result
            );

#endif

            sendTransferStatus();

            return;
        }

        // ----------------------------------------------------
        // Repeated START + READ
        // ----------------------------------------------------

        uint16_t received =
            Wire.requestFrom(
                (int)addr7,
                (int)readLen,
                (int)true
            );

        (void)received;

        uint16_t index = 0;

        unsigned long start =
            millis();

        while (
            index < readLen &&
            millis() - start < 100
        )
        {
            while (
                Wire.available() &&
                index < readLen
            )
            {
                readBuffer[index++] =
                    Wire.read();
            }

            if (index < readLen)
                delayMicroseconds(100);
        }

        readBufferLength =
            index;

        readBufferValid =
            true;

        transferLength =
            index;

        // ----------------------------------------------------
        // Result
        // ----------------------------------------------------

        if (index == readLen)
        {
            transferStatus0 =
                STATUS_COMPLETE;

            transferStatus1 =
                STATUS_SUCCESS;
        }
        else
        {
            transferStatus0 =
                STATUS_ERROR;

            transferStatus1 =
                STATUS_READ_INCOMPLETE;
        }

        // ----------------------------------------------------
        // Send response
        // ----------------------------------------------------

        sendReadResponse();
    }

    // ========================================================
    // FORCE READ 0x12
    // ========================================================

    void handleForceRead(
        const uint8_t *data,
        uint16_t len)
    {
        if (!readBufferValid)
        {
            transferStatus0 =
                STATUS_ERROR;

            transferStatus1 =
                STATUS_READ_INCOMPLETE;

            sendTransferStatus();

            return;
        }

        uint16_t requested = 0;

        if (len >= 2)
        {
            requested =
                ((uint16_t)data[0] << 8) |
                data[1];
        }

        if (requested > readBufferLength)
            requested = readBufferLength;

#if DEBUG_SERIAL

        Serial.print(
            "FORCE READ length="
        );

        Serial.println(
            requested
        );

#endif

        sendReadResponse(
            requested
        );
    }

    // ========================================================
    // READ RESPONSE 0x13
    //
    // Payload:
    //
    // byte 0 = status
    // byte 1 = length
    // byte 2... = data
    //
    // HID payload = 63 bytes
    // ========================================================

    void sendReadResponse(
        uint16_t lengthOverride = 0)
    {
        uint8_t report[63];

        memset(
            report,
            0,
            sizeof(report)
        );

        uint16_t count =
            lengthOverride ?
            lengthOverride :
            readBufferLength;

        if (count > 61)
            count = 61;

        report[0] =
            transferStatus0;

        report[1] =
            count;

        if (count > 0)
        {
            memcpy(
                &report[2],
                readBuffer,
                count
            );
        }

        // ----------------------------------------------------
        // IMPORTANT:
        // Do NOT change status back to IDLE here.
        // The host may query status using 0x15.
        // ----------------------------------------------------

        HID.SendReport(
            REPORT_DATA_READ_RESPONSE,
            report,
            sizeof(report)
        );

#if DEBUG_SERIAL

        Serial.print(
            "READ RESPONSE"
        );

        Serial.print(
            " status=0x"
        );

        Serial.print(
            transferStatus0,
            HEX
        );

        Serial.print(
            " length="
        );

        Serial.println(
            count
        );

        if (count > 0)
        {
            dumpHex(
                "  DATA: ",
                readBuffer,
                count
            );
        }

#endif
    }

    // ========================================================
    // TRANSFER STATUS RESPONSE 0x16
    //
    // Payload = 6 bytes
    //
    // byte 0 = status0
    // byte 1 = status1
    // byte 2 = retries MSB
    // byte 3 = retries LSB
    // byte 4 = length MSB
    // byte 5 = length LSB
    // ========================================================

    void sendTransferStatus()
    {
        uint8_t report[6];

        memset(
            report,
            0,
            sizeof(report)
        );

        report[0] =
            transferStatus0;

        report[1] =
            transferStatus1;

        report[2] =
            (transferRetries >> 8) & 0xFF;

        report[3] =
            transferRetries & 0xFF;

        report[4] =
            (transferLength >> 8) & 0xFF;

        report[5] =
            transferLength & 0xFF;

        HID.SendReport(
            REPORT_TRANSFER_STATUS_RESP,
            report,
            sizeof(report)
        );

#if DEBUG_SERIAL

        Serial.println(
            "TRANSFER STATUS RESPONSE"
        );

        Serial.print(
            "  Status0: 0x"
        );

        Serial.println(
            transferStatus0,
            HEX
        );

        Serial.print(
            "  Status1: 0x"
        );

        Serial.println(
            transferStatus1,
            HEX
        );

        Serial.print(
            "  Retries: "
        );

        Serial.println(
            transferRetries
        );

        Serial.print(
            "  Length: "
        );

        Serial.println(
            transferLength
        );

#endif
    }
};

// ============================================================
// GLOBAL DEVICE
// ============================================================

CP2112Device CP2112;

// ============================================================
// SETUP
// ============================================================

void setup()
{
#if DEBUG_SERIAL

    Serial.begin(115200);

    delay(500);

    Serial.println();

    Serial.println(
        "================================"
    );

    Serial.println(
        "ESP32-S3 CP2112 EMULATOR"
    );

    Serial.println(
        "DJI Battery Tool"
    );

    Serial.println(
        "================================"
    );

#endif

    // --------------------------------------------------------
    // I2C
    // --------------------------------------------------------

    Wire.begin(
        I2C_SDA,
        I2C_SCL
    );

    Wire.setClock(
        DEFAULT_I2C_SPEED
    );

#if DEBUG_SERIAL

    Serial.println(
        "I2C initialized"
    );

    Serial.print(
        "SDA = GPIO"
    );

    Serial.println(
        I2C_SDA
    );

    Serial.print(
        "SCL = GPIO"
    );

    Serial.println(
        I2C_SCL
    );

    Serial.print(
        "I2C clock = "
    );

    Serial.println(
        DEFAULT_I2C_SPEED
    );

#endif

    // --------------------------------------------------------
    // USB
    // --------------------------------------------------------

    USB.VID(
        CP2112_VID
    );

    USB.PID(
        CP2112_PID
    );

    USB.manufacturerName(
        "Silicon Laboratories"
    );

    USB.productName(
        "CP2112 HID USB-to-SMBus Bridge"
    );

    USB.serialNumber(
        "0001"
    );

    USB.firmwareVersion(
        0x0100
    );

    USB.usbVersion(
        0x0200
    );

    USB.usbPower(
        100
    );

    // --------------------------------------------------------
    // HID
    // --------------------------------------------------------

    CP2112.begin();

    // --------------------------------------------------------
    // USB
    // --------------------------------------------------------

    USB.begin();

    delay(1000);

#if DEBUG_SERIAL

    Serial.println();

    Serial.println(
        "USB CP2112 emulator started"
    );

    Serial.println(
        "VID = 10C4"
    );

    Serial.println(
        "PID = EA90"
    );

    Serial.println(
        "Revision = 0100"
    );

    Serial.println();

#endif
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    // Do NOT add large delays here.
    //
    // USB HID processing is handled by the USB stack.
    // Keep the loop responsive.

    delay(1);
}
