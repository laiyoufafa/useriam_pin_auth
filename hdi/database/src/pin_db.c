/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pin_db.h"
#include "adaptor_algorithm.h"
#include "adaptor_memory.h"
#include "adaptor_time.h"
#include "defines.h"
#include "file_operator.h"
#include "securec.h"

static PinDb g_pinDbOp = {CURRENT_VERSION, 0, NULL, false};

static ResultCode GetDataFromBuf(uint8_t **src, uint32_t *srcLen, uint8_t *dest, uint32_t destLen)
{
    if (destLen > *srcLen) {
        LOG_ERROR("bad len.");
        return RESULT_BAD_PARAM;
    }
    if (memcpy_s(dest, destLen, *src, destLen) != EOK) {
        LOG_ERROR("copy fail.");
        return RESULT_BAD_COPY;
    }

    *src = *src + destLen;
    *srcLen = *srcLen - destLen;
    return RESULT_SUCCESS;
}

static ResultCode UnpackPinDb(uint8_t *data, uint32_t dataLen)
{
    if (data == NULL || dataLen == 0) {
        LOG_ERROR("param is invalid.");
        return RESULT_BAD_PARAM;
    }

    uint8_t *temp = data;
    uint32_t tempLen = dataLen;
    if (GetDataFromBuf(&temp, &tempLen, (uint8_t *)&(g_pinDbOp.version), sizeof(g_pinDbOp.version)) != RESULT_SUCCESS) {
        LOG_ERROR("read version fail.");
        goto ERROR;
    }
    if (GetDataFromBuf(&temp, &tempLen, (uint8_t *)&(g_pinDbOp.pinIndexLen),
        sizeof(g_pinDbOp.pinIndexLen)) != RESULT_SUCCESS) {
        LOG_ERROR("read pinIndexLen fail.");
        goto ERROR;
    }
    if (g_pinDbOp.pinIndexLen > MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("pinIndexLen too large.");
        goto ERROR;
    }
    if (g_pinDbOp.pinIndexLen == 0) {
        g_pinDbOp.pinIndex = NULL;
        return RESULT_SUCCESS;
    }
    uint32_t mallocSize = sizeof(PinIndex) * g_pinDbOp.pinIndexLen;
    g_pinDbOp.pinIndex = (PinIndex *)Malloc(mallocSize);
    if (g_pinDbOp.pinIndex == NULL) {
        LOG_ERROR("pinIndex malloc fail.");
        goto ERROR;
    }
    if (mallocSize != tempLen) {
        LOG_ERROR("pinIndexLen too large.");
        goto ERROR;
    }
    if (memcpy_s(g_pinDbOp.pinIndex, mallocSize, temp, tempLen) != EOK) {
        LOG_ERROR("read pinIndex fail.");
        goto ERROR;
    }
    return RESULT_SUCCESS;

ERROR:
    DestroyPinDb();
    return RESULT_BAD_READ;
}

static ResultCode LoadPinDb()
{
    if (g_pinDbOp.isLoaded) {
        return RESULT_SUCCESS;
    }
    FileOperator *fileOp = GetFileOperator(DEFAULT_FILE_OPERATOR);
    if (!IsFileOperatorValid(fileOp)) {
        LOG_ERROR("fileOp invalid.");
        return RESULT_GENERAL_ERROR;
    }

    if (!fileOp->isFileExist(PIN_INDEX_NAME)) {
        g_pinDbOp.isLoaded = true;
        return RESULT_SUCCESS;
    }

    uint32_t dataLen = 0;
    ResultCode ret = (ResultCode)(fileOp->getFileLen(PIN_INDEX_NAME, &dataLen));
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("get filelen failed");
        return RESULT_BAD_READ;
    }

    uint8_t *data = Malloc(dataLen);
    if (data == NULL) {
        LOG_ERROR("parcel create failed");
        return RESULT_GENERAL_ERROR;
    }
    ret = fileOp->readFile(PIN_INDEX_NAME, data, dataLen);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("read_parcel_from_file failed.");
        goto EXIT;
    }

    ret = UnpackPinDb(data, dataLen);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("unpack db failed.");
        goto EXIT;
    }
    g_pinDbOp.isLoaded = true;
    LOG_INFO("LoadPinDb succ.");

EXIT:
    (void)memset_s(data, dataLen, 0, dataLen);
    Free(data);
    return ret;
}

void InitPinDb(void)
{
    ResultCode ret = LoadPinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("LoadPinDb fail.");
    }
    LOG_INFO("InitPinDb succ.");
}

void DestroyPinDb(void)
{
    if (g_pinDbOp.pinIndex != NULL) {
        Free(g_pinDbOp.pinIndex);
    }
    g_pinDbOp.version = CURRENT_VERSION;
    g_pinDbOp.pinIndexLen = 0;
    g_pinDbOp.pinIndex = NULL;
    g_pinDbOp.isLoaded = false;
    LOG_INFO("DestroyPinDb succ.");
}

static ResultCode CopyDataToBuf(uint8_t *data, uint32_t dataLen, uint8_t **buf, uint32_t *bufLen)
{
    if (memcpy_s(*buf, *bufLen, data, dataLen) != EOK) {
        LOG_ERROR("CopyFileName fail.");
        return RESULT_BAD_COPY;
    }

    *buf = *buf + dataLen;
    *bufLen = *bufLen - dataLen;
    return RESULT_SUCCESS;
}

static bool Uint2Str(uint64_t number, char *out, uint32_t length)
{
    if ((out == NULL) || (length != MAX_UINT_LEN)) {
        return false;
    }
    if (memset_s(out, length, 0, length) != EOK) {
        return false;
    }
    /* when number is zero, return string "0" directly */
    if (number == 0) {
        out[0] = '0';
        return true;
    }
    char outTemp[MAX_UINT_LEN] = {0};
    uint64_t temp = number;
    uint32_t index = 0;
    while (temp != 0) {
        outTemp[index] = temp % 10; /* 10 means decimal */
        temp = temp / 10; /* 10 means decimal */
        index++;
    }
    for (uint32_t i = 0; i < index; i++) {
        out[i] = outTemp[index - 1 - i] + '0'; /* trans number to ascii by add '0' */
    }

    return true;
}

static ResultCode GenerateFileName(uint64_t templateId, const char *prefix, const char *suffix,
    char *fileName, uint32_t fileNameLen)
{
    if (memset_s(fileName, fileNameLen, 0, fileNameLen) != EOK) {
        return RESULT_PIN_FAIL;
    }
    char *buf = fileName;
    uint32_t bufLen = fileNameLen;
    if (CopyDataToBuf((uint8_t *)prefix, strlen(prefix), (uint8_t **)&buf, &bufLen) != RESULT_SUCCESS) {
        LOG_ERROR("copy prefix fail.");
        return RESULT_BAD_COPY;
    }
    char templateIdStr[MAX_UINT_LEN] = {'\0'};
    if (!Uint2Str(templateId, templateIdStr, sizeof(templateIdStr))) {
        LOG_ERROR("Uint2Str error.");
        return RESULT_UNKNOWN;
    }
    if (CopyDataToBuf((uint8_t *)templateIdStr, strlen(templateIdStr), (uint8_t **)&buf, &bufLen) != RESULT_SUCCESS) {
        LOG_ERROR("copy templateIdStr fail.");
        return RESULT_BAD_COPY;
    }
    if (CopyDataToBuf((uint8_t *)suffix, strlen(suffix), (uint8_t **)&buf, &bufLen) != RESULT_SUCCESS) {
        LOG_ERROR("copy suffix fail.");
        return RESULT_BAD_COPY;
    }
    if (bufLen == 0) {
        LOG_ERROR("no space for endl.");
        return RESULT_BAD_COPY;
    }

    LOG_INFO("GenerateFileName succ.");
    return RESULT_SUCCESS;
}

/* This is for example only, Should be implemented in trusted environment. */
static ResultCode ReadPinFile(uint8_t *data, uint32_t dataLen, uint64_t templateId, const char *suffix)
{
    FileOperator *fileOp = GetFileOperator(DEFAULT_FILE_OPERATOR);
    if (!IsFileOperatorValid(fileOp)) {
        LOG_ERROR("fileOp invalid.");
        return RESULT_GENERAL_ERROR;
    }

    char fileName[MAX_FILE_NAME_LEN] = {'\0'};
    ResultCode ret = GenerateFileName(templateId, DEFAULT_FILE_HEAD, suffix, fileName, MAX_FILE_NAME_LEN);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("ReadPinFile Generate Pin FileName fail.");
        return RESULT_GENERAL_ERROR;
    }
    ret = fileOp->readFile(fileName, data, dataLen);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("read pin file fail.");
        return ret;
    }

    return RESULT_SUCCESS;
}

/* This is for example only, Should be implemented in trusted environment. */
static ResultCode WritePinFile(uint8_t *data, uint32_t dataLen, uint64_t templateId, const char *suffix)
{
    FileOperator *fileOp = GetFileOperator(DEFAULT_FILE_OPERATOR);
    if (!IsFileOperatorValid(fileOp)) {
        LOG_ERROR("fileOp invalid.");
        return RESULT_GENERAL_ERROR;
    }

    char fileName[MAX_FILE_NAME_LEN] = {'\0'};
    ResultCode ret = GenerateFileName(templateId, DEFAULT_FILE_HEAD, suffix, fileName, MAX_FILE_NAME_LEN);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WritePinFile Generate Pin FileName fail.");
        return RESULT_GENERAL_ERROR;
    }
    ret = fileOp->writeFile(fileName, data, dataLen);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WritePinFile fail.");
        return ret;
    }
    LOG_INFO("WritePinFile succ.");

    return RESULT_SUCCESS;
}

/* This is for example only, Should be implemented in trusted environment. */
static ResultCode RemovePinFile(uint64_t templateId, const char *suffix)
{
    FileOperator *fileOp = GetFileOperator(DEFAULT_FILE_OPERATOR);
    if (!IsFileOperatorValid(fileOp)) {
        LOG_ERROR("fileOp invalid.");
        return RESULT_GENERAL_ERROR;
    }
    char fileName[MAX_FILE_NAME_LEN] = {'\0'};
    ResultCode ret = GenerateFileName(templateId, DEFAULT_FILE_HEAD, suffix, fileName, MAX_FILE_NAME_LEN);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("GenerateCryptoFileName fail.");
        return RESULT_UNKNOWN;
    }
    if (fileOp->deleteFile(fileName) != RESULT_SUCCESS) {
        LOG_ERROR("file remove fail.");
        return RESULT_BAD_DEL;
    }

    LOG_INFO("RemovePinFile succ.");
    return ret;
}

static ResultCode RemoveAllFile(uint64_t templateId)
{
    ResultCode ret = RemovePinFile(templateId, CRYPTO_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("RemovePinCrypto fail.");
        return ret;
    }
    ret = RemovePinFile(templateId, ANTI_BRUTE_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("RemovePinAntiBrute fail.");
    }
    ret = RemovePinFile(templateId, SALT_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("RemovePinSalt fail.");
    }

    LOG_INFO("RemoveAllFile succ.");
    return RESULT_SUCCESS;
}

static uint64_t GeneratePinTemplateId()
{
    for (uint32_t i = 0; i < MAX_RANDOM_TIME; i++) {
        uint64_t templateId = INVALID_TEMPLATE_ID;
        SecureRandom((uint8_t *)&templateId, sizeof(templateId));
        if (templateId == INVALID_TEMPLATE_ID) {
            continue;
        }
        uint32_t j = 0;
        for (; j < g_pinDbOp.pinIndexLen; j++) {
            if (templateId == g_pinDbOp.pinIndex[i].templateId) {
                break;
            }
        }
        if (j == g_pinDbOp.pinIndexLen) {
            return templateId;
        }
    }
    LOG_ERROR("fail generate pin templateid.");
    return INVALID_TEMPLATE_ID;
}

static uint32_t SearchPinById(uint64_t templateId)
{
    if (g_pinDbOp.pinIndexLen == 0) {
        LOG_ERROR("no pin exist.");
        return MAX_CRYPTO_INFO_SIZE;
    }
    for (uint32_t index = 0; index < g_pinDbOp.pinIndexLen; index++) {
        if (g_pinDbOp.pinIndex[index].templateId == templateId) {
            LOG_INFO("SearchPinById succ.");
            return index;
        }
    }
    LOG_ERROR("no pin match.");
    return MAX_CRYPTO_INFO_SIZE;
}

static ResultCode DelPin(uint32_t index)
{
    /* This is for example only, Should be implemented in trusted environment. */
    ResultCode ret = RemoveAllFile(g_pinDbOp.pinIndex[index].templateId);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("Remove pin file fail.");
        return ret;
    }

    LOG_INFO("DelPin succ.");
    return RESULT_SUCCESS;
}

static bool IsPinDbValid(PinDb *pinDb)
{
    if (pinDb == NULL) {
        return false;
    }
    if (pinDb->version != CURRENT_VERSION) {
        return false;
    }
    if ((pinDb->pinIndexLen == 0) && (pinDb->pinIndex != NULL)) {
        return false;
    }
    if ((pinDb->pinIndexLen != 0) && (pinDb->pinIndex == NULL)) {
        return false;
    }
    if (pinDb->pinIndexLen > MAX_CRYPTO_INFO_SIZE) {
        return false;
    }
    return true;
}

static ResultCode GetBufFromData(uint8_t *src, uint32_t srcLen, uint8_t **dest, uint32_t *destLen)
{
    if (srcLen > *destLen) {
        LOG_ERROR("bad len.");
        return RESULT_BAD_PARAM;
    }
    if (memcpy_s(*dest, *destLen, src, srcLen) != EOK) {
        LOG_ERROR("copy fail.");
        return RESULT_BAD_COPY;
    }

    *dest = *dest + srcLen;
    *destLen = *destLen - srcLen;
    return RESULT_SUCCESS;
}

static ResultCode WritePinDb()
{
    if (!IsPinDbValid(&g_pinDbOp)) {
        LOG_ERROR("get invalid params.");
        return RESULT_BAD_PARAM;
    }
    ResultCode ret = RESULT_SUCCESS;
    FileOperator *fileOp = GetFileOperator(DEFAULT_FILE_OPERATOR);
    if (!IsFileOperatorValid(fileOp)) {
        LOG_ERROR("fileOp invalid.");
        return RESULT_GENERAL_ERROR;
    }

    uint32_t dataLen = sizeof(PinIndex) * g_pinDbOp.pinIndexLen + sizeof(uint32_t) * PIN_DB_TWO_PARAMS;
    uint8_t *data = Malloc(dataLen);
    if (data == NULL) {
        LOG_ERROR("malloc data fail.");
        return RESULT_GENERAL_ERROR;
    }
    uint8_t *temp = data;
    uint32_t tempLen = dataLen;
    if (GetBufFromData((uint8_t *)&(g_pinDbOp.version), sizeof(g_pinDbOp.version), &temp, &tempLen) != RESULT_SUCCESS) {
        ret = RESULT_BAD_WRITE;
        goto ERROR;
    }

    if (GetBufFromData((uint8_t *)&(g_pinDbOp.pinIndexLen), sizeof(g_pinDbOp.pinIndexLen),
        &temp, &tempLen) != RESULT_SUCCESS) {
        ret = RESULT_BAD_WRITE;
        goto ERROR;
    }
    if (g_pinDbOp.pinIndexLen != 0) {
        if (memcpy_s(temp, tempLen, g_pinDbOp.pinIndex, (sizeof(PinIndex) * g_pinDbOp.pinIndexLen)) != EOK) {
            ret = RESULT_BAD_WRITE;
            goto ERROR;
        }
    }

    if (fileOp->writeFile(PIN_INDEX_NAME, data, dataLen) != RESULT_SUCCESS) {
        LOG_ERROR("write_parcel_into_file failed.");
        ret = RESULT_BAD_WRITE;
        goto ERROR;
    }
    LOG_INFO("WritePinDb succ.");

ERROR:
    (void)memset_s(data, dataLen, 0, dataLen);
    Free(data);
    return ret;
}

static ResultCode DelPinInDb(uint32_t index)
{
    uint32_t pinIndexLen = g_pinDbOp.pinIndexLen - 1;
    if (pinIndexLen == 0) {
        (void)memset_s(g_pinDbOp.pinIndex,
            g_pinDbOp.pinIndexLen * sizeof(PinIndex), 0, g_pinDbOp.pinIndexLen * sizeof(PinIndex));
        Free(g_pinDbOp.pinIndex);
        g_pinDbOp.pinIndex = NULL;
    } else {
        uint32_t size = pinIndexLen * sizeof(PinIndex);
        PinIndex *pinIndex = (PinIndex *)Malloc(size);
        if (pinIndex == NULL) {
            LOG_ERROR("PinIndex malloc fail.");
            return RESULT_NO_MEMORY;
        }
        (void)memset_s(pinIndex, size, 0, size);
        for (uint32_t i = 0, j = 0; i < g_pinDbOp.pinIndexLen; i++) {
            if (i != index) {
                pinIndex[j] = g_pinDbOp.pinIndex[i];
                j++;
            }
        }
        (void)memset_s(g_pinDbOp.pinIndex,
            g_pinDbOp.pinIndexLen * sizeof(PinIndex), 0, g_pinDbOp.pinIndexLen * sizeof(PinIndex));
        Free(g_pinDbOp.pinIndex);
        g_pinDbOp.pinIndex = pinIndex;
    }
    g_pinDbOp.pinIndexLen = pinIndexLen;
    ResultCode ret = WritePinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WritePinDb fail.");
    }

    LOG_INFO("DelPinInDb succ.");
    return ret;
}

ResultCode DelPinById(uint64_t templateId)
{
    ResultCode ret = LoadPinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("LoadPinDb fail.");
        return ret;
    }
    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("no pin match.");
        return RESULT_BAD_MATCH;
    }

    ret = DelPin(index);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR(" DelPin fail.");
        return ret;
    }
    ret = DelPinInDb(index);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("DelPinInDb fail.");
        return ret;
    }
    LOG_INFO("DelPinById succ.");
    /* ignore index file remove result, return success when crypto file remove success */
    return ret;
}

static void InitPinIndex(PinIndex *pinIndex, uint64_t templateId, uint64_t subType)
{
    pinIndex->templateId = templateId;
    pinIndex->subType = subType;
}

static ResultCode AddPinInDb(uint64_t templateId, uint64_t subType)
{
    if (g_pinDbOp.pinIndexLen + 1 > MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("pinIndexLen too large.");
        return RESULT_BAD_PARAM;
    }
    uint32_t size = (g_pinDbOp.pinIndexLen + 1) * sizeof(PinIndex);
    PinIndex *pinIndex = (PinIndex *)Malloc(size);
    if (pinIndex == NULL) {
        LOG_ERROR("PinIndex malloc fail.");
        return RESULT_NO_MEMORY;
    }
    (void)memset_s(pinIndex, size, 0, size);
    if (g_pinDbOp.pinIndexLen != 0) {
        if (memcpy_s(pinIndex, size,
            g_pinDbOp.pinIndex, g_pinDbOp.pinIndexLen * sizeof(PinIndex)) != EOK) {
            LOG_ERROR("PinIndex copy fail.");
            (void)memset_s(pinIndex, size, 0, size);
            Free(pinIndex);
            return RESULT_NO_MEMORY;
        }
    }
    InitPinIndex(&pinIndex[g_pinDbOp.pinIndexLen], templateId, subType);
    if (g_pinDbOp.pinIndex != NULL) {
        Free(g_pinDbOp.pinIndex);
    }
    g_pinDbOp.pinIndex = pinIndex;
    g_pinDbOp.pinIndexLen++;
    ResultCode ret = WritePinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WritePinDb fail.");
    }

    LOG_INFO("AddPinInDb succ.");
    return ret;
}

static ResultCode RefreshPinDb(uint64_t *templateId, uint64_t subType)
{
    ResultCode ret = LoadPinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("LoadPinDb fail.");
        return ret;
    }
    *templateId = GeneratePinTemplateId();
    if (*templateId == INVALID_TEMPLATE_ID) {
        LOG_ERROR("GeneratePinTemplateId fail.");
        return RESULT_UNKNOWN;
    }
    ret = AddPinInDb(*templateId, subType);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("AddPinDb fail.");
        return ret;
    }

    LOG_INFO("RefreshPinDb succ.");
    return ret;
}

static ResultCode InitAntiBruteInfo(AntiBruteInfo *info)
{
    info->authErrorConut = INIT_AUTH_ERROR_COUNT;
    info->startFreezeTime = INIT_START_FREEZE_TIMES;
    return RESULT_SUCCESS;
}

ResultCode AddPin(PinEnrollParam *pinEnrollParam, uint64_t *templateId)
{
    if (pinEnrollParam == NULL || templateId == NULL) {
        LOG_ERROR("get invalid params.");
        return RESULT_BAD_PARAM;
    }
    ResultCode ret = RefreshPinDb(templateId, pinEnrollParam->subType);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("RefreshPinDb fail.");
        return ret;
    }

    /*
     * Generate new templateId, Transferred to tid and returned.
     * This is for example only, Should be implemented in trusted environment.
     */
    ret = WritePinFile(pinEnrollParam->pinData, CONST_PIN_DATA_LEN, *templateId, CRYPTO_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WriteCryptoFile fail.");
        return ret;
    }

    ret = WritePinFile(pinEnrollParam->salt, CONST_SALT_LEN, *templateId, SALT_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WriteSaltFile fail.");
        return ret;
    }

    AntiBruteInfo *initAntiBrute = Malloc(sizeof(AntiBruteInfo));
    if (initAntiBrute == NULL) {
        LOG_ERROR("malloc PAntiBruteInfo data fail.");
        return RESULT_GENERAL_ERROR;
    }
    ret = InitAntiBruteInfo(initAntiBrute);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("InitAntiBruteInfo fail.");
        goto EXIT;
    }
    ret = WritePinFile((uint8_t *)initAntiBrute, sizeof(AntiBruteInfo), *templateId, ANTI_BRUTE_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("WriteAntiBruteFile fail.");
        goto EXIT;
    }
    LOG_INFO("AddPin succ.");

EXIT:
    Free(initAntiBrute);
    return ret;
}

ResultCode DoGetSalt(uint64_t templateId, uint8_t *salt, uint32_t *saltLen)
{
    if (salt == NULL || saltLen == NULL || templateId == INVALID_TEMPLATE_ID) {
        LOG_ERROR("get invalid salt params.");
        return RESULT_BAD_PARAM;
    }

    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR(" no pin match.");
        return RESULT_BAD_MATCH;
    }

    ResultCode ret = ReadPinFile(salt, *saltLen, templateId, SALT_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("salt file read fail.");
        return ret;
    }
    LOG_INFO("DoGetSalt succ.");
    return RESULT_SUCCESS;
}

static ResultCode GetAntiBruteCountById(uint64_t templateId, uint32_t *count)
{
    ResultCode ret = LoadPinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("LoadPinDb fail.");
        return ret;
    }

    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("no pin index match.");
        return RESULT_BAD_MATCH;
    }

    AntiBruteInfo initAntiBrute = {INIT_AUTH_ERROR_COUNT, INIT_START_FREEZE_TIMES};
    ret = ReadPinFile((uint8_t *)&initAntiBrute, sizeof(AntiBruteInfo), templateId, ANTI_BRUTE_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("read AntiBrute startFreezeTime fail.");
        return ret;
    }
    *count = initAntiBrute.authErrorConut;

    return ret;
}

static ResultCode SetAntiBruteInfoById(uint64_t templateId, uint32_t count, uint64_t startFreezeTime)
{
    ResultCode ret = LoadPinDb();
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("LoadPinDb fail.");
        return ret;
    }

    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR(" no pin match.");
        return RESULT_BAD_MATCH;
    }

    AntiBruteInfo initAntiBrute = {count, startFreezeTime};
    ret = WritePinFile((uint8_t *)&initAntiBrute, sizeof(AntiBruteInfo), templateId, ANTI_BRUTE_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("write AntiBruteFileName fail.");
        return ret;
    }
    return ret;
}

ResultCode GetSubType(uint64_t templateId, uint64_t *subType)
{
    if (templateId == INVALID_TEMPLATE_ID) {
        LOG_ERROR("check param fail!");
        return RESULT_BAD_PARAM;
    }

    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("no pin match.");
        return RESULT_BAD_MATCH;
    }
    *subType = g_pinDbOp.pinIndex[index].subType;

    LOG_INFO("GetSubType succ.");
    return RESULT_SUCCESS;
}

ResultCode GetAntiBruteInfo(uint64_t templateId, uint32_t *authErrorConut, uint64_t *startFreezeTime)
{
    if (authErrorConut == NULL || startFreezeTime == NULL || templateId == INVALID_TEMPLATE_ID) {
        LOG_ERROR("check GetAntiBruteInfo param fail!");
        return RESULT_BAD_PARAM;
    }

    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("no pin match.");
        return RESULT_BAD_MATCH;
    }

    AntiBruteInfo initAntiBrute = {INIT_AUTH_ERROR_COUNT, INIT_START_FREEZE_TIMES};
    ResultCode ret = ReadPinFile((uint8_t *)&initAntiBrute, sizeof(AntiBruteInfo), templateId, ANTI_BRUTE_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("read AntiBrute startFreezeTime fail.");
        return ret;
    }
    *authErrorConut = initAntiBrute.authErrorConut;
    *startFreezeTime = initAntiBrute.startFreezeTime;

    LOG_INFO("GetAntiBruteInfo succ.");
    return ret;
}

static uint64_t ExponentialFuncTime(uint32_t authErrorConut)
{
    uint32_t ret = DEFAULT_VALUE;
    uint32_t exp = (authErrorConut - FIRST_EXPONENTIAL_PARA) / THIRD_EXPONENTIAL_PARA;
    for (uint32_t index = 0; index < exp; ++index) {
        ret *= SECOND_EXPONENTIAL_PARA;
    }
    return FIRST_EXPONENTIAL_PARA * ret;
}

uint64_t GetWaitTime(uint32_t authErrorConut)
{
    if (authErrorConut < FIRST_ANTI_BRUTE_COUNT) {
        return 0;
    }
    if (authErrorConut < ATTI_BRUTE_FIRST_STAGE) {
        if (authErrorConut == FIRST_ANTI_BRUTE_COUNT) {
            return ONE_MIN_TIME * MS_OF_S;
        }
        if (authErrorConut == SECOND_ANTI_BRUTE_COUNT) {
            return TEN_MIN_TIME * MS_OF_S;
        }
        if (authErrorConut == THIRD_ANTI_BRUTE_COUNT) {
            return THIRTY_MIN_TIME * MS_OF_S;
        }
        if (((authErrorConut - FIRST_ANTI_BRUTE_COUNT) % ANTI_BRUTE_COUNT_FREQUENCY) == 0) {
            return ONE_HOUR_TIME * MS_OF_S;
        }
        return 0;
    }
    if (authErrorConut > ATTI_BRUTE_SECOND_STAGE) {
        return ONE_DAY_TIME * MS_OF_S;
    }
    return ExponentialFuncTime(authErrorConut) * MS_OF_S;
}

ResultCode ComputeFreezeTime(uint64_t templateId, uint64_t *freezeTime, uint32_t count, uint64_t startFreezeTime)
{
    if (templateId == INVALID_TEMPLATE_ID || freezeTime == NULL) {
        LOG_ERROR("check ComputeFreezeTime param fail!");
        return RESULT_BAD_PARAM;
    }
    uint64_t timeValue = GetRtcTime();
    uint64_t waitTime = GetWaitTime(count);
    if (timeValue >= startFreezeTime) {
        uint64_t usedTime = timeValue - startFreezeTime;
        if (usedTime >= waitTime) {
            *freezeTime = 0;
        } else {
            *freezeTime = waitTime - usedTime;
        }
    } else {
        /* rtc time is reset, we should update startFreezeTime to timeValue */
        if (SetAntiBruteInfoById(templateId, count, timeValue) != RESULT_SUCCESS) {
            LOG_ERROR("SetAntiBruteInfoById fail.");
            return RESULT_BAD_PARAM;
        }
        *freezeTime = waitTime;
    }

    LOG_INFO("ComputeFreezeTime succ.");
    return RESULT_SUCCESS;
}

static uint32_t ComputeRemainingTimes(uint32_t errorCount)
{
    if (errorCount <= FIRST_ANTI_BRUTE_COUNT) {
        return FIRST_ANTI_BRUTE_COUNT - errorCount;
    }
    if (errorCount >= ATTI_BRUTE_FIRST_STAGE) {
        return REMAINING_TIMES_FREEZE;
    }
    return ANTI_BRUTE_COUNT_FREQUENCY - (errorCount - FIRST_ANTI_BRUTE_COUNT) % ANTI_BRUTE_COUNT_FREQUENCY;
}

ResultCode GetRemainTimes(uint64_t templateId, uint32_t *remainingAuthTimes, uint32_t authErrorConut)
{
    if (templateId == INVALID_TEMPLATE_ID || remainingAuthTimes == NULL) {
        LOG_ERROR("check GetRemainTimes param fail!");
        return RESULT_BAD_PARAM;
    }
    *remainingAuthTimes = ComputeRemainingTimes(authErrorConut);
    return RESULT_SUCCESS;
}

static ResultCode ClearAntiBruteInfoById(uint64_t templateId)
{
    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR(" no pin match.");
        return RESULT_BAD_MATCH;
    }
    ResultCode ret = SetAntiBruteInfoById(templateId, 0, INIT_START_FREEZE_TIMES);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("SetAntiBruteInfoById fail.");
    }
    return ret;
}

static ResultCode UpdateAntiBruteFile(uint64_t templateId, int32_t authResultSucc)
{
    if (templateId == INVALID_TEMPLATE_ID) {
        LOG_ERROR("check param fail.");
        return RESULT_BAD_PARAM;
    }

    if (authResultSucc == RESULT_SUCCESS) {
        ResultCode ret = ClearAntiBruteInfoById(templateId);
        if (ret != RESULT_SUCCESS) {
            LOG_ERROR("ClearAntiBruteInfoById fail.");
        }
        return ret;
    }

    uint64_t nowTime = GetRtcTime();
    uint32_t errorCount = 0;
    ResultCode ret = GetAntiBruteCountById(templateId, &errorCount);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("GetAntiBruteCountById fail.");
        return ret;
    }
    if (errorCount < ATTI_BRUTE_SECOND_STAGE) {
        errorCount++;
    }
    ret = SetAntiBruteInfoById(templateId, errorCount, nowTime);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("SetAntiBruteInfoById fail.");
    }
    return ret;
}

static int32_t CompareData(uint8_t *inputData, uint32_t inputDataLen, uint8_t *storeData, uint32_t storeDataLen)
{
    if (inputDataLen != storeDataLen) {
        LOG_ERROR("get false len.");
        return RESULT_COMPARE_FAIL;
    }
    if (memcmp(inputData, storeData, inputDataLen) == 0) {
        LOG_INFO("auth pin success.");
        return RESULT_SUCCESS;
    }
    LOG_ERROR("auth pin fail.");
    return RESULT_COMPARE_FAIL;
}

ResultCode AuthPinById(uint8_t *inputData, uint32_t inputDataLen, uint64_t templateId)
{
    if (inputData == NULL || inputDataLen == 0 || templateId == INVALID_TEMPLATE_ID) {
        LOG_ERROR("get invalid params.");
        return RESULT_BAD_PARAM;
    }

    uint32_t index = SearchPinById(templateId);
    if (index == MAX_CRYPTO_INFO_SIZE) {
        LOG_ERROR("no pin match.");
        return RESULT_BAD_MATCH;
    }

    uint32_t storeDataLen = inputDataLen;
    uint8_t *storeData = Malloc(storeDataLen);
    if (storeData == NULL) {
        LOG_ERROR("malloc storeData fail.");
        return RESULT_GENERAL_ERROR;
    }

    /* This is for example only, Should be implemented in trusted environment. */
    ResultCode compareRet = RESULT_COMPARE_FAIL;
    ResultCode ret = ReadPinFile(storeData, storeDataLen, templateId, CRYPTO_SUFFIX);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("Read pin store File fail.");
        goto EXIT;
    }
    compareRet = CompareData(inputData, inputDataLen, storeData, storeDataLen);
    if (compareRet != RESULT_SUCCESS) {
        LOG_ERROR("CompareData fail.");
    }
    /*
     * If the authentication succeeds, set authErrorConut to 0 and set startFreezeTime to the default value.
     * If the authentication fails, the value of authErrorConut is incremented by 1 and the
     * current time(startFreezeTime) is recorded.
     */
    ret = UpdateAntiBruteFile(templateId, compareRet);
    if (ret != RESULT_SUCCESS) {
        LOG_ERROR("UpdateAntiBruteFile fail.");
        goto EXIT;
    }
    ret = compareRet;
    LOG_INFO("AuthPinById end.");

EXIT:
    Free(storeData);
    return ret;
}

static bool FindTemplateIdFromList(uint64_t storeTemplateId, const uint64_t *templateIdList, uint32_t templateIdListLen)
{
    for (uint32_t i = 0; i < templateIdListLen; ++i) {
        if (templateIdList[i] == storeTemplateId) {
            return true;
        }
    }

    return false;
}

ResultCode VerifyTemplateDataPin(const uint64_t *templateIdList, uint32_t templateIdListLen)
{
    if (templateIdListLen != 0 && templateIdList == NULL) {
        LOG_ERROR("templateIdList should be not null, when templateIdListLen is not zero");
        return RESULT_BAD_PARAM;
    }
    uint32_t i = 0;
    for (; i < g_pinDbOp.pinIndexLen; i++) {
        if (FindTemplateIdFromList(g_pinDbOp.pinIndex[i].templateId, templateIdList, templateIdListLen)) {
            continue;
        }
        ResultCode ret = DelPinById(g_pinDbOp.pinIndex[i].templateId);
        if (ret != RESULT_SUCCESS) {
            LOG_ERROR("delete pin file fail.");
            return RESULT_BAD_DEL;
        }
    }
    LOG_INFO("VerifyTemplateDataPin succ.");
    return RESULT_SUCCESS;
}
