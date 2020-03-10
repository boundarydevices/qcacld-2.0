/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */



/**
 * @ingroup adf_os_public
 * @file adf_os_timer.h
 * This file abstracts OS timers.
 */

#ifndef _ADF_OS_TIMER_H
#define _ADF_OS_TIMER_H

#include <adf_os_types.h>
#include <adf_os_timer_pvt.h>


/**
 * @brief Platform timer object
 */
typedef __adf_os_timer_t           adf_os_timer_t;

#ifndef from_timer
static inline void timer_setup(struct timer_list *timer,
			       void (*callback)(struct timer_list *),
			       unsigned int flags)
{
	__setup_timer(timer, (TIMER_FUNC_TYPE)callback,
			(TIMER_DATA_TYPE)timer, flags);
}

#define from_timer(var, callback_timer, timer_fieldname) \
		container_of(callback_timer, typeof(*var), timer_fieldname)
#endif
/**
 * @brief Start a one-shot timer
 *
 * @param[in] timer     timer object pointer
 * @param[in] msec      expiration period in milliseconds
 */
static inline void
adf_os_timer_start(adf_os_timer_t *timer, int msec)
{
    __adf_os_timer_start(timer, msec);
}

/**
 * @brief Modify existing timer to new timeout value
 *
 * @param[in] timer     timer object pointer
 * @param[in] msec      expiration period in milliseconds
 */
static inline void
adf_os_timer_mod(adf_os_timer_t *timer, int msec)
{
    __adf_os_timer_mod(timer, msec);
}

/**
 * @brief Cancel a timer
 * The function will return after any running timer completes.
 *
 * @param[in] timer     timer object pointer
 *
 * @retval    TRUE      timer was cancelled and deactived
 * @retval    FALSE     timer was cancelled but already got fired.
 */
static inline a_bool_t
adf_os_timer_cancel(adf_os_timer_t *timer)
{
    return __adf_os_timer_cancel(timer);
}

/**
 * @brief Free a timer
 * The function will return after any running timer completes.
 *
 * @param[in] timer     timer object pointer
 *
 */
static inline void
adf_os_timer_free(adf_os_timer_t *timer)
{
    __adf_os_timer_free(timer);
}

#endif
