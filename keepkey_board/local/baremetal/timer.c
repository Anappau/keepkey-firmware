/*
 * This file is part of the KeepKey project.
 *
 * Copyright (C) 2015 KeepKey LLC
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/* === Includes ============================================================ */

#include <stddef.h>

#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/f2/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/cortex.h>

#include "keepkey_board.h"
#include "keepkey_leds.h"
#include "timer.h"

/* === Private Variables =================================================== */

static volatile uint32_t remaining_delay = UINT32_MAX;
static RunnableNode runnables[MAX_RUNNABLES];
static RunnableQueue free_queue = {NULL, 0};
static RunnableQueue active_queue = {NULL, 0};

/* === Private Functions =================================================== */

/*
 * runnable_queue_peek() - Get pointer to head node in task manager (queue)
 *
 * INPUT
 *     - queue: head pointer to linklist (queue)
 * OUTPUT
 *     head node in the queue
 */
static RunnableNode *runnable_queue_peek(RunnableQueue *queue)
{
    return(queue->head);
}

/*
 * runnable_queue_get() - Get the pointer to node that contains the
 * callback function (task)
 *
 * INPUT
 *     - queue: head pointer to linklist (queue)
 *     - callback: task function
 * OUTPUT
 *     pointer to a node containing the requested task function
 */
static RunnableNode *runnable_queue_get(RunnableQueue *queue, Runnable callback)
{
    RunnableNode *current = queue->head;
    RunnableNode *result = NULL;

    /* check queue is empty */
    if(current != NULL)
    {
        if(current->runnable == callback)
        {
            result = current;
            queue->head = current->next;
        }
        else
        {
            /* search through the linklist for node that contains the runnable
               callback function */
            RunnableNode *previous = current;
            current = current->next;

            while((current != NULL) && (result == NULL))
            {
                // Found the node!
                if(current->runnable == callback)
                {
                    result = current;
                    previous->next = current->next;
                    result->next = NULL;
                }

                previous = current;
                current = current->next;
            }
        }
    }

    if(result != NULL)
    {
        queue->size -= 1;
    }

    return(result);
}

/*
 * runnable_queue_push() - Push node to the task manger (queue)
 *
 * INPUT
 *     - queue: head pointer to the queue
 *     - node: pointer to a new node to be added
 * OUTPUT
 *     none
 */
static void runnable_queue_push(RunnableQueue *queue, RunnableNode *node)
{
    cm_disable_interrupts();

    if(queue->head != NULL)
    {
        node->next = queue->head;
    }
    else
    {
        node->next = NULL;
    }

    queue->head = node;
    queue->size += 1;
    cm_enable_interrupts();
}

/*
 * runnable_queue_pop() - Pop node from task manager (queue)
 *
 * INPUT
 *     - queue: head pointer to task manager
 * OUTPUT
 *     pointer to an available node retrieved from the queue
 */
static RunnableNode *runnable_queue_pop(RunnableQueue *queue)
{
    cm_disable_interrupts();

    RunnableNode *runnable_node = queue->head;

    if(runnable_node != NULL)
    {
        queue->head = runnable_node->next;
        queue->size -= 1;
    }

    cm_enable_interrupts();

    return(runnable_node);
}

/*
 * run_runnables() - Run task (callback function) located in task manager (queue)
 *
 * INPUT
 *     none
 * OUTPUT
 *     none
 */
static void run_runnables(void)
{
    // Do timer function work.
    RunnableNode *runnable_node = runnable_queue_peek(&active_queue);

    while(runnable_node != NULL)
    {
        RunnableNode *next = runnable_node->next;

        if(runnable_node->remaining != 0)
        {
            runnable_node->remaining -= 1;
        }

        if(runnable_node->remaining == 0)
        {
            if(runnable_node->runnable != NULL)
            {
                runnable_node->runnable(runnable_node->context);
            }

            if(runnable_node->repeating)
            {
                runnable_node->remaining = runnable_node->period;
            }
            else
            {
                runnable_queue_push(
                    &free_queue,
                    runnable_queue_get(&active_queue, runnable_node->runnable));
            }
        }

        runnable_node = next;
    }
}

/* === Functions =========================================================== */

/*
 * timer_init() - Timer 4 initialization.  Main timer for round robin tasking.
 *
 * INPUT
 *     none
 * OUTPUT
 *     none
 */
void timer_init(void)
{
    int i;

    for(i = 0; i < MAX_RUNNABLES; i++)
    {
        runnable_queue_push(&free_queue, &runnables[ i ]);
    }

    // Set up the timer.
    timer_reset(TIM4);
    timer_enable_irq(TIM4, TIM_DIER_UIE);
    timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

    /* 1000 * ( 120 / 12000000 ) = 1 ms intervals,
    where 1000 is the counter, 120 is the prescalar,
    and 12000000 is the clks/second */
    timer_set_prescaler(TIM4, 120000);
    timer_set_period(TIM4, 1);

    nvic_set_priority(NVIC_TIM4_IRQ, 16 * 2);
    nvic_enable_irq(NVIC_TIM4_IRQ);

    timer_enable_counter(TIM4);
}

/*
 * delay_us() - Micro second delay
 *
 * INPUT
 *     - us: count in micro seconds
 * OUTPUT
 *     none
 */
void delay_us(uint32_t us)
{
    uint32_t cnt = us * 20;

    while(cnt--)
    {
        __asm__("nop");
    }
}

/*
 * delay_ms() - Millisecond delay
 *
 * INPUT
 *     - ms: count in milliseconds
 * OUTPUT
 *     none
 */
void delay_ms(uint32_t ms)
{
    remaining_delay = ms;

    while(remaining_delay > 0) {}
}

/*
 * delay_ms_with_callback() - Millisecond delay allowing a callback for extra work
 *
 * INPUT
 *     - ms: count in milliseconds
 *     - callback_func: function to call during loops
 *     - frequency_ms: frequency in ms to do callback
 * OUTPUT
 *     none
 */
void delay_ms_with_callback(uint32_t ms, callback_func_t callback_func,
                            uint32_t frequency_ms)
{
    remaining_delay = ms;

    while(remaining_delay > 0)
    {
        if(remaining_delay % frequency_ms == 0)
        {
            (*callback_func)();
        }
    }
}

/*
 * suspend_s() - Suspend MCU. Sleep/suspend state not implemented (yet).
 *
 * INPUT
 *     - seconds: seconds to suspend MCU for
 * OUTPUT
 *     none
 */
__attribute__((optimize(0) )) bool suspend_s(uint32_t seconds)
{
    uint32_t last_remaining_delay;
    uint32_t new_remaining_delay = seconds * 1000;
    uint32_t current_remaining_delay;
    bool success = false;

    // Cannot verify operation of resume out of suspend mode for all cases
    // (yet), this suspend function uses a while loop dependent upon a
    // timer-interrupt decremented value, and checks that the value is
    // correctly decremented and tests for correctness upon completion.
    //
    // Set remaining delay, in a very safe way
    //
    current_remaining_delay = __atomic_add_fetch(
	    &remaining_delay,
	    0,
	    __ATOMIC_SEQ_CST
    );

    success = __atomic_compare_exchange(
	    &remaining_delay,
	    &current_remaining_delay,
	    &new_remaining_delay,
	    false,
	    __ATOMIC_SEQ_CST,
	    __ATOMIC_SEQ_CST
    );

    if (false == success)
    {
	// NOTE there is a race condition here, which could very seldomly
	// reset the device spuriously.
	//
	board_reset();
	// Should not get here, if so then return false
	//
	return false;
    }

    current_remaining_delay = new_remaining_delay;
    last_remaining_delay = current_remaining_delay;

    while (current_remaining_delay > 0) {

        // Verify that remaining_delay is being decrementing
	// within a safety window upon each iteration,
	// reset if not so.
	//
	if (
		current_remaining_delay > last_remaining_delay
		|| current_remaining_delay < last_remaining_delay - 1
	)
	{
	    board_reset();
	    // Should not get here, if it does then return false
	    //
	    return false;
	}

	// Reload local delay variables, very safely
	//
	last_remaining_delay = current_remaining_delay;

	// Be careful with volatiles
	//
        current_remaining_delay = __atomic_add_fetch(
		&remaining_delay,
		0,
	       	__ATOMIC_SEQ_CST
	);
    }

    // Verify that remaining_delay is 0, reset if not
    //
    if (0 != current_remaining_delay)
    {
	board_reset();
	// Should not get here, if it does then return false
	//
        return false;
    }

    return true;
}

/*
 * tim4_isr() - Timer 4 interrupt service routine
 *
 * INPUT
 *     none
 * OUTPUT
 *     none
 *
 */
void tim4_isr(void)
{
    uint32_t current_remaining_delay;
    uint32_t new_remaining_delay;
    bool success = false;

    current_remaining_delay = __atomic_add_fetch(
	    &remaining_delay, 
	    0, 
	    __ATOMIC_SEQ_CST
    );

    if (0 == current_remaining_delay) {
	    new_remaining_delay = 0;
    } else {
	    new_remaining_delay = current_remaining_delay - 1;
    }

    success = __atomic_compare_exchange(
	    &remaining_delay, 
	    &current_remaining_delay,
	    &new_remaining_delay,
	    false,
            __ATOMIC_SEQ_CST,
            __ATOMIC_SEQ_CST
    );
    
    if (false == success)
    {
	    board_reset();
    }

    run_runnables();
    timer_clear_flag(TIM4, TIM_SR_UIF);
}

/*
 * post_delayed() - Add delay to existing task (callback function) in task manager (queue)
 *
 * INPUT
 *     - callback: task function
 *     - context: pointer to task arguments
 *     - delay_ms: delay befor task starts
 * OUTPUT
 *     none
 */
void post_delayed(Runnable callback, void *context, uint32_t delay_ms)
{
    RunnableNode *runnable_node = runnable_queue_get(&active_queue, callback);

    if(runnable_node == NULL)
    {
        runnable_node = runnable_queue_pop(&free_queue);
    }

    runnable_node->runnable     = callback;
    runnable_node->context      = context;
    runnable_node->remaining    = delay_ms;
    runnable_node->period       = 0;
    runnable_node->repeating    = false;
    runnable_queue_push(&active_queue, runnable_node);
}

/*
 * post_periodic() - Add repeat and delay to existing task (callback function) in task manager (queue)
 *
 * INPUT
 *     - callback: task function
 *     - context: pointer to task arguments
 *     - period_m: task repeat interval (period)
 *     - delay_ms: delay befor task starts
 * OUTPUT
 *     none
 */
void post_periodic(Runnable callback, void *context, uint32_t period_ms,
                   uint32_t delay_ms)
{
    RunnableNode *runnable_node = runnable_queue_get(&active_queue, callback);

    if(runnable_node == NULL)
    {
        runnable_node = runnable_queue_pop(&free_queue);
    }

    runnable_node->runnable     = callback;
    runnable_node->context      = context;
    runnable_node->remaining    = delay_ms;
    runnable_node->period       = period_ms;
    runnable_node->repeating    = true;

    runnable_queue_push(&active_queue, runnable_node);
}

/*
 * remove_runnable() - Remove task from the task manager (queue)
 *
 * INPUT
 *     - callback: task function
 * OUTPUT
 *     none
 */
void remove_runnable(Runnable callback)
{
    RunnableNode *runnable_node = runnable_queue_get(&active_queue, callback);

    if(runnable_node != NULL)
    {
        runnable_queue_push(&free_queue, runnable_node);
    }
}

/*
 * clear_runnables() - Reset free_queue and active_queue to initial state
 *
 * INPUT
 *     none
 * OUTPUT
 *     none
 */
void clear_runnables(void)
{
    RunnableNode *runnable_node = runnable_queue_pop(&active_queue);

    while(runnable_node != NULL)
    {
        runnable_queue_push(&free_queue, runnable_node);
        runnable_node = runnable_queue_pop(&active_queue);
    }
}
