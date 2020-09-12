#ifndef _ASSET_MAP_H
#define _ASSET_MAP_H

#include "cpu.h"

struct assetmapobj* assetmap_new(struct cpu* cpu, int w, int h);
struct assetmapobj* assetmap_new_copydata(struct cpu* cpu, int w, int h, const uint8_t* data);
void assetmap_destroy(struct cpu* cpu, struct assetmapobj* assetmap);
value_t assetmap_fget(struct cpu* cpu, struct assetmapobj* assetmap, struct strobj* key);

#endif
