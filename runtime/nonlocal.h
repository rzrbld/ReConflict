#ifndef DS_NONLOCAL_H
#define DS_NONLOCAL_H
#include <setjmp.h>
#include <stdint.h>
/* A live host frame for a guest _setjmp3 caller. The native setjmp must be
 * expanded in that caller; it cannot live in a helper that already returned. */
typedef struct DS_JmpFrame {
    jmp_buf environment;
    struct DS_JmpFrame *previous;
    uint32_t owner, buffer, caller_esp;
    int active;
    /* Armed is written after setjmp and read after native longjmp. */
    volatile int armed;
} DS_JmpFrame;
void ds_jmp_enter(DS_JmpFrame *frame, uint32_t owner);
void ds_jmp_leave(DS_JmpFrame *frame);
void ds_jmp_bind(DS_JmpFrame *frame, uint32_t buffer, uint32_t caller_esp);
void ds_jmp_arm(DS_JmpFrame *frame);
unsigned ds_jmp_depth(void);
_Noreturn void ds_guest_longjmp(void);

#define DS_GUEST_SETJMP(frame, original) do { \
    RECOMP_REGS_OUT(); \
    ds_jmp_bind(&(frame), MEM32(g_esp), g_esp); \
    if (setjmp((frame).environment) == 0) { \
        RECOMP_CALL(original); \
        ds_jmp_arm(&(frame)); \
    } else { \
        RECOMP_REGS_IN(); \
        _flag_k = FK_NONE; _flag_a = _flag_b = _cf = 0; \
    } \
} while (0)
#endif
