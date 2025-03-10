#ifndef PROCESS_ID_H_
#define PROCESS_ID_H_

#include "common_macro.h"
#include <linux/cpumask.h>
#include <linux/limits.h>

/* Assuming all participage machine has the same number of CPUs and preempt is
 * disabled */
static WARN_UNUSED_RESULT force_inline bool
get_process_identifier(uint16_t machine_id, uint16_t *process_id)
{
	uint32_t cpu_id = __smp_processor_id();
	uint32_t cpu_global_id =
		nr_cpu_ids * ((unsigned int)machine_id - 1) + cpu_id;
	if (cpu_global_id > (uint32_t)U16_MAX) {
		PRINT_ERR("Currently only %u total cpus are supported\n",
			  U16_MAX);
		return false;
	}
	*process_id = (uint16_t)cpu_global_id;
	return true;
}

#endif // PROCESS_ID_H_
