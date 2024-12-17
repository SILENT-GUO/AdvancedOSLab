# CS 6210 Assignment 1 Report
### Author: Zebin Guo
### GTID: 904054219

## Section 1: The Understanding of GTThreads Package.
This package is a package for multi-core multi-threading work. The entry point is the main function in ```gt_matrix.c``` file. After parsing the input argument and before initialize matrices, It will create kernel threads of number ```num_cpus```. After matrices initialization, it will create a number of uthreads for matrix multiplication work At the time of each uthread creation, it will find the target of kthread running queue, and it will be added to that run queue.

Lifecycle of kthreads: 
When the ```gtthread_app_init``` is called, the kthread will go over the following steps:
1. Bind ```gtthread_app_start``` function to ```kthread_app_func``` function.
2. In ```kthread_create``` function, the ```kthread_handler``` is called, which invoke ```kthread_app_func``` function.
3. The current kthread will go into a while loop, which calls ```uthread_schedule``` function to get the next uthread to run. And this is the essential function for uthread scheduling. This function is also called in ```gtthread_app_exit``` function, which is called after all uthreads are created.
4. After the return value of ```kthread_best_sched_uthread``` is NULL, the kthread_flags will be set to all 1, which means the kthread is idle.
5. If a kthread is idle, it will break the while loop in the  ```gtthread_app_exit``` function to exit the current kthread and waiting for all kthread to finish.

Lifecycle of uthreads:
When the ```uthread_create``` function is called, the uthread will go over the following steps:
1. After the uthread is created, it will be added to the run queue of the target kthread.
2. Calling ```uthread_schedule``` function to get the next uthread to run. O(1) priority scheduling is implemented in this function. Two queue of each kthread is maintained, one is the active queue, and the other is the expire queue. The uthread is chosen from the active queue based on priority. If the active queue is empty, the active queue will be swapped with the expire queue.
3. When some uthread is running, it will call ```uthread_yield``` function to yield the CPU to the scheduler.
4. If the running queue of the target kthread is empty, the the return value of ```kthread_best_sched_uthread``` is NULL.

The flowchart about the GTThreads package can be found in the Appendix A.

## Section 2: The Algorithm and Rules of Credit Scheduler
The credit scheduler is a scheduler that assigns credits to each uthread. The credit scheduler is implemented in the ```uthread_schedule``` function. The credit scheduler is based on the following rules:
1. Each uthread has a credit value initially.
2. When a uthread is running, it will be charged credits.
3. When the credit of a uthread is less than or equals 0, it will be moved to the expire queue.
4. To fully utilize the CPU, the credit of a uthread in expired queue will be recharged to the initial value.
5. When the active queue is empty, the active queue will be swapped with the expire queue.

## Section 3: My Design and Implementation of Credit Scheduler
After parsing the argument, we maintain a global variable ```is_credit_scheduler``` to judge whether the credit scheduler is enabled. If it is not enabled, the O(1) priority scheduler will be used. If it is enabled, the credit scheduler will be used. Each thread will be assigned a credit value based on the credit group. The credit value is initialized to the credit group value. I modify the ```uthread_create``` function to accept the credit as an argument. The credit value will be assigned to the credit group value. In ```uthread_schedule``` function, I deduct the credit based on following rules:
1. Every uthread will be charged the number of cpu cycles it used.
2. If the uthread invoke the ```gt_yield``` function, it will be charged the number of cpu cycles it used only. Otherwise, it will be charged additional 25 credits. 25 credits is chosen so that groups with lower credits will have the chance to run. This policy maintains the fairness between the uthreads with different credit groups.
3. As mentioned in section 2, if the credit of a uthread is less than or equals 0, it will be moved to the expire queue using ```add_to_runqueue(kthread_runq->expire_runq, &(kthread_runq->kthread_runqlock), u_obj)``` function. Otherwise, it will be added to the active queue using ```add_to_runqueue(kthread_runq->active_runq, &(kthread_runq->kthread_runqlock), u_obj)``` function.

## Section 4: The Design and Implementation of CPU Yield Function and Load Balancing
In my design, the gt_yield function holds for a particular matrix group for all credit cases. Specifically,  I require all uthreads which requires to perform $32*32$ matrix. As they are having the same initial credit group, and to prevent the starvation of the lower credit group, I will keep track of the gt_yield time every uthread makes. After 10 times, the gt_yield function is no longer available. This design may not be the best design in terms of performance, but it's meaningful to show the correctness of the load balancing implementation and gt_yield function, with more details illustrated in Section 6.

In gt_yield function, it will be charged the number of cpu cycles it used only. The credit will be deducted based on the number of cpu cycles it used. The ```gt_yield``` function is designed to hold another ```uthread_schedule``` function to schedule the next uthread to run.

In the load balancing part, I will check whether the kthread is idle. If the kthread is idle, I will migrate the uthread from the other kthread to the idle kthread.It is implemented in the ```sched_find_best_uthread``` function. After all kernel threads are ready, we will recurse all the other kernel thread to see if there are active uthreads. If there are, we will migrate the uthread to the idle kernel thread, and directly move to the process where the newly added uthread is retreieved from the active queue and consider as the next candidate to run.

## Section 5: Experiments Results
- For the csv table outputs, please refer to the ```Detailed_output_no_lb.csv``` and ```Detailed_output_lb.csv``` for detailed data, and ```Cumulative_output_no_lb.csv``` and ```Cumulative_output_lb.csv``` for the grouped data in the submission folder.

The grouped plot for the CPU time, Wait time, and Execution time for the credit scheduling without load balancing and with load balancing is attached below, which can also be found in ```Comparison_no_load_balancing_2x2.jpg``` and ```Comparison_load_balancing_2x2.jpg``` files:

1. **Without Load Balancing**  
![No Load Balancing](Comparison_no_load_balancing_2x2.jpg)

2. **With Load Balancing**  
![With Load Balancing](Comparison_load_balancing_2x2.jpg)

## Section 6: Result Analysis
- **Does credit scheduler work as expect?**
  
Yes, the credit scheduler works as expected. The uthreads with higher credit groups will have more chances to run. The uthreads with lower credit groups will have less chances to run. This can be seen from the grouped plot for the CPU time, Wait time, and Execution time for the credit scheduling without load balancing and with load balancing. In my design of the credit scheduler, if the uthreads preemptes not voluntarily, it will be charged additional 25 credits. This policy maintains the relative fairness between the uthreads with different credit groups. Although the uthreads with higher credit groups will have more chances to run, the uthreads with lower credit groups can prevent starvation, and can be scheduled to run after a certain period of time.

Note that it's normal to observe the CPU time, waiting time and execution time for matrix size 32 are larger than the other matrix sizes. The reason accounts for the volutary gt_yield function, which will cost way more waiting time for them to be scheduled.
- **Does the load balancing mechanism improve the performance?**

Yes, the load balancing mechanism improves the performance. The total execution time of uthreads with high credits generally require less time to finish their tasks with load balancing enabled. As illustrated above, when some kthread is idle, the uthreads in other kthreads will be migrated to the idle kthread. Especially for tasks with higher credits, they have larger chances to be migrated to the idle kthread. This will reduce the waiting time of the uthreads in the active queue, and the uthreads in the expire queue will have more chances to run. This will improve the performance of the credit scheduler, which is shown in the grouped plot.

I also put three bar plots in the folder, which are ```Comparison_cpu_time.jpg```, ```Comparison_wait_time.jpg```, and ```Comparison_execution_time.jpg```. The three plots show the comparison of CPU time, Wait time, and Execution time for the credit scheduling without load balancing and with load balancing. The result of the waiting time comparison plot reveals similar conclusions as well.
- **Does credit scheduler perform better than O(1) priority scheduler?**
  
Yes. O(1) Priority Scheduler is simple, fast, and efficient in systems where priorities are relatively static. However, it suffers from issues like priority inversion and starvation, and it doesn’t adapt well to cloud-based environments. My version of credit scheduler provides more dynamic resource allocation, better handling of multi-tenant environments, and improved fairness in resource distribution.



## Appendix A: Flowchart of GTThreads Package
1. Detailed flowchart of GTThreads package. You may found the original flowchart in the ```workflow_cs6210.png``` file.
![alt text](workflow_cs6210.png)

## Appendix B: screenshot of the printed credit before/after yield with the queue of uthread credits.
![alt text](1726116562592.png)
In the screenshot, the credit of the uthread is deducted by the actual CPU use, which is negligible because of the call of the ```gt_yield``` function.
## Appendix C: screenshot of the printed queue of uthread credits with load-balancing.
![alt text](1726116672579.png)
In the screenshot, the uthread is migrated from the other kthread to the idle kthread. Although this idle kthread has no uthread in the run queue, it will not quit if we can find an active uthread in other kthreads. The uthread is retrieved from the active queue and considered as the next candidate to run.


*Please note that the data in csv files may not match the data in the plots, as the author have done some testing after finishing this report.*