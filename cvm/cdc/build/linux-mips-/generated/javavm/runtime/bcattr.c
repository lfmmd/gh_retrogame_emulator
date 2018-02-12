/*
 * This file contains an array of byte-code attributes.
 * It is generated from opcodes.list.
 */

#include "javavm/include/defs.h"
#include "javavm/include/bcattr.h"

const CVMUint16 CVMbcAttributes[256] = {
    /* opc_nop */
    0,
    /* opc_aconst_null */
    0,
    /* opc_iconst_m1 */
    0,
    /* opc_iconst_0 */
    0,
    /* opc_iconst_1 */
    0,
    /* opc_iconst_2 */
    0,
    /* opc_iconst_3 */
    0,
    /* opc_iconst_4 */
    0,
    /* opc_iconst_5 */
    0,
    /* opc_lconst_0 */
    0,
    /* opc_lconst_1 */
    0,
    /* opc_fconst_0 */
    0,
    /* opc_fconst_1 */
    0,
    /* opc_fconst_2 */
    0,
    /* opc_dconst_0 */
    0,
    /* opc_dconst_1 */
    0,
    /* opc_bipush */
    0,
    /* opc_sipush */
    0,
    /* opc_ldc */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_ldc_w */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_ldc2_w */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_iload */
    0,
    /* opc_lload */
    0,
    /* opc_fload */
    0,
    /* opc_dload */
    0,
    /* opc_aload */
    0,
    /* opc_iload_0 */
    0,
    /* opc_iload_1 */
    0,
    /* opc_iload_2 */
    0,
    /* opc_iload_3 */
    0,
    /* opc_lload_0 */
    0,
    /* opc_lload_1 */
    0,
    /* opc_lload_2 */
    0,
    /* opc_lload_3 */
    0,
    /* opc_fload_0 */
    0,
    /* opc_fload_1 */
    0,
    /* opc_fload_2 */
    0,
    /* opc_fload_3 */
    0,
    /* opc_dload_0 */
    0,
    /* opc_dload_1 */
    0,
    /* opc_dload_2 */
    0,
    /* opc_dload_3 */
    0,
    /* opc_aload_0 */
    0,
    /* opc_aload_1 */
    0,
    /* opc_aload_2 */
    0,
    /* opc_aload_3 */
    0,
    /* opc_iaload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_laload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_faload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_daload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_aaload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_baload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_caload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_saload */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_istore */
    0,
    /* opc_lstore */
    0,
    /* opc_fstore */
    0,
    /* opc_dstore */
    0,
    /* opc_astore */
    0,
    /* opc_istore_0 */
    0,
    /* opc_istore_1 */
    0,
    /* opc_istore_2 */
    0,
    /* opc_istore_3 */
    0,
    /* opc_lstore_0 */
    0,
    /* opc_lstore_1 */
    0,
    /* opc_lstore_2 */
    0,
    /* opc_lstore_3 */
    0,
    /* opc_fstore_0 */
    0,
    /* opc_fstore_1 */
    0,
    /* opc_fstore_2 */
    0,
    /* opc_fstore_3 */
    0,
    /* opc_dstore_0 */
    0,
    /* opc_dstore_1 */
    0,
    /* opc_dstore_2 */
    0,
    /* opc_dstore_3 */
    0,
    /* opc_astore_0 */
    0,
    /* opc_astore_1 */
    0,
    /* opc_astore_2 */
    0,
    /* opc_astore_3 */
    0,
    /* opc_iastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_lastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_fastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_dastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_aastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_bastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_castore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_sastore */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_pop */
    0,
    /* opc_pop2 */
    0,
    /* opc_dup */
    0,
    /* opc_dup_x1 */
    0,
    /* opc_dup_x2 */
    0,
    /* opc_dup2 */
    0,
    /* opc_dup2_x1 */
    0,
    /* opc_dup2_x2 */
    0,
    /* opc_swap */
    0,
    /* opc_iadd */
    0,
    /* opc_ladd */
    0,
    /* opc_fadd */
    CVM_BC_ATT_FP,
    /* opc_dadd */
    CVM_BC_ATT_FP,
    /* opc_isub */
    0,
    /* opc_lsub */
    0,
    /* opc_fsub */
    CVM_BC_ATT_FP,
    /* opc_dsub */
    CVM_BC_ATT_FP,
    /* opc_imul */
    0,
    /* opc_lmul */
    0,
    /* opc_fmul */
    CVM_BC_ATT_FP,
    /* opc_dmul */
    CVM_BC_ATT_FP,
    /* opc_idiv */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_ldiv */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_fdiv */
    CVM_BC_ATT_FP,
    /* opc_ddiv */
    CVM_BC_ATT_FP,
    /* opc_irem */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_lrem */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_frem */
    CVM_BC_ATT_FP,
    /* opc_drem */
    CVM_BC_ATT_FP,
    /* opc_ineg */
    0,
    /* opc_lneg */
    0,
    /* opc_fneg */
    CVM_BC_ATT_FP,
    /* opc_dneg */
    CVM_BC_ATT_FP,
    /* opc_ishl */
    0,
    /* opc_lshl */
    0,
    /* opc_ishr */
    0,
    /* opc_lshr */
    0,
    /* opc_iushr */
    0,
    /* opc_lushr */
    0,
    /* opc_iand */
    0,
    /* opc_land */
    0,
    /* opc_ior */
    0,
    /* opc_lor */
    0,
    /* opc_ixor */
    0,
    /* opc_lxor */
    0,
    /* opc_iinc */
    0,
    /* opc_i2l */
    0,
    /* opc_i2f */
    0,
    /* opc_i2d */
    0,
    /* opc_l2i */
    0,
    /* opc_l2f */
    0,
    /* opc_l2d */
    0,
    /* opc_f2i */
    0,
    /* opc_f2l */
    0,
    /* opc_f2d */
    0,
    /* opc_d2i */
    0,
    /* opc_d2l */
    0,
    /* opc_d2f */
    0,
    /* opc_i2b */
    0,
    /* opc_i2c */
    0,
    /* opc_i2s */
    0,
    /* opc_lcmp */
    0,
    /* opc_fcmpl */
    CVM_BC_ATT_FP,
    /* opc_fcmpg */
    CVM_BC_ATT_FP,
    /* opc_dcmpl */
    CVM_BC_ATT_FP,
    /* opc_dcmpg */
    CVM_BC_ATT_FP,
    /* opc_ifeq */
    CVM_BC_ATT_BRANCH,
    /* opc_ifne */
    CVM_BC_ATT_BRANCH,
    /* opc_iflt */
    CVM_BC_ATT_BRANCH,
    /* opc_ifge */
    CVM_BC_ATT_BRANCH,
    /* opc_ifgt */
    CVM_BC_ATT_BRANCH,
    /* opc_ifle */
    CVM_BC_ATT_BRANCH,
    /* opc_if_icmpeq */
    CVM_BC_ATT_BRANCH,
    /* opc_if_icmpne */
    CVM_BC_ATT_BRANCH,
    /* opc_if_icmplt */
    CVM_BC_ATT_BRANCH,
    /* opc_if_icmpge */
    CVM_BC_ATT_BRANCH,
    /* opc_if_icmpgt */
    CVM_BC_ATT_BRANCH,
    /* opc_if_icmple */
    CVM_BC_ATT_BRANCH,
    /* opc_if_acmpeq */
    CVM_BC_ATT_BRANCH,
    /* opc_if_acmpne */
    CVM_BC_ATT_BRANCH,
    /* opc_goto */
    CVM_BC_ATT_BRANCH | CVM_BC_ATT_NOCONTROLFLOW,
    /* opc_jsr */
    CVM_BC_ATT_BRANCH,
    /* opc_ret */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW,
    /* opc_tableswitch */
    CVM_BC_ATT_BRANCH | CVM_BC_ATT_NOCONTROLFLOW,
    /* opc_lookupswitch */
    CVM_BC_ATT_BRANCH | CVM_BC_ATT_NOCONTROLFLOW,
    /* opc_ireturn */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW | CVM_BC_ATT_RETURN,
    /* opc_lreturn */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW | CVM_BC_ATT_RETURN,
    /* opc_freturn */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW | CVM_BC_ATT_RETURN,
    /* opc_dreturn */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW | CVM_BC_ATT_RETURN,
    /* opc_areturn */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW | CVM_BC_ATT_RETURN,
    /* opc_return */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_NOCONTROLFLOW | CVM_BC_ATT_RETURN,
    /* opc_getstatic */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_putstatic */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_getfield */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_putfield */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_invokevirtual */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_invokespecial */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_invokestatic */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_invokeinterface */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_xxxunusedxxx */
    0,
    /* opc_new */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_newarray */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_anewarray */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_arraylength */
    CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_athrow */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_NOCONTROLFLOW,
    /* opc_checkcast */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_instanceof */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_monitorenter */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_monitorexit */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_wide */
    0,
    /* opc_multianewarray */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    /* opc_ifnull */
    CVM_BC_ATT_BRANCH,
    /* opc_ifnonnull */
    CVM_BC_ATT_BRANCH,
    /* opc_goto_w */
    CVM_BC_ATT_BRANCH | CVM_BC_ATT_NOCONTROLFLOW,
    /* opc_jsr_w */
    CVM_BC_ATT_BRANCH,
    /* opc_breakpoint */
    CVM_BC_ATT_GCPOINT,
    /* opc_aldc_ind_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_aldc_ind_w_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_invokestatic_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokestatic_checkinit_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokevirtual_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_getfield_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_agetfield_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_vinvokevirtual_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokevirtual_quick_w */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_putfield_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_invokenonvirtual_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokesuper_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokeignored_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_getfield2_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_checkcast_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_instanceof_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_nonnull_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_putfield2_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_ainvokevirtual_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokevirtualobject_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_invokeinterface_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_aldc_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_ldc_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_exittransition */
    0,
    /* opc_dinvokevirtual_quick */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_aldc_w_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_ldc_w_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_aputfield_quick */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_getfield_quick_w */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_ldc2_w_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_agetstatic_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_getstatic_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_getstatic2_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_aputstatic_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_putstatic_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_putstatic2_quick */
    CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_agetstatic_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_getstatic_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_getstatic2_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_aputstatic_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_putstatic_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_putstatic2_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_putfield_quick_w */
    CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_COND_GCPOINT | CVM_BC_ATT_QUICK,
    /* opc_new_checkinit_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_new_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_anewarray_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_multianewarray_quick */
    CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION | CVM_BC_ATT_QUICK,
    /* opc_prefix */
    0,
    /* opc_invokeinit */
    CVM_BC_ATT_INVOCATION | CVM_BC_ATT_GCPOINT | CVM_BC_ATT_THROWSEXCEPTION,
    0,
    0,
    0,
    0,
};
