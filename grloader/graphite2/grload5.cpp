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
#include "SkPaint.h"
#include "FontPlatformData.h"
#include "cutils/log.h"
#include "_arm.h"
#include "hb.h"
#include "hb-ot.h"
#include "hb-font-private.hh"
#include "hb-buffer-private.hh"

struct hb_graphite2_cluster_t {
  unsigned int base_char;
  unsigned int num_chars;
  unsigned int base_glyph;
  unsigned int num_glyphs;
  unsigned int cluster;
};

static void *myScratch = NULL;
static int myScratch_size = 0;

static void grensure(size_t num)
{
    if (myScratch_size < num)
    {
        myScratch = realloc(myScratch, num);
        myScratch_size = num;
    }
}

extern "C" void grhbng_shape(hb_font_t *font, hb_buffer_t *buffer, const hb_feature_t *features, unsigned int num_features)
{
    //SLOGD("grhbng_shape(font=%p, buffer=%p. face=%p, user_data=%p", font, buffer, font->face, font->face->user_data);
    gr_face *grface = gr_face_from_tf((SkTypeface *)(font->face->user_data), NULL);
    if (!grface)
        return hb_shape(font, buffer, features, num_features);
    gr_font *grfont = gr_make_font(font->x_scale, grface);
    
    gr_segment *seg = NULL;
    const gr_slot *is;
    unsigned int ci = 0, ic = 0;
    float curradvx = 0., curradvy = 0.;

    grensure(buffer->len * sizeof(uint32_t));
    unsigned int scratch_size = myScratch_size;
    char *scratch = (char *)myScratch;

#define ALLOCATE_ARRAY(Type, name, len) \
    Type *name = (Type *) scratch; \
    scratch += (len) * sizeof ((name)[0]); \
    scratch_size -= (len) * sizeof ((name)[0]);

    ALLOCATE_ARRAY (uint32_t, chars, buffer->len);

    for (unsigned int i = 0; i < buffer->len; ++i)
        chars[i] = buffer->info[i].codepoint;

    hb_tag_t script_tag[2];
    hb_ot_tags_from_script (hb_buffer_get_script (buffer), &script_tag[0], &script_tag[1]);

    seg = gr_make_seg (grfont, grface,
		     script_tag[1] == HB_TAG_NONE ? script_tag[0] : script_tag[1],
		     NULL,
		     gr_utf32, chars, buffer->len,
		     2 | (hb_buffer_get_direction (buffer) == HB_DIRECTION_RTL ? 1 : 0));

    if (!seg) return;

    unsigned int glyph_count = gr_seg_n_slots (seg);
    if (!glyph_count) {
        gr_seg_destroy (seg);
        return;
    }

    grensure(sizeof (hb_graphite2_cluster_t) * buffer->len + sizeof (hb_codepoint_t) * glyph_count);
    scratch  = (char *)myScratch;
    scratch_size = myScratch_size;

    ALLOCATE_ARRAY (hb_graphite2_cluster_t, clusters, buffer->len);
    ALLOCATE_ARRAY (hb_codepoint_t, gids, glyph_count);

    memset (clusters, 0, sizeof (clusters[0]) * buffer->len);

    clusters[0].cluster = buffer->info[0].cluster;
    hb_codepoint_t *pg = gids;
    for (is = gr_seg_first_slot (seg), ic = 0; is; is = gr_slot_next_in_segment (is), ic++)
    {
        unsigned int before = gr_slot_before (is);
        unsigned int after = gr_slot_after (is);
        *pg = gr_slot_gid (is);
        pg++;
        while (clusters[ci].base_char > before && ci)
        {
            clusters[ci-1].num_chars += clusters[ci].num_chars;
            clusters[ci-1].num_glyphs += clusters[ci].num_glyphs;
            ci--;
        }

        if (gr_slot_can_insert_before (is) && clusters[ci].num_chars && before >= clusters[ci].base_char + clusters[ci].num_chars)
        {
            hb_graphite2_cluster_t *c = clusters + ci + 1;
            c->base_char = clusters[ci].base_char + clusters[ci].num_chars;
            c->cluster = buffer->info[c->base_char].cluster;
            c->num_chars = before - c->base_char;
            c->base_glyph = ic;
            c->num_glyphs = 0;
            ci++;
        }
        clusters[ci].num_glyphs++;

        if (clusters[ci].base_char + clusters[ci].num_chars < after + 1)
	    clusters[ci].num_chars = after + 1 - clusters[ci].base_char;
    }
    ci++;

    //buffer->clear_output ();
    for (unsigned int i = 0; i < ci; ++i)
    {
        for (unsigned int j = 0; j < clusters[i].num_glyphs; ++j)
        {
            hb_glyph_info_t *info = &buffer->info[clusters[i].base_glyph + j];
            info->codepoint = gids[clusters[i].base_glyph + j];
            info->cluster = clusters[i].cluster;
        }
    }
    buffer->len = glyph_count;
    //buffer->swap_buffers ();

    if (HB_DIRECTION_IS_BACKWARD(buffer->props.direction))
        curradvx = gr_seg_advance_X(seg);

    hb_glyph_position_t *pPos;
    for (pPos = hb_buffer_get_glyph_positions (buffer, NULL), is = gr_seg_first_slot (seg);
           is; pPos++, is = gr_slot_next_in_segment (is))
    {
        pPos->x_offset = gr_slot_origin_X (is) - curradvx;
        pPos->y_offset = gr_slot_origin_Y (is) - curradvy;
        pPos->x_advance = gr_slot_advance_X (is, grface, grfont);
        pPos->y_advance = gr_slot_advance_Y (is, grface, grfont);
        if (HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
            curradvx -= pPos->x_advance;
        pPos->x_offset = gr_slot_origin_X (is) - curradvx;
        if (!HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
            curradvx += pPos->x_advance;
        pPos->y_offset = gr_slot_origin_Y (is) - curradvy;
        curradvy += pPos->y_advance;
    }
    if (!HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
        pPos[-1].x_advance += gr_seg_advance_X(seg) - curradvx;

    if (HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
        hb_buffer_reverse_clusters (buffer);

    gr_seg_destroy (seg);
}

#if (GRLOAD_API > 18)
unsigned short *find4calls(unsigned short *curr)
{
    int count = 0;
    unsigned short *res = NULL;
    while (!ARM_INSTR_IS_RETURN(curr))
    {
        if (ARM_INSTR_IS_CALL(curr))
        {
            ++count;
            if (count == 1)
                res = curr;
            else if (count > 3)
                return res;
        }
        else if (arm_instr_is_jump(curr))
        {
            count = 0;
            res = NULL;
        }
        curr = ARM_INSTR_NEXT(curr);
    }
    return NULL;
}

Elf32_Addr findsymbol(const char *targetname, Elf32_Addr srcfn)
{
    soinfo *soTarget = (soinfo *)dlopen(targetname, 0);
    unsigned *gotaddr = got_addr(soTarget, srcfn);
    SLOGD("Found got addr %p", gotaddr);
    if (!gotaddr) return 0;
    unsigned *pltaddr = plt_addr_arm(soTarget, gotaddr);
    SLOGD("Found pltaddr %p", pltaddr);
    if (!pltaddr) return 0;
    Elf32_Addr callsite = scan_call_arm(soTarget, reinterpret_cast<Elf32_Addr>(pltaddr));
    SLOGD("Found callsite %x", callsite);
    unsigned short *callloc = find4calls(reinterpret_cast<unsigned short *>(callsite));
    SLOGD("Found caller at %p", callloc);
    return get_calladdr_arm(callloc);
}   
#endif

func_map harfbuzzngmap[] = {
    { "hb_shape", "grhbng_shape", 0, 0, 0 },
};

#if (GRLOAD_API == 18)
extern "C" bool setup_grload4(JNIEnv *env, jobject thiz, int sdkVer, const char *libgrload);
#endif

extern "C" bool setup_grload5(JNIEnv *env, jobject thiz, int sdkVer, const char *libgrload)
{
    if (load_fns(libgrload, "libharfbuzz_ng.so", harfbuzzngmap, 1, sdkVer))
        return true;
}

extern "C" void Java_org_sil_palaso_Graphite_loadGraphite(JNIEnv* env, jobject thiz)
{
    int sdkVer = getSDKVersion(env);
    const char *libgrload = "libgrload5.so";

    if (setup_grandroid(env, thiz, libgrload, sdkVer)) return;
#if (GRLOAD_API == 18)
    if (setup_grload4(env, thiz, sdkVer, libgrload)) return;
#endif
    if (setup_grload5(env, thiz, sdkVer, libgrload)) return;

#if (GRLOAD_API > 18)
    Elf32_Addr hbshape_addr;
    Elf32_Addr lowerfn;
    soinfo *iculib = (soinfo *)dlopen("libicuuc.so", 0);
    if (!iculib)
    {
        SLOGD("Can't load libicuuc.so");
        return;
    }
    char name[] = "u_islower_xx";
    for (int i = 50; i < 54; ++i)
    {
        sprintf(name+10, "%d", i);
        lowerfn = reinterpret_cast<Elf32_Addr>(dlsym(iculib, name));
        if (lowerfn)
            break;
    }
    if (!lowerfn)
    {
        SLOGD("Can't find %s in libicuuc.so", name);
        return;
    }
    hbshape_addr = findsymbol("libwebviewchromium.so", lowerfn);
    if (!hbshape_addr)
    {
        SLOGD("Can't find hb_shape in libwebviewchromium.so");
        return;
    }
    else
        SLOGD("I think hb_shape is at %x", hbshape_addr);
    if (hook_code("libwebviewchromium.so", reinterpret_cast<void *>(grhbng_shape), reinterpret_cast<void *>(hbshape_addr), sdkVer))
    {
        SLOGD("Hooking hb_shape to grhbng_shape in libwebviewchromium.so failed");
        return;
    }
#endif

    // cleanup
    SLOGD("Returning from graphite load");
}

