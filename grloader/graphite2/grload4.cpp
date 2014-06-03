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
#include "harfbuzz.h"
#include "SkPaint.h"
#include "FontPlatformData.h"
#include "cutils/log.h"

#define MAKE_TAG(a, b, c, d) ((gr_uint32)((((gr_uint8)(a)<<24)|((gr_uint8)(b)<<16)|(gr_uint8)(c)<<8)|(gr_uint8)(d)))

static gr_uint32 script_tags[] = {
        0                               /* HB_Script_Common */,
        MAKE_TAG('g', 'r', 'e', 'k')    /* HB_Script_Greek */,
        MAKE_TAG('c', 'y', 'r', 'l')    /* HB_Script_Cyrillic */,
        MAKE_TAG('a', 'r', 'm', 'n')    /* HB_Script_Armenian */,
        MAKE_TAG('h', 'e', 'b', 'r')    /* HB_Script_Hebrew */,
        MAKE_TAG('a', 'r', 'a', 'b')    /* HB_Script_Arabic */,
        MAKE_TAG('s', 'y', 'r', 'c')    /* HB_Script_Syriac */,
        MAKE_TAG('t', 'h', 'a', 'a')    /* HB_Script_Thaana */,
        MAKE_TAG('d', 'e', 'v', 'a')    /* HB_Script_Devanagari */,
        MAKE_TAG('b', 'e', 'n', 'g')    /* HB_Script_Bengali */,
        MAKE_TAG('g', 'u', 'r', 'u')    /* HB_Script_Gurmukhi */,
        MAKE_TAG('g', 'u', 'j', 'r')    /* HB_Script_Gujarati */,
        MAKE_TAG('o', 'r', 'y', 'a')    /* HB_Script_Oriya */,
        MAKE_TAG('t', 'a', 'm', 'l')    /* HB_Script_Tamil */,
        MAKE_TAG('t', 'e', 'l', 'u')    /* HB_Script_Telugu */,
        MAKE_TAG('k', 'n', 'd', 'a')    /* HB_Script_Kannada */,
        MAKE_TAG('m', 'l', 'y', 'm')    /* HB_Script_Malayalam */,
        MAKE_TAG('s', 'i', 'n', 'h')    /* HB_Script_Sinhala */,
        MAKE_TAG('t', 'h', 'a', 'i')    /* HB_Script_Thai */,
        MAKE_TAG('l', 'a', 'o', 'o')    /* HB_Script_Lao */,
        MAKE_TAG('t', 'i', 'b', 't')    /* HB_Script_Tibetan */,
        MAKE_TAG('m', 'y', 'm', 'r')    /* HB_Script_Myanmar */,
        MAKE_TAG('g', 'e', 'o', 'r')    /* HB_Script_Georgian */,
        MAKE_TAG('h', 'a', 'n', 'g')    /* HB_Script_Hangul */,
        MAKE_TAG('o', 'g', 'a', 'm')    /* HB_Script_Ogham */,
        MAKE_TAG('r', 'u', 'n', 'r')    /* HB_Script_Runic */,
        MAKE_TAG('k', 'h', 'm', 'r')    /* HB_Script_Khmer */,
        MAKE_TAG('n', 'k', 'o', 'o')    /* HB_Script_Nko */,
        0                               /* HB_Script_Inherited */
};

bool grHBShape(HB_ShaperItem *item, gr_face *face, float size, bool isTextView, gr_feature_val *feats)
{
    int isrtl = item->item.bidiLevel & 1;
    gr_font *font = gr_make_font(size * 64, face);
    gr_segment *seg = gr_make_seg(font, face,
            item->item.script < HB_Script_Inherited ? script_tags[item->item.script] : 0, feats,
            gr_utf16, item->string + item->item.pos, item->item.length, isrtl ? 7 : 0);
	if (gr_seg_n_slots(seg) >= item->num_glyphs)
	{
		item->num_glyphs = gr_seg_n_slots(seg) / 2 + 1;
		return 0;       // send it round again to get more space
	}

    item->num_glyphs = gr_seg_n_slots(seg);
    HB_Glyph *gp = item->glyphs;
    HB_Fixed *ap = item->advances;
    HB_FixedPoint *op = item->offsets;
    unsigned short *cp = item->log_clusters;
    HB_Fixed currx = 0.;
    const gr_slot *is = gr_seg_first_slot(seg);
    
    if (isrtl)
        currx = gr_seg_advance_X(seg);
    for ( is = gr_seg_first_slot(seg); is; is = gr_slot_next_in_segment(is), ++gp, ++ap, ++op, ++cp)
    {
        *gp = gr_slot_gid(is);
        *ap = int(gr_slot_advance_X(is, face, font));
        op->y = int(gr_slot_origin_Y(is)) * (isTextView ? 1 : -1);
        if (isrtl)
        {
            currx -= *ap;
            op->x = int(gr_slot_origin_X(is)) - currx;
        }
        else
        {
            op->x = int(gr_slot_origin_X(is)) - currx;
            currx += *ap;
        }
        if (!isTextView)
            continue;
        //*cp = gr_cinfo_base(gr_seg_cinfo(seg, gr_slot_before(is)));

        if (!gr_slot_can_insert_before(is) && cp > item->log_clusters)
            *cp = cp[-1];
        else
            *cp = gr_cinfo_base(gr_seg_cinfo(seg, gr_slot_after(is)));
    }
    gr_seg_destroy(seg);
    gr_font_destroy(font);
    return 1;
}

extern "C" bool grHB_ShapeItem_tf(HB_ShaperItem *item)
{
    SkPaint *paint = (SkPaint *)(item->font->userData);
    SkTypeface *tf = paint->getTypeface();
    fontmap *f = fm_from_tf(tf);
    //SLOGD("grHB_ShapeItem_tf(%p->%p)", tf, face);
    if (!f || !f->grface) return HB_ShapeItem(item);
    else return grHBShape(item, f->grface, paint->getTextSize(), 1, f->grfeats);
}

extern "C" bool grHB_ShapeItem_pf(HB_ShaperItem *item)
{
    //SLOGD("grHB_ShapeItem_pf");
    FontPlatformData *pf = (FontPlatformData *)(item->font->userData);
    SkTypeface *tf = pf->typeface();
    fontmap *f = fm_from_tf(tf);
    if (!f || !f->grface) return HB_ShapeItem(item);
    else return grHBShape(item, f->grface, (pf->size() > 0 ? pf->size() : 12), 0, f->grfeats);
}

void *grSetupComplexFont(void *t, int script, void *platformData)
{
//    SLOGD("grSetupComplexFont");
    return platformData;
}


func_map harfbuzzmap[] = {
    { "HB_ShapeItem", "grHB_ShapeItem_tf", 0, 0, 0 },
    { "HB_ShapeItem", "grHB_ShapeItem_pf", "libwebcore.so", 0, 0 },
};

extern "C" bool setup_grload4(JNIEnv *env, jobject thiz, int sdkVer, const char *libgrload)
{
    if (load_fns(libgrload, "libharfbuzz.so", harfbuzzmap, 2, sdkVer))
        return true;

    if (sdkVer >=15 && sdkVer <= 18)
    {
        Elf32_Addr aSetupComplexFont;
        if (sdkVer <= 16)
            aSetupComplexFont = findfn("libwebcore.so", "libskia.so", "_ZN10SkTypeface14CreateFromFileEPKc", 1, 1);
        else
            aSetupComplexFont = findfn("libwebcore.so", "libskia.so", "_Z25SkCreateTypefaceForScript9HB_ScriptN10SkTypeface5StyleEN7SkPaint11FontVariantE", 1, 1);
        SLOGD("Found setupComplexFont at %x", aSetupComplexFont);
        if (!aSetupComplexFont || hook_code("libwebcore.so", reinterpret_cast<void *>(grSetupComplexFont), reinterpret_cast<void *>(aSetupComplexFont), sdkVer))
        {
            SLOGD("Hooking failed");
            return true;
        }
    }
    return false;
}

#if (GRLOAD_API == 15)
extern "C" void Java_org_sil_palaso_Graphite_loadGraphite(JNIEnv* env, jobject thiz)
{
    int sdkVer = getSDKVersion(env);
    const char *libgrload = "libgrload4.so";

    if (setup_grandroid(env, thiz, libgrload, sdkVer)) return;
    if (setup_grload4(env, thiz, sdkVer, libgrload)) return;

    // cleanup
    SLOGD("Returning from graphite load");
}
#endif
