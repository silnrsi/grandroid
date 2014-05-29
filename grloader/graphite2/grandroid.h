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
#ifndef GRANDROID_H
#define GRANDROID_H

#include "SkTypeface.h"
#include "graphite2/Font.h"
#include <android/asset_manager.h>
#include "ft2build.h"
#include <jni.h>

#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

typedef struct fontmap {
    struct fontmap *next;
    const char *name;
    AAsset *asset;
    SkTypeface *tf;
    gr_face *grface;
    FT_Face ftface;
    int rtl;
} fontmap;

extern "C" {
SkTypeface *grCreateFromName(const char name[], SkTypeface::Style style);
gr_face *gr_face_from_tf(SkTypeface *tf, const char *name);
int getSDKVersion(JNIEnv *env);
bool setup_grandroid(JNIEnv* env, jobject thiz, const char *libgrload, int sdkVer);
}
#endif
