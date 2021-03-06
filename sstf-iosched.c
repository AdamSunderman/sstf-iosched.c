/*

        elevator sstf
 
*/
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel.h>

struct sstf_data 
{

        struct list_head queue;

};  


static void sstf_merged_requests(struct request_queue *q, struct request *rq, struct request *next)
{

        list_del_init(&next->queuelist);

}


static int sstf_dispatch(struct request_queue *q, int force)
{

        struct sstf_data *sd = q->elevator->elevator_data;
        struct request *rq;

        rq = list_first_entry_or_null(&sd->queue, struct request, queuelist);
        if (rq) {

                list_del_init(&rq->queuelist);
                // Insert rq into end of dispatch queue q
                elv_dispatch_add_tail(q, rq);

                return 1;
        }

        return 0;

}


static void sstf_add_request(struct request_queue *q, struct request *rq)
{

        struct sstf_data *sd = q->elevator->elevator_data;
        
        // If the list is empty add it anywhere
        if(list_empty(&sd->queue)){

        		list_add(&rq->queuelist, &sd->queue);

        }
        else{
        		// If not loop through the queue tracking the current entry
				struct list_head *current;
				list_for_each_safe(current, &sd->queue){

					// Check the next entry after current to see if this is the insertion point
					struct list_head *current_next = list_entry(current->next, struct request, queuelist);
					// If the next block is higher position
					if(blk_rq_pos(current_next) > blk_rq_pos(rq)){
							// Add the new element in between current and current_next
							list_add(&rq->queuelist, current, current_next)
							break;

					}
				}        		
        }
}


static struct request * sstf_former_request(struct request_queue *q, struct request *rq)
{

        struct sstf_data *sd = q->elevator->elevator_data;

        if (rq->queuelist.prev == &sd->queue)
                return NULL;
        return list_prev_entry(rq, queuelist);

}


static struct request * sstf_latter_request(struct request_queue *q, struct request *rq)
{

        struct sstf_data *sd = q->elevator->elevator_data;

        if (rq->queuelist.next == &sd->queue)
                return NULL;
        return list_next_entry(rq, queuelist);

}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{

        struct sstf_data *sd;
        struct elevator_queue *eq;

        eq = elevator_alloc(q, e);
        if (!eq)
                return -ENOMEM;

        sd = kmalloc_node(sizeof(*sd), GFP_KERNEL, q->node);
        if (!sd) {
                kobject_put(&eq->kobj);
                return -ENOMEM;
        }
        eq->elevator_data = sd;

        INIT_LIST_HEAD(&sd->queue);
        spin_lock_irq(q->queue_lock);
        q->elevator = eq;
        spin_unlock_irq(q->queue_lock);
        return 0;

}


static void sstf_exit_queue(struct elevator_queue *e)
{

        struct sstf_data *sd = e->elevator_data;

        BUG_ON(!list_empty(&sd->queue));
        kfree(sd);

}


static struct elevator_type elevator_sstf = {
        .ops = {
                .elevator_merge_req_fn		= sstf_merged_requests,
                .elevator_dispatch_fn		= sstf_dispatch,
                .elevator_add_req_fn		= sstf_add_request,
                .elevator_former_req_fn		= sstf_former_request,
                .elevator_latter_req_fn		= sstf_latter_request,
                .elevator_init_fn		    = sstf_init_queue,
                .elevator_exit_fn		    = sstf_exit_queue,
        },
        .elevator_name = "sstf",
        .elevator_owner = THIS_MODULE,
};


static int __init sstf_init(void)
{

        return elv_register(&elevator_sstf);

}


static void __exit sstf_exit(void)
{

        elv_unregister(&elevator_sstf);

}


module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Austin Liang, Tanner Rousseau, Adam Sunderman");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF LOOK IO scheduler");