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

#ifndef PIN_AUTH_STUB_H
#define PIN_AUTH_STUB_H

#include "nocopyable.h"
#include "iremote_pinauth.h"
#include "iremote_stub.h"

namespace OHOS {
namespace UserIam {
namespace PinAuth {
class PinAuthStub : public IRemoteStub<IRemotePinAuth> {
public:
    DISALLOW_COPY_AND_MOVE(PinAuthStub);
    PinAuthStub();
    ~PinAuthStub() override;
    int32_t OnRemoteRequest(
        uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

private:
    void HandlerRegisterInputer(MessageParcel &data, MessageParcel &reply);
    void HandlerUnRegisterInputer(MessageParcel &data, MessageParcel &reply);
};
}  // namespace PinAuth
}  // namespace UserIam
}  // namespace OHOS

#endif  // PIN_AUTH_STUB_H