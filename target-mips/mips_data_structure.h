/* this file contains almost all the structures which are defined on the mips
 * platform and used in the syscalls 
 * 
 * this file is written by fenghao */

/* used in SYS_rt_sigprocmask and SYS_sigprocmask */

/* for now we handle only 32bit mips, if simplescalar itself
 * is compiled on 64 bit platform(e.g., x86-64), we have to
 * be careful to use types such as 'long'. define target types
 * should be a right way.
 *
 * Only a few types are covered presently, we should use target
 * types for all fields in the future--zfx
 */

/* for 32 bit mips */
#ifdef __x86_64__
typedef int t_long_t ;
typedef unsigned int t_ulong_t ;
#else
typedef long t_long_t ;
typedef unsigned long t_ulong_t ;
#endif

typedef t_long_t t_clock_t;
typedef t_long_t t_time_t;
typedef t_ulong_t t_pointer;

//#define NSIG_WORDS		4	
#define NSIG_WORDS		( 1024 / (8 * sizeof(t_long_t)) )

typedef struct
{
	t_ulong_t sig[NSIG_WORDS];
}mips_sigset_t;

typedef t_ulong_t mips_old_sigset_t;

/* used in SYS_stat */

struct ss_statbuf 
{
    t_ulong_t  ss_st_dev;
    t_long_t   ss_st_pad1[3];             /* Reserved for network id */
    t_ulong_t  ss_st_ino;
    word_t        ss_st_mode;
    word_t        ss_st_nlink;
    word_t        ss_st_uid;
    word_t        ss_st_gid;
    t_ulong_t  ss_st_rdev;
    t_long_t         ss_st_pad2[2];
    t_long_t         ss_st_size;
    t_long_t         ss_st_pad3;
    t_long_t         ss_st_atime;
    t_long_t         ss_reserved0;
    t_long_t         ss_st_mtime;
    t_long_t         ss_reserved1;
    t_long_t         ss_st_ctime;
    t_long_t         ss_reserved2;
    t_long_t         ss_st_blksize;
    t_long_t         ss_st_blocks;
    t_long_t         ss_st_pad4[14];
};


struct ss_statbuf64
{
  t_ulong_t    	ss_st_dev;
  t_ulong_t   	ss_pad0[3];     /* Reserved for st_dev expansion  */
  unsigned long long  ss_st_ino;
  word_t         ss_st_mode;
  word_t         ss_st_nlink;
  word_t         ss_st_uid;
  word_t         ss_st_gid;
  t_ulong_t	 ss_st_rdev;
  t_ulong_t   ss_pad1[3];     /* Reserved for st_rdev expansion  */
  long long       ss_st_size;
  
  t_long_t           ss_st_atime;
  t_ulong_t	 ss_reserved0;      /* Reserved for st_atime expansion  */
  t_long_t           ss_st_mtime;
  t_ulong_t	 ss_reserved1; /* Reserved for st_mtime expansion  */
  t_long_t       	 ss_st_ctime;
  t_ulong_t	 ss_reserved2;      /* Reserved for st_ctime expansion  */
  t_ulong_t   ss_st_blksize; 
  t_ulong_t	 ss_pad2;
  long long       ss_st_blocks;
};

/* used in SS_old_mmap whose definition is the same as on i386 platform */
struct mmap_arg_struct {
        t_ulong_t addr;
        t_ulong_t len;
        t_ulong_t prot;
        t_ulong_t flags;
        t_ulong_t fd;
        t_ulong_t offset;
};


/* used in SS_SYS_statfs and SS_SYS_fstatfs */

// typedef struct {
//         t_long_t   val[2];
// } __kernel_fsid_t;

struct ss_statfs {
        t_long_t           ss_f_type;
        t_long_t           ss_f_bsize;
        t_long_t           ss_f_frsize;       /* Fragment size - unsupported */
        t_long_t           ss_f_blocks;
        t_long_t           ss_f_bfree;
        t_long_t           ss_f_files;
        t_long_t           ss_f_ffree;

        /* Linux specials */
        t_long_t   ss_f_bavail;
        __kernel_fsid_t ss_f_fsid;
        t_long_t           ss_f_namelen;
        t_long_t           ss_f_spare[6];
};

struct ss_sgttyb {
  byte_t sg_ispeed;     /* input speed */
  byte_t sg_ospeed;     /* output speed */
  byte_t sg_erase;      /* erase character */
  byte_t sg_kill;       /* kill character */
  shalf_t sg_flags;     /* mode flags */
};

struct ss_timeval
{
  sword_t ss_tv_sec;		/* seconds */
  sword_t ss_tv_usec;		/* microseconds */
};

/* used in SS_SYS_setitimer and SS_SYS_getitimer */

struct ss_itimerval{
struct ss_timeval	ss_it_interval;
struct ss_timeval	ss_it_value;
};

/* target getrusage() buffer definition, the host stat buffer format is
   automagically mapped to/from this format in syscall.c */
struct ss_rusage
{
  struct ss_timeval ss_ru_utime;
  struct ss_timeval ss_ru_stime;
  sword_t ss_ru_maxrss;
  sword_t ss_ru_ixrss;
  sword_t ss_ru_idrss;
  sword_t ss_ru_isrss;
  sword_t ss_ru_minflt;
  sword_t ss_ru_majflt;
  sword_t ss_ru_nswap;
  sword_t ss_ru_inblock;
  sword_t ss_ru_oublock;
  sword_t ss_ru_msgsnd;
  sword_t ss_ru_msgrcv;
  sword_t ss_ru_nsignals;
  sword_t ss_ru_nvcsw;
  sword_t ss_ru_nivcsw;
};

struct ss_timezone
{
  sword_t ss_tz_minuteswest;	/* minutes west of Greenwich */
  sword_t ss_tz_dsttime;	/* type of dst correction */
};

struct ss_rlimit
{
  int ss_rlim_cur;		/* current (soft) limit */
  int ss_rlim_max;		/* maximum value for rlim_cur */
};

struct ss_sysinfo {
        t_long_t  ss_uptime;                    /* Seconds since boot */
        t_ulong_t ss_loads[3];         /* 1, 5, and 15 minute load averages */
        t_ulong_t ss_totalram;         /* Total usable main memory size */
        t_ulong_t ss_freeram;          /* Available memory size */
        t_ulong_t ss_sharedram;        /* Amount of shared memory */
        t_ulong_t ss_bufferram;        /* Memory used by buffers */
        t_ulong_t ss_totalswap;        /* Total swap space size */
        t_ulong_t ss_freeswap;         /* swap space still available */
        half_t ss_procs;           /* Number of current processes */
        half_t ss_pad;             /* explicit padding for m68k */
        t_ulong_t ss_totalhigh;        /* Total high memory size */
        t_ulong_t ss_freehigh;         /* Available high memory size */
        word_t ss_mem_unit;          /* Memory unit size in bytes */
        char ss_f[20-2*sizeof(t_long_t)-sizeof(int)]; /* Padding: libc5 uses this.. */
};

struct ss_timex {
    sword_t ss_modes;           /* mode selector */
    t_long_t ss_offset;         /* time offset (usec) */
    t_long_t ss_freq;           /* frequency offset (scaled ppm) */
    t_long_t ss_maxerror;       /* maximum error (usec) */
    t_long_t ss_esterror;       /* estimated error (usec) */
    sword_t ss_status;          /* clock command/status */
    t_long_t ss_constant;       /* pll time constant */
    t_long_t ss_precision;      /* clock precision (usec) (read only) */
    t_long_t ss_tolerance;      /* clock frequency tolerance (ppm)
                            (read only) */
    struct ss_timeval ss_time; /* current time (read only) */
    t_long_t ss_tick;           /* usecs between clock ticks */
};

struct ss_iovec{
	//void * ss_iov_base;
	t_pointer ss_iov_base;
	word_t ss_iov_len;
};

struct ss_sched_param {
        word_t ss_sched_priority;
};

struct ss_utimbuf {
        t_time_t ss_actime;  /* access time */
        t_time_t ss_modtime; /* modification time */
};

struct ss_tms {
        t_clock_t ss_tms_utime;
        t_clock_t ss_tms_stime;      
        t_clock_t ss_tms_cutime;
        t_clock_t ss_tms_cstime;
};

struct ss_ustat {
        t_long_t         ss_f_tfree;
        t_ulong_t  ss_f_tinode;
        char          ss_f_fname[6];
        char          ss_f_fpack[6];
};
