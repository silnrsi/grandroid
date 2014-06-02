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
#include "SkStream.h"
#include "SkTypeface.h"
//#include <androidfw/AssetManager.h>
//#include <android_runtime/android_util_AssetManager.h>
#include <android/asset_manager_jni.h>
#include "cutils/log.h"
#include "_linker.h"
#include <ctype.h>

class FontPlatformData;

static fontmap *myfonts = NULL;
static SkTypeface *tf_from_name(const char *name)
{
    fontmap *f;
    for (f = myfonts; f; f = f->next)
    {
//        SLOGD("Testing cache %s against %s", f->name, name);
        if (f->name && !strcmp(f->name, name))
            return f->tf;
    }
    return NULL;
}

extern "C" SkTypeface *grCreateFromName(const char name[], SkTypeface::Style style)
{
    SkTypeface *res = NULL;
    if (name)
        res = tf_from_name(name);
    SLOGD("Created %s from name as %p", name, res);
    if (!res)
        res = SkTypeface::CreateFromName(name, style);
    else
        res->ref();     // emulate ownership increase
    return res;
}

fontmap *fm_from_tf(SkTypeface *tf)
{
    fontmap *f;
    for (f = myfonts; f; f = f->next)
    {
        if (f->tf == tf)
            return f;
    }
    return NULL;
}

static void const *fr_gettable(const void *dat, unsigned int tag, size_t *len)
{
    FT_Face face = (FT_Face)dat;
    unsigned long length = 0;
    void *res;

    FT_Load_Sfnt_Table(face, tag, 0, NULL, &length);
    res = malloc(length);
    if (res)
    	FT_Load_Sfnt_Table(face, tag, 0, (FT_Byte *)(res), &length);
    if (len) *len = length;
    return res;
}

void fr_freetable(const void *dat, void const *buff)
{
    free(const_cast<void *>(buff));
}

static gr_face_ops ops = {sizeof(gr_face_ops), fr_gettable, fr_freetable};
static FT_Library gFTLibrary = NULL;

#define HB_TAG(c1,c2,c3,c4) ((unsigned int)((((uint8_t)(c1))<<24)|(((uint8_t)(c2))<<16)|(((uint8_t)(c3))<<8)|((uint8_t)(c4))))

inline unsigned int gettag(const char *s)
{ return HB_TAG(s[0], s[1], s[2], s[3]); }

static unsigned int jgettag(JNIEnv *env, jstring jtag)
{
    const char *tagstr = env->GetStringUTFChars(jtag, NULL);
    unsigned int res = gettag(tagstr);
    env->ReleaseStringUTFChars(jtag, tagstr);
    return res;
}

static unsigned int parse_tag(const char * &start, const char *send, int &def)
{
    unsigned int res = 0;
    const char *s;
    def = 1;
    while (start < send)
    {
        if (*start == '+')
        {
            ++start;
            break;
        }
        else if (isalnum(*start))
            break;
        else if (*start == '-')
        {
            def = 0;
            ++start;
            break;
        }
        ++start;
    }
    if (start >= send)
        return 0;

    bool isnum = true;
    for (s = start; s < send; ++s)
    {
        if (isalpha(*s))
            isnum = false;
        else if (!isdigit(*s))
            break;
    }
    if (s == start)
        return 0;

    if (isnum)
    {
        res = strtoul(s, const_cast<char **>(&start), 10);
    }
    else
    {
        int i;
        for (i = 0; i < 4 && start < s; ++start, ++i)
            res = (res << 8) | *start;
        res = res << ((4 - i) * 8);
        start = s;
    }
    return res;
}

static int parse_val(const char * &start, const char *send, int &def)
{
    int res = def;
    bool isneg = false;
    const char *s;
    while (start < send)
    {
        if (isdigit(*start))
        {
            s = start;
            break;
        }
        else if (*start == '+' || *start == '-')
        {
            s = start;
            ++start;
            break;
        }
        ++start;
    }
    if (start >= send)
        return 0;
    if (!isdigit(*start))   // don't consume a following +tag
    {
        --start;
        return 0;
    }
    res = strtol(s, const_cast<char **>(&start), 10);
    return res;
}

static void parse_features(gr_face *face, gr_feature_val *feats, JNIEnv *env, jstring jfeats)
{
    const char *featstr = env->GetStringUTFChars(jfeats, NULL);
    int len = env->GetStringUTFLength(jfeats);
    unsigned int tag;
    int val;
    const char *s = featstr;
    const char *send = featstr + len;
    while (s < send)
    {
        tag = parse_tag(s, send, val);
        if (tag)
        {
            const gr_feature_ref *fref;
            val = parse_val(s, send, val);
            fref = gr_face_find_fref(face, tag);
            if (fref)
                gr_fref_set_feature_value(fref, val, feats);
        }
    }
    env->ReleaseStringUTFChars(jfeats, featstr);
}
        

extern "C" jobject Java_org_sil_palaso_Graphite_addFontResource( JNIEnv *env, jobject thiz, jobject jassetMgr, jstring jpath, jstring jname, jint rtl, jstring lang, jstring feats )
{
    AAssetManager *mgr = AAssetManager_fromJava(env, jassetMgr);
    if (mgr == NULL) return 0;
    
    const char *str = env->GetStringUTFChars(jpath, NULL);
    AAsset *asset = AAssetManager_open(mgr, str, AASSET_MODE_BUFFER);
    SkMemoryStream *aStream = new SkMemoryStream(AAsset_getBuffer(asset), AAsset_getLength(asset), false);
    SLOGD("asset(%p) size=%d, vtable(%p)", asset, AAsset_getLength(asset), *(void **)aStream);
    SkTypeface * tf = SkTypeface::CreateFromStream(aStream);

    if (aStream == NULL || tf == NULL)
    {
        SLOGD("Failed to load stream and make a typeface");
        return 0;
    }
    //aStream->unref();
    jclass c = env->FindClass("android/graphics/Typeface");
    jmethodID cid = env->GetMethodID(c, "<init>", "(I)V");
    jobject res = env->NewObject(c, cid, (int)(void *)(tf));

    const char * tname = env->GetStringUTFChars(jname, NULL);
    char *name = (char *)malloc(strlen(tname) + 1);
    memcpy(name, tname, strlen(tname) + 1);
    env->ReleaseStringUTFChars(jname, tname);
    SLOGD("Adding font name %s tf(%p)", name, tf);
    FT_Face face;
    fontmap *f = new fontmap;
    f->next = myfonts;
    f->tf = tf;
    f->asset = asset;
    f->name = name;
    f->rtl = rtl ? 7 : 0;
    if (!gFTLibrary && FT_Init_FreeType(&gFTLibrary))
    {
        //delete f->tf;
        SLOGD("Can't load freetype");
        delete f;
        return 0;
    }
    FT_Open_Args    args;
    args.flags = FT_OPEN_MEMORY;
    args.memory_base = (FT_Byte *)(AAsset_getBuffer(asset));
    args.memory_size = AAsset_getLength(asset);
    FT_Error err = FT_Open_Face(gFTLibrary, &args, 0, &face);
    aStream->unref();
    if (err)
    {
        SLOGD("freetype failed to load face base = %p, size = %d", args.memory_base, args.memory_size);
        //delete f->tf;
        delete f;
        return 0;
    }
    f->grface = gr_make_face_with_ops(face, &ops, gr_face_preloadAll);
    f->ftface = face;
    // can't free the face since it's owned by grface now, and face needs asset
    if (!f->grface) SLOGD("Failed to create graphite font");

    // Now handle lang and feats
    if (env->GetStringLength(lang))
        f->grfeats = gr_face_featureval_for_lang(f->grface, jgettag(env, lang));
    else
        f->grfeats = gr_face_featureval_for_lang(f->grface, 0);

    if (env->GetStringLength(feats))
        parse_features(f->grface, f->grfeats, env, feats);
    
    myfonts = f;
#if 0
    if (rtl)
    {
        SkTypeface *tfw = SkTypeface::CreateFromStream(aStream);
        if (tfw)
        {
            fontmap *fw = new fontmap;
            fw->next = myfonts;
            fw->tf = tfw;
            fw->name = name;
            fw->asset = NULL;       // only one thing needs to own it
            fw->rtl = 1;
            fw->grface = f->grface;
            fw->ftface = NULL;      // only need one owner
            myfonts = fw;
        }
    }
#endif
//    return (int)(void *)(f->tf);
    return res;
}

int getSDKVersion(JNIEnv *env)
{
    bool success = true;

    // VERSION is a nested class within android.os.Build (hence "$" rather than "/")
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (NULL == versionClass)
        success = false;

    jfieldID sdkIntFieldID = NULL;
    if (success)
        success = (NULL != (sdkIntFieldID = env->GetStaticFieldID(versionClass, "SDK_INT", "I")));

    jint sdkInt = 0;
    if (success)
        sdkInt = env->GetStaticIntField(versionClass, sdkIntFieldID);
    env->DeleteLocalRef(versionClass);
    return sdkInt;
}

func_map typefacemap[] = {
    // SkTypeface::CreateFromName                        
    //{ "_ZN10SkTypeface14CreateFromNameEPKcNS_5StyleE", "_Z16grCreateFromNamePvPKcN10SkTypeface5StyleE", 0, 0, 0 },
    { "_ZN10SkTypeface14CreateFromNameEPKcNS_5StyleE", "grCreateFromName", 0, 0, 0 },
};

bool setup_grandroid(JNIEnv* env, jobject thiz, const char *libgrload, int sdkVer)
{
    if (myfonts) return true;
    return load_fns(libgrload, "libskia.so", typefacemap, 1, sdkVer);
}
