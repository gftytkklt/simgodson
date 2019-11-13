#ifndef __SAMPLING_H__
#define __SAMPLING_H__

#if 0

/* enum that indicates whether the simulator is currently warming, measuring, or fast-forwarding */
enum simulator_state_t {
    NOT_STARTED,
	MEASURING,
	WARMING,
	DRAINING,
	FAST_FORWARDING
};

extern enum simulator_state_t simulator_state;

/* sampling enabled.  1 if enabled, 0 otherwise */
extern unsigned int sampling_enabled;

/* sampling k value */
extern int sampling_k;

/* sampling measurement unit */
extern int sampling_munit;

/* sampling measurement unit */
extern int sampling_wunit;

/* TRUE if in-order warming is enabled*/
extern int sampling_allwarm;

/* sample size*/
extern counter_t sim_sample_size;

/* period of each sample */
extern counter_t sim_sample_period;

/* total number of instructions measured */
extern counter_t sim_detail_insn;

/* total number of instructions measured */
extern counter_t sim_meas_insn;

/* measured cycle */
extern tick_t sim_meas_cycle;

extern double standardDeviation;
extern double percentageErrorInterval;  /* not multiplied by 100%, so 3% is 0.03 */
extern int recommendedK;  /* K recommended to achieve +/- 3%*/

/* updates standardDeviation */
extern void doStatistics(void);

extern void simoo_backup_stats(void);
extern void simoo_restore_stats(void);
extern void switch_to_fast_forwarding(void);
extern void switch_to_warming(void);
extern void start_measuring(void);
extern void stop_measuring(void);
extern void run_fast(counter_t count);
void predecode_init(void);

#endif

#endif
