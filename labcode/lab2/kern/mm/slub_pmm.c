#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <slub_pmm.h>
#include <stdio.h>

const struct pmm_manager *lower_pmm_manager;
static list_entry_t kmem_cache_list;
static struct kmem_cache_cpu kmem_cache_cpu, kmem_cpu_cpu, kmem_node_cpu;
static struct kmem_cache_node kmem_cache_node, kmem_cpu_node, kmem_node_node;
static struct kmem_cache kmem_cache, kmem_cpu, kmem_node;
struct kmem_cache *kmem_cache_allocater, *kmem_cache_cpu_allocater, *kmem_cache_node_allocater;

static struct slab*
get_new_slab(struct Page *page, size_t align) {
    uintptr_t page_pa = page2pa(page);
    uintptr_t page_va = (uintptr_t)KADDR(page_pa);
    struct slab *new_slab = (struct slab*)page_va;
    new_slab->page = page;
    new_slab->inuse = 0;
    new_slab->nr_free_objs = (PGSIZE - sizeof(struct slab)) / align;
    list_init(&(new_slab->slab_list));
    page_va += sizeof(struct slab);
    memset((char*)page_va, 0, PGSIZE - sizeof(struct slab));
    for(int i = 0; i < new_slab->nr_free_objs; ++i) {
        if(i == new_slab->nr_free_objs - 1)
            ((free_object_t*)(page_va + i * align))->next = NULL;
        else 
            ((free_object_t*)(page_va + i * align))->next = (free_object_t*)(page_va + (i + 1) * align);
    }
    new_slab->free_obj = (free_object_t*)(page_va);
    return new_slab;
}

static void
kmem_cache_node_init(struct kmem_cache_node *kmem_node) {
    kmem_node->nr_partial = 0;
    list_init(&(kmem_node->slab_link));
}

static void
kmem_cache_cpu_init(struct kmem_cache_cpu *kmem_cpu) {
    kmem_cpu->freeobjs = 0;
    kmem_cpu->page=NULL;
    list_init(&(kmem_cpu->partial_link));
}

static void
kmem_cache_init(struct kmem_cache *kmem, char *name, size_t size) {
    kmem->cpu_partial = 128;
    kmem->min_partial = 8;
    kmem->inuse = 0;
    kmem->size = size;
    kmem->name = name;
    kmem->align = ROUNDUP(size + sizeof(free_object_t), sizeof(char*));
    list_init(&(kmem->cache_link));
    list_add_before(&kmem_cache_list, &(kmem->cache_link));
    kmem_cache_cpu_init(kmem->kmem_cpu);
    kmem_cache_node_init(kmem->kmem_node);
}

static void
slub_init(void) {
    lower_pmm_manager = &buddy_system_pmm_manager;
    lower_pmm_manager->init();
    list_init(&kmem_cache_list);

    kmem_cache_allocater = &kmem_cache;
    kmem_cache_cpu_allocater = &kmem_cpu;
    kmem_cache_node_allocater = &kmem_node;

    kmem_cache.kmem_cpu = &kmem_cache_cpu;
    kmem_cache.kmem_node = &kmem_cache_node;
    kmem_cpu.kmem_cpu = &kmem_cpu_cpu;
    kmem_cpu.kmem_node = &kmem_cpu_node;
    kmem_node.kmem_cpu = &kmem_node_cpu;
    kmem_node.kmem_node = &kmem_node_node;
    kmem_cache_init(&kmem_cache, "kmem_cache_allocater", sizeof(struct kmem_cache));
    kmem_cache_init(&kmem_cpu, "kmem_cache_cpu_allocater", sizeof(struct kmem_cache_cpu));
    kmem_cache_init(&kmem_node, "kmem_cache_node_allocater", sizeof(struct kmem_cache_node));
}

static void
slub_init_memmap(struct Page *base, size_t n) {
    lower_pmm_manager->init_memmap(base, n);
}

static struct Page*
slub_alloc_pages(size_t n) {
    return lower_pmm_manager->alloc_pages(n);
}

static void
slub_free_pages(struct Page *base, size_t n) {
    lower_pmm_manager->free_pages(base, n);
}

static size_t
slub_nr_free_pages(void) {
    return lower_pmm_manager->nr_free_pages();
}

void *kmem_cache_alloc(struct kmem_cache *kc) {
    struct slab* retslab = NULL;
    struct kmem_cache_cpu *kmem_cpu = kc->kmem_cpu;
    struct kmem_cache_node *kmem_node = kc->kmem_node;
    if(kmem_cpu->page == NULL) { // 初始时，kmem_cpu上没有slab
        struct Page *new_page = lower_pmm_manager->alloc_page();
        kmem_cpu->page = get_new_slab(new_page, kc->align);
        kmem_cpu->freeobjs += kmem_cpu->page->nr_free_objs;
        retslab = kmem_cpu->page;
    } else if(kmem_cpu->page->free_obj != NULL) { // kmem_cpu上的slab有obj
        retslab = kmem_cpu->page;
    } else if(!list_empty(&(kmem_cpu->partial_link))) { // kmem_cpu上的slab无空余obj，从cpu_partial_list上找
        kmem_cpu->page = le2slab(list_next(&(kmem_cpu->partial_link)), slab_list);
        list_del(&(kmem_cpu->page->slab_list));
        kmem_cpu->freeobjs -= kmem_cpu->page->nr_free_objs;
        retslab = kmem_cpu->page;
    } else if(!list_empty(&(kmem_node->slab_link))) { // kmem_cpu的partial_list上也没有空余slab，去kmem_node->partial中寻找
        kmem_cpu->page = le2slab(list_next(&(kmem_node->slab_link)), slab_list);
        list_del(&(kmem_cpu->page->slab_list));
        kmem_node->nr_partial--;
        retslab = kmem_cpu->page;
    } else { // kmem_node上也没有了，重新从 buddy 中申请一页
        struct Page *new_page = lower_pmm_manager->alloc_page();
        kmem_cpu->page = get_new_slab(new_page, kc->align);
        kmem_cpu->freeobjs += kmem_cpu->page->nr_free_objs;
        retslab = kmem_cpu->page;
    }
    if(retslab == NULL) return NULL;
    uintptr_t ret = (uintptr_t)(retslab->free_obj);
    retslab->nr_free_objs--;
    retslab->inuse++;
    retslab->free_obj = retslab->free_obj->next;
    kc->inuse++;
    return (char*)(ret + sizeof(free_object_t));
}

void kmem_cache_free(struct kmem_cache *kc, void *o) {
    struct slab *slab = (struct slab*)ROUNDDOWN(o, PGSIZE);
    free_object_t *obj = (free_object_t*)((uintptr_t)o - sizeof(free_object_t));
    struct kmem_cache_cpu *kmem_cpu = kc->kmem_cpu;
    struct kmem_cache_node *kmem_node = kc->kmem_node;
    obj->next = slab->free_obj;
    slab->free_obj = obj;
    slab->nr_free_objs++;
    slab->inuse--;
    kc->inuse--;
    if(slab->nr_free_objs == 1) { //原先是满的slab
        if(kmem_cpu->freeobjs > kc->cpu_partial) { //清空 cpu partial，将slab加入cpu partial
            for(list_entry_t *le; !list_empty(&(kmem_cpu->partial_link)); ) {
                le = list_next(&(kmem_cpu->partial_link));
                kmem_cpu->freeobjs-=le2slab(le, slab_list)->nr_free_objs;
                list_del(le);
                list_add(&(kmem_node->slab_link), le);
                kmem_node->nr_partial++;
            }
        } else { //直接将slab加入cpu partial
            list_add(&(kmem_cpu->partial_link), &(slab->slab_list));
        }
        kmem_cpu->freeobjs++;
    } else if(slab->inuse == 0 && kmem_node->nr_partial > kc->min_partial) {
        bool flag = 0; //检测slab是否属于kmem_node，若属于，则需要将这个slabfree掉。
        for(list_entry_t *le = list_next(&(kmem_node->slab_link)); le != &(kmem_node->slab_link); le = list_next(le))
            if(le == &(slab->slab_list)) { flag = 1; break; }
        if(!flag) return;
        list_del(&(slab->slab_list));
        lower_pmm_manager->free_page(slab->page);
    }
}

struct kmem_cache *kmem_cache_create(char *name, size_t size) {
    struct kmem_cache *new_kmem_cache = kmem_cache_alloc(kmem_cache_allocater);
    new_kmem_cache->kmem_cpu = kmem_cache_alloc(kmem_cache_cpu_allocater);
    new_kmem_cache->kmem_node = kmem_cache_alloc(kmem_cache_node_allocater);
    kmem_cache_init(new_kmem_cache, name, size);
    return new_kmem_cache;
}

void kmem_cache_destroy(struct kmem_cache *kc) {
    struct kmem_cache_cpu *kmem_cpu = kc->kmem_cpu;
    struct kmem_cache_node *kmem_node = kc->kmem_node;
    assert(kc->inuse == 0);
    if(kmem_cpu->page != NULL) {
        assert(kmem_cpu->page->inuse == 0);
        for(list_entry_t *le = list_next(&(kmem_cpu->partial_link)); le != &(kmem_cpu->partial_link); le = list_next(le))
            assert(le2slab(le, slab_list)->inuse==0);
        for(list_entry_t *le = list_next(&(kmem_node->slab_link)); le != &(kmem_node->slab_link); le = list_next(le))
            assert(le2slab(le, slab_list)->inuse==0);
        for(list_entry_t *le; !list_empty(&(kmem_cpu->partial_link)); ) {
            le = list_next(&(kmem_cpu->partial_link));
            lower_pmm_manager->free_page(le2slab(le, slab_list)->page);
        }
        for(list_entry_t *le; !list_empty(&(kmem_node->slab_link)); ) {
            le = list_next(&(kmem_node->slab_link));
            lower_pmm_manager->free_page(le2slab(le, slab_list)->page);
        }
        lower_pmm_manager->free_page(kmem_cpu->page->page);
    }
    list_del(&(kc->cache_link));
    kmem_cache_free(kmem_cache_node_allocater, kmem_cpu);
    kmem_cache_free(kmem_cache_cpu_allocater, kmem_cpu);
    kmem_cache_free(kmem_cache_allocater, kc);
}

static void 
slub_check(void) {
    lower_pmm_manager->check();
    struct kmem_cache *kmem64 = kmem_cache_create("testint64", sizeof(int64_t));
    struct kmem_cache *kmem32 = kmem_cache_create("testint32", sizeof(int32_t));
    struct Page *pagea = alloc_pages(10);
    struct Page *pageb = alloc_pages(10);
    assert(pagea != NULL);
    assert(pageb != NULL);
    int64_t **a = KADDR(page2pa(pagea));
    int32_t **b = KADDR(page2pa(pageb));
    for(int i = 0; i <= 1000; ++i) {
        assert((a[i] = kmem_cache_alloc(kmem64)) != NULL);
        assert((b[i] = kmem_cache_alloc(kmem32)) != NULL);
        assert(PADDR(a[i]) < npage * PGSIZE);
        assert(PADDR(b[i]) < npage * PGSIZE);
        *a[i] = i;
        *b[i] = 114514;
    }
    for(int i = 0; i <= 1000; ++i) {
        assert(*a[i] == i);
        assert(*b[i] == 114514);
    }
    for(int i = 0; i <= 1000; i += 2) {
        kmem_cache_free(kmem64, a[i]);
        kmem_cache_free(kmem32, b[i]);
    }   
    for(int i = 0; i <= 1000; i += 2) {
        assert((a[i] = kmem_cache_alloc(kmem64)) != NULL);
        assert((b[i] = kmem_cache_alloc(kmem32)) != NULL);
        assert(PADDR(a[i]) < npage * PGSIZE);
        assert(PADDR(b[i]) < npage * PGSIZE);
        *a[i] = i;
        *b[i] = 114514;
    }
    for(int i = 0; i <= 1000; ++i) {
        assert(*a[i] == i);
        assert(*b[i] == 114514);
    }
    for(int i = 0; i <= 1000; i ++) {
        kmem_cache_free(kmem64, a[i]);
        kmem_cache_free(kmem32, b[i]);
    }    
    for(int i = 0; i <= 1000; i ++) {
        assert((a[i] = kmem_cache_alloc(kmem64)) != NULL);
        assert((b[i] = kmem_cache_alloc(kmem32)) != NULL);
        assert(PADDR(a[i]) < npage * PGSIZE);
        assert(PADDR(b[i]) < npage * PGSIZE);
        *a[i] = i;
        *b[i] = 114514;
    }
    for(int i = 0; i <= 1000; ++i) {
        assert(*a[i] == i);
        assert(*b[i] == 114514);
    }
    for(int i = 0; i <= 1000; i ++) {
        kmem_cache_free(kmem64, a[i]);
        kmem_cache_free(kmem32, b[i]);
    }    
    kmem_cache_destroy(kmem64);
    kmem_cache_destroy(kmem32);
    cprintf("Slub checked!!\n");
}


const struct pmm_manager slub_pmm_manager = {
    .name = "slub_manager",
    .init = slub_init,
    .init_memmap = slub_init_memmap,
    .alloc_pages = slub_alloc_pages,
    .free_pages = slub_free_pages,
    .nr_free_pages = slub_nr_free_pages,
    .check = slub_check,
    // .kmem_cache_alloc = kmem_cache_alloc,
    // .kmem_cache_create = kmem_cache_create,
    // .kmem_cache_free = kmem_cache_free,
    // .kmem_cache_destroy = kmem_cache_destroy,
};

