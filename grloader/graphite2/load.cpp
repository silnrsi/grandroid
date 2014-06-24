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

#include <dlfcn.h>
#include "_linker.h"
#include <linux/elf.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include "cutils/log.h"
//#include "linker_phdr.h"

#ifdef ANDROID_ARM_LINKER
#include "_arm.h"
#endif

//typedef void (SkDraw::*drawTextFn)(const char *, size_t, SkScalar, SkScalar, const SkPaint&) const;

static pthread_mutex_t dl_lock = PTHREAD_MUTEX_INITIALIZER;

//drawTextFn pDrawText = &SkDraw::drawText;
void reloc_fns(soinfo *si, Elf32_Rel *rel, unsigned count, func_map *map, unsigned num_map, Elf32_Addr bias);
#ifdef ANDROID_SH_LINKER
void reloca_fns(soinfo *si, Elf32_Rela *rela, unsigned count, func_map *map, unsigned num_map, Elf32_Addr bias);
#endif

#define MAYBE_MAP_FLAG(x,from,to)    (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)            (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
                                      MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
                                      MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))
#define PAGE_START(x)  ((x) & ~PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x)    PAGE_START((x) + (PAGE_SIZE-1))

static int
phdr_table_set_load_prot(const Elf32_Phdr* phdr_table,
                          int               phdr_count,
                          Elf32_Addr        load_bias,
                          int               extra_prot_flags)
{
    const Elf32_Phdr* phdr = phdr_table;
    const Elf32_Phdr* phdr_limit = phdr + phdr_count;
    int res = 0;

    for (; phdr < phdr_limit; phdr++) {
        // It seems android-18 needs everything turned on (I think they write protect the GOT)
        if (phdr->p_type != PT_LOAD)    // only interested in protection of loaded sections
            continue;

        Elf32_Addr seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
        Elf32_Addr seg_page_end   = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;
//        SLOGD("Trying to unprotect %x + %x (%x=%x), PAGE_SIZE = %d, PAGE_MASK = %x, bias = %x, sp_start = %x, sp_end = %x", phdr->p_vaddr + load_bias, phdr->p_memsz, phdr->p_type, phdr->p_flags, PAGE_SIZE, PAGE_MASK, load_bias, seg_page_start, seg_page_end);

        int prots = PFLAGS_TO_PROT(phdr->p_flags) | extra_prot_flags;
        int ret = mprotect((void*)seg_page_start,
                           seg_page_end - seg_page_start,
                           prots);
        if (ret < 0) {
            res = -1;
            SLOGD("Failed to unprotect (%s) %x->%x [%x + %x, +%x]V%d", strerror(errno), seg_page_start, seg_page_end, phdr->p_vaddr, load_bias, phdr->p_memsz, prots);
        } else {
//            SLOGD("Unprotected %x->%x", seg_page_start, seg_page_end);
        }
    }
    return res;
}

bool load_fns(const char *srcname, const char *targetname, func_map *map, int num_map, int sdkVer)
{
    //SLOGD("load_fns %s -> %s", srcname, targetname);
    soinfo *soHead = (soinfo *)dlopen("libdl.so", 0);
    //SLOGD("Found libdl.so at %p (base = %p)", soHead->base);
    soinfo *soTarget = (soinfo *)dlopen(targetname, 0);
    SLOGD("Found %s at %p (base = %p)", targetname, soTarget, soTarget->base);
    soinfo *soSrc = (soinfo *)dlopen(srcname, 0);
    SLOGD("Found %s at %p (base = %p)", srcname, soSrc, soSrc->base);
    soinfo *si;
    int i, j;

    //SLOGD("Mapping from %s to %s", srcname, targetname);
    // turn names into pointers
    for (i = 0; i < num_map; ++i)
    {
//  See "ELF for the Arm Architecture" sect. 4.6.3, p 21. But we don't strip because that's Thumb code's job
        map[i].ptarget = dlsym(soTarget, map[i].starget);
        if (!map[i].ptarget)
        {
            SLOGD("Failed to find %s in %s", map[i].starget, targetname);
            continue;
        }
        map[i].psrc = dlsym(soSrc, map[i].ssrc);
        if (!map[i].psrc)
        {
            SLOGD("Failed to find %s in %s", map[i].ssrc, srcname);
            continue;
        }
    }
    
    pthread_mutex_lock(&dl_lock);
    // fixup referencing libraries
    for (si = soHead; si; si = si->next)
    {
        unsigned *d;
        // don't redirect ourselves, that could cause nasty loops
        if (si == soSrc)
            continue;
        if (si->dynamic)
        {
            Elf32_Addr bias = sdkVer > 16 ? si->load_bias : si->base;
            for (d = si->dynamic; *d; d +=2)
            {
                if (d[0] != DT_NEEDED) continue;
                soinfo *sid;
                if (sdkVer > 16)
                {
                    const char *c = si->strtab + d[1];
                    for (sid = soHead; sid; sid = sid->next)
                        if (!strcmp(c, sid->name))
                            break;
                    if (!sid)
                        continue;
                }
                else
                    sid = (soinfo *)d[1];
                if (!strcmp(sid->name, targetname))
                {
                    if (phdr_table_set_load_prot(si->phdr, si->phnum, si->base, PROT_WRITE))
                        continue;
                    //SLOGD("Relocating in %s base(%p) load_bias(%p)", si->name, si->base, si->load_bias);
//                    ((void **)d)[1] = soSrc;
                    if (si->plt_rel)
                        reloc_fns(si, si->plt_rel, si->plt_rel_count, map, num_map, bias);
                    if (si->rel)
                        reloc_fns(si, si->rel, si->rel_count, map, num_map, bias);
    #ifdef ANDROID_SH_LINKER
                    if (si->plt_rela)
                        reloca_fns(si, si->plt_rela, si->plt_rela_count, map, num_map, bias);
                    if (si->rela)
                        reloca_fns(si, si->rela, si->rela_count, map, num_map, bias);
    #endif
//                    soSrc->refcount++;
//                    if (soTarget->refcount > 0) soTarget->refcount--;
                    phdr_table_set_load_prot(si->phdr, si->phnum, bias, 0);
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&dl_lock);
    dlclose(soHead);
    dlclose(soTarget);
    dlclose(soSrc);
    return false;
}

void unload_loaded(char *srcname, char *tgtname, func_map *map, int num_map)
{
    soinfo *soSrc = (soinfo *)dlopen(srcname, 0);
    soinfo *soTgt = (soinfo *)dlopen(tgtname, 0);
    unsigned *d;

    strncpy((char *)soSrc->name, srcname, 128);
    strncpy((char *)soTgt->name, tgtname, 128);
    free(soSrc->symtab);
    
    for (d = soSrc->dynamic; *d; d +=2)
    {
        if (d[0] == DT_SYMTAB)
            soSrc->symtab = (Elf32_Sym *)d[1];
        else if (d[0] == DT_STRTAB)
            soSrc->strtab = (const char *)d[1];
        else if (d[0] == DT_HASH)
        {
            soSrc->nbucket = ((unsigned *)(soSrc->base + d[1]))[0];
            soSrc->nchain = ((unsigned *)(soSrc->base + d[1]))[1];
            soSrc->bucket = ((unsigned **)(soSrc->base + d[1]))[2];
            soSrc->chain = ((unsigned **)(soSrc->base + d[1]))[3];
        }
    }

    dlclose(soSrc);
    dlclose(soTgt);
}

void reloc_fns(soinfo *si, Elf32_Rel *rel, unsigned count, func_map *map, unsigned num_map, Elf32_Addr bias)
{
    unsigned idx, imap;
    Elf32_Sym *symtab = si->symtab;
    const char *strtab = si->strtab;

    for (idx = 0; idx < count; ++idx, ++rel)
    {
        unsigned type = ELF32_R_TYPE(rel->r_info);
#if defined(ANDROID_ARM_LINKER)
        if (type != R_ARM_JUMP_SLOT && type != R_ARM_GLOB_DAT && type != R_ARM_ABS32)
            continue;
#elif defined(ANDROID_X86_LINKER)
        if (type != R_386_JUMP_SLOT && type != R_386_GLOB_DAT)
            continue;
#else
    #error("No linker type specified")
#endif
        unsigned sym = ELF32_R_SYM(rel->r_info);
        char *roffset = (char *)(rel->r_offset + bias);
        //char *moffset = addpage(roffset, sizeof(map->psrc), PROT_READ);

        for (imap = 0; imap < num_map; imap++)
        {
            if ((map[imap].sonly == NULL || !strcmp(si->name, map[imap].sonly)) && !strcmp(strtab + symtab[sym].st_name, map[imap].starget))
            {
                //SLOGD("Relocating %s, type=%d at %p", map[imap].starget, type, roffset);
                switch(type)
                {
        #if defined(ANDROID_ARM_LINKER)
                case R_ARM_JUMP_SLOT :
                case R_ARM_GLOB_DAT :
                case R_ARM_ABS32 :
        #elif defined(ANDROID_X86_LINKER)
                case R_386_JUMP_SLOT :
                case R_386_GLOB_DAT :
        #else
            #error("No linker type specified")
        #endif
                    //SLOGD("Current = %p, expected = %p, changing to = %p", *(void **)roffset, map[imap].ptarget, map[imap].psrc);
                    *(void **)roffset = map[imap].psrc;
                    break;
                default :
                    break;
                }
            }
        }
    }
}

#ifdef ANDROID_SH_LINKER
void reloca_fns(soinfo *si, Elf32_Rela *rela, unsigned count, func_map *map, unsigned num_map, Elf32_Addr bias)
{
    unsigned idx, imap;

    for (idx = 0; idx < count; ++idx, ++rel)
    {
        unsigned type = ELF32_R_TYPE(rela->info);
#if defined(ANDROID_ARM_LINKER
        if (type != R_ARM_JUMP_SLOT || type != R_ARM_GLOB_DAT)
            continue;
#elif defined(ANDROID_X86_LINKER)
        if (type != R_386_JUMP_SLOT || type != R_386_GLOB_DAT)
            continue;
#endif
        unsigned sym = ELF32_R_SYM(rela->info);
        char *roffset = (char *)(rela->reloc + bias);
        // char *moffset = addpage(roffset, sizeof(map->psrc), PROT_READ);

        switch(type)
        {
#if defined(ANDROID_ARM_LINKER)
        case R_ARM_JUMP_SLOT :
        case R_ARM_GLOB_DAT :
#elif defined(ANDROID_X86_LINKER)
        case R_386_JUMP_SLOT :
        case R_386_GLOB_DAT :
#endif
            for (imap = 0; imap < num_map; imap++)
            {
                if (*(void **)roffset == map[imap].ptarget)
                    *(void **)roffset = map[imap].psrc;
            }
            break;
        default :
            break;
        }
    }
}
#endif

#if (GRLOAD_API > 10)

bool hook_code(const char *srclib, void *srcfn, void *tgtfn, int sdkVer)
{
    soinfo *soSrc = (soinfo *)dlopen(srclib, 0);
    Elf32_Addr bias = sdkVer > 16 ? soSrc->load_bias : soSrc->base;
    int fnoffset;
    pthread_mutex_lock(&dl_lock);
    if (!soSrc || phdr_table_set_load_prot(soSrc->phdr, soSrc->phnum, bias, PROT_WRITE))
        return true;
#if defined(ANDROID_ARM_LINKER)
    unsigned short *ptgt = reinterpret_cast<unsigned short *>(tgtfn);
    //SLOGD("Hooking %p to %p", ptgt, srcfn);
// must use thumb code here
    int i = -1;
    ptgt[++i] = 0xF8DF;   // LDR PC, [PC, #0]
    if (reinterpret_cast<unsigned>(tgtfn) & 2)
        ptgt[++i] = 0xF002;
    else
        ptgt[++i] = 0xF000;
    ptgt[++i] = reinterpret_cast<unsigned>(srcfn) & 0xFFFF;   // little endian
    ptgt[++i] = reinterpret_cast<unsigned>(srcfn) >> 16;
//    ptgt[0] = 0xE51FF004;       // LDR PC, [PC, #-4]        // PC-4 = here+4 given PC = here+8
//    ptgt[1] = reinterpret_cast<unsigned int>(srcfn) - 8;      // subtract 8 for pipeline
#endif
    phdr_table_set_load_prot(soSrc->phdr, soSrc->phnum, bias, 0);
    pthread_mutex_unlock(&dl_lock);
    ptgt = reinterpret_cast<unsigned short *>(reinterpret_cast<unsigned>(tgtfn) & 0xFFFFFFF0);
    //SLOGD("%p: %04x%04x %04x%04x %04x%04x %04x%04x", ptgt, ptgt[0], ptgt[1], ptgt[2], ptgt[3], ptgt[4], ptgt[5], ptgt[6], ptgt[7]);
    //SLOGD("%p: %04x%04x %04x%04x %04x%04x %04x%04x", ptgt+8, ptgt[8], ptgt[9], ptgt[10], ptgt[11], ptgt[12], ptgt[13], ptgt[14], ptgt[15]);
    return false;
}

unsigned *got_addr(const soinfo *si, unsigned fn)
{
    unsigned *g = si->plt_got;
    //SLOGD("Searching got at %p", g);
    int i;
    for (i = 0; i < si->plt_rel_count; ++i, ++g)
    {
        if (*g == fn)
            return g;
    }
    return 0;
}

#ifdef ANDROID_ARM_LINKER

unsigned *plt_addr_arm(const soinfo *si, unsigned gaddr)
{
    unsigned *plt = si->rel > si->plt_rel ? reinterpret_cast<unsigned *>(si->rel + si->rel_count)
                                          : reinterpret_cast<unsigned *>(si->plt_rel + si->plt_rel_count);    // assume this is where the plt starts
    unsigned *p, *pend, *pstart;
    int i;
    pstart = 0;
    //SLOGD("Searching for start of plt at %p from %p + %x", plt, si->rel, si->rel_count);
    for (p = plt, pend = plt + 2048; p < pend; ++p)  // scan to find first plt entry
    {
        if ((*p & 0xFFEFF000) == 0xE28FC000)        // search for an ADD IP, PC, #x of some kind
        {
            pstart = p;
            break;
        }
    }
    if (!pstart) return 0;
    pend = pstart + si->plt_rel_count * 3;
    //SLOGD("Search for %d entries from %p to %p", si->plt_rel_count, pstart, pend);
    for (p = pstart, i = 0; i < si->plt_rel_count; ++i, ++p)
    {
        unsigned offset = 0;
        unsigned *res = p;
        while (p < pend)
        {
//            if (p - pstart < 50) SLOGD("Reading %x at %p", *p, p);
            if ((*p & 0xFFEFF000) == 0xE28FC000)        // ADD IP, PC, imm
                offset = reinterpret_cast<unsigned>(p) + ((*p & 0xFF) << (32 - ((*p & 0xF00) >> 7))) + 8;
            else if ((*p & 0xFFEFF000) == 0xE28CC000)   // ADD IP, IP, imm
                offset += ((*p & 0xFF) << (32 - ((*p & 0xF00) >> 7)));
            else if ((*p & 0xFE5FF000) == 0xE41CF000)   // LDR PC, [IP, #imm]
            {
                offset += (*p & 0xFFF);
//                if (i < 20 || (offset < target + 100 && offset > target - 100)) SLOGD("Checking plt addr %p, jumps to %x vs %x", p, offset, target);
                if (*reinterpret_cast<unsigned int *>(offset) == gaddr)
                    return res;
                else
                    break;
            }
            ++p;
        }
    }
    return 0;
}

#define within(test, base, range) ((test) > (base) - range && (test) < (base) + range)
Elf32_Addr scan_call_arm(const soinfo *si, Elf32_Addr paddr)
{
    unsigned short *start = reinterpret_cast<unsigned short *>(si->rel + si->rel_count + si->plt_rel_count * 2);  // Get somewhere close to the start of the code
    unsigned short *end = reinterpret_cast<unsigned short *>(si->init_array);     // waaay off the end, but hey we have to stop somewhere!
        // put code in here to handle si->init_array being 0, so go to end of section (find appropriate section, get its end)
    unsigned short *p;
    //SLOGD("Scanning from %p to %p", start, end);
    for (p = start; p < end; ++p)
    {
        unsigned v = (p[0] << 16) | p[1];
        if ((v & 0xFC00D001) == 0xF400C000)     // calls to the plt are always -ve offsets
        {
            unsigned res = get_calladdr_arm(p);
            if (res == paddr)
                return reinterpret_cast<unsigned>(p);
        }
    }
    //SLOGD("Stopped searching at %p", p);
    return 0;
}

Elf32_Addr scan_sof_arm(const soinfo *si, Elf32_Addr paddr, int num, int backwards)
{
    unsigned short *end = backwards ? reinterpret_cast<unsigned short *>(si->rel + si->rel_count + si->plt_rel_count) 
                                    : reinterpret_cast<unsigned short *>(si->init_array);
    unsigned short *p = reinterpret_cast<unsigned short *>(paddr);
    //SLOGD("Scanning for sof from %p to %p", p, end);
    while (p != end)
    {
        if ((((p[0] << 16 | p[1]) & 0xFFFF4000) == 0xE92D4000
          || ((*p & 0xFF00) == 0xB500)) && !--num)
            return reinterpret_cast<unsigned>(p);
        if (backwards)
            --p;
        else
            ++p;
    }
    return 0;
}

#endif

Elf32_Addr findcallsite(const soinfo *soTarget, const char *srcname, const char *srcfn)
{
    soinfo *soSrc = (soinfo *)dlopen(srcname, 0);
    if (!soSrc || !soTarget) return 0;
    void *fnSrc = dlsym(soSrc, srcfn);
    if (!fnSrc) return 0;
    //unsigned *gotaddr = got_addr(soTarget, );
    //SLOGD("Found got addr %p", gotaddr);
    //if (!gotaddr) return 0;
    unsigned *pltaddr = plt_addr_arm(soTarget, reinterpret_cast<unsigned>(fnSrc));
    //SLOGD("Found pltaddr %p", pltaddr);
    if (!pltaddr) return 0;
    Elf32_Addr callsite = scan_call_arm(soTarget, reinterpret_cast<Elf32_Addr>(pltaddr));
    //SLOGD("Found callsite %x", callsite);
    return callsite;
}

Elf32_Addr findfn(const char *targetname, const char *srcname, const char *srcfn, int num, int backwards)
{
    //SLOGD("Finding fn in %s based on %s in %s", targetname, srcfn, srcname);
    soinfo *soTarget = (soinfo *)dlopen(targetname, 0);
    //phdr_table_set_load_prot(soTarget->phdr, soTarget->phnum, soTarget->base, PROT_READ);
    Elf32_Addr callsite = findcallsite(soTarget, srcname, srcfn);
    if (!callsite) return 0;
    Elf32_Addr res = scan_sof_arm(soTarget, callsite, num, backwards);
    //phdr_table_set_load_prot(soTarget->phdr, soTarget->phnum, soTarget->base, 0);
}

#endif // GRLOAD_API > 10
