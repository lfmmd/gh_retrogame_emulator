#ifndef INCLUDED_CVMJITCompileExpression
#define INCLUDED_CVMJITCompileExpression
	/* MACHINE GENERATED -- DO NOT EDIT DIRECTLY! */ 
/* Possible error states returned by CVMJITCompileExpression. */
#define JIT_IR_SYNTAX_ERROR -1
#define JIT_RESOURCE_NONE_ERROR -2

#ifndef IN_CVMJITCompileExpression_DATA
struct CVMJITCompileExpression_match_computation_state { 
	int	opcode; 
	CVMJITIRNodePtr	p; 
	CVMJITIRNodePtr	subtrees[2]; 
	int	which_submatch; 
	int	n_submatch; 
}; 

struct CVMJITCompileExpression_rule_computation_state { 
	CVMJITIRNodePtr	p; 
	int	ruleno; 
	int	subgoals_todo; 
	const struct CVMJITCompileExpression_action_pairs_t*	app; 
#define CVMJITCompileExpression_MAX_ARITY 3
	CVMJITIRNodePtr	* curr_submatch_root;
	CVMJITIRNodePtr	submatch_roots[CVMJITCompileExpression_MAX_ARITY];
	CVMJITCompileExpression_attribute *	curr_attribute;
	CVMJITCompileExpression_attribute	attributes[CVMJITCompileExpression_MAX_ARITY];
}; 

int
CVMJITCompileExpression_match( CVMJITIRNodePtr root, CVMJITCompilationContext* con );
int
CVMJITCompileExpression_synthesis( CVMJITIRNodePtr root, CVMJITCompilationContext* con );
int
CVMJITCompileExpression_action( CVMJITIRNodePtr root, CVMJITCompilationContext* con );

#endif

struct CVMJITCompileExpression_action_pairs_t { 
	unsigned short pathno;
	unsigned short subgoal;
}; 

struct CVMJITCompileExpression_rule_action {
	unsigned short	n_subgoals;
	unsigned short	action_pair_index;
};

#endif

/* Macros list for rule [  8] reg32: SLL32 reg32 ICONST_32 : */
#ifndef CVM_NEED_DO_INT_SHIFT_HELPER
#define CVM_NEED_DO_INT_SHIFT_HELPER
#endif

/* Macros list for rule [  9] reg32: SLL32 reg32 reg32 : */
#ifndef CVM_NEED_DO_REG_SHIFT_HELPER
#define CVM_NEED_DO_REG_SHIFT_HELPER
#endif

/* Macros list for rule [ 10] reg32: SRL32 reg32 ICONST_32 : */
#ifndef CVM_NEED_DO_INT_SHIFT_HELPER
#define CVM_NEED_DO_INT_SHIFT_HELPER
#endif

/* Macros list for rule [ 11] reg32: SRL32 reg32 reg32 : */
#ifndef CVM_NEED_DO_REG_SHIFT_HELPER
#define CVM_NEED_DO_REG_SHIFT_HELPER
#endif

/* Macros list for rule [ 12] reg32: SRA32 reg32 ICONST_32 : */
#ifndef CVM_NEED_DO_INT_SHIFT_HELPER
#define CVM_NEED_DO_INT_SHIFT_HELPER
#endif

/* Macros list for rule [ 13] reg32: SRA32 reg32 reg32 : */
#ifndef CVM_NEED_DO_REG_SHIFT_HELPER
#define CVM_NEED_DO_REG_SHIFT_HELPER
#endif

/* Macros list for rule [137] effect: VINTRINSIC parameters intrinsicMB : */
#ifndef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#define CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#endif

/* Macros list for rule [138] invoke32_result: INTRINSIC32 parameters intrinsicMB : */
#ifndef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#define CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#endif

/* Macros list for rule [139] invoke64_result: INTRINSIC64 parameters intrinsicMB : */
#ifndef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#define CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#endif

/* Macros list for rule [140] effect: VINTRINSIC iargs intrinsicMB : */
#ifndef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#define CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#endif

/* Macros list for rule [141] reg32: INTRINSIC32 iargs intrinsicMB : */
#ifndef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#define CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#endif

/* Macros list for rule [142] reg64: INTRINSIC64 iargs intrinsicMB : */
#ifndef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#define CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
#endif

/* Macros list for rule [166] root: BCOND_FLOAT reg32 reg32 : */
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
#ifndef CVM_NEED_DO_FCMP_HELPER
#define CVM_NEED_DO_FCMP_HELPER
#endif
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/

/* Macros list for rule [167] root: BCOND_DOUBLE reg64 reg64 : */
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
#ifndef CVM_NEED_DO_DCMP_HELPER
#define CVM_NEED_DO_DCMP_HELPER
#endif
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/

/* Macros list for rule [169] reg32: FCMPL reg32 reg32 : */
#ifndef CVM_NEED_DO_FCMP_HELPER
#define CVM_NEED_DO_FCMP_HELPER
#endif

/* Macros list for rule [170] reg32: FCMPG reg32 reg32 : */
#ifndef CVM_NEED_DO_FCMP_HELPER
#define CVM_NEED_DO_FCMP_HELPER
#endif

/* Macros list for rule [171] reg32: DCMPL reg64 reg64 : */
#ifndef CVM_NEED_DO_DCMP_HELPER
#define CVM_NEED_DO_DCMP_HELPER
#endif

/* Macros list for rule [172] reg32: DCMPG reg64 reg64 : */
#ifndef CVM_NEED_DO_DCMP_HELPER
#define CVM_NEED_DO_DCMP_HELPER
#endif

/* Macros list for rule [256] root: BCOND_FLOAT freg32 freg32 : */
#ifndef CVM_NEED_COMPARE_FLOATS_HELPER
#define CVM_NEED_COMPARE_FLOATS_HELPER
#endif

/* Macros list for rule [257] root: BCOND_DOUBLE freg64 freg64 : */
#ifndef CVM_NEED_COMPARE_FLOATS_HELPER
#define CVM_NEED_COMPARE_FLOATS_HELPER
#endif

/* Macros list for rule [258] reg32: IDIV32 reg32 reg32 : */
#ifndef CVM_NEED_WORD_BINARY_OP_WITH_REG32_RHS
#define CVM_NEED_WORD_BINARY_OP_WITH_REG32_RHS
#endif

/* Macros list for rule [259] reg32: IREM32 reg32 reg32 : */
#ifndef CVM_NEED_WORD_BINARY_OP_WITH_REG32_RHS
#define CVM_NEED_WORD_BINARY_OP_WITH_REG32_RHS
#endif

