#ifndef __ss_godson_decode_h__
#define __ss_godson_decode_h__

#if 0
extern void decode_stage_init(void);
extern void decode_stage(void);

extern void tracer_recover(int level,md_addr_t recover_pc);

/* list of externs */
extern int decode_ifq_size;
extern int decode_num;
extern int decode_head; 
extern int decode_tail;
extern struct inst_descript **decode_data;
extern counter_t sim_decode_insn;
extern int decode_width;
extern int decode_speed;

/* which level of mis-speculation we are on? */
extern int spec_level ;

extern int is_taken_delayslot ;  
extern int entering_spec; 
extern int need_to_correct_pc; 
extern int last_is_branch;
extern struct inst_descript *last_non_spec_rs;

extern int ghr_valid;
extern int ghr_predict;

extern void clear_create_vector(void);

/*
 * configure the instruction decode engine
 */

#define DNA			(0)

/* general register dependence decoders */
#define DGPR(N)			(N)
#define DGPR_D(N)		((N) &~1)

/* floating point register dependence decoders */
#define DFPR_L(N)		(((N)+32)&~1)
#define DFPR_F(N)		(((N)+32)&~1)
#define DFPR_D(N)		(((N)+32)&~1)

/* miscellaneous register dependence decoders */
#define DHI			(0+32+32)
#define DLO			(1+32+32)
#define DFCC			(2+32+32)
#define DTMP			(3+32+32)

#endif

#endif
