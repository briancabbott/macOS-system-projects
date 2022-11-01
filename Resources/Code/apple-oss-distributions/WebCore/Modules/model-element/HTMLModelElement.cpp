/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLModelElement.h"

#if ENABLE(MODEL_ELEMENT)

#include "ElementChildIterator.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLModelElement);

HTMLModelElement::HTMLModelElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

HTMLModelElement::~HTMLModelElement()
{
}

Ref<HTMLModelElement> HTMLModelElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLModelElement(tagName, document));
}

void HTMLModelElement::sourcesChanged()
{
    if (!document().hasBrowsingContext()) {
        setSourceURL(URL());
        return;
    }

    for (auto& element : childrenOfType<HTMLSourceElement>(*this)) {
        // FIXME: for now we use the first valid URL without looking at the mime-type.
        auto url = element.getNonEmptyURLAttribute(HTMLNames::srcAttr);
        if (url.isValid()) {
            setSourceURL(url);
            return;
        }
    }

    setSourceURL(URL());
}

void HTMLModelElement::setSourceURL(const URL& url)
{
    // FIXME: actually do something with that URL now.
    m_sourceURL = url;
}

void HTMLModelElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    sourcesChanged();
}

}

#endif // ENABLE(MODEL_ELEMENT)
