


/* PBCREP_GNUC_PRINTF(format_idx,arg_idx): Advise the compiler
 * that the arguments should be like printf(3); it may
 * optionally print type warnings.  */
#ifdef __GNUC__
#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define PBCREP_GNUC_PRINTF( format_idx, arg_idx )    \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#endif
#endif
#ifndef PBCREP_GNUC_PRINTF                /* fallback: no compiler hint */
# define PBCREP_GNUC_PRINTF( format_idx, arg_idx )
#endif
