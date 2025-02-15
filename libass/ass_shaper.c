/*
 * Copyright (C) 2011 Grigori Goronzy <greg@chown.ath.cx>
 *
 * This file is part of libass.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include "ass_compat.h"

#include "ass_shaper.h"
#include "ass_render.h"
#include "ass_font.h"
#include "ass_parse.h"
#include "ass_cache.h"
#include <limits.h>
#include <stdbool.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#ifdef _WIN32
#include <windows.h>
#include <usp10.h>
#include <vector>
#include <string>

std::vector<std::wstring> fonts_from_ass;
std::vector<std::wstring> legacy_fonts_detected;

void add_font_from_ass(const wchar_t* font_name) {
    fonts_from_ass.push_back(font_name);
}

bool is_legacy_font(HFONT hFont) {
    LOGFONT logfont;
    if (GetObject(hFont, sizeof(LOGFONT), &logfont)) {
        std::wstring font_name = logfont.lfFaceName;
        for (const auto& legacy_font : legacy_fonts_detected) {
            if (font_name == legacy_font) {
                return true;
            }
        }
        HDC hdc = GetDC(NULL);
        SelectObject(hdc, hFont);
        WCHAR test_text[] = L"مرحبا";
        WORD glyphs[10];
        int numChars = wcslen(test_text);
        SCRIPT_CACHE scriptCache = NULL;
        HRESULT hr = ScriptGetCMap(hdc, &scriptCache, test_text, numChars, 0, glyphs);
        ReleaseDC(NULL, hdc);
        ScriptFreeCache(&scriptCache);
        if (SUCCEEDED(hr)) {
            legacy_fonts_detected.push_back(font_name);
            return true;
        }
    }
    return false;
}

bool get_glyphs_using_uniscribe(HDC hdc, HFONT hFont, const wchar_t* text, int numChars, WORD* glyphs) {
    SCRIPT_CACHE scriptCache = NULL;
    SelectObject(hdc, hFont);
    HRESULT hr = ScriptGetCMap(hdc, &scriptCache, text, numChars, 0, glyphs);
    ScriptFreeCache(&scriptCache);
    return SUCCEEDED(hr);
}
#endif

bool ass_shaper_shape(ASS_Shaper *shaper, TextInfo *text_info) {
    int i;
    GlyphInfo *glyphs = text_info->glyphs;
    
    for (i = 0; i < text_info->length; i++) {
        if (glyphs[i].font && glyphs[i].font->desc.family) {
            std::wstring font_name = std::wstring(glyphs[i].font->desc.family, glyphs[i].font->desc.family + strlen(glyphs[i].font->desc.family));
            add_font_from_ass(font_name.c_str());
        }
    }

    bool success = shape_harfbuzz(shaper, glyphs, text_info->length);

#ifdef _WIN32
    if (!success) {
        for (const auto& font_name : fonts_from_ass) {
            HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font_name.c_str());
            if (is_legacy_font(hFont)) {
                HDC hdc = GetDC(NULL);
                WORD glyphs[256];
                if (get_glyphs_using_uniscribe(hdc, hFont, text_info->event_text, text_info->length, glyphs)) {
                    for (int i = 0; i < text_info->length; i++) {
                        glyphs[i].glyph_index = glyphs[i];
                    }
                }
                ReleaseDC(NULL, hdc);
                DeleteObject(hFont);
            }
        }
    }
#endif
    return success;
}
