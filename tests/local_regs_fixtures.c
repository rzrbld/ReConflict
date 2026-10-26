/* Generated-style ABI boundary fixtures; staged in global/local variants. */
void recomp_fixture_direct(void) {
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1110u);
    eax=MEM32(ecx);MEM32(ecx+4)=eax;PUSH32(esp,eax);
    RECOMP_CALL(native_consumer);RECOMP_FLAGS_IN();esp+=4;eax+=edx;
    RECOMP_FLAGS_OUT();esp+=4;return;
}
void recomp_fixture_indirect(void) {
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1111u);eax=123;edx=77;
    RECOMP_ICALL(0xabcdu);RECOMP_FLAGS_IN();eax+=edx;esp+=4;return;
}
void recomp_fixture_tail(void) {
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1112u);eax=32;edx=91;RECOMP_ITAIL(0xabcdu);return;
}
void recomp_fixture_fallthrough(void) {
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1113u);eax=MEM32(ecx);MEM32(ecx+4)=eax^edx;ecx+=12;esp+=4;
}
void recomp_fixture_cpuid(void) {
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1114u);eax=1;ecx=0xdeadbeef;CPUID(eax,ebx,ecx,edx);esp+=4;return;
}
void recomp_fixture_native_early(void) {
    if(ds_test_native_hook())return;
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1115u);eax=0x12345678;esp+=4;return;
}
void recomp_fixture_setjmp(void) {
    uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0;
    RECOMP_ENTER(0x1116u);
    DS_JmpFrame frame;ds_jmp_enter(&frame,0x1116u);
    ebp=0x1234;ebx=0x2345;esi=0x3456;edi=0x4567;
    PUSH32(esp,0);PUSH32(esp,0x1000);
    DS_GUEST_SETJMP(frame,recomp_desertstorm_005C9654);esp+=8;
    if(eax==0){ecx=0xbad0;ebp=0xbad1;RECOMP_CALL(fixture_throw);}
    ds_jmp_leave(&frame);esp+=4;return;
}
