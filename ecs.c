#include "ecs.h"
#include <stdio.h>
#include <string.h>

#define LOAD_FACTOR 0.5
#define TOMESTONE ((uint32_t)-1)

static uint32_t next_pow_of_2(uint32_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}

ecs_map_t *ecs_map_new(size_t size, uint32_t capacity)
{
    ecs_map_t *map = ECS_NEW(ecs_map_t, 1);
    map->dense = ecs_calloc(size, capacity);
    map->sparse = ecs_calloc(sizeof(ecs_bucket_t), capacity);
    map->item_size = size;
    map->load_capacity = capacity;
    map->count = 0;
    return map;
}

void ecs_map_free(ecs_map_t *map)
{
    free(map->dense);
    free(map->sparse);
    free(map);
}

static ecs_key_t hash(ecs_key_t key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
    return key;
}

void *ecs_map_get(const ecs_map_t *map, ecs_key_t key)
{
    ecs_key_t i = hash(key);
    ecs_bucket_t bucket = map->sparse[i % map->load_capacity];

    while (bucket.index != 0 && bucket.key != key)
    {
        i = next_pow_of_2(i);
        bucket = map->sparse[i % map->load_capacity];
    }

    return (char *)map->dense + (map->item_size * bucket.index);
}

static void grow(ecs_map_t *map)
{
    (void)map;
    abort();
}

void ecs_map_set(ecs_map_t *map, ecs_key_t key, const void *payload)
{
    ecs_key_t i = hash(key);
    ecs_bucket_t *bucket = &map->sparse[i % map->load_capacity];

    while (bucket->index != 0)
    {
        if (bucket->key == key)
        {
            void *loc = (char *)map->dense + (map->item_size * bucket->index);
            memcpy(loc, payload, map->item_size);
            return;
        }

        i = next_pow_of_2(i);
        bucket = &map->sparse[i % map->load_capacity];
    }

    bucket->key = key;
    bucket->index = map->count + 1;
    void *loc = (char *)map->dense + (map->item_size * bucket->index);
    memcpy(loc, payload, map->item_size);
    map->count++;

    if (map->count > map->load_capacity * LOAD_FACTOR)
    {
        grow(map);
    }
}

void ecs_map_remove(ecs_map_t *map, ecs_key_t key);

void ecs_map_inspect(ecs_map_t *map)
{
    printf("map: {\n"
           "  item_size: %ld bytes\n"
           "  count: %d items\n"
           "  load_capacity: %d\n",
           map->item_size, map->count, map->load_capacity);

    printf("  sparse: [\n");
    for (uint32_t i = 0; i < map->load_capacity; i++)
    {
        ecs_bucket_t bucket = map->sparse[i];
        printf("    %d: { key: %d, index: %d }\n", i, bucket.key, bucket.index);
    }
    printf("  ]\n");

    printf("  dense: [\n");
    for (uint32_t i = 0; i < map->load_capacity; i++)
    {
        void *item = (char *)map->dense + (map->item_size * i);
        printf("    %d: (", i);
        for (uint32_t byte = 0; byte < map->item_size; byte++)
        {
            printf("%X", *((char *)item + byte));
        }
        printf(")\n");
    }
    printf("  ]\n");

    printf("}\n");
}

ecs_world_t *ecs_init()
{
    ecs_world_t *world = ECS_NEW(ecs_world_t, 1);
    return world;
}

void ecs_destroy(ecs_world_t *world)
{
    free(world);
}
