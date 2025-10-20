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
    if (page != NULL) {
        list_del(&(page->page_link));
        --nr_free(log);
        while(log && n <= (1 << (log - 1))) {
            ++nr_free(--log);
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
buddy_check(void) {
    struct Page *page1 = alloc_page(), *page4 = alloc_pages(4);
    assert(page1 != NULL);
    assert(page4 != NULL);
    struct Page **p = KADDR(page2pa(page4));
    struct Page **p0 = KADDR(page2pa(page1)), *p1 = NULL;
    size_t total = buddy_system_nr_free_pages(), tmp = total;

    // basic test
    assert((p1 = alloc_pages(32)) != NULL);
    assert(page_ref(p1) == 0);
    assert(page2pa(p1) < npage * PGSIZE);
    assert((total -= 32) == buddy_system_nr_free_pages());
    int ul = 0;
    for(int i = 15; ~i; --i) {
        p0[i] = alloc_pages(1 << i);
        if(p0[i] == NULL) continue;
        assert(page_ref(p0[i]) == 0);
        assert(page2pa(p0[i]) < npage * PGSIZE);
        assert((total -= p0[i]->property) == buddy_system_nr_free_pages());
        ul += 1<<i;
    }
    total += 32;
    free_pages(p1, 32);
    assert(total == buddy_system_nr_free_pages());
    for(int i = 0; i < 32; ++i) {
        assert((p[i] = alloc_page()) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        assert((total -= p[i]->property) == buddy_system_nr_free_pages());
    }
    for(int i = 0; i < 32; i++) {
        total += p[i]->property;
        free_pages(p[i], p[i]->property);
        assert(total == buddy_system_nr_free_pages());
    }
    assert((p1 = alloc_pages(32)) != NULL);
    assert(page_ref(p1) == 0);
    assert(page2pa(p1) < npage * PGSIZE);
    assert((total -= p1->property) == buddy_system_nr_free_pages());
    assert(alloc_page() == NULL);
    for(int i = 0; i <= 15; ++i) if(p0[i] != NULL) {
        total += 1 << i;
        assert(p0[i]->property == 1 << i);
        free_pages(p0[i], 1 << i);
        assert(total == buddy_system_nr_free_pages());
    }
    total += 32;
    assert(p1->property == 32);
    free_pages(p1, 32);
    assert(total == buddy_system_nr_free_pages());
    assert(total == tmp);

    //small test
    for(int i = 0; i < 100; ++i) {
        assert((p[i] = alloc_pages((i * 5) % 128 + 1)) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        assert((total -= p[i]->property) == buddy_system_nr_free_pages());
    }
    for(int i = 100; i < 1000; ++i) {
        assert((p[i] = alloc_page()) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        assert((total -= p[i]->property) == buddy_system_nr_free_pages());
    }
    for(int i = 0; i < 1000; ++i) {
        for(int j = i + 1; j < 1000; ++j) {
            assert(p[i] != p[j]);
        }
    }
    for(int i = 0; i < 100; i += 2) {
        total += p[i]->property;
        free_pages(p[i], p[i]->property);
        assert(total == buddy_system_nr_free_pages());
    }
    for(int i = 100; i < 200; i += 2) {
        total += p[i]->property;
        free_pages(p[i], p[i]->property);
        assert(total == buddy_system_nr_free_pages());
    }
    for(int i = 0; i < 100; i += 2) {
        assert((p[i] = alloc_pages((i * 5) % 128 + 1)) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        assert((total -= p[i]->property) == buddy_system_nr_free_pages());
    }
    for(int i = 100; i < 200; i += 2) {
        assert((p[i] = alloc_page()) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        assert((total -= p[i]->property) == buddy_system_nr_free_pages());
    }
    for(int i = 200; i < 1000; ++i) {
        total += p[i]->property;
        free_pages(p[i], p[i]->property);
        assert(total == buddy_system_nr_free_pages());
    }
    for(int i = 200; i < 1000; ++i) {
        assert((p[i] = alloc_page()) != NULL);
        assert(page_ref(p[i]) == 0);
        assert(page2pa(p[i]) < npage * PGSIZE);
        assert((total -= p[i]->property) == buddy_system_nr_free_pages());
    }
    for(int i = 0; i < 1000; ++i) {
        for(int j = i + 1; j < 1000; ++j) {
            assert(p[i] != p[j]);
        }
    }
    for(int i = 0; i < 1000; ++i) {
        total += p[i]->property;
        free_pages(p[i], p[i]->property);
        assert(total == buddy_system_nr_free_pages());
    }
    assert(total == tmp);
    cprintf("Buddy system checked!\n");
    free_page(page1);
    free_pages(page4, 4);
    assert(total + 5 == buddy_system_nr_free_pages());
}

const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_manager",
    .init = buddy_system_init,
    .init_memmap = buddy_system_init_memmap,
    .alloc_pages = buddy_system_alloc_pages,
    .free_pages = buddy_system_free_pages,
    .nr_free_pages = buddy_system_nr_free_pages,
    .check = buddy_check,
};

