#ifndef ECS_H
#define ECS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef uintptr_t ecs_entity_t;

  // -- MAP --------------------------------------------------------------------
  // type unsafe hashtable

  typedef struct ecs_map_t ecs_map_t;

  typedef uint32_t (*ecs_hash_fn)(const void *);
  typedef bool (*ecs_key_equal_fn)(const void *, const void *);

#define ECS_MAP(fn, k, v, capacity)                                            \
  ecs_map_new(sizeof(k), sizeof(v), ecs_map_hash_##fn, ecs_map_equal_##fn,     \
              capacity)

  ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn,
                         ecs_key_equal_fn key_equal_fn, uint32_t capacity);
  void ecs_map_free(ecs_map_t *map);
  void *ecs_map_get(const ecs_map_t *map, const void *key);
  void ecs_map_set(ecs_map_t *map, const void *key, const void *payload);
  void ecs_map_remove(ecs_map_t *map, const void *key);
  void *ecs_map_values(ecs_map_t *map);
  uint32_t ecs_map_len(ecs_map_t *map);
  uint32_t ecs_map_hash_intptr(const void *key);
  uint32_t ecs_map_hash_string(const void *key);
  uint32_t ecs_map_hash_type(const void *key);
  bool ecs_map_equal_intptr(const void *a, const void *b);
  bool ecs_map_equal_string(const void *a, const void *b);
  bool ecs_map_equal_type(const void *a, const void *b);

#define ECS_MAP_VALUES_EACH(map, T, var, ...)                                  \
  do {                                                                         \
    uint32_t var##_count = ecs_map_len(map);                                   \
    T *var##_values = ecs_map_values(map);                                     \
    for (uint32_t var##_i = 0; var##_i < var##_count; var##_i++) {             \
      T *var = &var##_values[var##_i];                                         \
      __VA_ARGS__                                                              \
    }                                                                          \
  } while (0)

#ifndef NDEBUG
  void ecs_map_inspect(ecs_map_t *map); // assumes keys and values are ints
#endif

  // -- TYPE -------------------------------------------------------------------
  // set of component ids in sorted order

  typedef struct ecs_type_t ecs_type_t;

  ecs_type_t *ecs_type_new(uint32_t capacity);
  void ecs_type_free(ecs_type_t *type);
  ecs_type_t *ecs_type_copy(const ecs_type_t *from);
  uint32_t ecs_type_len(const ecs_type_t *type);
  bool ecs_type_equal(const ecs_type_t *a, const ecs_type_t *b);
  int32_t ecs_type_index_of(const ecs_type_t *type, ecs_entity_t e);
  void ecs_type_add(ecs_type_t *type, ecs_entity_t e);
  void ecs_type_remove(ecs_type_t *type, ecs_entity_t e);
  bool ecs_type_is_superset(const ecs_type_t *super, const ecs_type_t *sub);

#define ECS_TYPE_ADD(type, e, s)                                               \
  ecs_type_add(type, (ecs_component_t){e, sizeof(s)});

#define ECS_TYPE_EACH(type, var, ...)                                          \
  do {                                                                         \
    uint32_t var##_count = ecs_type_len(type);                                 \
    for (uint32_t var##_i = 0; var##_i < var##_count; var##_i++) {             \
      ecs_entity_t var = type->elements[var##_i];                              \
      __VA_ARGS__                                                              \
    }                                                                          \
  } while (0)

#ifndef NDEBUG
  void ecs_type_inspect(ecs_type_t *type);
#endif

  // -- SIGNATURE --------------------------------------------------------------
  // component ids in a defined order

  typedef struct ecs_signature_t ecs_signature_t;

  ecs_signature_t *ecs_signature_new(uint32_t count);
  ecs_signature_t *ecs_signature_new_n(uint32_t count, ...);
  void ecs_signature_free(ecs_signature_t *sig);
  ecs_type_t *ecs_signature_as_type(const ecs_signature_t *sig);

  // -- EDGE LIST --------------------------------------------------------------
  // archetype edges for graph traversal

  typedef struct ecs_edge_t ecs_edge_t;
  typedef struct ecs_edge_list_t ecs_edge_list_t;

  ecs_edge_list_t *ecs_edge_list_new(void);
  void ecs_edge_list_free(ecs_edge_list_t *edge_list);
  uint32_t ecs_edge_list_len(const ecs_edge_list_t *edge_list);
  void ecs_edge_list_add(ecs_edge_list_t *edge_list, ecs_edge_t edge);
  void ecs_edge_list_remove(ecs_edge_list_t *edge_list, ecs_entity_t component);

#define ECS_EDGE_LIST_EACH(edge_list, var, ...)                                \
  do {                                                                         \
    uint32_t var##_count = ecs_edge_list_len(edge_list);                       \
    for (uint32_t var##_i = 0; var##_i < var##_count; var##_i++) {             \
      ecs_edge_t var = edge_list->edges[var##_i];                              \
      __VA_ARGS__                                                              \
    }                                                                          \
  } while (0)

  // -- ARCHETYPE --------------------------------------------------------------
  // graph vertex. archetypes are tables where columns represent component data
  // and rows represent each entity. left edges point to other archetypes with
  // one less component, and right edges point to archetypes that store one
  // additional component.

  typedef struct ecs_archetype_t ecs_archetype_t;

  ecs_archetype_t *ecs_archetype_new(ecs_type_t *type,
                                     const ecs_map_t *component_index,
                                     ecs_map_t *type_index);
  void ecs_archetype_free(ecs_archetype_t *archetype);
  uint32_t ecs_archetype_add(ecs_archetype_t *archetype,
                             const ecs_map_t *component_index,
                             ecs_map_t *entity_index, ecs_entity_t e);
  uint32_t ecs_archetype_move_entity_right(ecs_archetype_t *left,
                                           ecs_archetype_t *right,
                                           const ecs_map_t *component_index,
                                           ecs_map_t *entity_index,
                                           uint32_t left_row);
  ecs_archetype_t *ecs_archetype_insert_vertex(ecs_archetype_t *root,
                                               ecs_archetype_t *left_neighbour,
                                               ecs_type_t *new_vertex_type,
                                               ecs_entity_t component_for_edge,
                                               const ecs_map_t *component_index,
                                               ecs_map_t *type_index);
  ecs_archetype_t *ecs_archetype_traverse_and_create(
      ecs_archetype_t *root, const ecs_type_t *type,
      const ecs_map_t *component_index, ecs_map_t *type_index);

#ifndef NDEBUG
  void ecs_archetype_inspect(ecs_archetype_t *archetype);
#endif

  // -- ENTITY COMPONENT SYSTEM ------------------------------------------------
  // functions below is the public api

  typedef struct ecs_view_t {
    void **component_arrays;
    uint32_t *indices;
  } ecs_view_t;

  typedef void (*ecs_system_fn)(ecs_view_t, uint32_t);

  typedef struct ecs_registry_t ecs_registry_t;

  ecs_registry_t *ecs_init(void);
  void ecs_destroy(ecs_registry_t *registry);
  ecs_entity_t ecs_entity(ecs_registry_t *registry);
  ecs_entity_t ecs_component(ecs_registry_t *registry, size_t component_size);
  ecs_entity_t ecs_system(ecs_registry_t *registry, ecs_signature_t *signature,
                          ecs_system_fn system);
  void ecs_attach(ecs_registry_t *registry, ecs_entity_t entity,
                  ecs_entity_t component);
  void ecs_set(ecs_registry_t *registry, ecs_entity_t entity,
               ecs_entity_t component, const void *data);
  void ecs_step(ecs_registry_t *registry);
  void *ecs_view(ecs_view_t view, uint32_t row, uint32_t column);

#define ECS_COMPONENT(registry, T) ecs_component(registry, sizeof(T));
#define ECS_SYSTEM(registry, system, n, ...)                                   \
  ecs_system(registry, ecs_signature_new_n(n, __VA_ARGS__), system)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ECS_H
