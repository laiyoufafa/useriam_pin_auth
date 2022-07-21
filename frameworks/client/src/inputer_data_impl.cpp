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

#include "inputer_data_impl.h"

#include <cstddef>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/ossl_typ.h>
#include <openssl/kdf.h>
#include <vector>

#include "iremote_inputer_data.h"
#include "iam_logger.h"

#define LOG_LABEL OHOS::UserIAM::Common::LABEL_PIN_AUTH_SDK

namespace OHOS {
namespace UserIAM {
namespace PinAuth {
InputerDataImpl::InputerDataImpl(std::vector<uint8_t> salt, sptr<IRemoteInputerData> remoteInputerData) : salt_(salt),
    remoteInputerData_(remoteInputerData)
{
}

InputerDataImpl::~InputerDataImpl()
{
}

void InputerDataImpl::OnSetData(int32_t authSubType, std::vector<uint8_t> data)
{
    IAM_LOGI("start");
    std::vector<uint8_t> scrypt;
    IAM_LOGI("data size is : %{public}zu", data.size());
    getScrypt(data, scrypt);
    remoteInputerData_->OnSetData(authSubType, scrypt);
}

void InputerDataImpl::getScrypt(std::vector<uint8_t> data, std::vector<uint8_t> &scrypt)
{
    IAM_LOGI("start");
    EVP_PKEY_CTX *pctx;
    unsigned char out[OUT_LENGTH];

    size_t outlen = sizeof(out);
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, NULL);
    if (EVP_PKEY_derive_init(pctx) <= 0) {
        IAM_LOGE("EVP_PKEY_derive_init error");
        return;
    }
    if (EVP_PKEY_CTX_set1_pbe_pass(pctx, data.data(), data.size()) <= 0) {
        IAM_LOGE("EVP_PKEY_CTX_set1_pbe_pass error");
        EVP_PKEY_CTX_free(pctx);
        return;
    }
    if (EVP_PKEY_CTX_set1_scrypt_salt(pctx, salt_.data(), salt_.size()) <= 0) {
        IAM_LOGE("EVP_PKEY_CTX_set1_scrypt_salt error");
        EVP_PKEY_CTX_free(pctx);
        return;
    }
    if (EVP_PKEY_CTX_set_scrypt_N(pctx, SCRYPT_N) <= 0) {
        IAM_LOGE("EVP_PKEY_CTX_set_scrypt_N error");
        EVP_PKEY_CTX_free(pctx);
        return;
    }
    if (EVP_PKEY_CTX_set_scrypt_r(pctx, SCRYPT_R) <= 0) {
        IAM_LOGE("EVP_PKEY_CTX_set_scrypt_r error");
        EVP_PKEY_CTX_free(pctx);
        return;
    }
    if (EVP_PKEY_CTX_set_scrypt_p(pctx, SCRYPT_P) <= 0) {
        IAM_LOGE("EVP_PKEY_CTX_set_scrypt_p error");
        EVP_PKEY_CTX_free(pctx);
        return;
    }
    if (EVP_PKEY_derive(pctx, out, &outlen) <= 0) {
        IAM_LOGE("EVP_PKEY_derive error");
        EVP_PKEY_CTX_free(pctx);
        return;
    }

    for (size_t i = 0; i < OUT_LENGTH; i++) {
        scrypt.push_back(out[i]);
    }

    EVP_PKEY_CTX_free(pctx);
}
} // namespace PinAuth
} // namespace UserIAM
} // namespace OHOS