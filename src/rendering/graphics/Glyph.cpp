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

#include "Glyph.h"
#include "base/utils/UTF8Text.h"
#include "rendering/FontManager.h"

namespace pag {
std::vector<GlyphHandle> Glyph::BuildFromText(const GlyphDocument* glyphDocument) {
  std::vector<GlyphHandle> glyphList;
  for (const auto& glyph : glyphDocument->glyphs) {
    glyphList.emplace_back(std::shared_ptr<Glyph>(new Glyph(glyph, glyphDocument->paint)));
  }
  return glyphList;
}

Glyph::Glyph(const std::shared_ptr<SimpleGlyph>& simpleGlyph, const TextPaint& textPaint)
    : simpleGlyph(simpleGlyph) {
  _isVertical = textPaint.isVertical;
  textStyle = textPaint.style;
  strokeOverFill = textPaint.strokeOverFill;
  fillColor = textPaint.fillColor;
  strokeColor = textPaint.strokeColor;
  strokeWidth = textPaint.strokeWidth;
  auto textFont = simpleGlyph->getFont();
  auto metrics = textFont.getMetrics();
  ascent = metrics.ascent;
  descent = metrics.descent;
  auto glyphId = simpleGlyph->getGlyphID();
  advance = textFont.getGlyphAdvance(glyphId);
  bounds = simpleGlyph->getBounds();
  auto name = simpleGlyph->getName();
  if (name == " ") {
    // 空格字符测量的 bounds 比较异常偏上，本身也不可见，这里直接按字幕 A 的上下边界调整一下。
    auto AGlyphID = textFont.getGlyphID("A");
    if (AGlyphID > 0) {
      auto ABounds = textFont.getGlyphBounds(AGlyphID);
      bounds.top = ABounds.top;
      bounds.bottom = ABounds.bottom;
    }
  }
  if (textPaint.isVertical) {
    if (name.size() == 1) {
      // 字母，数字，标点等字符旋转 90° 绘制，原先的水平 baseline 转为垂直 baseline，
      // 并水平向左偏移半个大写字母高度。
      extraMatrix.setRotate(90);
      auto offsetX = (metrics.capHeight + metrics.xHeight) * 0.25f;
      extraMatrix.postTranslate(-offsetX, 0);
      ascent += offsetX;
      descent += offsetX;
    } else {
      auto offset = textFont.getGlyphVerticalOffset(glyphId);
      extraMatrix.postTranslate(offset.x, offset.y);
      auto width = advance;
      advance = textFont.getGlyphAdvance(glyphId, true);
      ascent = -width * 0.5f;
      descent = width * 0.5f;
    }
    extraMatrix.mapRect(&bounds);
  }
}

bool Glyph::isVisible() const {
  return matrix.invertible() && alpha != 0.0f && !bounds.isEmpty();
}

Matrix Glyph::getTotalMatrix() const {
  auto m = extraMatrix;
  m.postConcat(matrix);
  return m;
}

void Glyph::computeAtlasKey(BytesKey* bytesKey) const {
  simpleGlyph->computeAtlasKey(bytesKey);
}
}  // namespace pag
