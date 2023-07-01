/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License") = 0;
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

#ifndef PIN_AUTH_INTERFACE_IPC_INTERFACE_CODE_H
#define PIN_AUTH_INTERFACE_IPC_INTERFACE_CODE_H

/* SAID: 941 */
namespace OHOS {
namespace UserIam {
namespace PinAuth {
enum class PinAuthInterfaceCode {
    REGISTER_INPUTER = 1,
    UNREGISTER_INPUTER = 2,
};
} // namespace PinAuth
} // namespace UserIam
} // namespace OHOS
#endif // PIN_AUTH_INTERFACE_IPC_INTERFACE_CODE_H
