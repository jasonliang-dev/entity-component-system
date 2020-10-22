#ifndef ECS_H
#define ECS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t ecs_key_t;

typedef struct ecs_bucket_t
{
    ecs_key_t key;
    uint32_t index;
} ecs_bucket_t;

typedef struct ecs_map_t
{
    size_t item_size;
    uint32_t count;
    uint32_t load_capacity;
    ecs_bucket_t *sparse;
    uint32_t *reverse_lookup;
    void *dense;
} ecs_map_t;

ecs_map_t *ecs_map_new(size_t size, uint32_t count);
void ecs_map_free(ecs_map_t *map);
void *ecs_map_get(const ecs_map_t *map, ecs_key_t key);
void ecs_map_set(ecs_map_t *map, ecs_key_t key, const void *payload);
void ecs_map_remove(ecs_map_t *map, ecs_key_t key);
void ecs_map_inspect(ecs_map_t *map);

typedef uint64_t ecs_entity_t;

typedef struct ecs_component_array_t
{
    void *elements;
    size_t size;
} ecs_component_array_t;

typedef struct ecs_archetype_t ecs_archetype_t;

typedef struct ecs_edge_t
{
    ecs_entity_t component;
    ecs_archetype_t *add;
    ecs_archetype_t *remove;
} ecs_edge_t;

struct ecs_archetype_t
{
    uint32_t length;
    ecs_entity_t *type;
    ecs_entity_t *entity_ids;
    ecs_component_array_t *components;
};

typedef struct ecs_world_t
{
    ecs_map_t *entity_index;
    ecs_archetype_t *root;
} ecs_world_t;

ecs_world_t *ecs_init(void);
void ecs_destroy(ecs_world_t *world);

#endif // ECS_H
