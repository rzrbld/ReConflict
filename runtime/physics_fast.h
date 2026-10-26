#ifndef DS_PHYSICS_FAST_H
#define DS_PHYSICS_FAST_H
#ifdef __FAST_MATH__
#error Physics rank updates require ordered arithmetic
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#elif defined(__clang__)
#pragma clang fp contract(off)
#endif
/* Read-only eligibility; successful arithmetic is in physics_rank_update.inc.
 * Include that fragment inside the original function's local flag scope. */
int ds_physics_rank_update_eligible(void);
#endif
