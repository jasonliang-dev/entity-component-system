#ifndef ECS_H
#define ECS_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ecs_bucket_t ecs_bucket_t;

    typedef struct ecs_map_t ecs_map_t;

    typedef uint32_t (*ecs_hash_fn)(const struct ecs_map_t *, const void *);
    typedef bool (*ecs_key_equal_fn)(const void *, const void *);

    struct ecs_map_t
    {
        ecs_hash_fn hash;
        ecs_key_equal_fn key_equal;
        size_t key_size;
        size_t item_size;
        uint32_t count;
        uint32_t load_capacity;
        ecs_bucket_t *sparse;
        uint32_t *reverse_lookup;
        void *dense;
    };

    ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn, ecs_key_equal_fn key_equal_fn,
                           uint32_t capacity);
    void ecs_map_free(ecs_map_t *map);
    void *ecs_map_get(const ecs_map_t *map, const void *key);
    void ecs_map_set(ecs_map_t *map, const void *key, const void *payload);
    void ecs_map_remove(ecs_map_t *map, const void *key);
    void ecs_map_inspect(ecs_map_t *map);
    uint32_t ecs_hash_int(const ecs_map_t *map, const void *key);
    uint32_t ecs_hash_string(const ecs_map_t *map, const void *key);
    uint32_t ecs_hash_direct(const ecs_map_t *map, const void *key);
    bool ecs_equal_string(const void *a, const void *b);
    bool ecs_equal_direct(const void *a, const void *b);

#define ECS_MAP(k, v, fn, capacity) ecs_map_new(sizeof(k), sizeof(v), ecs_hash_##fn, ecs_equal_##fn, capacity)

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

    typedef struct ecs_record_t
    {
        ecs_archetype_t *archetype;
        uint32_t row;
    } ecs_record_t;

    typedef struct ecs_registry_t
    {
        ecs_map_t *entity_index;
        ecs_archetype_t *root;
    } ecs_registry_t;

    ecs_registry_t *ecs_init(void);
    void ecs_destroy(ecs_registry_t *registry);
    ecs_entity_t ecs_entity(ecs_registry_t *registry, const char *types_str);
    ecs_entity_t ecs_component(ecs_registry_t *registry, size_t component_size);
    void ecs_system(ecs_registry_t *registry, void (*func)(), const char *types_str);
    void ecs_add(ecs_registry_t *registry, ecs_entity_t entity, ecs_entity_t component);
    void ecs_set(ecs_registry_t *registry, ecs_entity_t entity, ecs_entity_t component, void *payload);
    void ecs_remove(ecs_registry_t *registry, ecs_entity_t entity, ecs_entity_t component);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ECS_H
