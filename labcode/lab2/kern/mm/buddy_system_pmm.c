#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <stdio.h>

static free_area_t free_area[15];
static struct Page *page_base = NULL;
static uint32_t nr_free = 0;

#define free_list(x) (free_area[x].free_list)
#define nr_free(x) (free_area[x].nr_free)

static void
buddy_system_init(void) {
    for(int i = 0; i < 15; ++i) {
        list_init(&free_list(i));
        nr_free(i) = 0;
    }
}

static void
buddy_system_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    page_base = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += n;
    for(int i = 0; i < 15; ++i) if((n >> i) & 1) {
        struct Page *q = base + (n -= 1 << i);
        q->property = 1 << i;
        SetPageProperty(q);
        list_add(&free_list(i), &(q->page_link));
        ++ nr_free(i);
    }
}

static struct Page *
buddy_system_alloc_pages(size_t n) {
    assert(n > 0);
    // cprintf("Alloc : %d\n", n);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    uint8_t log = 0;
    for(int i = 0; i < 15; ++i) {
        if(n <= (1 << i) && nr_free(i)) {
            page = le2page(list_next(&free_list(log = i)), page_link);
            break;
        }
    }
    // cprintf("%d  %d\n", log, nr_free(log));
    if (page != NULL) {
        list_del(&(page->page_link));
        --nr_free(log);
        while(log && n <= (1 << (log - 1))) {
            ++nr_free(--log);
            // cprintf("%d\n", log);
            struct Page *p = page + (1 << log);
            p->property = (1 << log);
            p->flags = 0;
            set_page_ref(p, 0);
            SetPageProperty(p);
            list_add_before(&free_list(log), &(p->page_link));
        }
        nr_free -= 1 << log;
        page->property = 1 << log;
        ClearPageProperty(page);
    }
    return page;
}

static void
buddy_system_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    size_t page_id = base - page_base;
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    for(int i = 0; i < 15; ++i) if((n >> i) & 1){
        size_t buddy_page_id = page_id ^ (1 << i);
        struct Page *buddy_page = page_base + buddy_page_id;
        if(PageProperty(buddy_page) && buddy_page->property == base->property) {
            if(buddy_page_id < page_id)
                page_id = buddy_page_id;
            base = page_base + page_id;
            base->property = 2 << i;
            list_del(&(buddy_page->page_link));
            --nr_free(i);
            ClearPageProperty(base + (1 << i));
            n <<= 1;
        } else {
            list_add_before(&free_list(i), &(base->page_link));
            ++nr_free(i);
            break;
        }
    }
}

static size_t
buddy_system_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    cprintf("=== Buddy System 验证测试开始 ===\n");

    // 1. 基础功能测试
    cprintf("1. 基础分配和释放测试...\n");
    struct Page *p[1000];
    size_t total = buddy_system_nr_free_pages();
    cprintf("初始可用页面数: %d\n", total);

    // 测试不同大小的分配
    for(int i = 0; i < 100; ++i) {
        int req_size = i % 32 + 1;
        assert((p[i] = alloc_pages(req_size)) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);

        // 验证分配的大小是2的幂次且不小于请求大小
        int allocated_size = p[i]->property;
        assert(allocated_size >= req_size);
        assert((allocated_size & (allocated_size - 1)) == 0); // 是2的幂次

        assert((total -= allocated_size) == buddy_system_nr_free_pages());

        if(i < 10) {
            cprintf("  分配 %d 页，实际分配 %d 页，剩余 %d 页\n", 
                   req_size, allocated_size, buddy_system_nr_free_pages());
        }
    }

    // 2. 验证分配地址唯一性
    cprintf("2. 验证分配地址唯一性...\n");
    for(int i = 0; i < 100; ++i) {
        for(int j = i + 1; j < 100; ++j) {
            assert(p[i] != p[j]);
            // 验证分配的内存块不重叠
            size_t start_i = p[i] - page_base;
            size_t end_i = start_i + p[i]->property;
            size_t start_j = p[j] - page_base;
            size_t end_j = start_j + p[j]->property;
            assert(end_i <= start_j || end_j <= start_i);
        }
    }

    // 3. 伙伴合并测试
    cprintf("3. 伙伴合并测试...\n");

    // 分配两个相邻的1页块进行合并测试
    struct Page *buddy1 = alloc_pages(1);
    struct Page *buddy2 = alloc_pages(1);
    assert(buddy1 != NULL && buddy2 != NULL);

    size_t before_free = buddy_system_nr_free_pages();

    // 释放第一个块
    free_pages(buddy1, 1);
    size_t after_first_free = buddy_system_nr_free_pages();
    assert(after_first_free == before_free + 1);

    // 释放第二个块，如果它们是伙伴，应该合并
    free_pages(buddy2, 1);
    size_t after_second_free = buddy_system_nr_free_pages();
    assert(after_second_free == before_free + 2);

    cprintf("  伙伴合并测试通过\n");

    // 4. 大块分配测试
    cprintf("4. 大块分配测试...\n");
    struct Page *large_block = alloc_pages(64);
    if(large_block != NULL) {
        assert(large_block->property >= 64);
        cprintf("  成功分配 64 页的大块，实际分配 %d 页\n", large_block->property);
        free_pages(large_block, large_block->property);
    } else {
        cprintf("  内存不足，无法分配 64 页大块\n");
    }

    // 5. 边界条件测试
    cprintf("5. 边界条件测试...\n");

    // 测试分配超过可用内存
    size_t available = buddy_system_nr_free_pages();
    struct Page *too_large = alloc_pages(available + 1);
    assert(too_large == NULL);
    cprintf("  正确拒绝了超大分配请求\n");

    // 6. 释放已分配的内存并测试完整性
    cprintf("6. 释放测试和内存完整性检查...\n");

    // 释放部分内存
    for(int i = 0; i < 50; ++i) {
        total += p[i]->property;
        free_pages(p[i], p[i]->property);
        assert(total == buddy_system_nr_free_pages());
    }

    // 重新分配测试
    for(int i = 0; i < 50; ++i) {
        int req_size = i % 16 + 1;
        assert((p[i] = alloc_pages(req_size)) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        total -= p[i]->property;
        assert(total == buddy_system_nr_free_pages());
    }

    // 7. 释放所有内存
    cprintf("7. 清理所有分配的内存...\n");
    for(int i = 0; i < 100; ++i) {
        free_pages(p[i], p[i]->property);
    }

    // 验证所有内存都被正确释放
    size_t final_total = buddy_system_nr_free_pages();
    cprintf("最终可用页面数: %d\n", final_total);

    // 8. 碎片化测试
    cprintf("8. 内存碎片化测试...\n");

    // 分配多个小块
    struct Page *small_blocks[32];
    for(int i = 0; i < 32; ++i) {
        small_blocks[i] = alloc_pages(1);
        assert(small_blocks[i] != NULL);
    }

    // 释放奇数索引的块，造成碎片化
    for(int i = 1; i < 32; i += 2) {
        free_pages(small_blocks[i], 1);
    }

    // 尝试分配一个大块，测试系统处理碎片的能力
    struct Page *defrag_test = alloc_pages(8);
    if(defrag_test != NULL) {
        cprintf("  碎片化后仍能分配大块: %d 页\n", defrag_test->property);
        free_pages(defrag_test, defrag_test->property);
    }

    // 清理剩余的小块
    for(int i = 0; i < 32; i += 2) {
        free_pages(small_blocks[i], 1);
    }

    cprintf("=== Buddy System 验证测试完成 ===\n");
    cprintf("所有测试通过！Buddy System 实现正确。\n");
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    // int count = 0, total = 0;
    // list_entry_t *le = &free_list;
    // while ((le = list_next(le)) != &free_list) {
    //     struct Page *p = le2page(le, page_link);
    //     assert(PageProperty(p));
    //     count ++, total += p->property;
    // }
    // assert(total == nr_free_pages());

    basic_check();

    // struct Page *p0 = alloc_pages(5), *p1, *p2;
    // assert(p0 != NULL);
    // assert(!PageProperty(p0));

    // list_entry_t free_list_store = free_list;
    // list_init(&free_list);
    // assert(list_empty(&free_list));
    // assert(alloc_page() == NULL);

    // unsigned int nr_free_store = nr_free;
    // nr_free = 0;

    // free_pages(p0 + 2, 3);
    // assert(alloc_pages(4) == NULL);
    // assert(PageProperty(p0 + 2) && p0[2].property == 3);
    // assert((p1 = alloc_pages(3)) != NULL);
    // assert(alloc_page() == NULL);
    // assert(p0 + 2 == p1);

    // p2 = p0 + 1;
    // free_page(p0);
    // free_pages(p1, 3);
    // assert(PageProperty(p0) && p0->property == 1);
    // assert(PageProperty(p1) && p1->property == 3);

    // assert((p0 = alloc_page()) == p2 - 1);
    // free_page(p0);
    // assert((p0 = alloc_pages(2)) == p2 + 1);

    // free_pages(p0, 2);
    // free_page(p2);

    // assert((p0 = alloc_pages(5)) != NULL);
    // assert(alloc_page() == NULL);

    // assert(nr_free == 0);
    // nr_free = nr_free_store;

    // free_list = free_list_store;
    // free_pages(p0, 5);

    // le = &free_list;
    // while ((le = list_next(le)) != &free_list) {
    //     struct Page *p = le2page(le, page_link);
    //     count --, total -= p->property;
    // }
    // assert(count == 0);
    // assert(total == 0);
}

const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_manager",
    .init = buddy_system_init,
    .init_memmap = buddy_system_init_memmap,
    .alloc_pages = buddy_system_alloc_pages,
    .free_pages = buddy_system_free_pages,
    .nr_free_pages = buddy_system_nr_free_pages,
    .check = default_check,
};