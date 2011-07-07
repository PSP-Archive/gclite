/*
 * sysconf.c
 *
 *  Created on: 03/07/2011
 *      Author: code
 */

#include <psputils.h>
#include "game_categories_light.h"
#include "psppaf.h"
#include "vshitem.h"
#include "utils.h"
#include "config.h"
#include "stub_funcs.h"
#include "logger.h"

char user_buffer[256];
u32 backup[4];
int context_mode = 0;
SceSysconfItem *sysconf_item[2] = { NULL, NULL };

char *sysconf_str[] = {"gc0", "gc1" , "gc2"};

struct GCStrings {
    char *options[2];
    char *prefix[2];
    char *show[4];
} GCStrings;

struct GCStrings gc_opts = {
        {"Multi MS", "None"},
        {"CAT_ prefix", "None"},
        {"No", "Only MS", "Only IS", "All"},
};


void (*AddSysconfItem)(u32 *option, SceSysconfItem **item);
SceSysconfItem *(*GetSysconfItem)(void *arg0, void *arg1);

int (*vshGetRegistryValue)(u32 *option, char *name, void *arg2, int size, int *value);
int (*vshSetRegistryValue)(u32 *option, char *name, int size,  int *value);

int (*ResolveRefWString)(void *resource, u32 *data, int *a2, char **string, int *t0);
int (*GetPageNodeByID)(void *resource, char *name, SceRcoEntry **child);

void AddSysconfItemPatched(u32 *option, SceSysconfItem **item) {
    AddSysconfItem(option, item);
    for(int i = 0; i < sizeof(sysconf_item) / 4; i++) {
        if(!sysconf_item[i]) {
            sysconf_item[i] = (SceSysconfItem *)sce_paf_private_malloc(sizeof(SceSysconfItem));
        }
        sce_paf_private_memcpy(sysconf_item[i], *item, sizeof(SceSysconfItem));
        sysconf_item[i]->id = 5;
        sysconf_item[i]->text = sysconf_str[i];
        sysconf_item[i]->regkey = sysconf_str[i];
        kprintf("%s: adding %s\n", __func__, sysconf_item[i]->text);
        sysconf_item[i]->page = "page_psp_config_umd_autoboot";
        option[2] = 1;
        AddSysconfItem(option, &sysconf_item[i]);
    }
}

void HijackContext(SceRcoEntry *src, char **options, int n) {
    SceRcoEntry *plane = (SceRcoEntry *)((u32)src + src->first_child);
    SceRcoEntry *mlist = (SceRcoEntry *)((u32)plane + plane->first_child);
    u32 *mlist_param = (u32 *)((u32)mlist + mlist->param);

    /* Backup */
    if(backup[0] == 0 && backup[1] == 0 && backup[2] == 0 && backup[3] == 0)
    {
        backup[0] = mlist->first_child;
        backup[1] = mlist->child_count;
        backup[2] = mlist_param[16];
        backup[3] = mlist_param[18];
    }

    if(context_mode)
    {
        SceRcoEntry *base = (SceRcoEntry *)((u32)mlist + mlist->first_child);

        SceRcoEntry *item = (SceRcoEntry *)sce_paf_private_malloc(base->next_entry * n);
        u32 *item_param = (u32 *)((u32)item + base->param);

        mlist->first_child = (u32)item - (u32)mlist;
        mlist->child_count = n;
        mlist_param[16] = 13;
        mlist_param[18] = 6;

        int i;
        for(i = 0; i < n; i++)
        {
            sce_paf_private_memcpy(item, base, base->next_entry);

            item_param[0] = 0xDEAD;
            item_param[1] = (u32)options[i];

            if(i != 0) item->prev_entry = item->next_entry;
            if(i == n - 1) item->next_entry = 0;

            item = (SceRcoEntry *)((u32)item + base->next_entry);
            item_param = (u32 *)((u32)item + base->param);
        }
    }
    else
    {
        /* Restore */
        mlist->first_child = backup[0];
        mlist->child_count = backup[1];
        mlist_param[16] = backup[2];
        mlist_param[18] = backup[3];
    }

    sceKernelDcacheWritebackAll();
}

SceSysconfItem *GetSysconfItemPatched(void *arg0, void *arg1) {
    SceSysconfItem *item = GetSysconfItem(arg0, arg1);
    kprintf("%s: called, item->text: %s\n", __func__, item->text);
    for(int i = 0; i < sizeof(sysconf_str) / 4; i++) {
        if(sce_paf_private_strcmp(item->text, sysconf_str[i]) == 0) {
            context_mode = 1;
        }
    }
    return item;
}

int vshGetRegistryValuePatched(u32 *option, char *name, void *arg2, int size, int *value) {
    if (name) {
        kprintf("%s: name: %s\n", __func__, name);
        for(int i = 0; i < sizeof(sysconf_str) / 4; i++) {
            if(sce_paf_private_strcmp(name, sysconf_str[i]) == 0) {
                context_mode = 1;
                switch(i) {
                case 0:
                    *value = config.mode;
                    return 0;
                case 1:
                    *value = config.prefix;
                    return 0;
                case 2:
                    *value = config.uncategorized;
                    return 0;
                default:
                    *value = 0;
                    return 0;
                }
            }
        }
    }
    return vshGetRegistryValue(option, name, arg2, size, value);
}

int vshSetRegistryValuePatched(u32 *option, char *name, int size,  int *value) {
    u32 *cfg;
    if (name) {
        kprintf("%s: name: %s\n", __func__, name);
        for(int i = 0; i < sizeof(sysconf_str) / 4; i++) {
            if(sce_paf_private_strcmp(name, sysconf_str[i]) == 0) {
                switch(i) {
                case 0:
                    cfg = &config.mode;
                    break;
                case 1:
                    cfg = &config.prefix;
                    break;
                case 2:
                    cfg = &config.uncategorized;
                    break;
                default:
                    cfg = NULL;
                    break;
                }
                if(cfg) {
                    *cfg = *value;
                    save_config(&config);
                    break;
                }
            }
        }
    }
    return vshSetRegistryValue(option, name, size, value);
}

// scePafGetPageStringPatched
int ResolveRefWStringPatched(void *resource, u32 *data, int *a2, char **string, int *t0) {
    if (data[0] == 0xDEAD) {
        kprintf("%s: data: %s\n", __func__, data[1]);
        gc_utf8_to_unicode((wchar_t *) user_buffer, (char *) data[1]);
        *(wchar_t **) string = (wchar_t *) user_buffer;
        return 0;
    }
    return ResolveRefWString(resource, data, a2, string, t0);
}

// scePafGetPageChildPatched
int GetPageNodeByIDPatched(void *resource, char *name, SceRcoEntry **child) {
    int res = GetPageNodeByID(resource, name, child);
    if(name) {
        kprintf("%s: name: %s, mode: %i\n", __func__, name, context_mode);
        if (sce_paf_private_strcmp(name, "page_psp_config_umd_autoboot") == 0) {
            switch(context_mode) {
            case 0:
                HijackContext(*child, NULL, 0);
                break;
            case 1:
                HijackContext(*child, gc_opts.options, sizeof(gc_opts.options) / sizeof(char *));
                break;
            case 2:
                HijackContext(*child, gc_opts.prefix, sizeof(gc_opts.prefix) / sizeof(char *));
                break;
            case 3:
                HijackContext(*child, gc_opts.show, sizeof(gc_opts.show) / sizeof(char *));
                break;
            }
        }
    }
    return res;
}

void PatchVshmain2(u32 text_addr) {
    vshGetRegistryValue = redir2stub(text_addr+PATCHES->vshGetRegistryValueOffset, (u32)get_registry_stub, vshGetRegistryValuePatched);
    vshSetRegistryValue = redir2stub(text_addr+PATCHES->vshSetRegistryValueOffset, (u32)set_registry_stub, vshSetRegistryValuePatched);
}

void PatchPaf2(u32 text_addr) {
    GetPageNodeByID = redir2stub(text_addr+PATCHES->GetPageNodeByIDOffset, (u32)get_page_node_stub, GetPageNodeByIDPatched);
    ResolveRefWString = redir2stub(text_addr+PATCHES->ResolveRefWStringOffset, (u32)resolve_ref_wstring_stub, ResolveRefWStringPatched);
}

void PatchSysconf(u32 text_addr) {
    AddSysconfItem = redir_call(text_addr+PATCHES->AddSysconfItem, AddSysconfItemPatched);
    GetSysconfItem = redir_call(text_addr+PATCHES->GetSysconfItem, GetSysconfItemPatched);
}
