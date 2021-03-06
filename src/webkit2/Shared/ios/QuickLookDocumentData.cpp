/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "QuickLookDocumentData.h"

#if USE(QUICK_LOOK)

#include "Decoder.h"
#include "Encoder.h"

namespace WebKit {

void QuickLookDocumentData::append(CFDataRef data)
{
    m_data.append(data);
}

CFDataRef QuickLookDocumentData::decodedData() const
{
    ASSERT(m_data.size() <= 1);
    if (m_data.isEmpty())
        return nullptr;
    return m_data[0].get();
}

void QuickLookDocumentData::clear()
{
    m_data.clear();
}

void QuickLookDocumentData::encode(IPC::Encoder& encoder) const
{
    uint64_t size = 0;
    for (const auto& data : m_data)
        size += CFDataGetLength(data.get());
    encoder << size;

    for (const auto& data : m_data)
        encoder.encodeFixedLengthData(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()), 1);
}

bool QuickLookDocumentData::decode(IPC::Decoder& decoder, QuickLookDocumentData& documentData)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;

    if (!size)
        return true;

    uint8_t* const buffer = static_cast<uint8_t*>(CFAllocatorAllocate(kCFAllocatorDefault, size, 0));
    ASSERT(buffer);

    if (!decoder.decodeFixedLengthData(buffer, size, 1)) {
        CFAllocatorDeallocate(kCFAllocatorDefault, buffer);
        return false;
    }

    documentData.clear();
    documentData.m_data.append(adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer, size, kCFAllocatorDefault)));
    return true;
}

} // namespace WebKit

#endif // USE(QUICK_LOOK)
