#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "gt_include.h"


#define ROWS 256
#define COLS ROWS
#define SIZE COLS

#define NUM_CPUS 4
#define NUM_GROUPS NUM_CPUS
#define PER_GROUP_COLS (SIZE/NUM_GROUPS)

#define NUM_THREADS 128
#define PER_THREAD_ROWS (SIZE/NUM_THREADS)

#define GT_THREADS 1

int is_credit_scheduler = 0;
int is_load_balancing = 0;

void write_to_csv(uthread_timer_t **data_list);


/* A[SIZE][SIZE] X B[SIZE][SIZE] = C[SIZE][SIZE]
 * Let T(g, t) be thread 't' in group 'g'. 
 * T(g, t) is responsible for multiplication : 
 * A(rows)[(t-1)*SIZE -> (t*SIZE - 1)] X B(cols)[(g-1)*SIZE -> (g*SIZE - 1)] */

typedef struct matrix
{
	int m[SIZE][SIZE];

	int rows;
	int cols;
	unsigned int reserved[2];
} matrix_t;


typedef struct __uthread_arg
{
	matrix_t *_A, *_B, *_C;
	unsigned int reserved0;
	int matrix_size;
	unsigned int tid;
	unsigned int gid;
	int start_row; /* start_row -> (start_row + PER_THREAD_ROWS) */
	int start_col; /* start_col -> (start_col + PER_GROUP_COLS) */

}uthread_arg_t;

struct timeval tv1;

static void generate_matrix(matrix_t *mat, int val, int matrix_size)
{

	int i,j;
	mat->rows = matrix_size;
	mat->cols = matrix_size;
	for(i = 0; i < mat->rows;i++)
		for( j = 0; j < mat->cols; j++ )
		{
			mat->m[i][j] = val;
		}
	return;
}

static void print_matrix(matrix_t *mat)
{
	int i, j;

	for(i=0;i<SIZE;i++)
	{
		for(j=0;j<SIZE;j++)
			printf(" %d ",mat->m[i][j]);
		printf("\n");
	}

	return;
}

extern int uthread_create(uthread_t *, void *, void *, uthread_group_t, int);

static void * uthread_mulmat(void *p)
{
	int i, j, k;
	int start_row, end_row;
	int start_col, end_col;
	unsigned int cpuid;
	int num_yield = 0;
	struct timeval tv2;

#define ptr ((uthread_arg_t *)p)

	i=0; j= 0; k=0;

	start_row = 0;
	end_row = ptr->matrix_size;

#ifdef GT_GROUP_SPLIT
	start_col = ptr->start_col;
	end_col = (ptr->start_col + PER_THREAD_ROWS);
#else
	start_col = 0;
	end_col = ptr->matrix_size;
#endif

#ifdef GT_THREADS
	cpuid = kthread_cpu_map[kthread_apic_id()]->cpuid;
	fprintf(stderr, "Thread(id:%d, group:%d, cpu:%d) started\n",ptr->tid, ptr->gid, cpuid);
#else
	fprintf(stderr, "\nThread(id:%d, group:%d) started",ptr->tid, ptr->gid);
#endif

	for(i = start_row; i < end_row; i++)
		for(j = start_col; j < end_col; j++)
			for(k = 0; k < SIZE; k++)
				if(is_credit_scheduler == 1 && ptr->tid <= 32 && num_yield < 10){
					// printf("Thread(id:%d, group:%d, cpu:%d) is yielding\n",ptr->tid, ptr->gid, cpuid);
					gt_yield();
					num_yield ++;
				}
				ptr->_C->m[i][j] += ptr->_A->m[i][k] * ptr->_B->m[k][j];

#ifdef GT_THREADS
	gettimeofday(&tv2,NULL);
	if(tv2.tv_usec > tv1.tv_usec)
		fprintf(stderr, "Thread(id:%d, group:%d, cpu:%d) finished (TIME : %lu s and %lu us)\n",
			ptr->tid, ptr->gid, cpuid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
	else
		fprintf(stderr, "Thread(id:%d, group:%d, cpu:%d) finished (TIME : %lu s and %lu us)\n",
			ptr->tid, ptr->gid, cpuid, (tv2.tv_sec - tv1.tv_sec - 1), (1000000 + tv2.tv_usec - tv1.tv_usec)); //prevent overflow
#else
	gettimeofday(&tv2,NULL);
	fprintf(stderr, "\nThread(id:%d, group:%d) finished (TIME : %lu s and %lu us)",
			ptr->tid, ptr->gid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
#endif

#undef ptr
	return 0;
}

matrix_t A, B, C;

static void init_matrices(int matrix_size)
{
	generate_matrix(&A, 1, matrix_size);
	generate_matrix(&B, 1, matrix_size);
	generate_matrix(&C, 0, matrix_size);

	return;
}


void parse_args(int argc, char* argv[])
{
	int inx;

	for(inx=0; inx<argc; inx++)
	{
		if(argv[inx][0]=='-')
		{
			if(!strcmp(&argv[inx][1], "lb"))
			{
				//TODO: add option of load balancing mechanism
				is_load_balancing = 1;
				printf("enable load balancing\n");
			}
			else if(!strcmp(&argv[inx][1], "s"))
			{
				//TODO: add different types of scheduler
				inx++;
				if(!strcmp(&argv[inx][0], "0"))
				{
					printf("use priority scheduler\n");
				}
				else if(!strcmp(&argv[inx][0], "1"))
				{
					is_credit_scheduler = 1;
					printf("use credit scheduler\n");
				}
			}
		}
	}

	return;
}



uthread_arg_t uargs[NUM_THREADS];
uthread_t utids[NUM_THREADS];

int main(int argc, char *argv[])
{
	uthread_arg_t *uarg;
	int inx;

	parse_args(argc, argv);

	gtthread_app_init();

	init_matrices(SIZE);

	gettimeofday(&tv1,NULL);
	int thread_id = 0;
	if(!is_credit_scheduler){
		int num_threads_per_size = NUM_THREADS / 4;
		for(int size = 32; size <= 256; size *= 2){

			for(inx=0; inx<num_threads_per_size; inx++)
			{
				uarg = &uargs[thread_id];
				uarg->_A = &A;
				uarg->_B = &B;
				uarg->_C = &C;
				uarg->tid = thread_id;
				uarg->gid = (thread_id % NUM_GROUPS);
				uarg->matrix_size = size;
				uthread_create(&utids[thread_id], uthread_mulmat, uarg, uarg->gid, 0);
				thread_id++;
			}
		}
	}else{
		int num_threads_per_size_per_credit = NUM_THREADS / 4 / 4;
		for(int credit = 25; credit <= 100; credit += 25)
		{
			for(int size = 32; size <= 256; size *= 2)
			{		
				for(inx=0; inx < num_threads_per_size_per_credit; inx++)
				{
					uarg = &uargs[thread_id];
					uarg->_A = &A;
					uarg->_B = &B;
					uarg->_C = &C;
					uarg->tid = thread_id;
					uarg->gid = (thread_id % NUM_GROUPS);
					uarg->matrix_size = size;
					uthread_create(&utids[thread_id], uthread_mulmat, uarg, uarg->gid, credit);
					thread_id++;
				}
			}
		}
	}

	gtthread_app_exit();

	write_to_csv(uthread_time);
	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
void write_to_csv(uthread_timer_t **data_list) {
    FILE *detailed_file;
    FILE *cumulative_file;

    // Open detailed output file based on load balancing
    if (is_load_balancing == 1) {
        detailed_file = fopen("Detailed_output_lb.csv", "w");
        cumulative_file = fopen("Cumulative_output_lb.csv", "w");
    } else {
        detailed_file = fopen("Detailed_output_no_lb.csv", "w");
        cumulative_file = fopen("Cumulative_output_no_lb.csv", "w");
    }

    if (detailed_file == NULL || cumulative_file == NULL) {
        printf("Error opening file!\n");
        return;
    }

    // Write headers to both files
    fprintf(detailed_file, "%s\t%s\t%s\t%s\t%s\n", 
            "group_name", "thread_number", "cpu_time(us)", "wait_time(us)", "exec_time(us)");
    fprintf(cumulative_file, "%s\t%s\t%s\t%s\n", 
            "group_name", "mean_cpu_time(us)", "mean_wait_time(us)", "mean_exec_time(us)");

    int m_values[] = {32, 64, 128, 256};
    int c_values[] = {25, 50, 75, 100};
    int m_index = 0, c_index = 0;

    // Variables to accumulate values for cumulative data
    long total_cpu_time_us = 0;
    long total_exec_time_us = 0;
    long total_wait_time_us = 0;

    for (int i = 0; i < 128; i++) {
        // Calculate time in microseconds for the current thread
        long cpu_time_us = data_list[i]->cpu_time.tv_sec * 1000000 + data_list[i]->cpu_time.tv_usec;
        long exec_time_us = data_list[i]->running_time.tv_sec * 1000000 + data_list[i]->running_time.tv_usec;
        long wait_time_us = exec_time_us - cpu_time_us;

        // Accumulate values for the current group of 8 threads
        total_cpu_time_us += cpu_time_us;
        total_exec_time_us += exec_time_us;
        total_wait_time_us += wait_time_us;

        // Write detailed data for each thread with no spacing
        fprintf(detailed_file, "c_%d_m_%d\t%d\t%ld\t%ld\t%ld\n", 
				c_values[c_index], m_values[m_index], i, cpu_time_us, wait_time_us, exec_time_us);
		
		if ((i+1) % 8 == 0) {
            
            // After processing 8 threads, calculate and write cumulative data
            long mean_cpu_time_us = total_cpu_time_us / 8;
            long mean_exec_time_us = total_exec_time_us / 8;
            long mean_wait_time_us = total_wait_time_us / 8;

            fprintf(cumulative_file, "c_%d_m_%d\t%ld\t%ld\t%ld\n", 
                    c_values[c_index], m_values[m_index], 
                    mean_cpu_time_us, mean_wait_time_us, mean_exec_time_us);

            // Reset totals for the next group
            total_cpu_time_us = 0;
            total_exec_time_us = 0;
            total_wait_time_us = 0;

			c_index++;
            if (c_index == 4) {
                c_index = 0;
            }
        }

		if (i % 32 == 0 && i > 0) {
            m_index++;
        }
    }

    fclose(detailed_file);
    fclose(cumulative_file);

    printf("Data written to CSV files\n");
}