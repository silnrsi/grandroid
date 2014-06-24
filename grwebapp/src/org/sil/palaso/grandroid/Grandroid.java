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

package org.sil.palaso.grandroid;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.Bundle;
import android.util.Log;
import android.util.Base64;
import java.io.UnsupportedEncodingException;
import android.webkit.WebView;
import android.widget.TextView;
import org.sil.palaso.Graphite;

public class Grandroid extends Activity {

    Typeface mtfp;
    Typeface mtfs;
    Typeface mtfl;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Graphite.loadGraphite();
    	mtfp = (Typeface)Graphite.addFontResource(getAssets(), "Padauk.ttf", "padauk", 0, "shn", "wtri=1; ulon=1");
        mtfs = (Typeface)Graphite.addFontResource(getAssets(), "Scheherazade-R.ttf", "scher", 1, "", "");
        mtfl = (Typeface)Graphite.addFontResource(getAssets(), "CharisSILAfr-R.ttf", "charis", 0, "", "");
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onResume() {
        super.onResume();
    	WebView wv;
        TextView tv1;
        TextView tv2;
        TextView tv3;
        String s = "မိူဝ်ႈ/ၵေႃႇၸႃႉ/ႁူဝ်ပိုင်း/ၼၼ်ႉ၊/မီး/ၼုၵ်ႈၵပၢတ်ႈ/ၸဝ်ႈ/ဢေႃႈ။ ၼုၵ်ႈၵပၢတ်ႈၸဝ်ႈ/ၼၼ်ႉ၊/မီး/လူၺ်ႈ/ၵၼ်/တင်း/ၽြႃးပဵၼ်ၸဝ်ႈ/ လႄႈ/ၼုၵ်ႈၵပၢတ်ႈ/ၸဝ်ႈ/ၼၼ်ႉ၊/ပဵၼ်/ၽြႃးပဵၼ်ၸဝ်ႈ/ဢေႃႈ။- မၼ်းၸဝ်ႈ/ၼၼ်ႉ၊/မီး/ယူႇ/ၸဵမ်/မိူဝ်ႈ/ႁူဝ်/ပိုင်း/ႁူမ်ႈ/လူၺ်ႈ/ၵၼ်/တင်း/ ၽြႃးပဵၼ်ၸဝ်ႈ/လႄႈ၊- ".replace("/", "\u200B");
//    	String s = "မဂင်္ဂလာ|မဘ္ဘာ၊ ဤကဲ့|သို့|ရာ|ဇ|ဝင်|တင်|မည့် ကြေ|ညာ|ချက်|ကို ပြု|လုပ်|ပြီး|နောက် ဤညီ|လာ|ခံ|အ|စည်း|အ|ဝေး|ကြီး|က ကမ္ဘာ့|ကု|လ|သ|မဂ္ဂ|အ|ဖွဲ့|ဝင် နိုင်|ငံ အား|လုံး|အား ထို|ကြေ|ညာ|စာ|တမ်း|ကြီး၏ စာ|သား|ကို|အ|များ|ပြည်|သူ|တို့ ကြား|သိ|စေ|ရန် ကြေ|ညာ|ပါ|မည့် အ|ကြောင်း|ကို|လည်း|ကောင်း၊ ထို့|ပြင်|နိုင်|ငံ|များ၊ သို့|တည်း|မ|ဟုတ် နယ်|မြေ|များ၏ နိုင်|ငံ|ရေး အ|ဆင့်|အ|တ|န်း|ကို လိုက်၍ ခွဲ|ခြား|ခြင်း မ|ပြု|ဘဲ|အ|ဓိ|က|အား|ဖြင့် စာ|သင်|ကျောင်း|များ|နှင့် အ|ခြား|ပ|ညာ|ရေး အ|ဖွဲ့|အ|စည်း|များ|တွင် ထို|ကြေ|ညာ|စာ|တမ်း|ကြီး|ကို ဖြန့်|ချိ ဝေ|ငှ စေ|ရန်၊ မြင်|သာ|အောင် ပြ|သ|ထား|စေ|ရန်၊|ဖတ်|ကြား|စေ|ရန်|နှင့် အ|ဓိပ္ပါယ်|ရှင်း|လင်း ဖော်|ပြ|စေ|ရန် ဆောင်|ရွက်|ပါ|မည့် အ|ကြောင်း|ဖြင့် လည်း|ကောင်း ဆင့်|ဆို လိုက်|သည်။".replace("|", "\u200B");
    	String sa = "لمّا كان الاعتراف بالكرامة المتأصلة في جميع أعضاء الأسرة (البشرية) وبحقوقهم المتساوية\u221A الثابتة هو أساس الحرية والعدل \u06F1\u06F2\u06F3 والسلام في العالم.";
        //String sl = "\u00F0\u0259 k\u02B0\u00E6t\u02B0 s\u00E6\u0301t\u02B0 o\u0303\u0300\u014A mi\u0302\u02D0";
        String sl = "Grandroid says: Hello World!";
        String w = "\uFEFF<html><head/><body><p style=\"font-family: padauk\">" + s + "</p><p style=\"font-family: scher\">" + sa + "</p><p style=\"font-family: charis\">" + sl + "</p></body></html>";                   // <3>
     
        setContentView(R.layout.main);
        wv = (WebView) findViewById(R.id.wv);
        try {
            wv.loadData(Base64.encodeToString(w.getBytes("UTF-8"), 0), "text/html", "base64");
        }
        catch (UnsupportedEncodingException e) {
            Log.e("Grandroid", "Can't handle UTF-8, java is so broken");
        }
        tv1 = (TextView) findViewById(R.id.tv1);
        tv1.setText(s);
        tv1.setTypeface(mtfp, 0);
        tv1.setTextSize((float)16.0);
        tv2 = (TextView) findViewById(R.id.tv2);
        tv2.setText(sa);
        tv2.setTypeface(mtfs, 0);
        tv2.setTextSize((float)16.0);
        tv3 = (TextView) findViewById(R.id.tv3);
        tv3.setText(sl);
        tv3.setTypeface(mtfl, 0);
        tv3.setTextSize((float)16.0);
    }
}
