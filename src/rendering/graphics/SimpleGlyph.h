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

#pragma once

#include "pag/types.h"
#include "core/Font.h"

namespace pag {
class SimpleGlyph {
 public:
  SimpleGlyph(GlyphID glyphId, std::string name, Font font)
      : _glyphId(glyphId), _name(std::move(name)), _font(std::move(font)) {
  }

  GlyphID getGlyphID() const {
    return _glyphId;
  }

  std::string getName() const {
    return _name;
  }

  Font getFont() const {
    return _font;
  }

  Rect getBounds();

  void computeAtlasKey(BytesKey* bytesKey) const;

 private:
  GlyphID _glyphId;
  std::string _name;
  Font _font;
  Rect _bounds = Rect::MakeEmpty();
};

std::vector<std::shared_ptr<SimpleGlyph>> GetSimpleGlyphs(const TextDocument* textDocument);
}  // namespace pag
