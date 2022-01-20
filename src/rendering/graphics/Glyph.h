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

#include "core/Font.h"
#include "core/utils/BytesKey.h"
#include "pag/types.h"
#include "SimpleGlyph.h"

namespace pag {

/**
 * Defines values used in the style property of TextPaint.
 */
enum class TextStyle { Fill, Stroke, StrokeAndFill };

/**
 * Defines attributes for drawing text.
 */
struct TextPaint {
  TextStyle style = TextStyle::Fill;
  Color fillColor = Black;
  Color strokeColor = Black;
  float strokeWidth = 0;
  bool strokeOverFill = true;
  std::string fontFamily;
  std::string fontStyle;
  float fontSize = 24;
  bool fauxBold = false;
  bool fauxItalic = false;
  bool isVertical = false;
};

class Glyph;

typedef std::shared_ptr<Glyph> GlyphHandle;

struct GlyphDocument {
  std::vector<std::shared_ptr<SimpleGlyph>> glyphs;
  TextPaint paint;
};

/**
 * Glyph represents a single character for drawing.
 */
class Glyph {
 public:
  static std::vector<GlyphHandle> BuildFromText(const GlyphDocument* glyphDocument);

  virtual ~Glyph() = default;

  /**
   * Returns the Font object associated with this Glyph.
   */
  Font getFont() const {
    return simpleGlyph->getFont();
  }

  /**
   * Returns the id of this glyph in associated typeface.
   */
  GlyphID getGlyphID() const {
    return simpleGlyph->getGlyphID();
  }

  /**
   * Returns true if this glyph is visible for drawing.
   */
  bool isVisible() const;

  /**
   * Returns true if this glyph is for vertical text layouts.
   */
  bool isVertical() const {
    return _isVertical;
  }

  /**
   * Returns name of this glyph in utf8.
   */
  std::string getName() const {
    return simpleGlyph->getName();
  }

  /**
   * Returns the advance for this glyph.
   */
  float getAdvance() const {
    return advance;
  }

  /**
   * Returns the recommended distance to reserve above baseline
   */
  float getAscent() const {
    return ascent;
  }

  /**
   * Returns the recommended distance to reserve below baseline
   */
  float getDescent() const {
    return descent;
  }

  /**
   * Returns the bounding box relative to (0, 0) of this glyph. Returned bounds may be larger than
   * the exact bounds of this glyph.
   */
  const Rect& getBounds() const {
    return bounds;
  }

  /**
   * Returns the matrix for this glyph.
   */
  Matrix getMatrix() const {
    return matrix;
  }

  /**
   * Replaces transformation with specified matrix.
   */
  void setMatrix(const Matrix& m) {
    matrix = m;
  }

  /**
   * Returns the text style for this glyph.
   */
  TextStyle getStyle() const {
    return textStyle;
  }

  /**
   * Sets the text style for this glyph.
   */
  void setStyle(TextStyle style) {
    textStyle = style;
  }

  /**
   * Returns true if stroke is drawn on top of fill.
   */
  bool getStrokeOverFill() const {
    return strokeOverFill;
  }

  /**
   * Retrieves alpha from the color used when stroking and filling.
   */
  float getAlpha() const {
    return alpha;
  }

  /**
   * Replaces alpha of the color used when stroking and filling, leaving RGB unchanged.
   */
  void setAlpha(float newAlpha) {
    alpha = newAlpha;
  }

  /**
   * Retrieves RGB from the color used when filling.
   */
  Color getFillColor() const {
    return fillColor;
  }

  /**
   * Replaces RGB from the color used when filling.
   */
  void setFillColor(const Color& color) {
    fillColor = color;
  }

  /**
   * Retrieves RGB from the color used when stroking.
   */
  Color getStrokeColor() const {
    return strokeColor;
  }

  /**
   * Replaces RGB from the color used when stroking.
   */
  void setStrokeColor(const Color& color) {
    strokeColor = color;
  }

  /**
   * Returns the thickness of the pen used to outline the glyph.
   */
  float getStrokeWidth() const {
    return strokeWidth;
  }

  /**
   * Replaces the thickness of the pen used to outline the glyph.
   */
  void setStrokeWidth(float width) {
    strokeWidth = width;
  }

  /**
   * Returns the total matrix of this glyph which contains the style matrix.
   */
  Matrix getTotalMatrix() const;

  Matrix getExtraMatrix() const {
    return extraMatrix;
  }

  void computeAtlasKey(BytesKey* bytesKey) const;

 private:
  std::shared_ptr<SimpleGlyph> simpleGlyph;
  // read only attributes:
  float advance = 0;
  float ascent = 0;
  float descent = 0;
  Rect bounds = Rect::MakeEmpty();
  bool _isVertical = false;
  bool strokeOverFill = true;
  Matrix extraMatrix = Matrix::I();  // for vertical text or fauxItalic.
  // writable attributes:
  Matrix matrix = Matrix::I();
  TextStyle textStyle = TextStyle::Fill;
  float alpha = 1.0f;
  Color fillColor = Black;
  Color strokeColor = Black;
  float strokeWidth = 0;

  Glyph(const std::shared_ptr<SimpleGlyph>& simpleGlyph, const TextPaint& textPaint);
};
}  // namespace pag
