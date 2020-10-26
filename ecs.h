#ifndef ECS_H
#define ECS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct ecs_map_t ecs_map_t;

  typedef uint32_t (*ecs_hash_fn)(const void *);
  typedef bool (*ecs_key_equal_fn)(const void *, const void *);

#define ECS_MAP(fn, k, v, capacity)                                            \
  ecs_map_new(sizeof(k), sizeof(v), ecs_hash_##fn, ecs_equal_##fn, capacity)

  ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn,
                         ecs_key_equal_fn key_equal_fn, uint32_t capacity);
  void ecs_map_free(ecs_map_t *map);
  void *ecs_map_get(const ecs_map_t *map, const void *key);
  void ecs_map_set(ecs_map_t *map, const void *key, const void *payload);
  void ecs_map_remove(ecs_map_t *map, const void *key);
  uint32_t ecs_hash_intptr(const void *key);
  uint32_t ecs_hash_string(const void *key);
  bool ecs_equal_intptr(const void *a, const void *b);
  bool ecs_equal_string(const void *a, const void *b);

#ifndef NDEBUG
  void ecs_map_inspect(ecs_map_t *map); // assumes keys and values are ints
#endif

  typedef uint64_t ecs_entity_t;

  typedef struct ecs_type_t ecs_type_t;

  ecs_type_t *ecs_type_new(uint32_t capacity);
  void ecs_type_free(ecs_type_t *type);
  ecs_type_t *ecs_type_copy(const ecs_type_t *from);
  bool ecs_type_equal(const ecs_type_t *a, const ecs_type_t *b);
  bool ecs_type_contains(const ecs_type_t *type, ecs_entity_t e);
  void ecs_type_add(ecs_type_t *type, ecs_entity_t e);
  void ecs_type_remove(ecs_type_t *type, ecs_entity_t e);

#ifndef NDEBUG
  void ecs_type_inspect(ecs_type_t *type);
#endif

  typedef struct ecs_component_array_t {
    void *elements;
    size_t size;
  } ecs_component_array_t;

  typedef struct ecs_archetype_t ecs_archetype_t;

  typedef struct ecs_edge_t {
    ecs_entity_t component;
    ecs_archetype_t *add;
    ecs_archetype_t *remove;
  } ecs_edge_t;

  struct ecs_archetype_t {
    uint32_t capacity;
    uint32_t count;
    ecs_type_t *type;
    ecs_entity_t *entity_ids;
    ecs_component_array_t *components;
  };

  typedef struct ecs_record_t {
    ecs_archetype_t *archetype;
    uint32_t row;
  } ecs_record_t;

  typedef struct ecs_registry_t {
    ecs_map_t *entity_index;
    ecs_archetype_t *root;
    ecs_entity_t next_entity_id;
  } ecs_registry_t;

  ecs_registry_t *ecs_init(void);
  void ecs_destroy(ecs_registry_t *registry);
  ecs_entity_t ecs_entity(ecs_registry_t *registry);
  // ecs_entity_t ecs_entity(ecs_registry_t *registry, const char *types_str);
  // ecs_entity_t ecs_component(ecs_registry_t *registry, size_t
  // component_size); void ecs_system(ecs_registry_t *registry, void (*func)(),
  //                 const char *types_str);
  // void ecs_add(ecs_registry_t *registry, ecs_entity_t entity,
  //              ecs_entity_t component);
  // void ecs_set(ecs_registry_t *registry, ecs_entity_t entity,
  //              ecs_entity_t component, void *payload);
  // void ecs_remove(ecs_registry_t *registry, ecs_entity_t entity,
  //              ecs_entity_t component);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ECS_H
