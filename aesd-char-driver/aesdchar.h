/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include <linux/mutex.h>
#include "aesd-circular-buffer.h"

#define AESD_DEBUG 1

#undef PDEBUG
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
#    define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)
#endif

struct aesd_dev
{
    struct mutex lock;
    struct aesd_circular_buffer buffer;
    char *working_entry;
    size_t working_entry_size;
    struct cdev cdev;
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
