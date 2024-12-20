#ifndef __FWUPINT_H__
#define __FWUPINT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HID_CONTENT_PAYLOAD 52

#define RTK_RCP_DEVICE "/dev/ttyRTKRCP"

// NOTE - defines should match CFU Protocol Spec definitions
#define CFU_OFFER_METADATA_INFO_CMD                        (0xFF)
#define CFU_SPECIAL_OFFER_CMD                              (0xFE)
#define CFU_SPECIAL_OFFER_GET_STATUS                       (0x03)
#define CFU_SPECIAL_OFFER_NONCE                            (0x02)
#define CFU_SPECIAL_OFFER_NOTIFY_ON_READY                  (0x01)
#define FIRMWARE_OFFER_TOKEN_DRIVER                        (0xA0)
#define FIRMWARE_OFFER_TOKEN_SPEEDFLASHER                  (0xB0)
#define FIRMWARE_UPDATE_FLAG_FIRST_BLOCK                   (0x80)
#define FIRMWARE_UPDATE_FLAG_LAST_BLOCK                    (0x40)
#define FIRMWARE_UPDATE_FLAG_VERIFY                        (0x08)
#define FIRMWARE_UPDATE_STATUS_ERROR_COMPLETE              (0x03)
#define FIRMWARE_UPDATE_STATUS_ERROR_CRC                   (0x05)
#define FIRMWARE_UPDATE_STATUS_ERROR_INVALID               (0x0B)
#define FIRMWARE_UPDATE_STATUS_ERROR_INVALID_ADDR          (0x09)
#define FIRMWARE_UPDATE_STATUS_ERROR_NO_OFFER              (0x0A)
#define FIRMWARE_UPDATE_STATUS_ERROR_PENDING               (0x08)
#define FIRMWARE_UPDATE_STATUS_ERROR_PREPARE               (0x01)
#define FIRMWARE_UPDATE_STATUS_ERROR_SIGNATURE             (0x06)
#define FIRMWARE_UPDATE_STATUS_ERROR_VERIFY                (0x04)
#define FIRMWARE_UPDATE_STATUS_ERROR_VERSION               (0x07)
#define FIRMWARE_UPDATE_STATUS_ERROR_WRITE                 (0x02)
#define FIRMWARE_UPDATE_STATUS_SUCCESS                     (0x00)
#define OFFER_INFO_END_OFFER_LIST                          (0x02)
#define OFFER_INFO_START_ENTIRE_TRANSACTION                (0x00)
#define OFFER_INFO_START_OFFER_LIST                        (0x01)

typedef enum
{
    S_OK = 0,
    E_INVALIDARG,
    E_FAIL,
} FwUpdateRESULT;

enum FwUpdateOfferStatus
{
    // The offer needs to be skipped at this time indicating to
    // the host to please offer again during next applicable period.
    FIRMWARE_UPDATE_OFFER_SKIP = 0x00,

    // Once FIRMWARE_UPDATE_FLAG_LAST_BLOCK has been issued,
    // the accessory can then determine if the offer contents
    // apply to it.
    FIRMWARE_UPDATE_OFFER_ACCEPT = 0x01,

    // Once FIRMWARE_UPDATE_FLAG_LAST_BLOCK has been issued,
    // the accessory can then determine if the offer block contents apply to it.
    FIRMWARE_UPDATE_OFFER_REJECT = 0x02,

    // The offer needs to be delayed at this time.  The device has
    // nowhere to put the incoming blob.
    FIRMWARE_UPDATE_OFFER_BUSY = 0x03,

    // Used with the Offer Other response for the OFFER_NOTIFY_ON_READY
    // request, when the Accessory is ready to accept additional Offers.
    FIRMWARE_UPDATE_OFFER_COMMAND_READY = 0x04,

    // Response applicable to when the Offer request is not recognized.
    FIRMWARE_UPDATE_CMD_NOT_SUPPORTED = 0xFF
};

enum FwUpdateOfferRejectReason
{
    // The offer was rejected by the product due to the offer
    // version being older than the currently downloaded / existing firmware.
    FIRMWARE_OFFER_REJECT_OLD_FW = 0x00, //The offer was rejected by the product due to the offer version being older than the currently downloaded / existing firmware.

    // The offer was rejected due to it not being applicable to
    // the product�s primary MCU(Component ID).
    FIRMWARE_OFFER_REJECT_INV_MCU = 0x01,

    // MCU Firmware has been updated and a swap is currently pending.
    // No further Firmware Update processing can occur until the
    // target has been reset.
    FIRMWARE_UPDATE_OFFER_SWAP_PENDING = 0x02,

    // The offer was rejected due to a Version mismatch(Debug / Release for example)
    FIRMWARE_OFFER_REJECT_MISMATCH = 0x03,

    // The bank being offered for the component is currently in use.
    FIRMWARE_OFFER_REJECT_BANK = 0x04,

    // The offer's Platform ID does not correlate to the receiving
    // hardware product.
    FIRMWARE_OFFER_REJECT_PLATFORM = 0x05,

    // The offer's Milestone does not correlate to the receiving
    // hardware's Build ID.
    FIRMWARE_OFFER_REJECT_MILESTONE = 0x06,

    // The offer indicates an interface Protocol Revision that
    // the receiving product does not support.
    FIRMWARE_OFFER_REJECT_INV_PCOL_REV = 0x07,

    // The combination of Milestone & Compatibility Variants Mask did
    // not match the HW.
    FIRMWARE_OFFER_REJECT_VARIANT = 0x08
};

enum FwUpdateCommandResponseStatus
{
    // No Error, the requested function(s) succeeded.
    FIRMWARE_UPDATE_SUCCESS = 0x00,

    // Could not either:
    // 1.    Erase the upper block
    // 2.    Initialize the swap command scratch block
    // 3.    Copy the configuration data to the upper block
    FIRMWARE_UPDATE_ERROR_PREPARE = 0x01,

    // Could not write the bytes
    FIRMWARE_UPDATE_ERROR_WRITE = 0x02,

    // Could not set up the swap, in response to
    // FIRMWARE_UPDATE_FLAG_LAST_BLOCK
    FIRMWARE_UPDATE_ERROR_COMPLETE = 0x03,

    // Verification of the DWord failed, in response to
    // FIRMWARE_UPDATE_FLAG_VERIFY
    FIRMWARE_UPDATE_ERROR_VERIFY = 0x04,

    // CRC of the image failed, in response to FIRMWARE_UPDATE_FLAG_LAST_BLOCK
    FIRMWARE_UPDATE_ERROR_CRC = 0x05,

    // Firmware signature verification failed, in response to
    // FIRMWARE_UPDATE_FLAG_LAST_BLOCK
    FIRMWARE_UPDATE_ERROR_SIGNATURE = 0x06,

    // Firmware version verification failed, in response to
    // FIRMWARE_UPDATE_FLAG_LAST_BLOCK
    FIRMWARE_UPDATE_ERROR_VERSION = 0x07,

    // Firmware has already been updated and a swap is pending.
    // No further Firmware Update commands can be accepted until
    // the device has been reset.
    FIRMWARE_UPDATE_SWAP_PENDING = 0x08,

    // Firmware has detected an invalid destination address
    // within the message data content.
    FIRMWARE_UPDATE_ERROR_INVALID_ADDR = 0x09,

    // The Firmware Update Content Command was received without
    // first receiving a valid & accepted FW Update Offer.
    FIRMWARE_UPDATE_ERROR_NO_OFFER = 0x0A,

    // General error for the Firmware Update Content command,
    // such as an invalid applicable Data Length.
    FIRMWARE_UPDATE_ERROR_INVALID = 0x0B,
};

typedef struct
{
    // Byte 0
    uint8_t segment;

    // Byte 1
    uint8_t reserved : 6;
    uint8_t forceReset : 1;
    uint8_t forceIgnoreVersion : 1;

    // Byte 2
    uint8_t componentId;

    // Byte 3
    uint8_t token;
} __attribute__((packed)) ComponentInfo;

typedef struct
{
    // Byte 0
    uint8_t protocolRevision : 4;
    uint8_t bank : 2;
    uint8_t reserved0 : 2;

    // Byte 1
    uint8_t milestone : 4;
    uint8_t reserved1 : 4;

    // Byte 2 and 3
    uint16_t platformId;
} __attribute__((packed)) ProductInfo;

typedef struct
{
    uint8_t id;
    ComponentInfo componentInfo;
    uint32_t version;
    uint32_t compatVariantMask;
    ProductInfo productInfo;
} __attribute__((packed)) OfferData;

typedef union
{
    uint8_t data[17];
    OfferData offerData;
} __attribute__((packed)) OfferDataUnion;

typedef struct
{
    uint8_t id;
    uint32_t reserved0 : 24;
    uint32_t token : 8;
    uint32_t reserved1;
    uint32_t rrCode : 8;
    uint32_t reserved2 : 24;
    uint32_t status : 8;
    uint32_t reserved3 : 24;

} __attribute__((packed)) OfferResponseReportBlob;

typedef struct
{
    uint8_t id;
    uint8_t flags;
    uint8_t length;
    uint16_t sequenceNumber;
    uint32_t address;
    uint8_t data[MAX_HID_CONTENT_PAYLOAD];
} __attribute__((packed)) ContentData;

typedef struct
{
    uint8_t id;
    uint16_t sequenceNumber;
    uint16_t reserved0;

    uint32_t status : 8;
    uint32_t reserved1 : 24;

    uint32_t reserved2;
    uint32_t reserved3;
} __attribute__((packed)) ContentResponseReportBlob;

typedef struct
{
    uint32_t version;
    bool is_dualbank;
} t_versionInfo;

typedef struct
{
    bool is_dualbank;
    union
    {
        uint32_t version;
        struct
        {
            uint32_t _version_reserve: 8;   //!< reserved
            uint32_t _version_revision: 8; //!< revision version
            uint32_t _version_minor: 8;     //!< minor version
            uint32_t _version_major: 8;     //!< major version
        } header_sub_version; //!< ota header sub version
        struct
        {
            uint32_t _version_major: 4;     //!< major version
            uint32_t _version_minor: 8;     //!< minor version
            uint32_t _version_revision: 15; //!< revision version
            uint32_t _version_reserve: 5;   //!< reserved
        } img_sub_version; //!< other image sub version including patch, app, app data1-6
    } ver_info;
} T_IMAGE_VERSION;

bool EnterFwUpdateMode(void);
bool LoadOffer(char *OfferPath, char readBuff[16]);
bool FwUpdateOffer(char *OfferPath, uint8_t ForceIgnoreVersion, uint8_t ForceReset,
                   uint8_t *UpdateOfferStatus);
bool FwUpdateSrc(char *SrcBinPath, uint8_t ForceIgnoreVersion, uint8_t ForceReset);
bool FwGetVerison(T_IMAGE_VERSION *pVer);

bool FwUpdateMainProcess(char *OfferFolder, char *SrcBinFolder, uint8_t ForceIgnoreVersion,
                         uint8_t ForceReset);

int rtk_cfu_update(unsigned int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif