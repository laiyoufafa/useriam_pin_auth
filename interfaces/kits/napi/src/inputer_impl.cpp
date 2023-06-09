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

#include "inputer_impl.h"
#include <uv.h>
#include "pin_auth_common.h"
#include "pinauth_log_wrapper.h"
#include "securec.h"

using namespace OHOS::UserIAM::PinAuth;
namespace OHOS {
namespace PinAuth {
static void DeleteData(uv_work_t* work, int status)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, DeleteData start");
    InputerHolder *inputerHolder = reinterpret_cast<InputerHolder *>(work->data);
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "inputerHolder is null");
        delete work;
        return;
    }
    napi_delete_reference(inputerHolder->env, inputerHolder->inputer);
    delete inputerHolder;
    delete work;
}

InputerImpl::InputerImpl(napi_env env, napi_ref inputer)
{
    env_ = env;
    inputer_ = inputer;
}

InputerImpl::~InputerImpl()
{
    uv_loop_s *loop(nullptr);
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "loop is null");
        return;
    }
    uv_work_t *work = new (std::nothrow) uv_work_t;
    if (work == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "work is null");
        return;
    }
    InputerHolder *inputerHolder = new (std::nothrow) InputerHolder();
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "inputerHolder is null");
        delete work;
        return;
    }
    inputerHolder->env = env_;
    inputerHolder->inputer = inputer_;
    work->data = reinterpret_cast<void *>(inputerHolder);
    uv_queue_work(loop, work, [] (uv_work_t *work) {}, DeleteData);
}

static napi_status GetInputerInstance(InputerHolder *inputerHolder, napi_value *inputerDataVarCtor)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, GetCtorIInputerData start");
    napi_value cons = GetCtorIInputerData(inputerHolder->env, inputerHolder->inputerData);
    if (cons == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "GetCtorIInputerData failed");
        return napi_generic_failure;
    }
    return napi_new_instance(inputerHolder->env, cons, 0, nullptr, inputerDataVarCtor);
}

static void GetDataWork(uv_work_t* work, int status)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, GetDataWork start");
    InputerHolder *inputerHolder = reinterpret_cast<InputerHolder *>(work->data);
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "inputerHolder is null");
        delete work;
        return;
    }
    napi_value inputerDataVarCtor;
    napi_status napiStatus = GetInputerInstance(inputerHolder, &inputerDataVarCtor);
    if (napiStatus != napi_ok) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "napi_new_instance failed");
        goto EXIT;
    }
    napi_value undefined;
    napiStatus = napi_get_undefined(inputerHolder->env, &undefined);
    if (napiStatus != napi_ok) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "napi_get_undefined failed");
        goto EXIT;
    }
    napi_value return_val;
    napi_value type;
    napiStatus = napi_create_int32(inputerHolder->env, inputerHolder->authSubType, &type);
    if (napiStatus != napi_ok) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "napi_create_int32 failed");
        goto EXIT;
    }
    napi_value argv [PIN_PARAMS_TWO];
    napi_value callbackRef;
    napiStatus = napi_get_reference_value(inputerHolder->env, inputerHolder->inputer, &callbackRef);
    if (napiStatus != napi_ok) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "napi_get_reference_value failed");
        goto EXIT;
    }
    argv [PIN_PARAMS_ZERO] = type;
    argv [PIN_PARAMS_ONE] = inputerDataVarCtor;
    napiStatus = napi_call_function(inputerHolder->env, undefined, callbackRef, PIN_PARAMS_TWO, &argv[0], &return_val);
    if (napiStatus != napi_ok) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "napi_call_function failed");
        goto EXIT;
    }

EXIT:
    delete inputerHolder;
    delete work;
}

void InputerImpl::OnGetData(int32_t authSubType, std::shared_ptr<OHOS::UserIAM::PinAuth::IInputerData> inputerData)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl::OnGetData start");
    uv_loop_s *loop(nullptr);
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "loop is null");
        return;
    }
    uv_work_t *work = new (std::nothrow) uv_work_t;
    if (work == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "work is null");
        return;
    }
    InputerHolder *inputerHolder = new (std::nothrow) InputerHolder();
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "inputerHolder is null");
        delete work;
        return;
    }
    inputerHolder->env = env_;
    inputerHolder->inputer = inputer_;
    inputerHolder->authSubType = authSubType;
    inputerHolder->inputerData = inputerData;
    work->data = reinterpret_cast<void *>(inputerHolder);
    uv_queue_work(loop, work, [] (uv_work_t *work) {}, GetDataWork);
}

napi_value GetCtorIInputerData(napi_env env, std::shared_ptr<OHOS::UserIAM::PinAuth::IInputerData> &inputerData)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, GetCtorIInputerData start");
    if (inputerData == nullptr) {
        PINAUTH_HILOGI(MODULE_JS_NAPI, "GetCtorIInputerData inputerData nullptr");
        return nullptr;
    }
    InputerHolder *inputerHolder = new (std::nothrow) InputerHolder();
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGI(MODULE_JS_NAPI, "GetCtorIInputerData inputerHolder nullptr");
        return nullptr;
    }
    napi_property_descriptor clzDes[] = {
        DECLARE_NAPI_FUNCTION("onSetData", OHOS::PinAuth::OnSetData),
    };
    inputerHolder->inputerData = inputerData;
    napi_value cons;
    NAPI_CALL(env, napi_define_class(env, "InputerData", NAPI_AUTO_LENGTH,
        InputDataConstructor, (void*)inputerHolder,
        sizeof(clzDes) / sizeof(napi_property_descriptor), clzDes, &cons));
    return cons;
}

static napi_value HandleSetData(napi_env env, napi_value *args, size_t argcAsync, InputerHolder *inputerHolder)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, HandleSetData start");
    if (argcAsync != PIN_PARAMS_TWO) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, HandleSetData get bad argcAsync");
        return nullptr;
    }
    napi_valuetype valueType = napi_undefined;
    NAPI_CALL(env, napi_typeof(env, args[PIN_PARAMS_ZERO], &valueType));
    if (valueType != napi_number) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, HandleSetData get valueType error");
        return nullptr;
    }
    int32_t authSubType = VALID_AUTH_SUB_TYPE;
    NAPI_CALL(env, napi_get_value_int32(env, args[PIN_PARAMS_ZERO], &authSubType));
    if (authSubType == VALID_AUTH_SUB_TYPE) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, HandleSetData get authsubtype error");
        return nullptr;
    }
    bool isTypedArray = false;
    NAPI_CALL(env, napi_is_typedarray(env, args[PIN_PARAMS_ONE], &isTypedArray));
    if (!isTypedArray) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, HandleSetData check typed array error");
        return nullptr;
    }
    napi_typedarray_type arrayType = napi_int8_array;
    size_t length = 0;
    napi_value buffer = nullptr;
    size_t offset = 0;
    uint8_t *data = nullptr;
    NAPI_CALL(env, napi_get_typedarray_info(env, args[PIN_PARAMS_ONE], &arrayType, &length,
        reinterpret_cast<void **>(&data), &buffer, &offset));
    if (arrayType != napi_uint8_array) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, HandleSetData bad array type");
        return nullptr;
    }
    if (offset != 0) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, " offset is %{public}zu", offset);
        return nullptr;
    }
    std::vector<uint8_t> result(data, data + length);
    inputerHolder->inputerData->OnSetData(authSubType, result);
    napi_value res = nullptr;
    NAPI_CALL(env, napi_get_null(env, &res));
    return res;
}

napi_value OnSetData(napi_env env, napi_callback_info info)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, OnSetData start");
    size_t argcAsync = PIN_PARAMS_TWO;
    napi_value thisVar = nullptr;
    napi_value args[PIN_PARAMS_TWO] = {nullptr};
    NAPI_CALL(env, napi_get_cb_info(env, info, &argcAsync, args, &thisVar, nullptr));
    if (argcAsync != PIN_PARAMS_TWO) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, OnSetData get bad argcAsync");
        return nullptr;
    }
    InputerHolder *inputerHolder = nullptr;
    NAPI_CALL(env, napi_unwrap(env, thisVar, (void **)&inputerHolder));
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, OnSetData get null object");
        return nullptr;
    }
    if (inputerHolder->inputerData == nullptr) {
        PINAUTH_HILOGE(MODULE_JS_NAPI, "InputerImpl, OnSetData get null inputer");
        return nullptr;
    }
    return HandleSetData(env, args, argcAsync, inputerHolder);
}

napi_value InputDataConstructor(napi_env env, napi_callback_info info)
{
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, InputDataConstructor start");
    napi_value thisVar;
    void *data;
    size_t argcAsync = PIN_PARAMS_ONE;
    napi_value args[PIN_PARAMS_ONE] = {nullptr};
    NAPI_CALL(env, napi_get_cb_info(env, info, &argcAsync, args, &thisVar, &data));
    InputerHolder *inputerHolder = static_cast<InputerHolder *>(data);
    if (thisVar == nullptr) {
        PINAUTH_HILOGI(MODULE_JS_NAPI, "InputDataConstructor thisVar is nullptr");
        return nullptr;
    }
    if (inputerHolder == nullptr) {
        PINAUTH_HILOGI(MODULE_JS_NAPI, "InputDataConstructor inputerData is nullptr");
        return nullptr;
    }
    NAPI_CALL(env, napi_wrap(
        env,
        thisVar,
        inputerHolder,
        [](napi_env env, void *data, void *hint) {
            InputerHolder *inputerHolder = static_cast<InputerHolder *>(data);
            if (inputerHolder != nullptr) {
                delete inputerHolder;
            }
        },
        nullptr, nullptr));
    PINAUTH_HILOGI(MODULE_JS_NAPI, "InputerImpl, InputDataConstructor end");
    return thisVar;
}
} // namespace PinAuth
} // namespace OHOS
