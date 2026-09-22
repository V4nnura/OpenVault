#ifndef FALLOUT_PLIB_GNW_GRBUF_H_
#define FALLOUT_PLIB_GNW_GRBUF_H_

namespace fallout {

void draw_line(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void draw_box(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void draw_shaded_box(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor);
void cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void trans_cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void trans_buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void mask_buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* mask, int maskPitch, unsigned char* dest, int destPitch);
// [value] is an 8-bit fill byte; in most callers this is a palette color index.
void buf_fill(unsigned char* buf, int width, int height, int pitch, int value);
void buf_texture(unsigned char* buf, int width, int height, int pitch, void* texture, int xOffset, int yOffset);
void lighten_buf(unsigned char* buf, int width, int height, int pitch);
void swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2);
void buf_outline(unsigned char* buf, int width, int height, int pitch, int color);
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_GRBUF_H_ */
