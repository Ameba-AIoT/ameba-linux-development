#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "trace.h"
#include "ttydevice.h"
#include "FwUpdate.h"

#define REPORT_LENGTH_STANDARD 61

#define FwUpdateVersion_reportid 0x2A //feature
#define FWUpdateContent_reportid 0x2A
#define FWUpdateContentResponse_reportid 0x2C
#define FWUpdateOffer_reportid 0x2D
#define FWUpdateOfferResponse_reportid 0x2D

#define FwUpdateVersion_LEN 60
#define FWUpdateContent_LEN 60
#define FWUpdateContentResponse_LEN 16
#define FWUpdateOffer_LEN 16
#define FWUpdateOfferResponse_LEN 16

uint8_t repBuff[REPORT_LENGTH_STANDARD];
static bool getRemainDataHandle(uint8_t *buff, uint16_t len)
{
    //read remain data before enter fw update mode
    //just drop the remain data
    (void)(buff);
    if (len == kMaxFrameSize)
    {
        return true;
    }
    return false;
}

static bool getVersionReponseHandle(uint8_t *buff, uint16_t len)
{
    static uint8_t index = 0;
    char recv_s[512] = {'\0'};
    int n = 0;
    int count = 0;

    //log_prompt("%s(): uart receive: %d\n", __func__, len);
    log_debug("%s(): uart receive: %d",  __func__, len);
    if (index + len >= FwUpdateVersion_LEN + 1 + 1)
    {
        log_err("%s(): unexpected error happened", __func__);
        exit(1);
    }
    memcpy(&repBuff[index], buff, len);
    index += len;
    if (index == FwUpdateVersion_LEN + 1)
    {
        n = snprintf(recv_s, sizeof(recv_s) - 1, "%s(): uart receive: ", __func__);
        count += n;
        for (uint8_t i = 0; i < index; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", repBuff[i]);
            count += n;
        }
        //log_prompt("%s(): %s\n", __func__, recv_s);
        log_debug("%s", recv_s);
        index = 0;
        return true;
    }
    return false;
}

static bool getOfferReponseHandle(uint8_t *buff, uint16_t len)
{
    static uint8_t index = 0;
    char recv_s[256] = {'\0'};
    int n = 0;
    int count = 0;

    //log_prompt("%s(): uart receive: %d\n", __func__, len);
    log_debug("%s(): uart receive: %d",  __func__, len);
    if (index + len >= FWUpdateOfferResponse_LEN + 1 + 1)
    {
        log_err("%s(): unexpected error happened", __func__);
        exit(1);
    }
    memcpy(&repBuff[index], buff, len);
    index += len;
    if (index == FWUpdateOfferResponse_LEN + 1)
    {
        n = snprintf(recv_s, sizeof(recv_s) - 1, "%s(): uart receive: ", __func__);
        count += n;
        for (uint8_t i = 0; i < index; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", repBuff[i]);
            count += n;
        }
        //log_prompt("%s(): %s\n", __func__, recv_s);
        log_debug("%s", recv_s);
        index = 0;
        return true;
    }
    return false;
}

static bool ReponseHandle(uint8_t *buff, uint16_t len)
{
    static uint8_t index = 0;
    char recv_s[256] = {'\0'};
    int n = 0;
    int count = 0;

    //log_prompt("%s(): uart receive: %d\n", __func__, len);
    log_debug("%s(): uart receive: %d",  __func__, len);
    if (index + len >= FWUpdateContentResponse_LEN + 1 + 1)
    {
        log_err("%s(): unexpected error happened", __func__);
        exit(1);
    }
    memcpy(&repBuff[index], buff, len);
    index += len;
    //#define FWUpdateContentResponse_LEN 16
    //#define FWUpdateOffer_LEN 16
    //#define FWUpdateOfferResponse_LEN 16
    if (index == FWUpdateContentResponse_LEN + 1)
    {
        n = snprintf(recv_s, sizeof(recv_s) - 1, "%s(): uart receive: ", __func__);
        count += n;
        for (uint8_t i = 0; i < index; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", repBuff[i]);
            count += n;
        }
        //log_prompt("%s(): %s\n", __func__, recv_s);
        log_debug("%s", recv_s);
        index = 0;
        return true;
    }
    return false;
}

static bool ProcessSrecBin(int srcBinFd, ContentData *pcontentData)
{
    pcontentData->length = 0;
    uint8_t pBuff[64] = { 0 };
    ssize_t ret = 0;

    // Read the address offset from the binary file
    ret = read(srcBinFd, pBuff, sizeof(pcontentData->address));
    if (ret == 0)
    {
        log_info("Stream reached the end of file");
        return false;
    }
    else if (ret < 0)
    {
        log_err("%s(): 1 fail to read src bin, errno %d(%s) ", __func__, errno, strerror(errno));
        exit(1);
    }
    pcontentData->address = (pBuff[3] << 24) + (pBuff[2] << 16) + (pBuff[1] << 8) + pBuff[0];

    //log_prompt("pcontentData->address 0x%08x \n", pcontentData->address);
    //log_err("pcontentData->address 0x%08x", pcontentData->address);

    // Read the byte length from the binary file
    ret = read(srcBinFd, pBuff, sizeof(pcontentData->length));
    if (ret == 0)
    {
        log_err("%s(): 1 src bin is wrong", __func__);
        exit(1);
    }
    else if (ret < 0)
    {
        log_err("%s(): 2 fail to read src bin, errno %d(%s) ", __func__, errno, strerror(errno));
        exit(1);
    }
    pcontentData->length = pBuff[0];

    if (pcontentData->length == 0)
    {
        return false;
    }
    else if (pcontentData->length > 52)
    {
        log_err("%s(): 2 src bin is wrong", __func__);
        exit(1);
    }

    // Read the content block using the length we just collected
    ret = read(srcBinFd, pBuff, pcontentData->length);
    if (ret == 0)
    {
        log_err("%s(): 3 src bin is wrong", __func__);
        exit(1);
    }
    else if (ret < 0)
    {
        log_err("%s(): 3 fail to read src bin, errno %d(%s) ", __func__, errno, strerror(errno));
        exit(1);
    }

    // Copy the content data into out var
    memcpy(pcontentData->data, pBuff, pcontentData->length);

    return true;
}

bool EnterFwUpdateMode(void)
{
    int err;
    int count = 0;
    char ttydevname[48] = {'\0'};
    uint16_t dev_name_len = sizeof(ttydevname);

    log_err("fw update tool version: 20240126 V1.0.0");


    // uint8_t cmd[9] = {0x85, 0x03, 0x83, 0x78, 0x03, 0x00, 0x5d, 0x00, 0x01};
    // the following array data is encode by HDLC from the upper array data
    uint8_t cmd[13] = {0x7e, 0x85, 0x03, 0x83, 0x78, 0x03, 0x00, 0x5d, 0x00, 0x01, 0x0b, 0x37, 0x7e};

    if (ttyWrite(cmd, sizeof(cmd)) ==  0)
    {
        log_prompt("enable fw update mode \n");
        log_info("enable fw update mode");
    }
    else
    {
        log_prompt("fail to enable fw update mode \n");
        log_err("fail to enable fw update mode");
        return false;
    }

    if (!ttyGetDeviceName(ttydevname, &dev_name_len))
    {
        return false;
    }

    printf("[%s] \n", ttydevname);
    if (strcmp(ttydevname, RTK_RCP_DEVICE) == 0)
    {
        device_close();
        usleep(200000);
        while (1)
        {
            printf("wait device: %s \n", ttydevname);
            if (access(ttydevname, F_OK) == 0)
            {
                printf("device: %s is up \n", ttydevname);
                break;
            }
            usleep(200000);
        }
        device_open();
    }

    err = 0;
    while (err == 0)
    {
        err = ttyWaitForFrame(1000000, getRemainDataHandle);
        count ++;
        log_debug("read remaining data before enter fw update mode (count %d)", count);
    }

    return true;
}

bool LoadOffer(char *OfferPath, char readBuff[16])
{
    int rett;
    int offerfd = -1;

    log_debug("LoadOffer: the size of OfferDataUnion is %d", sizeof(OfferDataUnion));

    // Attempt to open the fw offerPath file
    offerfd = open(OfferPath, O_RDONLY);
    if (offerfd == -1)
    {
        log_err("LoadOffer: fail to open offer file(%s), errno %d(%s)", OfferPath, errno, strerror(errno));
        return false;
    }

    // read data as a block:
    rett = read(offerfd, readBuff, 16);
    if (rett != 16)
    {
        if (rett == -1)
        {
            log_err("LoadOffer: fail to read offer file(%s), errno %d(%s)", OfferPath, errno, strerror(errno));
        }
        else if (rett < 16)
        {
            log_err("LoadOffer: offer file(%s) is wrong", OfferPath);
        }
        else
        {
            log_err("LoadOffer: unknown error hanppened when read offer file");
        }
    }
    close(offerfd);
    return rett == 16 ? true : false;
}

bool FwUpdateOffer(char *OfferPath, uint8_t ForceIgnoreVersion, uint8_t ForceReset,
                   uint8_t *UpdateOfferStatus)
{
    int rett;
    int err;
    OfferDataUnion offerDataUnion = { 0 };
    char readBuff[16] = { 0 };
    uint8_t reportBuffer[REPORT_LENGTH_STANDARD] = { 0 };
    uint32_t reportLength = FWUpdateOffer_LEN + 1;
    int offerfd = -1;

    log_debug("the size of OfferDataUnion is %d", sizeof(OfferDataUnion));

    // Attempt to open the fw offerPath file
    offerfd = open(OfferPath, O_RDONLY);
    if (offerfd == -1)
    {
        log_err("fail to open offer file(%s), errno %d(%s)", OfferPath, errno, strerror(errno));
        return false;
    }

    // read data as a block:
    rett = read(offerfd, readBuff, 16);
    if (rett != 16)
    {
        if (rett == -1)
        {
            log_err("fail to read offer file(%s), errno %d(%s)", OfferPath, errno, strerror(errno));
        }
        else if (rett < 16)
        {
            log_err("offer file(%s) is wrong", OfferPath);
        }
        else
        {
            log_err("unknown error hanppened when read offer file");
        }
        close(offerfd);
        return false;
    }
    memcpy(&offerDataUnion.data[1], readBuff, sizeof(readBuff));
    close(offerfd);

    offerDataUnion.offerData.id = FWUpdateOffer_reportid;
    offerDataUnion.offerData.componentInfo.forceReset = ForceReset;
    offerDataUnion.offerData.componentInfo.forceIgnoreVersion = ForceIgnoreVersion;
    memcpy(reportBuffer, &offerDataUnion, sizeof(offerDataUnion));

    if (ttyWrite(reportBuffer, reportLength) ==  0)
    {
        log_prompt("SetOutputReport for Offer: %s \n", OfferPath);

        log_info("SetOutputReport for Offer:    \
                \n bank: %d                     \
                \n milestone: %d                 \
                \n platformId: 0x%X             \
                \n protocolRevision: 0x%X        \
                \n compatVariantMask: 0x%X      \
                \n componentId: 0x%X           \
                \n forceIgnoreVersion: 0x%X      \
                \n forceReset: 0x%X             \
                \n segment: 0x%X                 \
                \n token: 0x%X\n",
                 offerDataUnion.offerData.productInfo.bank,
                 offerDataUnion.offerData.productInfo.milestone,
                 offerDataUnion.offerData.productInfo.platformId,
                 offerDataUnion.offerData.productInfo.protocolRevision,
                 offerDataUnion.offerData.compatVariantMask,
                 offerDataUnion.offerData.componentInfo.componentId,
                 offerDataUnion.offerData.componentInfo.forceIgnoreVersion,
                 offerDataUnion.offerData.componentInfo.forceReset,
                 offerDataUnion.offerData.componentInfo.segment,
                 offerDataUnion.offerData.componentInfo.token);
    }
    else
    {
        log_err("SetOutputReport failed");
        log_prompt("SetOutputReport failed\n");
        return false;
    }

    err = ttyWaitForFrame(2000000, getOfferReponseHandle);
    if (err == 0)
    {
        OfferResponseReportBlob *pOfferResponseReportBlob = (OfferResponseReportBlob *)repBuff;

#if 0
        log_info("status: %d            \
                \n rrCode: %d           \
                \n token: %d            \
                \n reserved0: 0x%X      \
                \n reserved1: 0x%X      \
                \n reserved2: 0x%X      \
                \n reserved3: 0x%X",
                 pOfferResponseReportBlob->status,
                 pOfferResponseReportBlob->rrCode,
                 pOfferResponseReportBlob->token,
                 pOfferResponseReportBlob->reserved0,
                 pOfferResponseReportBlob->reserved1,
                 pOfferResponseReportBlob->reserved2,
                 pOfferResponseReportBlob->reserved3);
#endif

        *UpdateOfferStatus = pOfferResponseReportBlob->status;

        log_prompt("Response: FW Update offer status is %d\n", *UpdateOfferStatus);
        log_info("Response: FW Update offer %s status is %d", OfferPath, *UpdateOfferStatus);
        return true;
    }
    else
    {
        log_err("error %d happened while waiting for Offer Command Response Report", err);
        return false;
    }
    return false;
}

bool FwUpdateSrc(char *SrcBinPath, uint8_t ForceIgnoreVersion, uint8_t ForceReset)
{
    (void)(ForceIgnoreVersion);
    (void)(ForceReset);
    bool ret = false;
    ContentData contentdata = { 0 };
    uint32_t reportLength;

    int SrcBinFd = -1;

    log_err("FwUpdateSrc %s", SrcBinPath);
    // Attempt to open the firmware srec file
    SrcBinFd = open(SrcBinPath, O_RDONLY);
    if (SrcBinFd == -1)
    {
        log_err("fail to open SrcBin file(%s), errno %d(%s)", SrcBinPath, errno, strerror(errno));
        goto Exit;
    }

    contentdata.sequenceNumber = 0;
    contentdata.address = 0;

    // First block
    contentdata.flags = FIRMWARE_UPDATE_FLAG_FIRST_BLOCK;
    uint32_t startAddress = 0;
    uint32_t totalContentPacketCount = 0;
    uint32_t contentPacketsSent = 0;


    // Walk the entire file to see how many content packets need to be sent
    while (ProcessSrecBin(SrcBinFd, &contentdata))
    {
        totalContentPacketCount++;
    }
    log_prompt("totalContentPacketCount %d\n", totalContentPacketCount);
    log_warn("totalContentPacketCount %d", totalContentPacketCount);

    // Reset stream to start
    {
        off_t offset = -1;
        offset = lseek(SrcBinFd, 0, SEEK_SET);
        if (offset == -1)
        {
            log_err("ret of lseek is %ld, errno %d, %s", offset, errno, strerror(errno));
        }
    }

    log_prompt("Beginning content packet transfers:\n");
    log_warn("Beginning content packet transfers:");
    while (ProcessSrecBin(SrcBinFd, &contentdata))
    {
        contentdata.flags = 0;
        // Establish starting absolute address offset
        if (contentPacketsSent == 0)
        {
            contentdata.flags = FIRMWARE_UPDATE_FLAG_FIRST_BLOCK;
            startAddress = contentdata.address;
        }

        contentdata.id = FWUpdateContent_reportid;
        reportLength = FWUpdateContent_LEN + 1;

        // Subtract the start address from absolute address
        contentdata.address -= startAddress;
        if (contentPacketsSent + 1 == totalContentPacketCount)
        {
            // Last block
            contentdata.flags = FIRMWARE_UPDATE_FLAG_LAST_BLOCK;
        }

        // Send out the content
        if (ttyWrite((uint8_t *)&contentdata, reportLength) == 0)
        {

        }
        else
        {
            log_err("Error occurred on SetOutputReport 0x%X:", contentdata.address);
            goto Exit;
        }

        int err = ttyWaitForFrame(2000000, ReponseHandle);
        // If completionEvent was signaled, then a read just completed
        // so get the status and leave this loop and process the data
        if (err == 0)
        {
            // ReadEventTriggered
            ContentResponseReportBlob *pContentResponseReportBlob = (ContentResponseReportBlob *)repBuff;;
            if (pContentResponseReportBlob->status != FIRMWARE_UPDATE_SUCCESS)
            {
                log_err("\nFW Update not Completed due to content response error, status: %d, sequenceNumber: %d\n",
                        pContentResponseReportBlob->status,
                        pContentResponseReportBlob->sequenceNumber);
                goto Exit;
            }
            else if (pContentResponseReportBlob->sequenceNumber != contentdata.sequenceNumber)
            {
                log_err("\nWaiting for matching ccr to my cr\n");
            }
            else
            {
                //success
            }
        }
        else
        {
            log_err("error %d happened while waiting for Contentdata Response Report", err);
            goto Exit;
        }

        contentdata.sequenceNumber++;
        contentPacketsSent++;
        if (contentPacketsSent == totalContentPacketCount)
        {
            log_info("Successfully sent %d content packets (100%% complete)\n", contentPacketsSent);
        }
    }

    // Make sure we sent 100% of packets
    if ((contentPacketsSent * 100.0 / totalContentPacketCount) < 100.0)
    {
        log_err("Never sent final block command because either srec "
                "file not completed or there were no content packets to "
                "send in the file\n");
        ret = false;
    }
    else
    {
        // Succeeded
        ret = true;
    }

Exit:
    if (SrcBinFd != -1)
    {
        close(SrcBinFd);
        SrcBinFd = -1;
    }
    return ret;
}

bool FwGetVerison(T_IMAGE_VERSION *pVer)
{
    int err = 0;
    uint8_t cmdBuffer[1] = { FwUpdateVersion_reportid };

    char recv_s[512] = {'\0'};
    int n = 0;
    int count = 0;

    if (ttyWrite(cmdBuffer, 1) !=  0)
    {
        log_err("GetVerison failed");
        log_prompt("GetVerison failed\n");
        return false;
    }

    err = ttyWaitForFrame(2000000, getVersionReponseHandle);
    if (err == 0)
    {
        n = snprintf(recv_s, sizeof(recv_s) - 1, "%s(): ", __func__);
        count += n;
        for (uint8_t i = 0; i < FwUpdateVersion_LEN + 1; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", repBuff[i]);
            count += n;
        }
        log_prompt("%s\n", recv_s);
        log_debug("%s(): %s",  __func__, recv_s);
    }
    else
    {
        log_err("error %d happened while waiting for GetVerison Command Response Report", err);
        return false;
    }

    if (pVer)
    {
        pVer->is_dualbank = (repBuff[9] & 0x3) == 2 ? false : true;
        pVer->ver_info.version = (repBuff[8] << 24) + (repBuff[7] << 16) + (repBuff[6] << 8) + repBuff[5];
    }
    return true;
}
