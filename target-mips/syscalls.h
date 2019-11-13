#ifndef __mips_syscalls_h__
#define __mips_syscalls_h__

/* SimpleScalar linux/mips system call codes, note these
   codes reside in register $r2 at the point a `syscall' inst is executed, not
   all of these codes are implemented, see the main switch statement in
   syscall.c for a list of implemented system calls */

#define SS_SYS_syscall	4000
/* SS_SYS_exit was moved to mips.h */
#define	SS_SYS_fork		4002
#define	SS_SYS_read		4003
/* SS_SYS_write was moved to mips.h */
#define	SS_SYS_open		4005
#define	SS_SYS_close	4006
#define SS_SYS_waitpid  4007						/*  7 is old: wait */
#define	SS_SYS_creat	4008
#define	SS_SYS_link		4009
#define	SS_SYS_unlink	4010
#define	SS_SYS_execv	4011
#define	SS_SYS_chdir	4012
#define SS_SYS_time		4013						/* 13 is old: time */
#define	SS_SYS_mknod		4014
#define	SS_SYS_chmod		4015
#define	SS_SYS_lchown		4016
#define SS_SYS_ni_syscall1	4017						
#define SS_SYS_stat			4018					/* 18 is old: stat */
#define	SS_SYS_lseek		4019
#define	SS_SYS_getpid		4020
#define	SS_SYS_mount		4021
#define	SS_SYS_oldumount	4022
#define SS_SYS_setuid		4023  					/* 23 is old: setuid */
#define	SS_SYS_getuid		4024
#define SS_SYS_stime		4025					/* 25 is old: stime */
#define	SS_SYS_ptrace		4026
#define SS_SYS_alarm		4027					/* 27 is old: alarm */
#define SS_SYS_fstat		4028					/* 28 is old: fstat */
#define SS_SYS_pause		4029					/* 29 is old: pause */
#define SS_SYS_utime		4030					/* 30 is old: utime */
#define	SS_SYS_ni_syscall2	4031					
#define SS_SYS_ni_syscall3	4032					
#define	SS_SYS_access		4033
#define	SS_SYS_nice			4034					/* 34 is old: nice */
#define	SS_SYS_ni_syscall4	4035					
#define	SS_SYS_sync			4036
#define	SS_SYS_kill			4037
#define	SS_SYS_rename		4038
#define	SS_SYS_mkdir		4039					
#define	SS_SYS_rmdir		4040
#define	SS_SYS_dup			4041
#define	SS_SYS_pipe			4042
#define	SS_SYS_times		4043					/* 43 is old: times */
#define	SS_SYS_ni_syscall5	4044
#define	SS_SYS_brk			4045					/* 45 is unused */
#define	SS_SYS_setgid		4046					/* 46 is old: setgid */
#define	SS_SYS_getgid		4047
#define	SS_SYS_ni_syscall6	4048					
#define SS_SYS_geteuid		4049					/* 49 is unused */
#define SS_SYS_getegid		4050				    /* 50 is unused */
#define	SS_SYS_acct			4051
#define	SS_SYS_umount		4052					
#define	SS_SYS_ni_syscall7	4053					
#define	SS_SYS_ioctl		4054
#define	SS_SYS_fcntl		4055
#define	SS_SYS_ni_syscall8	4056					
#define	SS_SYS_setpgid		4057
#define	SS_SYS_ni_syscall9	4058
#define	SS_SYS_olduname		4059
#define	SS_SYS_umask		4060
#define	SS_SYS_chroot		4061
#define	SS_SYS_ustat		4062
#define	SS_SYS_dup2			4063					/* 63 is unused */
#define	SS_SYS_getppid		4064
#define	SS_SYS_getpgrp		4065
#define SS_SYS_setsid		4066					
#define	SS_SYS_sigaction	4067					
#define	SS_SYS_sgetmask		4068					
#define	SS_SYS_ssetmask	  	4069
#define	SS_SYS_setreuid		4070
#define	SS_SYS_setregid		4071
#define SS_SYS_sigsuspend	4072					
#define	SS_SYS_sigpending	4073
#define	SS_SYS_sethostname	4074
#define	SS_SYS_setrlimit	4075
#define	SS_SYS_getrlimit	4076
#define	SS_SYS_getrusage	4077					
#define	SS_SYS_gettimeofday	4078
#define	SS_SYS_settimeofday	4079
#define	SS_SYS_getgroups	4080
#define	SS_SYS_setgroups	4081
#define	SS_SYS_ni_syscall10	4082
#define	SS_SYS_symlink		4083
#define	SS_SYS_lstat		4084
#define	SS_SYS_readlink		4085
#define	SS_SYS_uselib		4086
#define	SS_SYS_swapon		4087
#define	SS_SYS_reboot		4088
#define	SS_old_readdir		4089
#define	SS_old_mmap			4090
#define	SS_SYS_munmap		4091
#define	SS_SYS_truncate		4092
#define	SS_SYS_ftruncate	4093
#define	SS_SYS_fchmod		4094
#define	SS_SYS_fchown		4095
#define	SS_SYS_getpriority	4096
#define	SS_SYS_setpriority	4097
#define	SS_SYS_ni_syscall11	4098
#define	SS_SYS_statfs		4099
#define	SS_SYS_fstatfs		4100
#define	SS_SYS_ioperm		4101
#define	SS_SYS_socketcall	4102
#define  SS_SYS_syslog		4103						
#define	SS_SYS_setitimer	4104						
#define	SS_SYS_getitimer	4105
#define	SS_SYS_newstat		4106
#define	SS_SYS_newlstat		4107						
#define	SS_SYS_newfstat		4108
#define	SS_SYS_uname		4109
#define	SS_SYS_iopl			4110
#define	SS_SYS_vhangup		4111
#define	SS_SYS_ni_syscall12	4112
#define	SS_SYS_vm86			4113
#define	SS_SYS_wait4		4114
#define	SS_SYS_swapoff		4155						
#define	SS_SYS_sysinfo		4116
#define	SS_SYS_ipc			4117
#define	SS_SYS_fsync		4118
#define	SS_SYS_sigreturn	4119						
#define	SS_SYS_clone		4120
#define	SS_SYS_setdomainname 4121
#define	SS_SYS_newuname		4122
#define	SS_SYS_ni_syscall13 4123
#define	SS_SYS_adjtimex		4124
#define	SS_SYS_mprotect		4125
#define	SS_SYS_sigprocmask	4126
#define	SS_SYS_create_module		4127
#define	SS_SYS_init_module			4128
#define	SS_SYS_delete_module		4129
#define	SS_SYS_get_kernel_syms		4130
#define	SS_SYS_quotactl				4131
#define	SS_SYS_getpgid				4132				
#define	SS_SYS_fchdir				4133
#define	SS_SYS_bdflush				4134
#define	SS_SYS_sysfs				4135
#define	SS_SYS_personality			4136
#define	SS_SYS_ni_syscall14			4137
#define	SS_SYS_setfsuid				4138
#define  SS_SYS_setfsgid			4139				
#define	SS_SYS_llseek				4140
#define	SS_SYS_getdents				4141
#define	SS_SYS_select				4142
#define	SS_SYS_flock				4143
#define	SS_SYS_msync				4144
#define	SS_SYS_readv				4145
#define	SS_SYS_writev				4146
#define	SS_SYS_cacheflush			4147				
#define	SS_SYS_cachectl				4148
#define	SS_SYS_sysmips				4149
#define	SS_SYS_ni_syscall15			4150
#define SS_SYS_getsid     			4151				
#define SS_SYS_fdatasync  			4152
#define SS_SYS_sysctl    			4153
#define	SS_SYS_mlock				4154
#define SS_SYS_munlock   			4155
#define	SS_SYS_mlockall				4156
#define	SS_SYS_munlockall			4157
#define	SS_SYS_sched_setparam		4158
#define	SS_SYS_sched_getparam		4159
#define	SS_SYS_sched_setscheduler	4160
#define	SS_SYS_sched_getscheduler	4161
#define	SS_SYS_sched_yield			4162
#define	SS_SYS_sched_get_priority_max	4163
#define	SS_SYS_sched_get_priority_min	4164
#define	SS_SYS_sched_rr_get_interval	4165
#define	SS_SYS_nanosleep			4166
#define	SS_SYS_mremap				4167
#define	SS_SYS_accept				4168
#define	SS_SYS_bind					4169
#define	SS_SYS_connect				4170
#define	SS_SYS_getpeername			4171
#define	SS_SYS_getsockname			4172
#define	SS_SYS_getsockopt			4173
#define	SS_SYS_listen				4174
#define	SS_SYS_recv					4175
#define	SS_SYS_recvform				4176
#define	SS_SYS_recvmsg				4177
#define	SS_SYS_send					4178
#define	SS_SYS_sendmsg				4179
#define	SS_SYS_sendto				4180
#define	SS_SYS_setsockopt			4181
#define	SS_SYS_shutdown				4182
#define	SS_SYS_socket				4183
#define	SS_SYS_socketpair			4184
#define	SS_SYS_setresuid			4185
#define	SS_SYS_getresuid			4186
#define	SS_SYS_query_module			4187
#define	SS_SYS_poll					4188
#define	SS_SYS_nfsservctl			4189
#define	SS_SYS_setresgid			4190
#define	SS_SYS_getresgid			4191
#define	SS_SYS_prctl				4192
#define	SS_SYS_rt_sigreturn			4193
#define	SS_SYS_rt_sigaction			4194
#define	SS_SYS_rt_sigprocmask		4195
#define	SS_SYS_rt_sigpending		4196
#define	SS_SYS_rt_sigtimedwait		4197
#define	SS_SYS_rt_sigqueueinfo		4198
#define	SS_SYS_rt_sigsuspend		4199
#define	SS_SYS_pread			4200
#define	SS_SYS_pwrite			4201
#define	SS_SYS_chown			4202
#define	SS_SYS_getcwd			4203
#define	SS_SYS_capget			4204
#define	SS_SYS_capset			4205
#define	SS_SYS_sigaltstack		4206
#define	SS_SYS_sendfile			4207
#define	SS_SYS_ni_syscall16		4208
#define	SS_SYS_ni_syscall17		4209
#define	SS_SYS_mmap2			4210
#define	SS_SYS_truncate64		4211
#define	SS_SYS_ftruncate64		4212
#define	SS_SYS_stat64			4213
#define	SS_SYS_lstat64			4214
#define	SS_SYS_fstat64			4215
#define	SS_SYS_pivot_root		4216
#define	SS_SYS_mincore			4217
#define	SS_SYS_madvise			4218
#define	SS_SYS_getdents64		4219
#define	SS_SYS_fcntl64			4220
#define	SS_SYS_ni_syscall18		4221
#define	SS_SYS_gettid			4222
#define	SS_SYS_readahead		4223

/* may not be used */
#define SS_SYS_getpagesize		4224
#define SS_SYS_getdtablesize	4225
#define SS_SYS_sigvec			4226
#define SS_SYS_sigblock			4227
#define SS_SYS_sigsetmask		4228
#define SS_SYS_utimes			4229

 /* linux-mips */
#define SS_IOCTL_TCGETA          0x5401
#define SS_IOCTL_TCSETA          0x5402  /* Clashes with SNDCTL_TMR_START sound ioctl */
#define SS_IOCTL_TCSETAW         0x5403
#define SS_IOCTL_TCSETAF         0x5404
#define SS_IOCTL_TCSBRK          0x5405
#define SS_IOCTL_TCXONC          0x5406
#define SS_IOCTL_TCFLSH          0x5407
#define SS_IOCTL_TCGETS          0x540d
#define SS_IOCTL_TCSETS          0x540e
#define SS_IOCTL_TCSETSW         0x540f
#define SS_IOCTL_TCSETSF         0x5410
#define SS_IOCTL_TIOCEXCL        0x740d          /* set exclusive use of tty */
#define SS_IOCTL_TIOCNXCL        0x740e          /* reset exclusive use of tty */
#define SS_IOCTL_TIOCOUTQ        0x7472          /* output queue size */
#define SS_IOCTL_TIOCSTI         0x5472          /* simulate terminal input */
#define SS_IOCTL_TIOCMGET        0x741d          /* get all modem bits */
#define SS_IOCTL_TIOCMBIS        0x741b          /* bis modem bits */
#define SS_IOCTL_TIOCMBIC        0x741c          /* bic modem bits */
#define SS_IOCTL_TIOCGLTC        0x7474                  /* get special local chars */
#define SS_IOCTL_TIOCSLTC        0x7475                  /* set special local chars */
#define SS_IOCTL_TIOCGETP        0x7408
#define SS_IOCTL_TIOCSETP        0x7409
#define SS_IOCTL_TIOCSETN        0x740a                  /* TIOCSETP wo flush */


/* internal system call buffer size, used primarily for file name arguments,
   argument larger than this will be truncated */
#define MAXBUFSIZE 		1024

/* total bytes to copy from a valid pointer argument for ioctl() calls,
   syscall.c does not decode ioctl() calls to determine the size of the
   arguments that reside in memory, instead, the ioctl() proxy simply copies
   NUM_IOCTL_BYTES bytes from the pointer argument to host memory */
#define NUM_IOCTL_BYTES		128

#ifndef __x86_64__
/* copy from /usr/include/bits/stat.h, it is not defined if _USE_LARGEFILE64
 * is not defined */
struct stat64
  {
    __dev_t st_dev;			/* Device.  */
    unsigned int __pad1;

    __ino_t __st_ino;			/* 32bit file serial number.	*/
    __mode_t st_mode;			/* File mode.  */
    __nlink_t st_nlink;			/* Link count.  */
    __uid_t st_uid;			/* User ID of the file's owner.	*/
    __gid_t st_gid;			/* Group ID of the file's group.*/
    __dev_t st_rdev;			/* Device number, if device.  */
    unsigned int __pad2;
    __off64_t st_size;			/* Size of file, in bytes.  */
    __blksize_t st_blksize;		/* Optimal block size for I/O.  */

    __blkcnt64_t st_blocks;		/* Number 512-byte blocks allocated. */
#ifdef __USE_MISC
    /* Nanosecond resolution timestamps are stored in a format
       equivalent to 'struct timespec'.  This is the type used
       whenever possible but the Unix namespace rules do not allow the
       identifier 'timespec' to appear in the <sys/stat.h> header.
       Therefore we have to handle the use of this header in strictly
       standard-compliant sources special.  */
    struct timespec st_atim;		/* Time of last access.  */
    struct timespec st_mtim;		/* Time of last modification.  */
    struct timespec st_ctim;		/* Time of last status change.  */
#else
    __time_t st_atime;			/* Time of last access.  */
    unsigned long int st_atimensec;	/* Nscecs of last access.  */
    __time_t st_mtime;			/* Time of last modification.  */
    unsigned long int st_mtimensec;	/* Nsecs of last modification.  */
    __time_t st_ctime;			/* Time of last status change.  */
    unsigned long int st_ctimensec;	/* Nsecs of last status change.  */
#endif
    __ino64_t st_ino;			/* File serial number.		*/
  };
#else

/* Note stat64 has the same shape as stat for x86-64.  */
struct stat64
  {
    __dev_t st_dev;		/* Device.  */
#if __WORDSIZE == 64
    __ino64_t st_ino;		/* File serial number.  */
    __nlink_t st_nlink;		/* Link count.  */
    __mode_t st_mode;		/* File mode.  */
#else
    unsigned int __pad1;
    __ino_t __st_ino;			/* 32bit file serial number.	*/
    __mode_t st_mode;			/* File mode.  */
    __nlink_t st_nlink;			/* Link count.  */
#endif
    __uid_t st_uid;		/* User ID of the file's owner.	*/
    __gid_t st_gid;		/* Group ID of the file's group.*/
#if __WORDSIZE == 64
    int pad0;
    __dev_t st_rdev;		/* Device number, if device.  */
    __off_t st_size;		/* Size of file, in bytes.  */
#else
    __dev_t st_rdev;			/* Device number, if device.  */
    unsigned int __pad2;
    __off64_t st_size;			/* Size of file, in bytes.  */
#endif
    __blksize_t st_blksize;	/* Optimal block size for I/O.  */
    __blkcnt64_t st_blocks;	/* Nr. 512-byte blocks allocated.  */
#ifdef __USE_MISC
    /* Nanosecond resolution timestamps are stored in a format
       equivalent to 'struct timespec'.  This is the type used
       whenever possible but the Unix namespace rules do not allow the
       identifier 'timespec' to appear in the <sys/stat.h> header.
       Therefore we have to handle the use of this header in strictly
       standard-compliant sources special.  */
    struct timespec st_atim;		/* Time of last access.  */
    struct timespec st_mtim;		/* Time of last modification.  */
    struct timespec st_ctim;		/* Time of last status change.  */
# define st_atime st_atim.tv_sec	/* Backward compatibility.  */
# define st_mtime st_mtim.tv_sec
# define st_ctime st_ctim.tv_sec
#else
    __time_t st_atime;			/* Time of last access.  */
    unsigned long int st_atimensec;	/* Nscecs of last access.  */
    __time_t st_mtime;			/* Time of last modification.  */
    unsigned long int st_mtimensec;	/* Nsecs of last modification.  */
    __time_t st_ctime;			/* Time of last status change.  */
    unsigned long int st_ctimensec;	/* Nsecs of last status change.  */
#endif
#if __WORDSIZE == 64
    long int __unused[3];
#else
    __ino64_t st_ino;			/* File serial number.		*/
#endif
  };

#endif


/* used in SS_SYS_sched_setparam */

struct sched_param {
        int sched_priority;
};      


/* open(2) flags for SimpleScalar target, syscall.c automagically maps *
 * between these codes to/from host open(2) flags                 *
 *                                                            *
 * these flags are defined on mips platform                       */
#define SS_O_ACCMODE   0x0003	
#define SS_O_RDONLY	   0x0000
#define SS_O_WRONLY	   0x0001
#define SS_O_RDWR	   0x0002
#define SS_O_APPEND	   0x0008
#define SS_O_SYNC      0x0010
#define SS_O_NONBLOCK  0x0080
#define SS_O_CREAT	   0x0100
#define SS_O_TRUNC	   0x0200
#define SS_O_EXCL	   0x0400
#define SS_O_NOCTTY	   0x0800
#define SS_FASYNC      0x1000  /* fcntl, for BSD compatibility */ 	
#define SS_O_LARGEFILE 0x2000  /* allow large file opens */
#define SS_O_DIRECT    0x8000  /* direct disk access hint */
#define SS_O_DIRECTORY 0x10000 /* must be a directory */
#define SS_O_NOFOLLOW  0x20000 /* don't follow links */


/* open(2) flags translation table for SimpleScalar target */
struct {
  int ss_flag;
  int local_flag;
} ss_flag_table[] = {
  /* target flag */	/* host flag */
  { SS_O_RDONLY,	O_RDONLY },
  { SS_O_WRONLY,	O_WRONLY },
  { SS_O_RDWR,		O_RDWR },
  { SS_O_ACCMODE, O_ACCMODE}, 
  { SS_O_APPEND,	O_APPEND },
#ifdef O_SYNC
  { SS_O_SYNC,		O_SYNC },
#endif
  { SS_O_NONBLOCK,	O_NONBLOCK },
  { SS_O_CREAT,		O_CREAT },
  { SS_O_TRUNC,		O_TRUNC },
  { SS_O_EXCL,		O_EXCL },
  { SS_O_NOCTTY,	O_NOCTTY },
#ifdef FASYNC
  { SS_FASYNC,      FASYNC },
#endif
#ifdef O_LARGEFILE
  { SS_O_LARGEFILE, O_LARGEFILE },
#endif
#ifdef O_DIRECT
  { SS_O_DIRECT,   O_DIRECT },
#endif
#ifdef O_DIRECTORY
  { SS_O_DIRECTORY, O_DIRECTORY },
#endif
#ifdef O_NOFOLLOW
  { SS_O_NOFOLLOW, O_NOFOLLOW },
#endif
};
#define SS_NFLAGS	(sizeof(ss_flag_table)/sizeof(ss_flag_table[0]))

#endif /* __mips_syscalls_h__ */

