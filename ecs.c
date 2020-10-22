#include "ecs.h"
#include <stdio.h>
#include <string.h>

#define LOAD_FACTOR 0.5
#define TOMESTONE ((uint32_t)-1)

#define ECS_NEW(type, count) ((type *)ecs_malloc((count) * sizeof(type)))

static inline void *ecs_malloc(size_t bytes)
{
    void *mem = malloc(bytes);
    assert(mem != NULL);
    return mem;
}

static inline void *ecs_calloc(size_t items, size_t bytes)
{
    void *mem = calloc(items, bytes);
    assert(mem != NULL);
    return mem;
}

static inline void *ecs_realloc(void *mem, size_t bytes)
{
    mem = realloc(mem, bytes);
    assert(mem != NULL);
    return mem;
}

static inline uint32_t next_pow_of_2(uint32_t n)
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
    map->sparse = ecs_calloc(sizeof(ecs_bucket_t), capacity);
    map->reverse_lookup = ecs_malloc(sizeof(uint32_t) * (capacity * LOAD_FACTOR + 1));
    map->dense = ecs_malloc(size * (capacity * LOAD_FACTOR + 1));
    map->item_size = size;
    map->load_capacity = capacity;
    map->count = 0;
    return map;
}

void ecs_map_free(ecs_map_t *map)
{
    free(map->sparse);
    free(map->reverse_lookup);
    free(map->dense);
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
    uint32_t next = 0;

    while (bucket.index != 0)
    {
        if (bucket.key == key && bucket.index != TOMESTONE)
        {
            break;
        }

        i += next_pow_of_2(next++);
        bucket = map->sparse[i % map->load_capacity];
    }

    if (bucket.index == 0 || bucket.index == TOMESTONE)
    {
        return NULL;
    }

    return (char *)map->dense + (map->item_size * bucket.index);
}

static void grow(ecs_map_t *map, float growth_factor)
{
    uint32_t new_capacity = map->load_capacity * growth_factor;
    ecs_bucket_t *new_sparse = ecs_calloc(sizeof(ecs_bucket_t), new_capacity);
    uint32_t *new_reverse_lookup = ecs_malloc(sizeof(uint32_t) * (new_capacity * LOAD_FACTOR + 1));
    map->dense = ecs_realloc(map->dense, map->item_size * (new_capacity * LOAD_FACTOR + 1));

    for (uint32_t i = 0; i < map->load_capacity; i++)
    {
        ecs_bucket_t bucket = map->sparse[i];

        if (bucket.index == 0 || bucket.index == TOMESTONE)
        {
            continue;
        }

        ecs_key_t hashed = hash(bucket.key);
        ecs_bucket_t *other = &new_sparse[hashed % new_capacity];
        uint32_t next = 0;

        while (other->index != 0)
        {
            hashed += next_pow_of_2(next++);
            other = &new_sparse[hashed % new_capacity];
        }

        other->key = bucket.key;
        other->index = bucket.index;
        new_reverse_lookup[bucket.index] = hashed % new_capacity;
    }

    free(map->sparse);
    free(map->reverse_lookup);
    map->sparse = new_sparse;
    map->reverse_lookup = new_reverse_lookup;
    map->load_capacity = new_capacity;
}

void ecs_map_set(ecs_map_t *map, ecs_key_t key, const void *payload)
{
    ecs_key_t i = hash(key);
    ecs_bucket_t *bucket = &map->sparse[i % map->load_capacity];
    uint32_t next = 0;
    ecs_bucket_t *first_tomestone = NULL;

    while (bucket->index != 0)
    {
        if (bucket->key == key)
        {
            void *loc = (char *)map->dense + (map->item_size * bucket->index);
            memcpy(loc, payload, map->item_size);
            return;
        }

        if (!first_tomestone && bucket->index == TOMESTONE)
        {
            first_tomestone = bucket;
        }

        i += next_pow_of_2(next++);
        bucket = &map->sparse[i % map->load_capacity];
    }

    if (first_tomestone)
    {
        bucket = first_tomestone;
    }

    bucket->key = key;
    bucket->index = map->count + 1;
    void *loc = (char *)map->dense + (map->item_size * bucket->index);
    memcpy(loc, payload, map->item_size);
    map->reverse_lookup[bucket->index] = i % map->load_capacity;
    map->count++;

    if (map->count >= map->load_capacity * LOAD_FACTOR)
    {
        grow(map, 2);
    }
}

void ecs_map_remove(ecs_map_t *map, ecs_key_t key)
{
    ecs_key_t i = hash(key);
    ecs_bucket_t bucket = map->sparse[i % map->load_capacity];
    uint32_t next = 0;

    while (bucket.index != 0)
    {
        if (bucket.key == key && bucket.index != TOMESTONE)
        {
            break;
        }

        i += next_pow_of_2(next++);
        bucket = map->sparse[i % map->load_capacity];
    }

    if (bucket.index == 0 || bucket.index == TOMESTONE)
    {
        return;
    }

    void *tmp = alloca(map->item_size);
    void *left = (char *)map->dense + (map->item_size * bucket.index);
    void *right = (char *)map->dense + (map->item_size * map->count);
    memcpy(tmp, left, map->item_size);
    memcpy(left, right, map->item_size);
    memcpy(right, tmp, map->item_size);

    map->sparse[map->reverse_lookup[map->count]].index = bucket.index;
    map->sparse[map->reverse_lookup[bucket.index]].index = TOMESTONE;

    uint32_t reverse_tmp = map->reverse_lookup[bucket.index];
    map->reverse_lookup[bucket.index] = map->reverse_lookup[map->count];
    map->reverse_lookup[map->count] = reverse_tmp;

    map->count--;
}

void ecs_map_inspect(ecs_map_t *map)
{
    printf("\nmap: {\n"
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
    for (uint32_t i = 0; i < map->load_capacity * LOAD_FACTOR + 1; i++)
    {
        if (i == map->count + 1)
        {
            printf("    -- end of load --\n");
        }

        int item = *(int *)((char *)map->dense + (map->item_size * i));
        printf("    %d: %d\n", i, item);
    }
    printf("  ]\n");

    printf("  reverse_lookup: [\n");
    for (uint32_t i = 0; i < map->load_capacity * LOAD_FACTOR + 1; i++)
    {
        if (i == map->count + 1)
        {
            printf("    -- end of load --\n");
        }

        printf("    %d: %d\n", i, map->reverse_lookup[i]);
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
