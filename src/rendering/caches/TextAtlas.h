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

#include <unordered_map>
#include "gpu/Paint.h"
#include "pag/file.h"
#include "pag/types.h"
#include "rendering/graphics/Glyph.h"

namespace pag {
class RenderCache;
class AtlasGlyph;
class Atlas;

struct AtlasLocator {
  size_t pageIndex = 0;
  Rect location = Rect::MakeEmpty();
};

std::shared_ptr<GlyphDocument> CreateGlyphDocument(const TextDocument* textDocument);

class TextAtlas {
 public:
  static std::unique_ptr<TextAtlas> Make(ID assetID, Property<TextDocumentHandle>* sourceText,
                                         std::vector<TextAnimator*>* animators);

  ~TextAtlas();

  bool getLocator(const GlyphHandle& glyph, PaintStyle style, AtlasLocator* locator);

  float getScale() const {
    return _scale;
  }

  std::shared_ptr<Texture> getMaskAtlasTexture(size_t pageIndex) const;

  std::shared_ptr<Texture> getColorAtlasTexture(size_t pageIndex) const;

  void generateIfNeeded(Context* context, RenderCache* renderCache);

  std::shared_ptr<GlyphDocument> getGlyphDocument(const TextDocument* textDocument) const;

 private:
  TextAtlas(ID assetID,
            std::unordered_map<const TextDocument*, std::shared_ptr<GlyphDocument>> glyphs);

  void initAtlasGlyphs();

  ID assetID = 0;
  std::unordered_map<const TextDocument*, std::shared_ptr<GlyphDocument>> glyphs;
  float _scale = 1.0f;
  std::vector<AtlasGlyph*> maskGlyphs;
  std::vector<AtlasGlyph*> colorGlyphs;
  Atlas* maskAtlas = nullptr;
  Atlas* colorAtlas = nullptr;
};
}  // namespace pag
