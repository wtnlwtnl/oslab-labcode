#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

static void
SJF_init(struct run_queue *rq)
{
    list_init(&(rq->run_list));
    rq->proc_num = 0;
}

static void
SJF_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(list_empty(&(proc->run_link)));
    list_add_before(&(rq->run_list), &(proc->run_link));
    proc->rq = rq;
    rq->proc_num ++;
}

static void
SJF_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));
    rq->proc_num --;
}

static struct proc_struct *
SJF_pick_next(struct run_queue *rq)
{
    list_entry_t *le = list_next(&(rq->run_list));
    if (le == &(rq->run_list)) {
        return NULL;
    }

    struct proc_struct *min_proc = le2proc(le, run_link);
    struct proc_struct *p;
    
    // Iterate through the list to find the process with the smallest 'lab6_priority' (burst time)
    while ((le = list_next(le)) != &(rq->run_list)) {
        p = le2proc(le, run_link);
        if (p->lab6_priority < min_proc->lab6_priority) {
            min_proc = p;
        }
    }
    return min_proc;
}

static void
SJF_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    // SJF is non-preemptive, so we don't need to check time slice here.
    // The process will run until it yields or exits.
}

struct sched_class sjf_sched_class = {
    .name = "SJF_scheduler",
    .init = SJF_init,
    .enqueue = SJF_enqueue,
    .dequeue = SJF_dequeue,
    .pick_next = SJF_pick_next,
    .proc_tick = SJF_proc_tick,
};
