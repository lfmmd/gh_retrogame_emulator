/*
 * This interface contains opc_ constant values,
 * a table of opcode names, and a table of instruction lengths.
 * It is generated from opcodes.list.
 * It is vm dependent, because it includes the quick opcodes.
 */
package opcodeconsts;
public interface OpcodeConst
{
    public static final int opc_nop	= 0;
    public static final int opc_aconst_null	= 1;
    public static final int opc_iconst_m1	= 2;
    public static final int opc_iconst_0	= 3;
    public static final int opc_iconst_1	= 4;
    public static final int opc_iconst_2	= 5;
    public static final int opc_iconst_3	= 6;
    public static final int opc_iconst_4	= 7;
    public static final int opc_iconst_5	= 8;
    public static final int opc_lconst_0	= 9;
    public static final int opc_lconst_1	= 10;
    public static final int opc_fconst_0	= 11;
    public static final int opc_fconst_1	= 12;
    public static final int opc_fconst_2	= 13;
    public static final int opc_dconst_0	= 14;
    public static final int opc_dconst_1	= 15;
    public static final int opc_bipush	= 16;
    public static final int opc_sipush	= 17;
    public static final int opc_ldc	= 18;
    public static final int opc_ldc_w	= 19;
    public static final int opc_ldc2_w	= 20;
    public static final int opc_iload	= 21;
    public static final int opc_lload	= 22;
    public static final int opc_fload	= 23;
    public static final int opc_dload	= 24;
    public static final int opc_aload	= 25;
    public static final int opc_iload_0	= 26;
    public static final int opc_iload_1	= 27;
    public static final int opc_iload_2	= 28;
    public static final int opc_iload_3	= 29;
    public static final int opc_lload_0	= 30;
    public static final int opc_lload_1	= 31;
    public static final int opc_lload_2	= 32;
    public static final int opc_lload_3	= 33;
    public static final int opc_fload_0	= 34;
    public static final int opc_fload_1	= 35;
    public static final int opc_fload_2	= 36;
    public static final int opc_fload_3	= 37;
    public static final int opc_dload_0	= 38;
    public static final int opc_dload_1	= 39;
    public static final int opc_dload_2	= 40;
    public static final int opc_dload_3	= 41;
    public static final int opc_aload_0	= 42;
    public static final int opc_aload_1	= 43;
    public static final int opc_aload_2	= 44;
    public static final int opc_aload_3	= 45;
    public static final int opc_iaload	= 46;
    public static final int opc_laload	= 47;
    public static final int opc_faload	= 48;
    public static final int opc_daload	= 49;
    public static final int opc_aaload	= 50;
    public static final int opc_baload	= 51;
    public static final int opc_caload	= 52;
    public static final int opc_saload	= 53;
    public static final int opc_istore	= 54;
    public static final int opc_lstore	= 55;
    public static final int opc_fstore	= 56;
    public static final int opc_dstore	= 57;
    public static final int opc_astore	= 58;
    public static final int opc_istore_0	= 59;
    public static final int opc_istore_1	= 60;
    public static final int opc_istore_2	= 61;
    public static final int opc_istore_3	= 62;
    public static final int opc_lstore_0	= 63;
    public static final int opc_lstore_1	= 64;
    public static final int opc_lstore_2	= 65;
    public static final int opc_lstore_3	= 66;
    public static final int opc_fstore_0	= 67;
    public static final int opc_fstore_1	= 68;
    public static final int opc_fstore_2	= 69;
    public static final int opc_fstore_3	= 70;
    public static final int opc_dstore_0	= 71;
    public static final int opc_dstore_1	= 72;
    public static final int opc_dstore_2	= 73;
    public static final int opc_dstore_3	= 74;
    public static final int opc_astore_0	= 75;
    public static final int opc_astore_1	= 76;
    public static final int opc_astore_2	= 77;
    public static final int opc_astore_3	= 78;
    public static final int opc_iastore	= 79;
    public static final int opc_lastore	= 80;
    public static final int opc_fastore	= 81;
    public static final int opc_dastore	= 82;
    public static final int opc_aastore	= 83;
    public static final int opc_bastore	= 84;
    public static final int opc_castore	= 85;
    public static final int opc_sastore	= 86;
    public static final int opc_pop	= 87;
    public static final int opc_pop2	= 88;
    public static final int opc_dup	= 89;
    public static final int opc_dup_x1	= 90;
    public static final int opc_dup_x2	= 91;
    public static final int opc_dup2	= 92;
    public static final int opc_dup2_x1	= 93;
    public static final int opc_dup2_x2	= 94;
    public static final int opc_swap	= 95;
    public static final int opc_iadd	= 96;
    public static final int opc_ladd	= 97;
    public static final int opc_fadd	= 98;
    public static final int opc_dadd	= 99;
    public static final int opc_isub	= 100;
    public static final int opc_lsub	= 101;
    public static final int opc_fsub	= 102;
    public static final int opc_dsub	= 103;
    public static final int opc_imul	= 104;
    public static final int opc_lmul	= 105;
    public static final int opc_fmul	= 106;
    public static final int opc_dmul	= 107;
    public static final int opc_idiv	= 108;
    public static final int opc_ldiv	= 109;
    public static final int opc_fdiv	= 110;
    public static final int opc_ddiv	= 111;
    public static final int opc_irem	= 112;
    public static final int opc_lrem	= 113;
    public static final int opc_frem	= 114;
    public static final int opc_drem	= 115;
    public static final int opc_ineg	= 116;
    public static final int opc_lneg	= 117;
    public static final int opc_fneg	= 118;
    public static final int opc_dneg	= 119;
    public static final int opc_ishl	= 120;
    public static final int opc_lshl	= 121;
    public static final int opc_ishr	= 122;
    public static final int opc_lshr	= 123;
    public static final int opc_iushr	= 124;
    public static final int opc_lushr	= 125;
    public static final int opc_iand	= 126;
    public static final int opc_land	= 127;
    public static final int opc_ior	= 128;
    public static final int opc_lor	= 129;
    public static final int opc_ixor	= 130;
    public static final int opc_lxor	= 131;
    public static final int opc_iinc	= 132;
    public static final int opc_i2l	= 133;
    public static final int opc_i2f	= 134;
    public static final int opc_i2d	= 135;
    public static final int opc_l2i	= 136;
    public static final int opc_l2f	= 137;
    public static final int opc_l2d	= 138;
    public static final int opc_f2i	= 139;
    public static final int opc_f2l	= 140;
    public static final int opc_f2d	= 141;
    public static final int opc_d2i	= 142;
    public static final int opc_d2l	= 143;
    public static final int opc_d2f	= 144;
    public static final int opc_i2b	= 145;
    public static final int opc_i2c	= 146;
    public static final int opc_i2s	= 147;
    public static final int opc_lcmp	= 148;
    public static final int opc_fcmpl	= 149;
    public static final int opc_fcmpg	= 150;
    public static final int opc_dcmpl	= 151;
    public static final int opc_dcmpg	= 152;
    public static final int opc_ifeq	= 153;
    public static final int opc_ifne	= 154;
    public static final int opc_iflt	= 155;
    public static final int opc_ifge	= 156;
    public static final int opc_ifgt	= 157;
    public static final int opc_ifle	= 158;
    public static final int opc_if_icmpeq	= 159;
    public static final int opc_if_icmpne	= 160;
    public static final int opc_if_icmplt	= 161;
    public static final int opc_if_icmpge	= 162;
    public static final int opc_if_icmpgt	= 163;
    public static final int opc_if_icmple	= 164;
    public static final int opc_if_acmpeq	= 165;
    public static final int opc_if_acmpne	= 166;
    public static final int opc_goto	= 167;
    public static final int opc_jsr	= 168;
    public static final int opc_ret	= 169;
    public static final int opc_tableswitch	= 170;
    public static final int opc_lookupswitch	= 171;
    public static final int opc_ireturn	= 172;
    public static final int opc_lreturn	= 173;
    public static final int opc_freturn	= 174;
    public static final int opc_dreturn	= 175;
    public static final int opc_areturn	= 176;
    public static final int opc_return	= 177;
    public static final int opc_getstatic	= 178;
    public static final int opc_putstatic	= 179;
    public static final int opc_getfield	= 180;
    public static final int opc_putfield	= 181;
    public static final int opc_invokevirtual	= 182;
    public static final int opc_invokespecial	= 183;
    public static final int opc_invokestatic	= 184;
    public static final int opc_invokeinterface	= 185;
    public static final int opc_xxxunusedxxx	= 186;
    public static final int opc_new	= 187;
    public static final int opc_newarray	= 188;
    public static final int opc_anewarray	= 189;
    public static final int opc_arraylength	= 190;
    public static final int opc_athrow	= 191;
    public static final int opc_checkcast	= 192;
    public static final int opc_instanceof	= 193;
    public static final int opc_monitorenter	= 194;
    public static final int opc_monitorexit	= 195;
    public static final int opc_wide	= 196;
    public static final int opc_multianewarray	= 197;
    public static final int opc_ifnull	= 198;
    public static final int opc_ifnonnull	= 199;
    public static final int opc_goto_w	= 200;
    public static final int opc_jsr_w	= 201;
    public static final int opc_breakpoint	= 202;
    public static final int opc_aldc_ind_quick	= 203;
    public static final int opc_aldc_ind_w_quick	= 204;
    public static final int opc_invokestatic_quick	= 205;
    public static final int opc_invokestatic_checkinit_quick	= 206;
    public static final int opc_invokevirtual_quick	= 207;
    public static final int opc_getfield_quick	= 208;
    public static final int opc_agetfield_quick	= 209;
    public static final int opc_vinvokevirtual_quick	= 210;
    public static final int opc_invokevirtual_quick_w	= 211;
    public static final int opc_putfield_quick	= 212;
    public static final int opc_invokenonvirtual_quick	= 213;
    public static final int opc_invokesuper_quick	= 214;
    public static final int opc_invokeignored_quick	= 215;
    public static final int opc_getfield2_quick	= 216;
    public static final int opc_checkcast_quick	= 217;
    public static final int opc_instanceof_quick	= 218;
    public static final int opc_nonnull_quick	= 219;
    public static final int opc_putfield2_quick	= 220;
    public static final int opc_ainvokevirtual_quick	= 221;
    public static final int opc_invokevirtualobject_quick	= 222;
    public static final int opc_invokeinterface_quick	= 223;
    public static final int opc_aldc_quick	= 224;
    public static final int opc_ldc_quick	= 225;
    public static final int opc_exittransition	= 226;
    public static final int opc_dinvokevirtual_quick	= 227;
    public static final int opc_aldc_w_quick	= 228;
    public static final int opc_ldc_w_quick	= 229;
    public static final int opc_aputfield_quick	= 230;
    public static final int opc_getfield_quick_w	= 231;
    public static final int opc_ldc2_w_quick	= 232;
    public static final int opc_agetstatic_quick	= 233;
    public static final int opc_getstatic_quick	= 234;
    public static final int opc_getstatic2_quick	= 235;
    public static final int opc_aputstatic_quick	= 236;
    public static final int opc_putstatic_quick	= 237;
    public static final int opc_putstatic2_quick	= 238;
    public static final int opc_agetstatic_checkinit_quick	= 239;
    public static final int opc_getstatic_checkinit_quick	= 240;
    public static final int opc_getstatic2_checkinit_quick	= 241;
    public static final int opc_aputstatic_checkinit_quick	= 242;
    public static final int opc_putstatic_checkinit_quick	= 243;
    public static final int opc_putstatic2_checkinit_quick	= 244;
    public static final int opc_putfield_quick_w	= 245;
    public static final int opc_new_checkinit_quick	= 246;
    public static final int opc_new_quick	= 247;
    public static final int opc_anewarray_quick	= 248;
    public static final int opc_multianewarray_quick	= 249;
    public static final int opc_prefix	= 250;
    public static final int opc_invokeinit	= 251;

    public static final String[] opcNames = {
	"nop", "aconst_null", "iconst_m1", "iconst_0",
	"iconst_1", "iconst_2", "iconst_3", "iconst_4",
	"iconst_5", "lconst_0", "lconst_1", "fconst_0",
	"fconst_1", "fconst_2", "dconst_0", "dconst_1",
	"bipush", "sipush", "ldc", "ldc_w",
	"ldc2_w", "iload", "lload", "fload",
	"dload", "aload", "iload_0", "iload_1",
	"iload_2", "iload_3", "lload_0", "lload_1",
	"lload_2", "lload_3", "fload_0", "fload_1",
	"fload_2", "fload_3", "dload_0", "dload_1",
	"dload_2", "dload_3", "aload_0", "aload_1",
	"aload_2", "aload_3", "iaload", "laload",
	"faload", "daload", "aaload", "baload",
	"caload", "saload", "istore", "lstore",
	"fstore", "dstore", "astore", "istore_0",
	"istore_1", "istore_2", "istore_3", "lstore_0",
	"lstore_1", "lstore_2", "lstore_3", "fstore_0",
	"fstore_1", "fstore_2", "fstore_3", "dstore_0",
	"dstore_1", "dstore_2", "dstore_3", "astore_0",
	"astore_1", "astore_2", "astore_3", "iastore",
	"lastore", "fastore", "dastore", "aastore",
	"bastore", "castore", "sastore", "pop",
	"pop2", "dup", "dup_x1", "dup_x2",
	"dup2", "dup2_x1", "dup2_x2", "swap",
	"iadd", "ladd", "fadd", "dadd",
	"isub", "lsub", "fsub", "dsub",
	"imul", "lmul", "fmul", "dmul",
	"idiv", "ldiv", "fdiv", "ddiv",
	"irem", "lrem", "frem", "drem",
	"ineg", "lneg", "fneg", "dneg",
	"ishl", "lshl", "ishr", "lshr",
	"iushr", "lushr", "iand", "land",
	"ior", "lor", "ixor", "lxor",
	"iinc", "i2l", "i2f", "i2d",
	"l2i", "l2f", "l2d", "f2i",
	"f2l", "f2d", "d2i", "d2l",
	"d2f", "i2b", "i2c", "i2s",
	"lcmp", "fcmpl", "fcmpg", "dcmpl",
	"dcmpg", "ifeq", "ifne", "iflt",
	"ifge", "ifgt", "ifle", "if_icmpeq",
	"if_icmpne", "if_icmplt", "if_icmpge", "if_icmpgt",
	"if_icmple", "if_acmpeq", "if_acmpne", "goto",
	"jsr", "ret", "tableswitch", "lookupswitch",
	"ireturn", "lreturn", "freturn", "dreturn",
	"areturn", "return", "getstatic", "putstatic",
	"getfield", "putfield", "invokevirtual", "invokespecial",
	"invokestatic", "invokeinterface", "xxxunusedxxx", "new",
	"newarray", "anewarray", "arraylength", "athrow",
	"checkcast", "instanceof", "monitorenter", "monitorexit",
	"wide", "multianewarray", "ifnull", "ifnonnull",
	"goto_w", "jsr_w", "breakpoint", "aldc_ind_quick",
	"aldc_ind_w_quick", "invokestatic_quick", "invokestatic_checkinit_quick", "invokevirtual_quick",
	"getfield_quick", "agetfield_quick", "vinvokevirtual_quick", "invokevirtual_quick_w",
	"putfield_quick", "invokenonvirtual_quick", "invokesuper_quick", "invokeignored_quick",
	"getfield2_quick", "checkcast_quick", "instanceof_quick", "nonnull_quick",
	"putfield2_quick", "ainvokevirtual_quick", "invokevirtualobject_quick", "invokeinterface_quick",
	"aldc_quick", "ldc_quick", "exittransition", "dinvokevirtual_quick",
	"aldc_w_quick", "ldc_w_quick", "aputfield_quick", "getfield_quick_w",
	"ldc2_w_quick", "agetstatic_quick", "getstatic_quick", "getstatic2_quick",
	"aputstatic_quick", "putstatic_quick", "putstatic2_quick", "agetstatic_checkinit_quick",
	"getstatic_checkinit_quick", "getstatic2_checkinit_quick", "aputstatic_checkinit_quick", "putstatic_checkinit_quick",
	"putstatic2_checkinit_quick", "putfield_quick_w", "new_checkinit_quick", "new_quick",
	"anewarray_quick", "multianewarray_quick", "prefix", "invokeinit",
	"??252", "??253", "hardware", "software",
    };

    public static final int[] opcLengths = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 2, 3, 2, 3,
	3, 2, 2, 2, 2, 2, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 2, 2, 2, 2, 2, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 3, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
	0, 0, 1, 1, 1, 1, 1, 1, 3, 3,
	3, 3, 3, 3, 3, 5, 0, 3, 2, 3,
	1, 1, 3, 3, 1, 1, 0, 4, 3, 3,
	5, 5, 1, 2, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	3, 3, 3, 5, 2, 2, 1, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 4,
	0, 3, 0, 0, 0, 0,
    };
}
