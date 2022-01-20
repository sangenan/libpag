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

#include "TextAtlas.h"
#include "RenderCache.h"
#include "gpu/Canvas.h"
#include "gpu/Surface.h"
#include "rendering/graphics/Glyph.h"

namespace pag {
struct TextRun {
  Paint paint;
  Font textFont = {};
  std::vector<GlyphID> glyphIDs;
  std::vector<Point> positions;
};

class AtlasGlyph {
 public:
  explicit AtlasGlyph(std::shared_ptr<SimpleGlyph> glyph) : glyph(std::move(glyph)) {
  }

  AtlasGlyph(std::shared_ptr<SimpleGlyph> glyph, float strokeWidth)
      : glyph(std::move(glyph)), _strokeWidth(strokeWidth), _style(PaintStyle::Stroke) {
  }

  void computeStyleKey(BytesKey* styleKey) const;

  void computeAtlasKey(BytesKey* bytesKey) const;

  GlyphID glyphId() const {
    return glyph->getGlyphID();
  }

  Rect bounds() const {
    return glyph->getBounds();
  }

  Font font() const {
    return glyph->getFont();
  }

  PaintStyle style() const {
    return _style;
  }

  float strokeWidth() const {
    return _strokeWidth;
  }

 private:
  std::shared_ptr<SimpleGlyph> glyph;
  float _strokeWidth = 0;
  PaintStyle _style = PaintStyle::Fill;
};

void AtlasGlyph::computeStyleKey(BytesKey* styleKey) const {
  styleKey->write(static_cast<uint32_t>(_style));
  styleKey->write(_strokeWidth);
  styleKey->write(font().getTypeface()->uniqueID());
  styleKey->write(font().getSize());
}

void AtlasGlyph::computeAtlasKey(BytesKey* bytesKey) const {
  glyph->computeAtlasKey(bytesKey);
  bytesKey->write(static_cast<uint32_t>(_style));
  bytesKey->write(_strokeWidth);
}

class Atlas {
 public:
  static std::unique_ptr<Atlas> Make(Context* context, float scale,
                                     const std::vector<AtlasGlyph*>& glyphs, int maxTextureSize,
                                     bool alphaOnly = true);

  ~Atlas();

  bool getLocator(const GlyphHandle& glyph, PaintStyle style, AtlasLocator* locator);

 private:
  struct Page {
    std::vector<TextRun*> textRuns;
    int width = 0;
    int height = 0;
    std::shared_ptr<Texture> texture;
  };

  void initPages(const std::vector<AtlasGlyph*>& glyphs, float scale, int maxTextureSize);

  void draw(Context* context, float scale, bool alphaOnly);

  std::vector<Page> pages;
  std::unordered_map<BytesKey, AtlasLocator, BytesHasher> glyphLocators;

  friend class TextAtlas;
};

class RectanglePack {
 public:
  int width() const {
    return _width;
  }

  int height() const {
    return _height;
  }

  Point addRect(int w, int h) {
    w += Padding;
    h += Padding;
    auto area = (_width - x) * (_height - y);
    if ((x + w - _width) * y > area || (y + h - _height) * x > area) {
      if (_width <= _height) {
        x = _width;
        y = Padding;
        _width += w;
      } else {
        x = Padding;
        y = _height;
        _height += h;
      }
    }
    auto point = Point::Make(static_cast<float>(x), static_cast<float>(y));
    if (x + w - _width < y + h - _height) {
      x += w;
      _height = std::max(_height, y + h);
    } else {
      y += h;
      _width = std::max(_width, x + w);
    }
    return point;
  }

  void reset() {
    _width = Padding;
    _height = Padding;
    x = Padding;
    y = Padding;
  }

 private:
  static constexpr int Padding = 1;
  int _width = Padding;
  int _height = Padding;
  int x = Padding;
  int y = Padding;
};

std::unique_ptr<Atlas> Atlas::Make(Context* context, float scale,
                                   const std::vector<AtlasGlyph*>& glyphs, int maxTextureSize,
                                   bool alphaOnly) {
  if (glyphs.empty()) {
    return nullptr;
  }
  auto atlas = std::make_unique<Atlas>();
  atlas->initPages(glyphs, scale, maxTextureSize);
  atlas->draw(context, scale, alphaOnly);
  return atlas;
}

static std::unique_ptr<TextRun> CreateTextRun(const AtlasGlyph& glyph) {
  auto textRun = std::make_unique<TextRun>();
  textRun->textFont = glyph.font();
  textRun->paint.setStyle(glyph.style());
  if (glyph.style() == PaintStyle::Stroke) {
    textRun->paint.setStrokeWidth(glyph.strokeWidth());
  }
  return textRun;
}

void Atlas::initPages(const std::vector<AtlasGlyph*>& glyphs, float scale, int maxTextureSize) {
  std::vector<BytesKey> styleKeys = {};
  std::unordered_map<BytesKey, std::vector<const AtlasGlyph*>, BytesHasher> styleMap = {};
  for (auto& glyph : glyphs) {
    BytesKey styleKey = {};
    glyph->computeStyleKey(&styleKey);
    auto size = styleMap.size();
    styleMap[styleKey].push_back(glyph);
    if (styleMap.size() != size) {
      styleKeys.push_back(styleKey);
    }
  }
  auto maxPageSize = static_cast<int>(std::floor(static_cast<float>(maxTextureSize) / scale));
  RectanglePack pack;
  Page page;
  int pageIndex = 0;
  for (const auto& key : styleKeys) {
    auto firstGlyph = styleMap[key][0];
    auto textRun = CreateTextRun(*firstGlyph).release();
    for (auto& glyph : styleMap[key]) {
      auto glyphWidth = static_cast<int>(glyph->bounds().width());
      auto glyphHeight = static_cast<int>(glyph->bounds().height());
      int strokeWidth = 0;
      if (glyph->style() == PaintStyle::Stroke) {
        strokeWidth = static_cast<int>(ceil(glyph->strokeWidth()));
      }
      auto x = glyph->bounds().x() - static_cast<float>(strokeWidth);
      auto y = glyph->bounds().y() - static_cast<float>(strokeWidth);
      auto width = glyphWidth + strokeWidth * 2;
      auto height = glyphHeight + strokeWidth * 2;
      auto packWidth = pack.width();
      auto packHeight = pack.height();
      auto point = pack.addRect(width, height);
      if (pack.width() > maxPageSize || pack.height() > maxPageSize) {
        page.textRuns.push_back(textRun);
        page.width = static_cast<int>(ceil(static_cast<float>(packWidth) * scale));
        page.height = static_cast<int>(ceil(static_cast<float>(packHeight) * scale));
        pages.push_back(std::move(page));
        textRun = CreateTextRun(*firstGlyph).release();
        page = {};
        pack.reset();
        point = pack.addRect(width, height);
        pageIndex++;
      }
      textRun->glyphIDs.push_back(glyph->glyphId());
      textRun->positions.push_back({-x + point.x, -y + point.y});
      AtlasLocator locator;
      locator.pageIndex = pageIndex;
      locator.location =
          Rect::MakeXYWH(point.x, point.y, static_cast<float>(width), static_cast<float>(height));
      locator.location.scale(scale, scale);
      BytesKey bytesKey;
      glyph->computeAtlasKey(&bytesKey);
      glyphLocators[bytesKey] = locator;
    }
    page.textRuns.push_back(textRun);
  }
  page.width = static_cast<int>(ceil(static_cast<float>(pack.width()) * scale));
  page.height = static_cast<int>(ceil(static_cast<float>(pack.height()) * scale));
  pages.push_back(std::move(page));
}

void DrawTextRun(Canvas* canvas, const std::vector<TextRun*>& textRuns, float scale) {
  auto totalMatrix = canvas->getMatrix();
  for (auto& textRun : textRuns) {
    canvas->setMatrix(totalMatrix);
    canvas->concat(Matrix::MakeScale(scale));
    auto glyphs = &textRun->glyphIDs[0];
    auto positions = &textRun->positions[0];
    canvas->drawGlyphs(glyphs, positions, textRun->glyphIDs.size(), textRun->textFont,
                       textRun->paint);
  }
  canvas->setMatrix(totalMatrix);
}

void Atlas::draw(Context* context, float scale, bool alphaOnly) {
  for (auto& page : pages) {
    auto surface = Surface::Make(context, page.width, page.height, alphaOnly);
    auto atlasCanvas = surface->getCanvas();
    DrawTextRun(atlasCanvas, page.textRuns, scale);
    page.texture = surface->getTexture();
  }
}

static void ComputeAtlasKey(const Glyph* glyph, PaintStyle style, BytesKey* atlasKey) {
  glyph->computeAtlasKey(atlasKey);
  atlasKey->write(static_cast<uint32_t>(style));
  atlasKey->write(style == PaintStyle::Fill ? 0 : glyph->getStrokeWidth());
}

bool Atlas::getLocator(const GlyphHandle& glyph, PaintStyle style, AtlasLocator* locator) {
  BytesKey bytesKey;
  ComputeAtlasKey(glyph.get(), style, &bytesKey);
  auto iter = glyphLocators.find(bytesKey);
  if (iter == glyphLocators.end()) {
    return false;
  }
  if (locator) {
    *locator = iter->second;
  }
  return true;
}

Atlas::~Atlas() {
  for (auto& page : pages) {
    for (auto* textRun : page.textRuns) {
      delete textRun;
    }
  }
}

static TextPaint CreateTextPaint(const TextDocument* textDocument) {
  TextPaint textPaint = {};
  if (textDocument->applyFill && textDocument->applyStroke) {
    textPaint.style = TextStyle::StrokeAndFill;
  } else if (textDocument->applyStroke) {
    textPaint.style = TextStyle::Stroke;
  } else {
    textPaint.style = TextStyle::Fill;
  }
  textPaint.fillColor = textDocument->fillColor;
  textPaint.strokeColor = textDocument->strokeColor;
  textPaint.strokeWidth = textDocument->strokeWidth;
  textPaint.strokeOverFill = textDocument->strokeOverFill;
  textPaint.fontFamily = textDocument->fontFamily;
  textPaint.fontStyle = textDocument->fontStyle;
  textPaint.fontSize = textDocument->fontSize;
  textPaint.fauxBold = textDocument->fauxBold;
  textPaint.fauxItalic = textDocument->fauxItalic;
  textPaint.isVertical = textDocument->direction == TextDirection::Vertical;
  return textPaint;
}

std::shared_ptr<GlyphDocument> CreateGlyphDocument(const TextDocument* textDocument) {
  auto glyphDocument = std::make_shared<GlyphDocument>();
  glyphDocument->glyphs = GetSimpleGlyphs(textDocument);
  glyphDocument->paint = CreateTextPaint(textDocument);
  return glyphDocument;
}

std::unique_ptr<TextAtlas> TextAtlas::Make(ID assetID, Property<TextDocumentHandle>* sourceText,
                                           std::vector<TextAnimator*>*) {
  std::unordered_map<const TextDocument*, std::shared_ptr<GlyphDocument>> glyphs;
  if (sourceText->animatable()) {
    auto animatableProperty = reinterpret_cast<AnimatableProperty<TextDocumentHandle>*>(sourceText);
    auto textDocument = animatableProperty->keyframes[0]->startValue.get();
    glyphs[textDocument] = CreateGlyphDocument(textDocument);
    for (const auto& keyframe : animatableProperty->keyframes) {
      textDocument = keyframe->endValue.get();
      glyphs[textDocument] = CreateGlyphDocument(textDocument);
    }
  } else {
    auto textDocument = sourceText->getValueAt(0).get();
    glyphs[textDocument] = CreateGlyphDocument(textDocument);
  }
  if (glyphs.empty()) {
    return nullptr;
  }
  return std::unique_ptr<TextAtlas>(new TextAtlas(assetID, std::move(glyphs)));
}

static void SortAtlasGlyphs(std::vector<AtlasGlyph*>* glyphs) {
  if (glyphs->empty()) {
    return;
  }
  std::sort(glyphs->begin(), glyphs->end(), [](const AtlasGlyph* a, const AtlasGlyph* b) -> bool {
    auto aWidth = a->bounds().width();
    auto aHeight = a->bounds().height();
    auto bWidth = b->bounds().width();
    auto bHeight = b->bounds().height();
    return aWidth * aHeight > bWidth * bHeight || aWidth > bWidth || aHeight > bHeight;
  });
}

TextAtlas::TextAtlas(ID assetID,
                     std::unordered_map<const TextDocument*, std::shared_ptr<GlyphDocument>> glyphs)
    : assetID(assetID), glyphs(std::move(glyphs)) {
  initAtlasGlyphs();
}

void TextAtlas::initAtlasGlyphs() {
  std::vector<BytesKey> atlasKeys;
  for (auto& pair : glyphs) {
    auto& paint = pair.second->paint;
    for (auto& glyph : pair.second->glyphs) {
      auto hasColor = glyph->getFont().getTypeface()->hasColor();
      auto& atlasGlyphs = hasColor ? colorGlyphs : maskGlyphs;
      if (!hasColor) {
        if (paint.style == TextStyle::Stroke || paint.style == TextStyle::StrokeAndFill) {
          auto atlasGlyph = std::make_unique<AtlasGlyph>(glyph, paint.strokeWidth);
          BytesKey atlasKey;
          atlasGlyph->computeAtlasKey(&atlasKey);
          if (std::find(atlasKeys.begin(), atlasKeys.end(), atlasKey) == atlasKeys.end()) {
            atlasGlyphs.push_back(atlasGlyph.release());
            atlasKeys.push_back(atlasKey);
          }
        }
      }
      if (paint.style == TextStyle::Fill || paint.style == TextStyle::StrokeAndFill) {
        auto atlasGlyph = std::make_unique<AtlasGlyph>(glyph);
        BytesKey atlasKey;
        atlasGlyph->computeAtlasKey(&atlasKey);
        if (std::find(atlasKeys.begin(), atlasKeys.end(), atlasKey) == atlasKeys.end()) {
          atlasGlyphs.push_back(atlasGlyph.release());
          atlasKeys.push_back(atlasKey);
        }
      }
    }
  }
  SortAtlasGlyphs(&maskGlyphs);
  SortAtlasGlyphs(&colorGlyphs);
}

TextAtlas::~TextAtlas() {
  for (const auto* glyph : maskGlyphs) {
    delete glyph;
  }
  for (const auto* glyph : colorGlyphs) {
    delete glyph;
  }
  delete maskAtlas;
  delete colorAtlas;
}

bool TextAtlas::getLocator(const GlyphHandle& glyph, PaintStyle style, AtlasLocator* locator) {
  if (glyph->getFont().getTypeface()->hasColor()) {
    return colorAtlas && colorAtlas->getLocator(glyph, style, locator);
  }
  return maskAtlas && maskAtlas->getLocator(glyph, style, locator);
}

std::shared_ptr<Texture> TextAtlas::getMaskAtlasTexture(size_t pageIndex) const {
  if (maskAtlas && maskAtlas->pages.size() > pageIndex) {
    return maskAtlas->pages[pageIndex].texture;
  }
  return nullptr;
}

std::shared_ptr<Texture> TextAtlas::getColorAtlasTexture(size_t pageIndex) const {
  if (colorAtlas && colorAtlas->pages.size() > pageIndex) {
    return colorAtlas->pages[pageIndex].texture;
  }
  return nullptr;
}

std::shared_ptr<GlyphDocument> TextAtlas::getGlyphDocument(const TextDocument* textDocument) const {
  auto iter = glyphs.find(textDocument);
  if (iter == glyphs.end()) {
    return nullptr;
  }
  return iter->second;
}

void TextAtlas::generateIfNeeded(Context* context, RenderCache* renderCache) {
  auto scale = renderCache->getAssetMaxScale(assetID);
  bool scaleChanged = std::abs(_scale - scale) > 0.01f;
  auto maxTextureSize = context->caps()->maxTextureSize;
  if (maskAtlas == nullptr || scaleChanged) {
    delete maskAtlas;
    maskAtlas = Atlas::Make(context, scale, maskGlyphs, maxTextureSize).release();
  }
  if (colorAtlas == nullptr || scaleChanged) {
    delete colorAtlas;
    colorAtlas = Atlas::Make(context, scale, colorGlyphs, maxTextureSize, false).release();
  }
  _scale = scale;
}
}  // namespace pag
