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

#ifndef IINPUTER_DATA_STUB_H
#define IINPUTER_DATA_STUB_H

#include "nocopyable.h"
#include "iremote_inputer.h"
#include "iremote_stub.h"

namespace OHOS {
namespace UserIAM {
namespace PinAuth {
class IInputerDataStub : public IRemoteStub<IRemoteInputerData> {
public:
    DISALLOW_COPY_AND_MOVE(IInputerDataStub);
    IInputerDataStub() = default;
    ~IInputerDataStub() override = default;
    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

private:
    void HandlerOnSetData(MessageParcel &data, MessageParcel &reply);
};
} // namespace PinAuth
} // namespace UserIAM
} // namespace OHOS

#endif  // IINPUTER_DATA_STUB_H