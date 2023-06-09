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

#ifndef FACERECOGNITION_PIN_AUTH_HELPER_H
#define FACERECOGNITION_PIN_AUTH_HELPER_H

#include "napi/native_api.h"
#include "napi/native_common.h"

namespace OHOS {
namespace PinAuth {
napi_value Init(napi_env env, napi_value exports);
napi_value GetCtor(napi_env env);
napi_value PinAuthServiceConstructor(napi_env env, napi_callback_info info);
napi_value RegisterInputer(napi_env env, napi_callback_info info);
napi_value UnregisterInputer(napi_env env, napi_callback_info info);
napi_value EnumExport(napi_env env, napi_value exports);
napi_value AuthSubTypeConstructor(napi_env env);
} // namespace PinAuth
} // namespace OHOS
#endif // FACERECOGNITION_PIN_AUTH_HELPER_H
