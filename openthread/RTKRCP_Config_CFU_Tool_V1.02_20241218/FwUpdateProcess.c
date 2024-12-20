#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "FwUpdate.h"
#include "trace.h"

#include "ttydevice.h"

#define OfferBinSuffix      ".offer.bin"
#define SrcBinSuffix        ".payload.bin"
#define FwBinCountMax       20
#define FwDirnameLenMax     512
#define FwNameLenMax        128
#define FwPathLenMax        (FwDirnameLenMax + FwNameLenMax)

#define APP_DATA_1_PATTERN        "_App_Data1."

#define FWImageTyeBOOT      5

typedef struct
{
    char OfferName[FwNameLenMax];
    char SrcBinName[FwNameLenMax];
    uint8_t image_type;
    bool is_download;
    bool is_srcbin_exist;
} T_FwFile_Unit;

typedef struct
{
    char offerfile_dirname[FwDirnameLenMax];
    char srcbinfile_dirname[FwDirnameLenMax];
    T_FwFile_Unit fileName[FwBinCountMax];
} T_FwFile;

T_FwFile FwFiles;

static bool getOfferBins(char *OfferFolder)
{
    DIR *dir;
    struct dirent *entry;
    uint16_t index = 0;
    bool rett = true;
    if (strlen(OfferFolder) + 1 > FwDirnameLenMax)
    {
        log_err("directory name of offer bin is too long");
        return false;
    }
    memset(FwFiles.offerfile_dirname, 0, FwDirnameLenMax);
    if ((dir = opendir(OfferFolder)) == NULL)
    {
        log_err("fail to open the folder");
        return false;
    }

    log_prompt("\n");
    while ((entry = readdir(dir)) != NULL)
    {
        log_prompt("%s\n", entry->d_name);
        if (strstr(entry->d_name, OfferBinSuffix) != NULL)
        {
            if (strlen(entry->d_name) + 1 > FwNameLenMax)
            {
                rett = false;
                break;
            }
            else
            {
                if (index < FwBinCountMax)
                {
                    //check the content of offer
                    {
                        bool ret = true;
                        char OfferPath[FwPathLenMax];
                        uint16_t len = 0;
                        char offerBuff[16] = {0};
                        strcpy(OfferPath, OfferFolder);
                        len = strlen(OfferPath);
                        if (OfferPath[len - 1] != '/')
                        {
                            OfferPath[len] = '/';
                            OfferPath[len + 1] = '\0';
                        }
                        strcat(OfferPath, entry->d_name);
                        ret = LoadOffer(OfferPath, offerBuff);
                        if (ret)
                        {
                            FwFiles.fileName[index].image_type = offerBuff[1] & 0x0F;
                        }
                        else
                        {
                            log_warn("warn: skip %s", entry->d_name);
                            continue;
                        }
                    }
                    strcpy(FwFiles.fileName[index].OfferName, entry->d_name);
                }
                else
                {
                    log_err("err: more than %d offers", FwBinCountMax);
                    rett = false;
                    break;
                }
            }
            index ++;
        }
    }
    closedir(dir);
    strcpy(FwFiles.offerfile_dirname, OfferFolder);
    return rett;
}

static bool getPayloadBins(char *SrcBinFolder)
{
    DIR *dir;
    struct dirent *entry;
    bool rett = true;
    if (strlen(SrcBinFolder) + 1 > FwDirnameLenMax)
    {
        log_err("directory name of payload bin is too long");
        return false;
    }
    memset(&FwFiles.srcbinfile_dirname, 0, FwDirnameLenMax);
    if ((dir = opendir(SrcBinFolder)) == NULL)
    {
        log_err("fail to open the folder");
        return false;
    }

    log_prompt("\n");
    while ((entry = readdir(dir)) != NULL)
    {
        log_prompt("%s\n", entry->d_name);
        if (strstr(entry->d_name, SrcBinSuffix) != NULL)
        {
            if (strlen(entry->d_name) + 1 > FwNameLenMax)
            {
                rett = false;
                break;
            }
            else
            {
                char FileName[FwNameLenMax];
                char FileName_target[FwNameLenMax];
                char *p;
                uint16_t index = 0;
                strcpy(FileName, entry->d_name);
                p = strstr(FileName, SrcBinSuffix);
                p[0] = '\0';
                while (index < FwBinCountMax)
                {
                    strcpy(FileName_target, FwFiles.fileName[index].OfferName);
                    p = strstr(FileName_target, OfferBinSuffix);
                    if (p)
                    {
                        p[0] = '\0';
                        if (strcmp(FileName_target, FileName) == 0)
                        {
                            strcpy(FwFiles.fileName[index].SrcBinName, entry->d_name);
                            FwFiles.fileName[index].is_srcbin_exist = true;
                            FwFiles.fileName[index].is_download = false;
                            break;
                        }
                    }
                    index ++;
                }
            }
        }
    }
    closedir(dir);
    strcpy(FwFiles.srcbinfile_dirname, SrcBinFolder);
    return rett;
}

bool FwUpdateMainProcess(char *OfferFolder, char *SrcBinFolder, uint8_t ForceIgnoreVersion,
                         uint8_t ForceReset)
{
    (void)(ForceReset);
    uint8_t round = 1;
    bool FwUpdateComplete = false;

    int ret = 0;
    char OfferPath[FwPathLenMax];
    char SrcBinPath[FwPathLenMax];
    uint16_t len = 0;
    uint8_t status = 0;

    uint16_t index = 0;
    uint16_t count = 0;

    char ttydevname[48] = {'\0'};
    uint16_t dev_name_len = sizeof(ttydevname);
    int exttime = 10000; //unit: us
    T_IMAGE_VERSION ver;
    T_IMAGE_VERSION ver_tmp;

    log_prompt("%s(): CFU Func V1.02 (20241218) \n", __func__);
    log_err("%s(): CFU Func V1.02 (20241218) \n", __func__);

    memset(&FwFiles, 0, sizeof(FwFiles));
    if (!getOfferBins(OfferFolder))
    {
        return false;
    }
    if (!getPayloadBins(SrcBinFolder))
    {
        return false;
    }
    for (uint16_t i = 0; i < FwBinCountMax; i++)
    {
        log_info("------------fw pair (%d) %s -------------", i,
                 FwFiles.fileName[i].is_srcbin_exist ? "exists" : "does not exist");
        log_info("  offer   %s", FwFiles.fileName[i].OfferName);
        log_info("  payload %s", FwFiles.fileName[i].SrcBinName);
        log_info("  image type %d", FwFiles.fileName[i].image_type);
    }

    if (!EnterFwUpdateMode())
    {
        log_prompt("fail to enter fw update mode\n");
        log_err("fail to enter fw update mode");
    }

    while (!FwUpdateComplete)
    {
        count = 0;
        if (round % 2 == 1)
        {
            // try to get version
            // make sure the RCP works after RCP reboot.
            if (FwGetVerison(&ver_tmp))
            {
                if (round == 1)
                {
                    memcpy(&ver, &ver_tmp, sizeof(ver));
                    log_prompt("[round %d] Before upgrading: version is 0x%04x and daulbank is %d \n", round,
                               ver.ver_info.version,
                               ver.is_dualbank);
                    log_info("[round %d] Before upgrading: version is 0x%04x and daulbank is %d", round,
                             ver.ver_info.version,
                             ver.is_dualbank);

                    if (ver.is_dualbank)
                    {
                        //dual bank
                        log_prompt("[cfu_print_current_version_and_bank_num] OTA-header current_ver %d.%d.%d.%d \n",
                                   ver.ver_info.header_sub_version._version_major,
                                   ver.ver_info.header_sub_version._version_minor,
                                   ver.ver_info.header_sub_version._version_revision,
                                   ver.ver_info.header_sub_version._version_reserve);
                        //dual bank
                        log_info("[cfu_print_current_version_and_bank_num] OTA-header current_ver %d.%d.%d.%d",
                                 ver.ver_info.header_sub_version._version_major,
                                 ver.ver_info.header_sub_version._version_minor,
                                 ver.ver_info.header_sub_version._version_revision,
                                 ver.ver_info.header_sub_version._version_reserve);
                    }
                    else
                    {
                        //single bank
                        log_prompt("[cfu_print_current_version_and_bank_num] app current_ver %d.%d.%d.%d \n",
                                   ver.ver_info.img_sub_version._version_major,
                                   ver.ver_info.img_sub_version._version_minor,
                                   ver.ver_info.img_sub_version._version_revision,
                                   ver.ver_info.img_sub_version._version_reserve);
                        //single bank
                        log_info("[cfu_print_current_version_and_bank_num] app current_ver %d.%d.%d.%d",
                                 ver.ver_info.img_sub_version._version_major,
                                 ver.ver_info.img_sub_version._version_minor,
                                 ver.ver_info.img_sub_version._version_revision,
                                 ver.ver_info.img_sub_version._version_reserve);
                    }
                }
            }
            else
            {
                log_prompt("[round %d] fail to get version\n", round);
                log_err("[round %d] fail to get version", round);
                ret = -1;
                goto Exit;
            }

            //just send offer
            log_prompt("\n===============just send offer, round %d ============\n", round);
            log_err("======================just send offer, round %d =========================", round);
            index = 0;
            while (index < FwBinCountMax)
            {
                bool is_send_offer = true;
                if (ForceIgnoreVersion != 0)
                {
                    is_send_offer = FwFiles.fileName[index].is_download ? false : true;
                }
                if (FwFiles.fileName[index].image_type == FWImageTyeBOOT && round > 2)
                {
                    //BOOT Patch is always dualbank, just send in round 1 & 2
                    is_send_offer = false;
                    log_warn("skip send %s in roud %d", FwFiles.fileName[index].OfferName, round);
                }
                if (!ver.is_dualbank && round > 2 && strstr(FwFiles.fileName[index].OfferName, APP_DATA_1_PATTERN))
                {
                    //BOOT Patch is always dualbank, just send in round 1 & 2
                    is_send_offer = false;
                    log_warn("skip send %s in roud %d", FwFiles.fileName[index].OfferName, round);
                }
                if (FwFiles.fileName[index].is_srcbin_exist && is_send_offer)
                {
                    strcpy(OfferPath, FwFiles.offerfile_dirname);
                    len = strlen(OfferPath);
                    if (OfferPath[len - 1] != '/')
                    {
                        OfferPath[len] = '/';
                        OfferPath[len + 1] = '\0';
                    }
                    strcat(OfferPath, FwFiles.fileName[index].OfferName);
                    if (FwUpdateOffer(OfferPath, !!ForceIgnoreVersion, 0, &status) == false)
                    {
                        ret = -2;
                        goto Exit;
                    }
                }
                index ++;
            }
        }
        else
        {
            //send offer and payload bin
            log_prompt("\n==============send offer and payload, round %d ===========\n", round);
            log_err("======================send offer and payload, round %d =========================", round);
            index = 0;
            while (index < FwBinCountMax)
            {
                bool is_send_offer = true;
                if (ForceIgnoreVersion != 0)
                {
                    is_send_offer = FwFiles.fileName[index].is_download ? false : true;
                }
                if (FwFiles.fileName[index].image_type == FWImageTyeBOOT && round > 2)
                {
                    //BOOT Patch is always dualbank, just send in round 1 & 2
                    is_send_offer = false;
                    log_warn("skip send %s in roud %d", FwFiles.fileName[index].OfferName, round);
                }
                if (!ver.is_dualbank && round > 2 && strstr(FwFiles.fileName[index].OfferName, APP_DATA_1_PATTERN))
                {
                    //BOOT Patch is always dualbank, just send in round 1 & 2
                    is_send_offer = false;
                    log_warn("skip send %s in roud %d", FwFiles.fileName[index].OfferName, round);
                }
                if (FwFiles.fileName[index].is_srcbin_exist && is_send_offer)
                {
                    strcpy(OfferPath, FwFiles.offerfile_dirname);
                    len = strlen(OfferPath);
                    if (OfferPath[len - 1] != '/')
                    {
                        OfferPath[len] = '/';
                        OfferPath[len + 1] = '\0';
                    }
                    strcat(OfferPath, FwFiles.fileName[index].OfferName);

                    strcpy(SrcBinPath, FwFiles.srcbinfile_dirname);
                    len = strlen(SrcBinPath);
                    if (SrcBinPath[len - 1] != '/')
                    {
                        SrcBinPath[len] = '/';
                        SrcBinPath[len + 1] = '\0';
                    }
                    strcat(SrcBinPath, FwFiles.fileName[index].SrcBinName);

                    if (FwUpdateOffer(OfferPath, !!ForceIgnoreVersion, 0, &status) == true)
                    {
                        if (status == FIRMWARE_UPDATE_OFFER_ACCEPT)
                        {
                            count ++;
                            if (FwUpdateSrc(SrcBinPath, !!ForceIgnoreVersion, 0) == true)
                            {
                                FwFiles.fileName[index].is_download = true;
                            }
                            else
                            {
                                ret = -2;
                                goto Exit;
                            }
                        }
                    }
                    else
                    {
                        ret = -2;
                        goto Exit;
                    }
                }
                index ++;
            }

            if (ver.is_dualbank)
            {
                log_prompt("complete for dual bank \n");
                FwUpdateComplete = true;
            }
            else
            {
                if (count == 0)
                {
                    log_prompt("complete for single bank \n");
                    FwUpdateComplete = true;
                }
                else
                {
                    //after all payload.bin has been sent, the RCP enters ‘complete’ state，
                    //and will refuse to reponse any commands. The the RCP will be reboot after 1s.
                    ttyGetDeviceName(ttydevname, &dev_name_len);
                    printf("[%s] \n", ttydevname);

                    if (strcmp(ttydevname, RTK_RCP_DEVICE) == 0)
                    {
                        //if RCP connects with host dircetly, such as:
                        //  ____________        __________
                        // | 8771HTV   |  uart  | host    |
                        // |___________|<------>|_________|
                        // or
                        //  ____________        __________
                        // | 8771GUV   |  usb   | host    |
                        // |___________|<------>|_________|
                        //after the RCP reboot, the ttyRTKRCP will be deattached
                        //we should wait for the ttyRTKRCP reattached
                        device_close();
                        while (1)
                        {
                            printf("1 wait device: [%s] \n", ttydevname);
                            usleep(200000);
                            if (access(ttydevname, F_OK) == 0)
                            {
                                printf("1 device: %s is up \n", ttydevname);
                                break;
                            }
                        }
                        // 1s wait for RCP reboot
                        sleep(1);
                        //wait for RCP are reattached
                        while (1)
                        {
                            log_prompt("2 wait device: [%s] \n", ttydevname);
                            usleep(200000);
                            if (access(ttydevname, F_OK) == 0)
                            {
                                log_prompt("2 device: %s is up \n", ttydevname);
                                break;
                            }
                        }
                        if (device_open() == -1)
                        {
                            log_prompt("fail to reopen RCP_CAM \n");
                            log_err("fail to reopen RCP_CAM \n");
                            ret = -1;
                            goto Exit;
                        }
                    }
                    else
                    {
                        //if RCP connects with host indircetly, such as:
                        //  ____________        __________         ________
                        // | 8771HTV   |  uart  | FT232   |  usb   | host |
                        // |___________|<------>|_________|<------>|______|
                        //Due to the RCP refuse to reponse any commands, the FwGetVerison will return a error.
                        //and only when the RCP has been reboot, the FwGetVerison will return a successful result.
                        //we check weather the RCP reboot successfully by getting version.
                        //we try to get version from RCP until 20s (retry_count * waitforFame timeout)
                        //expired or version response returned.
                        // if 20s expired, there is an error.
                        uint32_t retry_count = 0;
                        bool rett = false;

                        while (retry_count < 20 && rett == false)
                        {
                            rett = FwGetVerison(NULL);
                            retry_count ++;
                            log_prompt("ret of FwGetVerison is %d, count %d \n", rett, retry_count);
                        }
                        if (retry_count >= 20 && rett == false)
                        {
                            ret = -1;
                            goto Exit;
                        }
                    }
                }
            }
        }
        round ++;
    }

#if 0  //sync with Mandy Chong 20240530
    if (!ver.is_dualbank)
    {
        // for single bank, there is no playload from host to controller in the last round,
        // then the controller will be reboot after Ns timeout
        // during the Ns, we can get versions.
        // 8771HTV N = 2; 8771GUV N = 2;
        if (FwGetVerison(&ver))
        {
            log_prompt("After upgrade: version is 0x%04x and daulbank is %d \n", ver.ver_info.version,
                       ver.is_dualbank);
            log_info("After upgrade: version is 0x%04x and daulbank is %d", ver.ver_info.version,
                     ver.is_dualbank);

            if (ver.is_dualbank)
            {
                //never enter this branch
                //dual bank
                log_prompt("[cfu_print_current_version_and_bank_num] OTA-header current_ver %d.%d.%d.%d \n",
                           ver.ver_info.header_sub_version._version_major,
                           ver.ver_info.header_sub_version._version_minor,
                           ver.ver_info.header_sub_version._version_revision,
                           ver.ver_info.header_sub_version._version_reserve);
                //dual bank
                log_info("[cfu_print_current_version_and_bank_num] OTA-header current_ver %d.%d.%d.%d",
                         ver.ver_info.header_sub_version._version_major,
                         ver.ver_info.header_sub_version._version_minor,
                         ver.ver_info.header_sub_version._version_revision,
                         ver.ver_info.header_sub_version._version_reserve);
            }
            else
            {
                //single bank
                log_prompt("[cfu_print_current_version_and_bank_num] app current_ver %d.%d.%d.%d \n",
                           ver.ver_info.img_sub_version._version_major,
                           ver.ver_info.img_sub_version._version_minor,
                           ver.ver_info.img_sub_version._version_revision,
                           ver.ver_info.img_sub_version._version_reserve);
                //single bank
                log_info("[cfu_print_current_version_and_bank_num] app current_ver %d.%d.%d.%d",
                         ver.ver_info.img_sub_version._version_major,
                         ver.ver_info.img_sub_version._version_minor,
                         ver.ver_info.img_sub_version._version_revision,
                         ver.ver_info.img_sub_version._version_reserve);
            }
        }
    }
#endif

Exit:
    //more 500ms, for insure that the RCP has been reboot successfully
    //when RCP connects with host indircetly.
    ttyGetDeviceName(ttydevname, &dev_name_len);
    printf("[%s] \n", ttydevname);
    if (strcmp(ttydevname, RTK_RCP_DEVICE) == 0)
    {
        exttime = 100000;//unit: us
    }
    else
    {
        exttime = 500000;//unit: us
    }
    switch (ret)
    {
    case -1:
        //The controller enters CFU mode but receives GetVersion cmd
        //after 10s expired, the controller will be reboot
        usleep(10000000 + exttime);
        break;
    case -2:
        //8771HTV error 2s;
        //8771GUV error 2s;
        usleep(2000000 + exttime);
        break;
    case 0:
        //8771HTV single bank: received offer but no playload 2s
        //8771HTV dual bank: 'complete' 1s;
        //8771GUV single bank: received offer but no playload 2s
        //8771GUV dual bank: 'complete' 1s;
        if (ver.is_dualbank)
        {
            usleep(1000000 + exttime);
        }
        else
        {
            usleep(2000000 + exttime);
        }
        break;
    }

    //if the device node is ttyRTKRCP, we wait for that the ttyRTKRCP are reattached.
    if (strcmp(ttydevname, RTK_RCP_DEVICE) == 0)
    {
        device_close();
        while (1)
        {
            if (access(ttydevname, F_OK) == 0)
            {
                printf("3 device: %s is up \n", ttydevname);
                break;
            }
            printf("3 wait device: [%s] \n", ttydevname);
            usleep(200000);
        }
        device_open();
    }

    return ret == 0 ? true : false;
}

bool FwVersionMainProcess()
{
    int ret = 0;
    char ttydevname[48] = {'\0'};
    uint16_t dev_name_len = sizeof(ttydevname);
    int exttime = 10000; //unit: us
    T_IMAGE_VERSION ver;

    if (!EnterFwUpdateMode())
    {
        log_prompt("fail to enter fw update mode\n");
        log_err("fail to enter fw update mode");
    }

    if (FwGetVerison(&ver))
    {
        log_prompt("version is 0x%04x and daulbank is %d \n", ver.ver_info.version, ver.is_dualbank);
        log_info("version is 0x%04x and daulbank is %d", ver.ver_info.version, ver.is_dualbank);

        if (ver.is_dualbank)
        {
            //dual bank
            log_prompt("[cfu_print_current_version_and_bank_num] OTA-header current_ver %d.%d.%d.%d \n",
                       ver.ver_info.header_sub_version._version_major,
                       ver.ver_info.header_sub_version._version_minor,
                       ver.ver_info.header_sub_version._version_revision,
                       ver.ver_info.header_sub_version._version_reserve);
            //dual bank
            log_info("[cfu_print_current_version_and_bank_num] OTA-header current_ver %d.%d.%d.%d",
                     ver.ver_info.header_sub_version._version_major,
                     ver.ver_info.header_sub_version._version_minor,
                     ver.ver_info.header_sub_version._version_revision,
                     ver.ver_info.header_sub_version._version_reserve);
        }
        else
        {
            //single bank
            log_prompt("[cfu_print_current_version_and_bank_num] app current_ver %d.%d.%d.%d \n",
                       ver.ver_info.img_sub_version._version_major,
                       ver.ver_info.img_sub_version._version_minor,
                       ver.ver_info.img_sub_version._version_revision,
                       ver.ver_info.img_sub_version._version_reserve);
            //single bank
            log_info("[cfu_print_current_version_and_bank_num] app current_ver %d.%d.%d.%d",
                     ver.ver_info.img_sub_version._version_major,
                     ver.ver_info.img_sub_version._version_minor,
                     ver.ver_info.img_sub_version._version_revision,
                     ver.ver_info.img_sub_version._version_reserve);
        }

    }
    else
    {
        log_prompt("fail to get version\n");
        log_err("fail to get version");
    }

    //more 500ms, for insure that the RCP has been reboot successfully
    //when RCP connects with host indircetly.
    ttyGetDeviceName(ttydevname, &dev_name_len);
    printf("[%s] \n", ttydevname);
    if (strcmp(ttydevname, RTK_RCP_DEVICE) == 0)
    {
        exttime = 100000;//unit: us
    }
    else
    {
        exttime = 500000;//unit: us
    }
    usleep(2000000 + exttime);
    //if the device node is ttyRTKRCP, we wait for that the ttyRTKRCP are reattached.
    if (strcmp(ttydevname, RTK_RCP_DEVICE) == 0)
    {
        device_close();
        while (1)
        {
            if (access(ttydevname, F_OK) == 0)
            {
                printf("3 device: %s is up \n", ttydevname);
                break;
            }
            printf("3 wait device: [%s] \n", ttydevname);
            usleep(200000);
        }
        device_open();
    }
    return ret == 0 ? true : false;
}
/*++

Routine Description:

    Called from main.

Arguments:

    argc -- Number of command line arguments.
    argv -- Command line arguments.

Return Value:

    S_OK on success or underlying failure code.
--*/

FwUpdateRESULT FwUpdateMain(int argc, char *argv[])
{
    FwUpdateRESULT hr = S_OK;
    char *offerPath;
    char *srecBinPath;
    uint8_t forceIgnoreVersion = 0;
    uint8_t forceReset = 0;

    if (argc < 5)
    {
        log_prompt("Error, too few parameters.\n");
        log_err("Error, too few parameters.");
        hr = E_INVALIDARG;
        goto Exit;
    }

    offerPath = argv[3];
    srecBinPath = argv[4];

    // Walk through list of arguments past the mandatory ones and see if they match our options
    int optionalArgsToParse = argc - 5;
    for (int i = 0; i < optionalArgsToParse; i++)
    {
        if (!strcmp(argv[i + 5], "forceIgnoreVersion"))
        {
            forceIgnoreVersion = 1;
        }
        else if (!strcmp(argv[i + 5], "forceReset"))
        {
            forceReset = 1;
        }
    }

    bool returnVal = false;

    returnVal = FwUpdateMainProcess(offerPath, srecBinPath, forceIgnoreVersion, forceReset);

    if (returnVal)
    {
        //endtime
        log_prompt("FW Update Completed Successfully!\n");
        log_info("FW Update Completed Successfully!");
        hr = S_OK;
    }
    else
    {
        log_prompt("FW Update not performed on offer %s\n", offerPath);
        log_err("FW Update not performed on offer %s\n", offerPath);
        hr = E_FAIL;
    }
Exit:
    return hr;
}

int rtk_cfu_update(unsigned int argc, char *argv[])
{
    int ret = 0;

    if (device_open() < 0)
    {
        log_prompt("fail to open or set %s \n", argv[2]);
        log_err("fail to open or set %s \n", argv[2]);
        return -1;
    }

    if (!strcmp(argv[1], "update"))
    {
        ret = FwUpdateMain(argc, argv);
        log_prompt("update.\n");
    }
    else if (!strcmp(argv[1], "version"))
    {
        ret = FwVersionMainProcess();
        log_prompt("version.\n");
    }
    else
    {
        log_prompt("Failed to parse input tokens.\n");
        log_err("Failed to parse input tokens.\n");
    }
    device_close();
    return ret;
}