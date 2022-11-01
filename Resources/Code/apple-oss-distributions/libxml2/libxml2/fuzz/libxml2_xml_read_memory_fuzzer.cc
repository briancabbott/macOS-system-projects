// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <functional>
#include <limits>
#include <string>

#include "libxml/parser.h"
#include "libxml/xmlsave.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static void ignore (void* ctx, const char* msg, ...) {
  // Error handler to avoid spam of error messages from libxml parser.
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  xmlSetGenericErrorFunc(NULL, &ignore);

  // Test default empty options value and some random combination.
  std::string data_string(reinterpret_cast<const char*>(data), size);
  const std::size_t data_hash = std::hash<std::string>()(data_string);
  const int max_option_value = std::numeric_limits<int>::max();
  const int random_option_value = data_hash % max_option_value;
  const int options[] = {0, random_option_value};

  for (const auto option_value : options) {
    size_t data_string_length = data_string.length();
    assert(data_string_length < std::numeric_limits<int>::max());
    if (auto doc = xmlReadMemory(data_string.c_str(), static_cast<int>(data_string_length),
                                 "noname.xml", NULL, option_value | XML_PARSE_NONET)) {
      auto buf = xmlBufferCreate();
      assert(buf);
      auto ctxt = xmlSaveToBuffer(buf, NULL, 0);
      xmlSaveDoc(ctxt, doc);
      xmlSaveClose(ctxt);
      xmlFreeDoc(doc);
      xmlBufferFree(buf);
    }
  }

  return 0;
}
