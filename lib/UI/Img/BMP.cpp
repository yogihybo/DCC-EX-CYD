#include <Img/BMP.h>

BMP::BMP(fs::File bmp) : ImgHandler(bmp) { }

BMP::BMP(fs::File bmp, TFT_eSPI* tft, int16_t x, int16_t y) : BMP(bmp) {
  draw(tft, x, y);
}

uint16_t BMP::read16() {
  uint16_t result;
  _img.read((uint8_t*)&result, sizeof(result));
  return result;
}

uint32_t BMP::read32() {
  uint32_t result;
  _img.read((uint8_t*)&result, sizeof(result));
  return result;
}

bool BMP::dimensions(uint32_t& w, uint32_t& h) {
  if (read16() == 0x4D42) {
    _img.seek(18);
    w = read32();
    h = read32();
    _img.seek(0);
    return true;
  }
  return false;
}

bool BMP::draw(TFT_eSPI* tft, int16_t x, int16_t y, int16_t target_w, int16_t target_h) {
  if ((x >= tft->width()) || (y >= tft->height())) {
    return false;
  }

  uint32_t seekOffset;
  uint16_t w, h, row;
  uint8_t  r, g, b;

  if (read16() == 0x4D42) {
    read32();
    read32();
    seekOffset = read32();
    read32();
    w = read32();
    h = read32();

    if (read16() == 1 && read16() == 24 && read32() == 0) {
      if (target_w == -1) target_w = w;
      if (target_h == -1) target_h = h;

      y += target_h - 1;

      bool oldSwapBytes = tft->getSwapBytes();
      tft->setSwapBytes(true);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];
      uint16_t outLine[target_w];

      int32_t h_ratio = (h << 8) / target_h;
      int32_t w_ratio = (w << 8) / target_w;

      for (int16_t trow = 0; trow < target_h; trow++) {
        uint16_t src_row = (trow * h_ratio) >> 8;
        if (src_row >= h) src_row = h - 1;
        
        _img.seek(seekOffset + (src_row * (w * 3 + padding)));
        _img.read(lineBuffer, sizeof(lineBuffer));

        for (int16_t tcol = 0; tcol < target_w; tcol++) {
          uint16_t src_col = (tcol * w_ratio) >> 8;
          if (src_col >= w) src_col = w - 1;
          
          uint16_t idx = src_col * 3;
          b = lineBuffer[idx];
          g = lineBuffer[idx + 1];
          r = lineBuffer[idx + 2];
          outLine[tcol] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        tft->pushImage(x, y--, target_w, 1, outLine);
      }
      tft->setSwapBytes(oldSwapBytes);
    } else {
      return false;
    }
  }

  return true;
}
