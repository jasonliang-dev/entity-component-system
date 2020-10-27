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
  ecs_map_new(sizeof(k), sizeof(v), ecs_map_hash_##fn, ecs_map_equal_##fn, capacity)

  ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn,
                         ecs_key_equal_fn key_equal_fn, uint32_t capacity);
  void ecs_map_free(ecs_map_t *map);
  void *ecs_map_get(const ecs_map_t *map, const void *key);
  void ecs_map_set(ecs_map_t *map, const void *key, const void *payload);
  void ecs_map_remove(ecs_map_t *map, const void *key);
  uint32_t ecs_map_hash_intptr(const void *key);
  uint32_t ecs_map_hash_string(const void *key);
  uint32_t ecs_map_hash_type(const void *key);
  bool ecs_map_equal_intptr(const void *a, const void *b);
  bool ecs_map_equal_string(const void *a, const void *b);
  bool ecs_map_equal_type(const void *a, const void *b);

#ifndef NDEBUG
  void ecs_map_inspect(ecs_map_t *map); // assumes keys and values are ints
#endif

  typedef uintptr_t ecs_entity_t;

  typedef struct ecs_type_t {
    uint32_t capacity;
    uint32_t count;
    ecs_entity_t *elements;
  } ecs_type_t;

  ecs_type_t *ecs_type_new(uint32_t capacity);
  void ecs_type_free(ecs_type_t *type);
  ecs_type_t *ecs_type_copy(const ecs_type_t *from);
  ecs_entity_t *ecs_type_get_array(ecs_type_t *type);
  uint32_t ecs_type_len(const ecs_type_t *type);
  bool ecs_type_equal(const ecs_type_t *a, const ecs_type_t *b);
  int32_t ecs_type_index_of(const ecs_type_t *type, ecs_entity_t e);
  void ecs_type_add(ecs_type_t *type, ecs_entity_t e);
  void ecs_type_remove(ecs_type_t *type, ecs_entity_t e);

#define ECS_TYPE_ADD(type, e, s)                                               \
  ecs_type_add(type, (ecs_component_t){e, sizeof(s)});

#define ECS_TYPE_EACH(type, var, ...)                                          \
  {                                                                            \
    uint32_t var##_count = ecs_type_len(type);                                 \
    for (uint32_t var##_i = 0; var##_i < var##_count; var##_i++) {             \
      ecs_entity_t var = type->elements[var##_i];                              \
      __VA_ARGS__                                                              \
    }                                                                          \
  }

#ifndef NDEBUG
  void ecs_type_inspect(ecs_type_t *type);
#endif

  typedef struct ecs_archetype_t ecs_archetype_t;

  typedef struct ecs_edge_t {
    ecs_entity_t component;
    ecs_archetype_t *add;
    ecs_archetype_t *remove;
  } ecs_edge_t;

  struct ecs_archetype_t {
    uint32_t capacity;
    uint32_t count;
    uint32_t edges_allocd;
    ecs_type_t *type;
    ecs_entity_t *entity_ids;
    void **components;
    ecs_edge_t **edges;
  };

  ecs_archetype_t *ecs_archetype_new(ecs_type_t *type,
                                     const ecs_map_t *component_index);
  void ecs_archetype_free(ecs_archetype_t *archetype);
  uint32_t ecs_archetype_add(ecs_archetype_t *archetype,
                             ecs_map_t *component_index, ecs_entity_t e);

  typedef struct ecs_record_t {
    ecs_archetype_t *archetype;
    uint32_t row;
  } ecs_record_t;

  typedef struct ecs_registry_t {
    ecs_map_t *entity_index;
    ecs_map_t *component_index;
    ecs_map_t *named_entities;
    ecs_map_t *type_index;
    ecs_archetype_t *root;
    ecs_entity_t next_entity_id;
  } ecs_registry_t;

  ecs_registry_t *ecs_init(void);
  void ecs_destroy(ecs_registry_t *registry);
  ecs_entity_t ecs_entity(ecs_registry_t *registry);
  ecs_entity_t ecs_component(ecs_registry_t *registry, char *name,
                             size_t component_size);
  void ecs_name(ecs_registry_t *registry, ecs_entity_t entity, char *name);
  void ecs_attach(ecs_registry_t *registry, ecs_entity_t entity, ecs_entity_t component);
  void ecs_attach_w_name(ecs_registry_t *registry, ecs_entity_t entity, char *component_name);

#define ECS_COMPONENT(registry, T) ecs_component(registry, #T, sizeof(T));

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ECS_H
