#include "ecs.h"
#include "greatest.h"

TEST map_empty()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_free(map);
    PASS();
}

TEST map_set()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    int x = 10;
    ecs_map_set(map, 1, &x);
    ecs_map_free(map);
    PASS();
}

TEST map_get()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    int x = 10;
    ecs_map_set(map, 1, &x);
    int *val = ecs_map_get(map, 1);
    ASSERT(val != NULL);
    ASSERT_EQ(*val, 10);
    ecs_map_free(map);
    PASS();
}

TEST map_set_multiple()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 2, &(int){20});
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 10);
    ASSERT_EQ(*(int *)ecs_map_get(map, 2), 20);
    ecs_map_free(map);
    PASS();
}

TEST map_update()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 1, &(int){100});
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 100);
    ecs_map_free(map);
    PASS();
}

TEST map_remove()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_remove(map, 1);
    ASSERT_EQ_FMT(NULL, ecs_map_get(map, 1), "%p");
    ecs_map_free(map);
    PASS();
}

TEST map_set_multiple_and_remove()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 2, &(int){20});
    ecs_map_set(map, 3, &(int){30});
    ecs_map_remove(map, 3);
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 10);
    ASSERT_EQ(ecs_map_get(map, 3), NULL);
    ecs_map_free(map);
    PASS();
}

// NOTE: if ya changed the hash function this test breaks. duh.
TEST map_hash_collision()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    // for each key (hash(key) % 10) == 5
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 4, &(int){40});
    ecs_map_set(map, 26, &(int){260});
    ecs_map_set(map, 42, &(int){420});
    ecs_map_set(map, 44, &(int){440});
    ecs_map_remove(map, 26);
    ASSERT_EQ(ecs_map_get(map, 26), NULL);
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 10);
    ASSERT_EQ(*(int *)ecs_map_get(map, 44), 440);
    ecs_map_free(map);
    PASS();
}

TEST map_set_after_tomestone()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 4, &(int){40});
    ecs_map_set(map, 26, &(int){260});
    ecs_map_set(map, 44, &(int){440});
    ecs_map_remove(map, 26);
    ecs_map_set(map, 42, &(int){420});
    ASSERT_EQ(ecs_map_get(map, 26), NULL);
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 10);
    ASSERT_EQ(*(int *)ecs_map_get(map, 42), 420);
    ASSERT_EQ(*(int *)ecs_map_get(map, 44), 440);
    ecs_map_free(map);
    PASS();
}

TEST map_set_a_lot(const int count)
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);

    for (int i = 1; i < count; i++)
    {
        ecs_map_set(map, i, &(int){i * 10});
    }

    for (int i = 1; i < count; i++)
    {
        ASSERT_EQ(*(int *)ecs_map_get(map, i), i * 10);
    }

    ecs_map_free(map);
    PASS();
}

TEST map_remove_a_lot(const int count)
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);

    for (int i = 1; i < count; i++)
    {
        ecs_map_set(map, i, &(int){i * 10});
    }

    for (int i = 1; i + 1 < count / 2; i += 2)
    {
        ecs_map_remove(map, i);
    }

    for (int i = 1; i + 1 < count / 2; i += 2)
    {
        ASSERT_EQ_FMT(NULL, ecs_map_get(map, i), "%p");
        ASSERT_EQ(*(int *)ecs_map_get(map, i + 1), (i + 1) * 10);
    }

    ecs_map_free(map);
    PASS();
}

SUITE(map)
{
    RUN_TEST(map_empty);
    RUN_TEST(map_set);
    RUN_TEST(map_get);
    RUN_TEST(map_set_multiple);
    RUN_TEST(map_update);
    RUN_TEST(map_remove);
    RUN_TEST(map_set_multiple_and_remove);
    RUN_TEST(map_hash_collision);
    RUN_TEST(map_set_after_tomestone);
    for (int i = 10; i <= 100000; i *= 10)
    {
        RUN_TEST1(map_set_a_lot, i);
        RUN_TEST1(map_remove_a_lot, i);
    }
}

TEST ecs_minimal()
{
    ecs_world_t *world = ecs_init();
    ecs_destroy(world);
    PASS();
}

SUITE(ecs)
{
    RUN_TEST(ecs_minimal);
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(map);
    RUN_SUITE(ecs);
    GREATEST_MAIN_END();
}
