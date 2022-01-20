/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "SimpleGlyph.h"
#include "FontManager.h"
#include "base/utils/UTF8Text.h"

namespace pag {
Rect SimpleGlyph::getBounds() {
  if (_bounds.isEmpty()) {
    _bounds = _font.getGlyphBounds(_glyphId);
  }
  return _bounds;
}

void SimpleGlyph::computeAtlasKey(BytesKey* bytesKey) const {
  auto flags = static_cast<uint32_t>(_glyphId);
  if (_font.isFauxBold()) {
    flags |= 1 << 16;
  }
  if (_font.isFauxItalic()) {
    flags |= 1 << 17;
  }
  bytesKey->write(flags);
  bytesKey->write(_font.getTypeface()->uniqueID());
}

std::vector<std::shared_ptr<SimpleGlyph>> GetSimpleGlyphs(const TextDocument* textDocument) {
  Font textFont = {};
  textFont.setFauxBold(textDocument->fauxBold);
  textFont.setFauxItalic(textDocument->fauxItalic);
  textFont.setSize(textDocument->fontSize);
  auto typeface =
      FontManager::GetTypefaceWithoutFallback(textDocument->fontFamily, textDocument->fontStyle);
  bool hasTypeface = typeface != nullptr;
  std::unordered_map<std::string, std::shared_ptr<SimpleGlyph>> glyphMap;
  std::vector<std::shared_ptr<SimpleGlyph>> glyphList;
  const char* textStart = &(textDocument->text[0]);
  const char* textStop = textStart + textDocument->text.size();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    UTF8Text::NextChar(&textStart);
    auto length = textStart - oldPosition;
    auto name = std::string(oldPosition, length);
    if (glyphMap.find(name) != glyphMap.end()) {
      glyphList.push_back(glyphMap[name]);
      continue;
    }
    GlyphID glyphId = 0;
    if (hasTypeface) {
      glyphId = typeface->getGlyphID(name);
      if (glyphId != 0) {
        textFont.setTypeface(typeface);
      }
    }
    if (glyphId == 0) {
      auto fallbackTypeface = FontManager::GetFallbackTypeface(name, &glyphId);
      textFont.setTypeface(fallbackTypeface);
    }
    auto glyph = std::make_shared<SimpleGlyph>(glyphId, name, textFont);
    glyphMap[name] = glyph;
    glyphList.push_back(glyph);
  }
  return glyphList;
}
}  // namespace pag
