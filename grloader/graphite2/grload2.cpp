/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

    Alternatively, you may use this library under the terms of the Mozilla
    Public License (http://mozilla.org/MPL) or under the GNU General Public
    License, as published by the Free Sofware Foundation; either version
    2 of the license or (at your option) any later version.
*/

#include "grandroid.h"
#include "graphite2/Segment.h"
#include <dlfcn.h>
#include "_linker.h"
#include "SkScalar.h"
#include "SkPaint.h"
#include "SkDraw.h"
#include "SkGlyphCache.h"
#include "SkBitmapProcShader.h"
#include "SkBlitter.h"
#include "SkDrawProcs.h"
#include "cutils/log.h"

class SkFaceRec;

// copied from skia/src/core/SkDraw.cpp
#define kStdStrikeThru_Offset       (-SK_Scalar1 * 6 / 21)
#define kStdUnderline_Offset        (SK_Scalar1 / 9)
#define kStdUnderline_Thickness     (SK_Scalar1 / 18)

static void draw_paint_rect(const SkDraw* draw, const SkPaint& paint,
                            const SkRect& r, SkScalar textSize) {
    if (paint.getStyle() == SkPaint::kFill_Style) {
        draw->drawRect(r, paint);
    } else {
        SkPaint p(paint);
        p.setStrokeWidth(SkScalarMul(textSize, paint.getStrokeWidth()));
        draw->drawRect(r, p);
    }
}

static void handle_aftertext(const SkDraw* draw, const SkPaint& paint,
                             SkScalar width, const SkPoint& start) {
    uint32_t flags = paint.getFlags();

    if (flags & (SkPaint::kUnderlineText_Flag |
                 SkPaint::kStrikeThruText_Flag)) {
        SkScalar textSize = paint.getTextSize();
        SkScalar height = SkScalarMul(textSize, kStdUnderline_Thickness);
        SkRect   r;

        r.fLeft = start.fX;
        r.fRight = start.fX + width;

        if (flags & SkPaint::kUnderlineText_Flag) {
            SkScalar offset = SkScalarMulAdd(textSize, kStdUnderline_Offset,
                                             start.fY);
            r.fTop = offset;
            r.fBottom = offset + height;
            draw_paint_rect(draw, paint, r, textSize);
        }
        if (flags & SkPaint::kStrikeThruText_Flag) {
            SkScalar offset = SkScalarMulAdd(textSize, kStdStrikeThru_Offset,
                                             start.fY);
            r.fTop = offset;
            r.fBottom = offset + height;
            draw_paint_rect(draw, paint, r, textSize);
        }
    }
}

// hack to deal with suprious SkToU8 in SkPaint.h. This should never get called.
// unsigned char SkToU8(unsigned int x)
// { return 32; }

const char *preproctext(const char *text, size_t bytelen, gr_encform enctype, int rtl)
{
    const char *ptext = text;
    if (rtl == 3)
    {
        ptext = (const char *)malloc(bytelen);
        if (!ptext) ptext = text;
        else
        {
            for (const char *p = text, *q = ptext + bytelen; q > ptext; p += enctype, q -= enctype)
            { memmove((void *)(q - enctype), (void *)p, enctype); }
        }
    }
    return ptext;
}

// mangled name: _ZNK8mySkDraw8drawTextEPKcjffRK7SkPaint 
extern "C" void grDrawText(SkDraw *t, const char *text, size_t bytelen, SkScalar x, SkScalar y, const SkPaint& paint)
{
    int rtl = 0;
    fontmap *f = fm_from_tf(paint.getTypeface());
    gr_encform enctype = gr_encform(paint.getTextEncoding() + 1);
    if ((t->fMatrix->getType() & SkMatrix::kPerspective_Mask) || enctype > 2 || !f || !f->grface)
    {
        t->drawText(text, bytelen, x, y, paint);
        return;
    }

    if (text == NULL || bytelen == 0 || t->fClip->isEmpty() ||
            (paint.getAlpha() == 0 && paint.getXfermode() == NULL))
        return;

    gr_font *font = gr_make_font(paint.getTextSize(), f->grface); // textsize in pixels
    if (!font) return;

    const char *ptext = preproctext(text, bytelen, enctype, rtl);
    size_t numchar = gr_count_unicode_characters(enctype, ptext, ptext + bytelen, NULL);
    gr_segment *seg = gr_make_seg(font, f->grface, 0, f->grfeats, enctype, ptext, numchar, f->rtl);
    if (!seg)
    {
        gr_font_destroy(font);
        return;
    }

    SkScalar underlineWidth = 0;
    SkPoint  underlineStart;
    SkPoint  segWidth;

    t->fMatrix->mapXY(gr_seg_advance_X(seg), gr_seg_advance_Y(seg), &segWidth);
    underlineStart.set(0, 0);
    if (paint.getFlags() & (SkPaint::kUnderlineText_Flag | SkPaint::kStrikeThruText_Flag))
    {
        underlineWidth = segWidth.fX;
        SkScalar offsetX = 0;
        if (paint.getTextAlign() == SkPaint::kCenter_Align)
            offsetX = SkScalarHalf(underlineWidth);
        else if (paint.getTextAlign() == SkPaint::kRight_Align)
            offsetX = underlineWidth;
        underlineStart.set(x - offsetX, y);
    }

    SkAutoGlyphCache    autoCache(paint, t->fMatrix);
    SkGlyphCache*       cache = autoCache.getCache();
    unsigned int bitmapstore[sizeof(SkBitmapProcShader) >> 2];
    SkBlitter*  blitter = SkBlitter::Choose(*t->fBitmap, *t->fMatrix, paint,
                                bitmapstore, sizeof(bitmapstore));

    if (paint.getTextAlign() == SkPaint::kRight_Align)
    {
        x -= segWidth.fX;
        y -= segWidth.fY;       // do we top align too?
    }
    else if (paint.getTextAlign() == SkPaint::kCenter_Align)
    {
        x -= SkScalarHalf(segWidth.fX);
        y -= SkScalarHalf(segWidth.fY);
    }

    SkDraw1Glyph        dlg;
    SkDraw1Glyph::Proc  proc = dlg.init(t, blitter, cache);
    const gr_slot *s;
    for (s = gr_seg_first_slot(seg); s; s = gr_slot_next_in_segment(s))
    {
        SkPoint pos;
        t->fMatrix->mapXY(gr_slot_origin_X(s) + x, -gr_slot_origin_Y(s) + y, &pos);
        const SkGlyph& glyph = cache->getGlyphIDMetrics(gr_slot_gid(s));
        if (glyph.fWidth)
            proc(dlg, glyph, SkFixedFloor(SkScalarToFixed(pos.fX)), SkFixedFloor(SkScalarToFixed(pos.fY)));
    }

    if (underlineWidth)
    {
        autoCache.release();
        handle_aftertext(t, paint, underlineWidth, underlineStart);
    }
    gr_seg_destroy(seg);
    gr_font_destroy(font);
}

extern "C" SkScalar grMeasureText(SkPaint *t, const void* textData, size_t length, SkRect *bounds, SkScalar zoom)
{
    int rtl = 0;
    const char* text = (const char *)textData;
    fontmap *f = fm_from_tf(t->getTypeface());
    gr_encform enctype = gr_encform(t->getTextEncoding() + 1);
    if (enctype > 2 || !f || !f->grface)
        return t->measureText(textData, length, bounds, zoom);
    gr_font *font = gr_make_font(zoom ? SkScalarMul(t->getTextSize(), zoom) : t->getTextSize(), f->grface);
    if (!font) return 0;

//    const char *ptext = preproctext(text, length, enctype, rtl);
    size_t numchar = gr_count_unicode_characters(enctype, text, text + length, NULL);
    gr_segment *seg = gr_make_seg(font, f->grface, 0, f->grfeats, enctype, text, numchar, f->rtl ? 1 : 0);
    if (!seg)
    {
        gr_font_destroy(font);
        return 0;
    }
    SkScalar width = gr_seg_advance_X(seg);
    if (bounds)
    {
        bounds->fLeft = 0;
        bounds->fBottom = 0;
        bounds->fRight = width;
        bounds->fTop = gr_seg_advance_Y(seg);
    }
    gr_seg_destroy(seg);
    gr_font_destroy(font);
    return width;
}

extern "C" int grGetTextWidths(SkPaint *t, const void* textData, size_t byteLength, SkScalar widths[], SkRect bounds[])
{
    int rtl = 0;
    const char* text = (const char *)textData;
    fontmap *f = fm_from_tf(t->getTypeface());
    gr_encform enctype = gr_encform(t->getTextEncoding() + 1);
    if (enctype > 2 || !f || !f->grface)
        return t->getTextWidths(textData, byteLength, widths, bounds);
    gr_font *font = gr_make_font(t->getTextSize(), f->grface);
    if (!font) return 0;

//    const char *ptext = preproctext(text, byteLength, enctype, rtl);
    size_t numchar = gr_count_unicode_characters(enctype, text, text + byteLength, NULL);
    gr_segment *seg = gr_make_seg(font, f->grface, 0, f->grfeats, enctype, text, numchar, f->rtl > 0 ? 1 : 0);
    if (!seg)
    {
        gr_font_destroy(font);
        return 0;
    }
    float width = 0;
    if (rtl) width = gr_seg_advance_X(seg);
    for (int i = 0; i < numchar; ++i)
    {
        const gr_char_info *c = gr_seg_cinfo(seg, i);
        int a = gr_cinfo_after(c);
        int b = gr_cinfo_before(c);
        const gr_slot *s;
        const gr_slot *as = NULL, *bs = NULL;
        for (s = gr_seg_first_slot(seg); s; s = gr_slot_next_in_segment(s))
        {
            if (gr_slot_index(s) == a)
            {
                as = s;
                if (bs) break;
            }
            else if (gr_slot_index(s) == b)
            {
                bs = s;
                if (as) break;
            }
        }
        *widths = (rtl ? -1 : 1) * ((as ? gr_slot_origin_X(as) : (rtl ? 0 : gr_seg_advance_X(seg))) - width);
        if (bounds)
        {
            bounds->fLeft = rtl ? *widths + width : width;
            bounds->fRight = rtl ? width : *widths + width;
            bounds->fBottom = 0;
            bounds->fTop = 0;
            ++bounds;
        }
        if (rtl)
            width -= *widths;
        else
            width += *widths;
        ++widths;
    }
    return numchar;
}

func_map skiamap[] = {
    // SkDraw::DrawText,                            mySkDraw::DrawText
    { "_ZNK6SkDraw8drawTextEPKcjffRK7SkPaint",      "grDrawText", 0, 0, reinterpret_cast<void *>(grDrawText) },
    // SkPaint::measureText                         mySkPaint::measureText
    { "_ZNK7SkPaint11measureTextEPKvjP6SkRectf",    "grMeasureText", 0, 0, reinterpret_cast<void *>(grMeasureText) },
    // SkPaint::getTextWidths                       mySkPaint::getTextWidths
    { "_ZNK7SkPaint13getTextWidthsEPKvjPfP6SkRect", "grGetTextWidths", 0, 0, reinterpret_cast<void *>(grGetTextWidths) },
    { 0, 0, 0, 0, 0 }
};

extern "C" bool setup_grload2(JNIEnv *env, jobject thiz, int sdkVer, const char *libgrload)
{
    if (load_fns(libgrload, "libskia.so", skiamap, 3, sdkVer))
        return true;

    func_map *fn;
    for (fn = skiamap; fn->starget != NULL; ++fn)
    {
        Elf32_Addr aRef = findfn("libskia.so", "libskia.so", fn->starget, 1, 1);
        if (!aRef || hook_code("libskia.so", fn->psrc, reinterpret_cast<void *>(aRef), sdkVer))
        {
            SLOGD("Hooking %s->%s failed", fn->starget, fn->ssrc);
            return true;
        }
    }
    return false;
}

#if (GRLOAD_API == 10)
extern "C" void Java_org_sil_palaso_Graphite_loadGraphite(JNIEnv* env, jobject thiz)
{
    int sdkVer = 10;
    const char *libgrload = "libgrload2.so";

    SLOGD("Loading in SDK version %d", sdkVer);
    if (setup_grandroid(env, thiz, libgrload, sdkVer)) return;
    SLOGD("Now version 2 stuff");
    if (setup_grload2(env, thiz, sdkVer, libgrload)) return;

    // cleanup
    SLOGD("Returning from graphite load");
}
#endif
