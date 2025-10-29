#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <stdio.h>

static free_area_t free_area[15];//声明15个空闲区域管理结构，对应伙伴系统的15个级别
static struct Page *page_base = NULL;//保存整个页面数组的基地址指针，用作所有页面地址计算的参考点
static uint32_t nr_free = 0;//记录系统中所有空闲页面的总数量

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
buddy_system_init_memmap(struct Page *base, size_t n) {//定义伙伴系统内存映射初始化函数，参数base是页面数组起始地址，n是页面数量
    assert(n > 0);
    struct Page *p = base;
    page_base = base;//保存页面数组的基地址到全局变量
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;// 清空页面的标志位和属性字段，重置页面状态
        set_page_ref(p, 0);
    }
    nr_free += n;//总空闲页增多
    for(int i = 0; i < 15; ++i) if((n >> i) & 1) {
        struct Page *q = base + (n -= 1 << i);//计算当前要创建的空闲块起始位置，同时从n中减去块大小
        q->property = 1 << i;
        SetPageProperty(q);// 标记该页面为空闲块的头页面
        list_add(&free_list(i), &(q->page_link));//将空闲块加入到第i级空闲链表中
        ++ nr_free(i);//增加第i级空闲块的计数
    }
}

static struct Page *
buddy_system_alloc_pages(size_t n) {//定义伙伴系统分配函数，参数n是请求的页面数，返回分配的页面指针
    assert(n > 0);//确保请求分配的页面数大于0
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;//初始化页面指针为NULL，用于存储找到的空闲页面
    uint8_t log = 0;//记录找到的空闲块的级别
    for(int i = 0; i < 15; ++i) {
        if(n <= (1 << i) && nr_free(i)) {//检查当前级别的块大小是否足够且该级别有空闲块可用
            page = le2page(list_next(&free_list(log = i)), page_link);// 获取第i级空闲链表的第一个页面
            break;
        }
    }
    if (page != NULL) {
        list_del(&(page->page_link));//将找到的页面从空闲链表中移除
        --nr_free(log);//将对应级别的空闲块计数减1
        while(log && n <= (1 << (log - 1))) {// 当块可以继续分割且分割后仍能满足需求时进入分割循环
            ++nr_free(--log);//块大小减半，同时增加新级别的空闲块计数
            struct Page *p = page + (1 << log);//计算分割后右半部分（伙伴块）的起始页面地址
            p->property = (1 << log);//设置伙伴块的大小属性为2^log页
            p->flags = 0;//清空伙伴块的标志位
            set_page_ref(p, 0);//将伙伴块的引用计数设置为0
            SetPageProperty(p);//标记伙伴块为空闲状态
            list_add_before(&free_list(log), &(p->page_link));//将伙伴块加入到对应级别的空闲链表中
        }
        nr_free -= 1 << log;//从总空闲页数中减去实际分配的页面数
        page->property = 1 << log;//设置分配页面的大小属性为实际分配的页面数
        ClearPageProperty(page);//清除分配页面的空闲标记，表示该页面已被分配
    }
    return page;
}

static void
buddy_system_free_pages(struct Page *base, size_t n) {//定义伙伴系统释放函数，参数base是要释放的页面起始地址，n是释放的页面数量
    assert(n > 0);
    size_t page_id = base - page_base;//计算要释放页面在整个页面数组中的索引ID
    base->property = n;//设置释放块的大小属性为n页
    SetPageProperty(base);//标记该页面为空闲块的头页面
    nr_free += n;

    for(int i = 0; i < 15; ++i) if((n >> i) & 1){
        size_t buddy_page_id = page_id ^ (1 << i);
        struct Page *buddy_page = page_base + buddy_page_id;
        if(PageProperty(buddy_page) && buddy_page->property == base->property) {// 检查伙伴页面是否空闲
            if(buddy_page_id < page_id)
                page_id = buddy_page_id;
            base = page_base + page_id;// 更新base指针指向合并后块的起始页面
            base->property = 2 << i;
            list_del(&(buddy_page->page_link));//将伙伴块从其所在的空闲链表中移除
            --nr_free(i);
            ClearPageProperty(base + (1 << i));// 清除合并块右半部分的空闲标记，只保留左半部分标记
            n <<= 1;
        } else {
            list_add_before(&free_list(i), &(base->page_link));//将当前块加入到第i级空闲链表中
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

