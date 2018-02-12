#line 29 "../../src/portlibs/jit/risc/jitgrammarincludes.jcs"


#include "javavm/include/defs.h"
#include "javavm/include/objects.h"
#include "javavm/include/classes.h"
#include "javavm/include/utils.h"
#include "javavm/include/ccee.h"
#include "javavm/include/ccm_runtime.h"
#include "javavm/include/globals.h"
#include "javavm/include/basictypes.h"
#include "javavm/include/jit/jitir.h"
#include "javavm/include/jit/jit_defs.h"
#include "javavm/include/jit/jitcontext.h"
#include "javavm/include/jit/jitirnode.h"
#include "javavm/include/jit/jitirblock.h"
#include "portlibs/jit/risc/include/porting/jitriscemitter.h"
#include "portlibs/jit/risc/include/export/jitregman.h"
#include "portlibs/jit/risc/include/porting/ccmrisc.h"
#include "portlibs/jit/risc/jitstackman.h"
#include "portlibs/jit/risc/jitgrammar.h"
#include "portlibs/jit/risc/jitopcodes.h"
#include "javavm/include/jit/jitstackmap.h"
#include "javavm/include/jit/jitfixup.h"
#include "javavm/include/jit/jitconstantpool.h"
#include "javavm/include/jit/jitcodebuffer.h"
#include "javavm/include/jit/jitintrinsic.h"
#include "javavm/include/jit/jitutils.h"
#include "javavm/include/jit/jitstats.h"
#include "javavm/include/jit/jitcomments.h"
#include "javavm/include/jit/jitmemory.h"
#include "generated/javavm/include/jit/jitcodegen.h"
#include "generated/javavm/include/gc_config.h"

#include "javavm/include/clib.h"
#include "javavm/include/porting/ansi/setjmp.h"
#include "javavm/include/porting/jit/jit.h"
#include "javavm/include/porting/doubleword.h"
#include "javavm/include/porting/endianness.h"

#line 29 "../../src/portlibs/jit/risc/jitgrammardefs.jcs"


/*
    #define CVMJITCompileExpression_DEBUG 1
    #define id(p) ((p)->tag)
*/

#ifndef CVM_INLINE
#define CVM_INLINE /* */
#ifdef __GNUC__
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 90)
#undef  CVM_INLINE
#define CVM_INLINE inline
#endif
#endif /* __GNUC__ */
#endif /* CVM_INLINE */

/*
 * Some macros that create the regman register mask for some registers we use.
 */
#undef  ARG1
#undef  ARG2
#undef  ARG3
#undef  ARG4
#undef  RESULT1
#undef  RESULT2
#define ARG1    (1U<<CVMCPU_ARG1_REG)
#define ARG2    (1U<<CVMCPU_ARG2_REG)
#define ARG3    (1U<<CVMCPU_ARG3_REG)
#define ARG4    (1U<<CVMCPU_ARG4_REG)
#define RESULT1 (1U<<CVMCPU_RESULT1_REG)
#define RESULT2 (1U<<CVMCPU_RESULT2_REG)

/*
 * Some useful constants.
 */
/* offset of length field in array */
#undef  ARRAY_LENGTH_OFFSET
#define ARRAY_LENGTH_OFFSET  offsetof(CVMArrayOfAnyType, length)
/* Array data offset */
#undef  ARRAY_DATA_OFFSET
#define ARRAY_DATA_OFFSET     offsetof(CVMArrayOfAnyType, elems)
/* cb's offset in the object header */
#undef  OBJECT_CB_OFFSET
#define OBJECT_CB_OFFSET     offsetof(CVMObjectHeader, clas)
/* MethodTable offset in cb */ 
#undef  CB_VTBL_OFFSET
#define CB_VTBL_OFFSET       offsetof(CVMClassBlock, methodTablePtrX)

#ifdef CVM_SEGMENTED_HEAP
    /* The offset */
#undef GC_SEGMENT_CTV_OFFSET
#define GC_SEGMENT_CTV_OFFSET offsetof(CVMGenSegment, cardTableVirtual)
#endif

/* Purpose: Verifies the condition, else throws a JIT error. */
#define validateStack(condition, stackName) { \
    CVMassert(condition);                    \
    if (!(condition)) {                      \
        CVMJITerror(con, CANNOT_COMPILE, #stackName " stack too small"); \
    } \
}

#ifdef CVM_TRACE_JIT
/* The following strings are used to emit codegen comments for the checks for
   each of the type of exceptions listed in the CVMJITIRTrapID enum list: */
const char *const trapCheckComments[CVMJITIR_NUM_TRAP_IDS] = {
    "NULL check",
    "ArrayIndexOutOfBounds check",
    "DivideByZero check",
};
#endif

#define IRRecordState(p,v)	(p)->curRootCnt = (v)
#define IRGetState(p)		((p)->curRootCnt)

/* The following are for collecting stats about compilation stack usage: */
#ifdef CVM_JIT_COLLECT_STATS

#define INITIALIZE_STACK_STATS(name) \
    int name##StackElements = 0;

#define statsPushStack(name) { \
    name##StackElements++;  \
    CVMJITstatsRecordUpdateMax(con, CVMJIT_STATS_ACTUAL_##name##_STACK_MAX, \
                               name##StackElements);                        \
}
#define statsPopStack(name) { \
    name##StackElements--; \
}

#define statsPushResource() { \
    int stackElements; \
    stackElements = ((CVMInt8*)con->cgsp - (CVMInt8*)con->cgstackInit) / \
                    sizeof(struct CVMJITStackElement);                   \
    CVMJITstatsRecordUpdateMax(con, CVMJIT_STATS_ACTUAL_RESOURCE_STACK_MAX, \
                               stackElements); \
}
#define statsPopResource()

#else /* !CVM_JIT_COLLECT_STATS */
#define INITIALIZE_STACK_STATS(name)
#define statsPushStack(name)
#define statsPopStack(name)
#define statsPushResource()
#define statsPopResource()
#endif /* CVM_JIT_COLLECT_STATS */

#define INITIALIZE_MATCH_STACK \
    INITIALIZE_STACK_STATS(MATCH) \
    struct CVMJITCompileExpression_match_computation_state* mcp = \
	(struct CVMJITCompileExpression_match_computation_state*)\
	(con->compilationStateStack);

#define MATCH_PUSH( _p, op, l, r, n, arity ){ \
    mcp->p = (_p); \
    mcp->opcode = (op); \
    mcp->subtrees[0] = (l); \
    mcp->subtrees[1] = (r); \
    mcp->which_submatch = (n); \
    mcp++->n_submatch = (arity); \
    statsPushStack(MATCH); \
    validateStack(((void*)mcp < (void*)con->cgstackInit), Match); \
}

#define MATCH_POP( _p, op, l, r, n ) { \
    statsPopStack(MATCH); \
    _p = (--mcp)->p; \
    op = mcp->opcode; \
    l = mcp->subtrees[0]; \
    r = mcp->subtrees[1]; \
    n = mcp->which_submatch; \
    CVMassert( (void*)mcp >= con->compilationStateStack );\
}

#define GET_MATCH_STACK_TOP	(mcp-1)

#define MATCH_STACK_EMPTY	(mcp == con->compilationStateStack)

#define INITIALIZE_GOAL_STACK \
    goal_top = \
	(struct CVMJITCompileExpression_rule_computation_state*)\
	(con->compilationStateStack);

#define GOAL_STACK_TOP \
    ((struct CVMJITCompileExpression_rule_computation_state*)con->cgstackInit)

#define GOAL_STACK_EMPTY 	(goal_top == con->compilationStateStack)


/*
 * Return the set of registers that the value should be computed into.
 * The main purpose of the function is to examine any decoration that
 * was put on the node to see if this node will have special register
 * requirements.
	computeTargetRegs((con), &(goal_top->attributes[0].u.rs), thisNode); \
 */
static void
computeTargetRegs(
    CVMJITCompilationContext* con,
    CVMJITCompileExpression_attribute * attr,
    CVMJITIRNode* node)
{
    node = CVMJITirnodeValueOf(node);
#if 0
    CVMconsolePrintf("computeTargetRegs: nodeID(%d)\n", node->nodeID);
#endif
    switch (node->decorationType) {
        case CVMJIT_REGHINT_DECORATION: {
	    CVMInt16 targetReg = node->decorationData.regHint;
	    if (targetReg == -1) {
		attr->u.rs.target = CVMRM_ANY_SET;
#ifdef CVM_JIT_USE_FP_HARDWARE
		attr->u.rs.float_target = CVMRM_FP_ANY_SET;
#endif
	    } else {
#ifdef CVM_JIT_USE_FP_HARDWARE
		int tag = CVMJITgetTypeTag(node);
		if (tag == CVM_TYPEID_FLOAT || tag == CVM_TYPEID_DOUBLE) {
		    attr->u.rs.target = CVMRM_ANY_SET;
		    attr->u.rs.float_target =  1U << targetReg;
		} else {
		    attr->u.rs.target =  1U << targetReg;
		    attr->u.rs.float_target = CVMRM_FP_ANY_SET;
		}
#else
		attr->u.rs.target =  1U << targetReg;
#endif
	    }
	    break;
	}
        default:
	    attr->u.rs.target = CVMRM_ANY_SET;
#ifdef CVM_JIT_USE_FP_HARDWARE
	    attr->u.rs.float_target = CVMRM_FP_ANY_SET;
#endif
	    break;
    }
}

#ifdef CVM_DEBUG_ASSERTS
#define SET_ATTRIBUTE_TYPE(n, type_) \
        goal_top->attributes[n].type = (type_)
#else
#define SET_ATTRIBUTE_TYPE(n, type_) ((void)0)
#endif

/* Purpose: Default synthesis action for chain rules */
#define DEFAULT_SYNTHESIS_CHAIN(con, thisNode) \
	DEFAULT_SYNTHESIS1(con, thisNode)

/* Purpose: Default synthesis action for a rule with a unary node. */
#define DEFAULT_SYNTHESIS1(con, thisNode) \
	(thisNode)->regsRequired = submatch_roots[0]->regsRequired

/* Purpose: Default synthesis action for a rule with a binary node. */
#define DEFAULT_SYNTHESIS2(con, thisNode) \
	(thisNode)->regsRequired = (submatch_roots[0]->regsRequired | \
	     submatch_roots[1]->regsRequired)

#define DEFINE_SYNTHESIS(con, thisNode_)				    \
     DEFAULT_SYNTHESIS1(con, thisNode_);				    \
     {									    \
	 CVMJITIRNode* lhs =						    \
             CVMJITirnodeValueOf(CVMJITirnodeGetLeftSubtree(thisNode_));    \
	 /* see if lhs is a USED node at the same spill location as the	    \
	  * DEFINE node. If so, then eager loading of the USED node is	    \
	  * a bad idea because it is already up-to-date and will never	    \
	  * be stored.							    \
	  */								    \
	 if (CVMJITirnodeIsEndInlining(lhs)) {				    \
             lhs = CVMJITirnodeGetLeftSubtree(lhs);			    \
         }								    \
	 if (CVMJITirnodeIsUsedNode(lhs)) {				    \
	     CVMJITDefineOp* defineOp = CVMJITirnodeGetDefineOp(thisNode_); \
	     CVMJITUsedOp* usedOp = CVMJITirnodeGetUsedOp(lhs);		    \
	     if (usedOp->spillLocation ==				    \
		 CVMJITirnodeGetUsedOp(CVMJITirnodeValueOf(		    \
                     defineOp->usedNode))->spillLocation)		    \
	     {								    \
		 /* this will prevent eager loading of the USED node	    \
		  * which may not get used before block exit. */	    \
		 lhs->regsRequired = CVMCPU_AVOID_METHOD_CALL;		    \
	     }								    \
	 }								    \
     }

/* Purpose: Default inheritance action for chain rules */
#define DEFAULT_INHERITANCE_CHAIN(con, thisNode) {			 \
    if (GOAL_STACK_EMPTY) {						 \
        SET_ATTRIBUTE_TYPE(0, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
	computeTargetRegs((con), &(goal_top->attributes[0]), thisNode);	 \
	goal_top->attributes[0].u.rs.avoid = CVMCPU_AVOID_NONE;		 \
    } else {								 \
        goal_top->attributes[0] = *goal_top[-1].curr_attribute;		 \
    }									 \
}

/* Purpose: Default inheritance action for a rule with arity 1. */
#define DEFAULT_INHERITANCE1(con, thisNode_) {				 \
	CVMassert(subgoals_todo == 1);					 \
        SET_ATTRIBUTE_TYPE(0, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
	computeTargetRegs((con), &(goal_top->attributes[0]),		 \
				 goal_top->submatch_roots[0]);		 \
	goal_top->attributes[0].u.rs.avoid = CVMCPU_AVOID_NONE;		 \
}

/* Purpose: Default inheritance action for a rule with arity 2. */
#define DEFAULT_INHERITANCE2(con, thisNode_) {				 \
	CVMJITIRNodePtr n0 = goal_top->submatch_roots[0];		 \
	CVMJITIRNodePtr n1 = goal_top->submatch_roots[1];		 \
        CVMassert(subgoals_todo == 2);                                   \
        SET_ATTRIBUTE_TYPE(0, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
        SET_ATTRIBUTE_TYPE(1, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
        computeTargetRegs((con), &(goal_top->attributes[0]), n0);	 \
        goal_top->attributes[0].u.rs.avoid = n1->regsRequired;		 \
        computeTargetRegs((con), &(goal_top->attributes[1]), n1);	 \
        goal_top->attributes[1].u.rs.avoid = CVMCPU_AVOID_NONE;          \
}

/* Purpose: Default inheritance action for a rule with a binary node,
            but we only want to target the rhs. */
#define ASSIGN_INHERITANCE(con, thisNode_) {				 \
	CVMJITIRNode *thisNode = (thisNode_);				 \
	CVMassert(subgoals_todo == 1);					 \
        SET_ATTRIBUTE_TYPE(0, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
	computeTargetRegs((con), &(goal_top->attributes[0]),		 \
				 CVMJITirnodeGetRightSubtree(thisNode)); \
	goal_top->attributes[0].u.rs.avoid = CVMCPU_AVOID_NONE;		 \
}

/* Purpose: Default synthesis action for a rule with a flattened tree
            containing a unary node with a binary node as its child. */
#define L_BINARY_SYNTHESIS(con, thisNode_) { \
    CVMJITIRNode *thisNode = (thisNode_);				\
    CVMJITIRNode *lhs = CVMJITirnodeGetLeftSubtree(thisNode);		\
    DEFAULT_SYNTHESIS2((con), lhs);					\
    thisNode->regsRequired = lhs->regsRequired;				\
}

/* Purpose: Default inheritance action for a rule with a flattened tree
            containing a unary node with a binary node as its child. */
#define L_BINARY_INHERITANCE(con, thisNode) \
    DEFAULT_INHERITANCE2((con), CVMJITirnodeGetLeftSubtree(thisNode))

/* Purpose: Default synthesis and inheritance actions for a rule with a
            unary node, with another unary node as its left subtree. */
#define UNARY_UNARY_reg_SYNTHESIS(con, thisNode_) {		\
    CVMJITIRNode *thisNode = (thisNode_);			\
    CVMJITIRNode *lhs = CVMJITirnodeGetLeftSubtree(thisNode);	\
    DEFAULT_SYNTHESIS1((con), lhs);				\
    thisNode->regsRequired = lhs->regsRequired;			\
}
#define UNARY_UNARY_reg_INHERITANCE DEFAULT_INHERITANCE1

/* Purpose: Default synthesis and inheritance actions for a rule with a
            flattened tree containing a binary node with another binary
            node as its left hand side child */
#define L_BINARY_R_UNARY_SYNTHESIS BINARY_BINARY_reg_reg_reg_SYNTHESIS
#define BINARY_BINARY_reg_reg_reg_SYNTHESIS(con, thisNode_) { \
    CVMJITIRNode *thisNode = (thisNode_);				\
    CVMJITIRNode *lhs = CVMJITirnodeGetLeftSubtree(thisNode);		\
    CVMJITIRNode *rhs = CVMJITirnodeGetRightSubtree(thisNode);		\
    DEFAULT_SYNTHESIS2((con), lhs);					\
    DEFAULT_SYNTHESIS1((con), rhs);					\
    thisNode->regsRequired = lhs->regsRequired | rhs->regsRequired;	\
}
#define L_BINARY_R_UNARY_INHERITANCE BINARY_BINARY_reg_reg_reg_INHERITANCE
#define BINARY_BINARY_reg_reg_reg_INHERITANCE DEFAULT_INHERITANCE3
#define DEFAULT_INHERITANCE3(con, thisNode) {				    \
    SET_ATTRIBUTE_TYPE(0, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID);        \
    SET_ATTRIBUTE_TYPE(1, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID);        \
    SET_ATTRIBUTE_TYPE(2, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID);        \
    /* The left operand should avoid the next two operands: */		    \
    computeTargetRegs((con), &(goal_top->attributes[0]),        	    \
        goal_top->submatch_roots[0]);					    \
    goal_top->attributes[0].u.rs.avoid  =				    \
	goal_top->submatch_roots[1]->regsRequired |			    \
	goal_top->submatch_roots[2]->regsRequired;			    \
    /* The mid operand should avoid the right operand: */		    \
    computeTargetRegs((con), &(goal_top->attributes[1]),        	    \
        goal_top->submatch_roots[1]);					    \
    goal_top->attributes[1].u.rs.avoid  =				    \
	goal_top->submatch_roots[2]->regsRequired;			    \
    /* The right operand doesn't need to avoid anything: */		    \
    computeTargetRegs((con), &(goal_top->attributes[2]),        	    \
         goal_top->submatch_roots[2]);					    \
    goal_top->attributes[2].u.rs.avoid = CVMCPU_AVOID_NONE;                 \
}

/* Purpose: Default synthesis and inheritance actions for a rule with a
            flattened tree containing a binary node with another binary
            node as its right hand side child */
#define BINARY_reg_BINARY_reg_reg_SYNTHESIS(con, thisNode_) { \
    CVMJITIRNode *thisNode = (thisNode_);				\
    CVMJITIRNode *lhs = CVMJITirnodeGetLeftSubtree(thisNode);		\
    CVMJITIRNode *rhs = CVMJITirnodeGetRightSubtree(thisNode);		\
    DEFAULT_SYNTHESIS1((con), lhs);					\
    DEFAULT_SYNTHESIS2((con), rhs);					\
    thisNode->regsRequired = lhs->regsRequired | rhs->regsRequired;	\
}
#define BINARY_reg_BINARY_reg_reg_INHERITANCE DEFAULT_INHERITANCE3

/* Purpose: Default synthesis and inheritance actions for a rule with a
            flattened tree containing a binary node with another binary
            node as its right hand side child */
#define BINARY_UNARY_BINARY_reg_reg_reg_SYNTHESIS(con, thisNode_) {	  \
    CVMJITIRNode *thisNode = (thisNode_);				  \
    CVMJITIRNode *lhs =							  \
        CVMJITirnodeGetLeftSubtree(CVMJITirnodeGetLeftSubtree(thisNode)); \
    CVMJITIRNode *rhs = CVMJITirnodeGetRightSubtree(thisNode);		  \
    DEFAULT_SYNTHESIS2((con), lhs);					  \
    DEFAULT_SYNTHESIS1((con), rhs);					  \
    thisNode->regsRequired = lhs->regsRequired | rhs->regsRequired;	  \
}
#define BINARY_UNARY_BINARY_reg_reg_reg_INHERITANCE DEFAULT_INHERITANCE3

/* Purpose: Default synthesis and inheritance actions for a rule with a
            flattened tree containing a binary node with a unary
            node as its left hand side child, a binary node as it
            rhigh hand side child. */
#define BINARY_UNARY_reg_BINARY_reg_reg_SYNTHESIS(con, thisNode_) {	  \
    CVMJITIRNode *thisNode = (thisNode_);				  \
    CVMJITIRNode *lhs =							  \
        CVMJITirnodeGetLeftSubtree(CVMJITirnodeGetLeftSubtree(thisNode)); \
    CVMJITIRNode *rhs =	CVMJITirnodeGetRightSubtree(thisNode);		  \
    DEFAULT_SYNTHESIS1((con), lhs);					  \
    DEFAULT_SYNTHESIS2((con), rhs);					  \
    thisNode->regsRequired = lhs->regsRequired | rhs->regsRequired;	  \
}
#define BINARY_UNARY_reg_BINARY_reg_reg_INHERITANCE DEFAULT_INHERITANCE3

/*
 * TARGETING: Macros for specifying preferred target registers for an IR node.
 */

/* NOTE: _SET_TARGET() is private and only to be called by other macros. */
#define _SET_TARGET(n, regset) { \
        SET_ATTRIBUTE_TYPE(n, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
	goal_top->attributes[n].u.rs.target = regset;   \
}

#define SET_TARGET1(thisNode, regset) {			\
	DEFAULT_INHERITANCE1(con, (thisNode));		\
	_SET_TARGET(0, (regset));			\
}

#define SET_TARGET2(thisNode, regset1, regset2) {	\
	DEFAULT_INHERITANCE2(con, (thisNode));		\
	_SET_TARGET(0, (regset1));			\
	_SET_TARGET(1, (regset2));			\
}

/* Set the register targetting information without doing any inheritance work. */
#define SET_TARGET3_WO_INHERITANCE(thisNode, regset1, regset2, regset3) { \
	_SET_TARGET(0, (regset1));			\
	_SET_TARGET(1, (regset2));			\
	_SET_TARGET(2, (regset3));			\
}

#define SET_TARGET2_0(thisNode, regset1) {		\
	DEFAULT_INHERITANCE2(con, (thisNode));		\
	_SET_TARGET(0, (regset1));			\
}

#define SET_TARGET2_1(thisNode, regset2) {		\
	DEFAULT_INHERITANCE2(con, (thisNode));		\
	_SET_TARGET(1, (regset2));			\
}

/* 
 * AVOIDING: Macros for specifying registers that the IR node prefers to avoid
 * when doing register targeting.
 *
 * Targeting above is handled with full register sets. Avoiding below
 * is handled using predefined register sets. This allows us to encode
 * the avoid sets using just a few bits rather than 32-bits for GPRs
 * plus 32-bits for FPRs. This is significant because the avoid set has
 * to be stored in the regsRequired field if the IR node, so storing
 * in just a few bits saves us two words in each node.
 */

#define CVMCPU_NUM_AVOID_SETS 4
const CVMRMregset CVMCPUavoidSets[CVMCPU_NUM_AVOID_SETS] = {
    CVMRM_EMPTY_SET,   /* nothing to avoid */
    ~CVMRM_SAFE_SET,   /* AVOID_C_CALL */
    CVMRM_ANY_SET,     /* AVOID_METHOD_CALL */
    CVMRM_ANY_SET      /* AVOID_C_CALL and AVOID_METHOD_CALL */
};

#define CVMCPUconvertAvoidBitsToAvoidSet(avoidBits)		\
    (CVMassert((CVMUint32)avoidBits < CVMCPU_NUM_AVOID_SETS),	\
     CVMCPUavoidSets[avoidBits])

#ifdef CVM_JIT_USE_FP_HARDWARE
const CVMRMregset CVMCPUfloatAvoidSets[CVMCPU_NUM_AVOID_SETS] = {
    CVMRM_EMPTY_SET,      /* nothing to avoid */
    ~CVMRM_FP_SAFE_SET,   /* AVOID_C_CALL */
    CVMRM_FP_ANY_SET,     /* AVOID_METHOD_CALL */
    CVMRM_FP_ANY_SET      /* AVOID_C_CALL and AVOID_METHOD_CALL */
};

#define CVMCPUconvertAvoidBitsToFloatAvoidSet(avoidBits)	\
    (CVMassert((CVMUint32)avoidBits < CVMCPU_NUM_AVOID_SETS),	\
     CVMCPUfloatAvoidSets[avoidBits])
#endif /* CVM_JIT_USE_FP_HARDWARE */


#define SET_AVOID(n, regset) \
	((n)->regsRequired = (regset))

#define SET_AVOID_C_CALL(n) \
	SET_AVOID((n), CVMCPU_AVOID_C_CALL)

#define SET_AVOID_METHOD_CALL(n) \
	SET_AVOID((n), CVMCPU_AVOID_METHOD_CALL)

#define SET_CALL_CONTEXT(n, callContext_) \
        SET_ATTRIBUTE_TYPE(n, CVMJIT_EXPRESSION_ATTRIBUTE_CALL_CONTEXT), \
        goal_top->attributes[n].u.callContext = (callContext_)

#define GET_CALL_CONTEXT(con) \
        (CVMassert(((goal_top-1)->curr_attribute)->type == \
                  CVMJIT_EXPRESSION_ATTRIBUTE_CALL_CONTEXT), \
         (CVMCPUCallContext *)(((goal_top-1)->curr_attribute)->u.callContext))

#define SET_TARGET_IARG(con, node) \
        iargTarget((con), (node), goal_top, subgoals_todo)

#define END_TARGET_IARG(con, node) \
        iargTarget((con), (node), goal_top, subgoals_todo)

#define SET_AVOID_INTRINSIC_CALL(con, node) \
        SET_AVOID((node), intrinsicRequired((con), (node), submatch_roots))

#define SET_TARGET_INTRINSIC_CALL(con, node) \
        DEFAULT_INHERITANCE2((con), (node))

#ifdef CVMJIT_INTRINSICS

#ifdef CVMJIT_INTRINSICS_HAVE_PLATFORM_SPECIFIC_REG_ARGS
#define GET_REG_ARGS(properties) \
	(((properties) & CVMJITINTRINSIC_REG_ARGS) != 0)
#else
#define GET_REG_ARGS(properties)	CVM_FALSE
#endif

static CVMJITRegsRequiredType
intrinsicRequired(CVMJITCompilationContext *con, CVMJITIRNode *intrinsicNode,
                  CVMJITIRNodePtr *submatch_roots)
{
    CVMJITRegsRequiredType required;
    CVMUint16 intrinsicID = CVMJITirnodeGetBinaryOp(intrinsicNode)->data;
    CVMJITIntrinsic *irec = &CVMglobals.jit.intrinsics[intrinsicID - 1];
    const CVMJITIntrinsicConfig *config = irec->config;
    CVMUint16 properties = config->properties;
    required = submatch_roots[0]->regsRequired |
	       submatch_roots[1]->regsRequired;

    if ((properties & CVMJITINTRINSIC_OPERATOR_ARGS) != 0) {
        const CVMJITIntrinsicEmitterVtbl *emitter;
        emitter = (const CVMJITIntrinsicEmitterVtbl *)
                      config->emitterOrCCMRuntimeHelper;
        required = emitter->getRequired(con, intrinsicNode, required);
    } else {
        /* Just like a C call, we avoid the safe set because we'll be
           effectively making a C call to the helper: */
	CVMBool useRegArgs = GET_REG_ARGS(properties);
        required = CVMCPUCCALLgetRequired(con, required, intrinsicNode,
					  irec, useRegArgs);
    }
    return required;
}

static void
iargTarget(CVMJITCompilationContext *con, CVMJITIRNode *iargNode,
           struct CVMJITCompileExpression_rule_computation_state *goal_top,
           int subgoals_todo)
{
    CVMUint16 intrinsicID;
    CVMJITIntrinsic *irec;
    const CVMJITIntrinsicConfig *config;
    CVMUint16 properties;

    if (iargNode->tag != CVMJIT_ENCODE_NULL_IARG) {
        intrinsicID = CVMJIT_IARG_INTRINSIC_ID(iargNode);
    } else {
        intrinsicID = CVMJIT_NULL_IARG_INTRINSIC_ID(iargNode);
    }

    irec = &CVMglobals.jit.intrinsics[intrinsicID - 1];
    config = irec->config;
    properties = config->properties;

    if (iargNode->tag != CVMJIT_ENCODE_NULL_IARG) {
        int argType = CVMJITgetTypeTag(iargNode);
        int argNo = CVMJIT_IARG_ARG_NUMBER(iargNode);
        int argWordIndex = CVMJIT_IARG_WORD_INDEX(iargNode);
        CVMRMregset targetSet;

        CVMassert(CVMJITgetOpcode(iargNode) ==
                  (CVMJIT_IARG << CVMJIT_SHIFT_OPCODE));
        if ((properties & CVMJITINTRINSIC_OPERATOR_ARGS) != 0) {
            const CVMJITIntrinsicEmitterVtbl *emitter =
                         (const CVMJITIntrinsicEmitterVtbl *)
                             config->emitterOrCCMRuntimeHelper;
            targetSet = emitter->getArgTarget(con, argType, argNo,
                                              argWordIndex);
            SET_TARGET2_0(iargNode, targetSet);

        } else {
            CVMCPUCallContext *callContext;
            CVMBool useRegArgs = GET_REG_ARGS(properties);
            CVMassert((properties & CVMJITINTRINSIC_C_ARGS) != 0 ||
		      useRegArgs);
            if (argNo == 0) {
                callContext = CVMCPUCCallnewContext(con);
                CVMCPUCCALLinitArgs(con, callContext, irec, CVM_TRUE,
				    useRegArgs);
            } else {
                callContext = GET_CALL_CONTEXT(con);
            }
            targetSet = CVMCPUCCALLgetArgTarget(con, callContext,
		            argType, argNo, argWordIndex, useRegArgs);
            SET_TARGET2_0(iargNode, targetSet);
            SET_CALL_CONTEXT(1, callContext);
        }
    } else {
        if ((properties & CVMJITINTRINSIC_C_ARGS) != 0) {
            if (irec->numberOfArgs != 0) {
		CVMBool useRegArgs = GET_REG_ARGS(properties);
                CVMCPUCallContext *callContext = GET_CALL_CONTEXT(con);
                CVMCPUCCALLdestroyArgs(con, callContext, irec, CVM_TRUE,
				       useRegArgs);
            }
        }
    }
}

#else
#define intrinsicRequired(con, node, submatch_roots)    (CVMCPU_AVOID_NONE)
#define iargTarget(con, node, goal_top, subgoals_todo)
#endif /* CVMJIT_INTRINSICS */

#define GET_REGISTER_GOALS					\
        (CVMassert((goal_top-1)->curr_attribute->type ==	\
                  CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID),	\
         (goal_top-1)->curr_attribute->u.rs.target),		\
         CVMCPUconvertAvoidBitsToAvoidSet(			\
             (goal_top-1)->curr_attribute->u.rs.avoid)

#ifdef CVM_JIT_USE_FP_HARDWARE
#define GET_FLOAT_REGISTER_GOALS				\
        (CVMassert((goal_top-1)->curr_attribute->type ==	\
                    CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID),	\
         (goal_top-1)->curr_attribute->u.rs.float_target),	\
        CVMCPUconvertAvoidBitsToFloatAvoidSet(			\
              (goal_top-1)->curr_attribute->u.rs.avoid)
#endif

#define FLUSH_GOAL_TOP(con) \
        ((con)->goal_top = goal_top)

/**********************************************************************
 * RISC common definition of the codegen-time expression stack and
 * operations on it.
 * ( Optimization: all references to tag should be ifdef DEBUG, so we don't
 * even set it if we're not going to do the assertions. )
 */

/* Purpose: Pushes a resource. */
CVM_INLINE static void
pushResource(CVMJITCompilationContext* con, CVMRMResource* rp) 
{
    struct CVMJITStackElement* sp = ++(con->cgsp);
    statsPushResource();
#ifdef CVM_DEBUG_ASSERTS
    sp->tag = CVMJITStackTagResource;
#endif
    CVMassert(CVMRMgetRefCount(con, rp) > 0);
    sp->u.r = rp;
    validateStack((sp < con->cgstackLimit), Resource);
}

/* Purpose: Pops a resource. */
CVM_INLINE static CVMRMResource*
popResource(CVMJITCompilationContext* con)
{
    CVMRMResource* rp;
    struct CVMJITStackElement* sp = (con->cgsp)--;
    statsPopResource();
    CVMassert(sp >= con->cgstackInit);
    CVMassert(sp->tag == CVMJITStackTagResource);
    rp = sp->u.r;
    CVMassert(CVMRMgetRefCount(con, rp) > 0);
    return rp;
}

/* Purpose: Pushes a condition operand. */
CVM_INLINE static void
pushIConst32(CVMJITCompilationContext* con, CVMInt32 value)
{
    struct CVMJITStackElement* sp = ++(con->cgsp);
    statsPushResource();
#ifdef CVM_DEBUG_ASSERTS
    sp->tag = CVMJITStackTagConstant;
#endif
    sp->u.i = value;
    validateStack((sp < con->cgstackLimit), Resource);
}

/* Purpose: Pops a condition operand. */
CVM_INLINE static CVMInt32
popIConst32(CVMJITCompilationContext* con)
{
    struct CVMJITStackElement* sp = (con->cgsp)--;
    statsPopResource();
    CVMassert(sp >= con->cgstackInit);
    CVMassert(sp->tag == CVMJITStackTagConstant);
    return sp->u.i;
}

/* Purpose: Pushes an ALURhs operand. */
CVM_INLINE static void
pushALURhs(CVMJITCompilationContext* con, CVMCPUALURhs* ap)
{
    struct CVMJITStackElement* sp = ++(con->cgsp);
    statsPushResource();
#ifdef CVM_DEBUG_ASSERTS
    sp->tag = CVMJITStackTagALURhs;
#endif
    sp->u.aluRhs = ap;
    validateStack((sp < con->cgstackLimit), Resource);
}

/* Purpose: Pops an ALURhs operand. */
CVM_INLINE static CVMCPUALURhs*
popALURhs(CVMJITCompilationContext* con)
{
    struct CVMJITStackElement* sp = (con->cgsp)--;
    statsPopResource();
    CVMassert(sp >= con->cgstackInit);
    CVMassert(sp->tag == CVMJITStackTagALURhs);
    return sp->u.aluRhs;
}

/* Purpose: Pushes a MemSpec operand. */
CVM_INLINE static void
pushMemSpec(CVMJITCompilationContext *con, CVMCPUMemSpec *mp)
{
    struct CVMJITStackElement *sp = ++(con->cgsp);
    statsPushResource();
#ifdef CVM_DEBUG_ASSERTS
    sp->tag = CVMJITStackTagMemSpec;
#endif
    sp->u.memSpec = mp;
    validateStack((sp < con->cgstackLimit), Resource);
}

/* Purpose: Pops a MemSpec operand. */
CVM_INLINE static CVMCPUMemSpec *
popMemSpec(CVMJITCompilationContext *con)
{
    struct CVMJITStackElement* sp = (con->cgsp)--;
    statsPopResource();
    CVMassert(sp >= con->cgstackInit);
    CVMassert(sp->tag == CVMJITStackTagMemSpec);
    return sp->u.memSpec;
}

/* Push an Address */
CVM_INLINE static void
pushAddress(CVMJITCompilationContext *con, CVMAddr p)
{
    struct CVMJITStackElement *sp = ++(con->cgsp);
    statsPushResource();
#ifdef CVM_DEBUG_ASSERTS
    sp->tag = CVMJITStackTagAddress;
#endif
    sp->u.p = p;
    validateStack((sp < con->cgstackLimit), Resource);
}

/* Purpose: Pops a pointer to anything. */
CVM_INLINE static CVMAddr
popAddress(CVMJITCompilationContext *con)
{
    struct CVMJITStackElement* sp = (con->cgsp)--;
    statsPopResource();
    CVMassert(sp >= con->cgstackInit);
    CVMassert(sp->tag == CVMJITStackTagAddress);
    return sp->u.p;
}

/*
 * "Convenience" functions.
 */
/* Purpose: Pushes an ALURhs register operand. */
CVM_INLINE static void
pushALURhsResource(CVMJITCompilationContext* con, CVMRMResource* rp)
{
    pushALURhs(con, CVMCPUalurhsNewRegister(con, rp));
}

/* Purpose: Pushes an ALURhs constant operand. */
CVM_INLINE static void
pushALURhsConstant(CVMJITCompilationContext* con, CVMInt32 cv)
{
    pushALURhs(con, CVMCPUalurhsNewConstant(con, cv));
}

/* Purpose: Pushes a MemSpec immediate operand. */
CVM_INLINE static void
pushMemSpecImmediate(CVMJITCompilationContext *con, CVMInt32 value)
{
    pushMemSpec(con, CVMCPUmemspecNewImmediate(con, value));
}

/* Purpose: Pushes a MemSpec register operand. */
CVM_INLINE static void
pushMemSpecRegister(CVMJITCompilationContext *con, CVMBool offsetIsToBeAdded,
                    CVMRMResource *offsetReg)
{
    pushMemSpec(con,
                CVMCPUmemspecNewRegister(con, offsetIsToBeAdded, offsetReg));
}

#line 335 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Sets a value to a static field of an object. */
static void setStaticField(CVMJITCompilationContext *con,
			   CVMJITRMContext* rc,
                           CVMInt32 opcode,
                           CVMBool isVolatile)
{
    /* NOTE: For a non-LVM build, the staticFieldSpec is the
       staticFieldAddress.  For an LVM build, the staticFieldSpec is the
       fieldblock of the static field. */

    /* store over static-field-ref over static field address */
    CVMRMResource *src = popResource(con); /* Right Hand Side first. */
    CVMRMResource *staticField = popResource(con); /* lhs next.*/

    /* %comment l024 */

    if (isVolatile) {
        CVMCPUemitMemBarRelease(con);
    }

    CVMRMpinResource(CVMRM_INT_REGS(con), staticField,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(rc, src, rc->anySet, CVMRM_EMPTY_SET);

    CVMJITcsSetPutStaticFieldInstruction(con);

    CVMCPUemitMemoryReferenceImmediate(con, opcode,
        CVMRMgetRegisterNumber(src), CVMRMgetRegisterNumber(staticField), 0);
    if (isVolatile) {
        CVMCPUemitMemBar(con);
    }

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), staticField);
    CVMRMrelinquishResource(rc, src);
}

#line 429 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


#ifdef CVM_NEED_DO_INT_SHIFT_HELPER
/* NOTE: Maybe the shift emitter could be consolidated into the
   BinaryALU emitter?

          It seems like the right thing to do.  The only reason it is broken
          out now is because of how the ARM uses it, but it uses override rules
	  now. If it is consolidated in the BINARYALU emitter, then we can,
          consolidate some codegen rules and helpers. */
/* Purpose: Emits code for a shift operation with a const shiftAmount.  Also
            masks off the offset with 0x1f before shifting per VM spec. */
static void doIntShift(CVMJITCompilationContext *con, int shiftOp,
                       CVMJITIRNodePtr thisNode,
                       CVMRMregset target, CVMRMregset avoid)
{
    CVMRMResource *lhs = popResource(con);
    CVMRMResource* dest;
    int lhsRegNo = CVMRMgetRegisterNumberUnpinned(lhs);
    CVMInt32 shiftOffset =
        CVMJITirnodeGetConstant32(CVMJITirnodeGetRightSubtree(thisNode))->j.i;
    CVMJITRMContext* rc = CVMRM_INT_REGS(con);

    /* If the dest node has a regHint and the register number is the same as
     * the register the lhs is already loaded into, then reuse the lhs
     * register as the dest register. This is common when locals are
     * shifted.
     */
    if (thisNode->decorationType == CVMJIT_REGHINT_DECORATION &&
	lhsRegNo != -1 &&
	(1U << lhsRegNo) == target &&
	CVMRMgetRefCount(CVMRM_INT_REGS(con), lhs) == 1)
    {
	/* relinquish first so dirty resources are not spilled */
	CVMRMrelinquishResource(rc, lhs);
	lhs = NULL;
	dest = CVMRMgetResourceSpecific(rc, lhsRegNo, 1);
	CVMassert(lhsRegNo == CVMRMgetRegisterNumber(dest));
    } else {
	/* avoid dest target when pinning lhs */
	CVMRMpinResource(rc, lhs, ~target, target);
	dest = CVMRMgetResource(rc, target, avoid, 1);
	lhsRegNo = CVMRMgetRegisterNumber(lhs);
    }

    CVMCPUemitShiftByConstant(con, shiftOp, CVMRMgetRegisterNumber(dest),
			      lhsRegNo, shiftOffset & 0x1f);

    if (lhs != NULL) {
	CVMRMrelinquishResource(rc, lhs);
    }
    CVMRMoccupyAndUnpinResource(rc, dest, thisNode);
    pushResource(con, dest);
}
#endif

#ifdef CVM_NEED_DO_REG_SHIFT_HELPER
/* Purpose: Emits code for a shift operation with an unknown shiftAmount.
            The emitter is responsible for ensuring that the shiftAmount is
            masked with 0x1f before shifting per VM spec. */
static void doRegShift(CVMJITCompilationContext *con, int shiftOp,
                       CVMJITIRNodePtr thisNode,
                       CVMRMregset target, CVMRMregset avoid)
{
    CVMRMResource *rhs = popResource(con);
    CVMRMResource *lhs = popResource(con);
    CVMRMResource *dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					   target, avoid, 1);

    CVMRMpinResource(CVMRM_INT_REGS(con), lhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(CVMRM_INT_REGS(con), rhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);

    CVMCPUemitShiftByRegister(con, shiftOp, CVMRMgetRegisterNumber(dest),
        CVMRMgetRegisterNumber(lhs), CVMRMgetRegisterNumber(rhs));

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}
#endif

#line 561 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

static void
wordUnaryOp(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					   target, avoid, 1);

    CVMRMpinResource(CVMRM_INT_REGS(con), src,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUemitUnaryALU(con, opcode,
	CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(src),
	CVMJIT_NOSETCC);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}

static void
wordBinaryOp(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMCPUALURhs* rhs = popALURhs(con);
    CVMRMResource* lhs = popResource(con);
    CVMRMResource* dest;
    int lhsRegNo = CVMRMgetRegisterNumberUnpinned(lhs);
    CVMJITRMContext* rc = CVMRM_INT_REGS(con);

#ifdef IAI_IMPROVED_CONSTANT_ENCODING
    /*
     * If rhs is a constant that is not encodable, but the negative of the
     * constant is, then reverse the sign if doing an Add or Sub.
     */
    if (CVMCPUalurhsIsConstant(rhs) &&
	(opcode == CVMCPU_ADD_OPCODE || opcode == CVMCPU_SUB_OPCODE))
    {
	int op1;
	int op2;
	if (opcode == CVMCPU_ADD_OPCODE) {
	    op1 = CVMCPU_ADD_OPCODE;
	    op2 = CVMCPU_SUB_OPCODE;
	} else {
	    op1 = CVMCPU_SUB_OPCODE;
	    op2 = CVMCPU_ADD_OPCODE;
	}
	if (!CVMCPUalurhsIsEncodableAsImmediate(op1, rhs->constValue) &&
	    CVMCPUalurhsIsEncodableAsImmediate(op2, -rhs->constValue))
	{
	    opcode = op2;
	    rhs->constValue = -rhs->constValue;
	}
    }
#endif

    /* avoid dest target when pinning rhs */
    CVMCPUalurhsPinResource(rc, opcode, rhs, ~target, target);

    /* If the dest node has a regHint and the register number is the same as
     * the register the lhs is already loaded into, then reuse the lhs
     * register as the dest register. This is common when locals are
     * incremented.
     */
    if (thisNode->decorationType == CVMJIT_REGHINT_DECORATION &&
	lhsRegNo != -1 &&
	(1U << lhsRegNo) == target &&
	CVMRMgetRefCount(rc, lhs) == 1)
    {
	/* relinquish first so dirty resources are not spilled */
	CVMRMrelinquishResource(rc, lhs);
	lhs = NULL;
	dest = CVMRMgetResourceSpecific(rc, lhsRegNo, 1);
	CVMassert(lhsRegNo == CVMRMgetRegisterNumber(dest));
    } else {
	/* avoid dest target when pinning lhs */
	CVMRMpinResource(rc, lhs, ~target, target);
	dest = CVMRMgetResource(rc, target, avoid, 1);
	lhsRegNo = CVMRMgetRegisterNumber(lhs);
    }

    CVMCPUemitBinaryALU(con, opcode, CVMRMgetRegisterNumber(dest),
			lhsRegNo, CVMCPUalurhsGetToken(con, rhs),
			CVMJIT_NOSETCC);
    if (lhs != NULL) {
	CVMRMrelinquishResource(rc, lhs);
    }
    CVMCPUalurhsRelinquishResource(rc, rhs);
    CVMRMoccupyAndUnpinResource(rc, dest, thisNode);
    pushResource(con, dest);
}

#ifdef CVM_NEED_WORD_BINARY_OP_WITH_REG32_RHS
static void
wordBinaryOpWithReg32Rhs(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* operand = popResource(con);
    pushALURhsResource(con, operand);
    wordBinaryOp(con, opcode, thisNode, target, avoid);
}
#endif

/* Purpose: Emits a call to a Unary CCM helper. */
static void
unaryHelper(
    CVMJITCompilationContext *con,
    void *helperAddress,
    CVMJITIRNodePtr thisNode,
    CVMRMregset outgoingSpillRegSet,
    int resultSize)
{
    CVMRMResource *src = popResource(con);
    CVMRMResource *dest;

    /* Pin the input to CVMCPU_ARG1_REG because the helper expects it there: */
    src = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), src, CVMCPU_ARG1_REG);

    /* Spill the outgoing registers if necessary: */
    CVMRMminorSpill(con, outgoingSpillRegSet);

#ifdef CVMCPU_HAS_64BIT_REGISTERS
    {
        int argSize = CVMRMgetSize(src);
        if (argSize == 2) {
            /* argSize = 2 means the argument is doubleword */
            CVMCPUemitMoveTo64BitRegister(con, CVMCPU_ARG1_REG, 
                                          CVMCPU_ARG1_REG);
        }
    }
#endif

    /* Emit the call to the helper to compute the result: */
    CVMCPUemitAbsoluteCall(con, helperAddress, 
			   CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
    CVMJITcsBeginBlock(con);

#ifdef CVMCPU_HAS_64BIT_REGISTERS
    {
        if (resultSize == 2) {
            /* resultSize = 2 means the result is doubleword. */
            CVMCPUemitMoveFrom64BitRegister(con, CVMCPU_RESULT1_REG,
                                            CVMCPU_RESULT1_REG);
        }
    }
#endif

    /* Release resources and publish the result: */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con), CVMCPU_RESULT1_REG,
				    resultSize);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}

/* Purpose: Emits a call to a Binary CCM helper. */
static void
binaryHelper(
    CVMJITCompilationContext *con,
    void* helperAddress,
    CVMJITIRNodePtr thisNode,
    CVMBool checkZero,
    int argSize, /* total size in words of all arguments */
    int resultSize)
{
    const char *symbolName;
    CVMCodegenComment *comment;
    CVMRMResource* rhs = popResource(con);
    CVMRMResource* lhs = popResource(con);
    CVMRMResource* dest;
    int lhsReg, rhsReg;
    CVMRMregset outSet;

    CVMJITpopSymbolName(con, symbolName);
    CVMJITpopCodegenComment(con, comment);
    lhsReg = CVMCPU_ARG1_REG;
    if (argSize == 2) {
	rhsReg = CVMCPU_ARG2_REG;
	outSet = ARG1|ARG2;
    } else if (argSize == 3){
	rhsReg = CVMCPU_ARG3_REG;
	outSet = ARG1|ARG2|ARG3;
    } else {
	CVMassert(argSize == 4);
	rhsReg = CVMCPU_ARG3_REG;
	outSet = ARG1|ARG2|ARG3|ARG4;
    }
    /* Pin the input to the first two arguments because the helper expects it
       there. Note that if the rhs is already in a register, then pin it first
       so it is setup using a move instruction. Otherwise you run the risk of
       it getting clobbered when pinning the lhs, and then having to reload
       if from memory (after possibly also having spilled it). */
    if (CVMRMgetRegisterNumberUnpinned(rhs) != -1) {
	rhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), rhs, rhsReg);
	lhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), lhs, lhsReg);
    } else {
	rhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), rhs, rhsReg);
	lhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), lhs, lhsReg);
    }

    if (checkZero) {
	if (CVMRMdirtyJavaLocalsCount(con) == 0) {
	    if (resultSize == 2) {
		CVMRMResource *scratch =
		    CVMRMgetResource(CVMRM_INT_REGS(con),
				     CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
		int destReg = CVMRMgetRegisterNumber(scratch);
                CVMCPUemitBinaryALURegister(con, CVMCPU_OR_OPCODE,
					    destReg, rhsReg, rhsReg+1,
					    CVMJIT_SETCC);
#ifndef CVMCPU_HAS_ALU_SETCC
		CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
				  destReg, CVMCPUALURhsTokenConstZero);
					  
#endif
		CVMRMrelinquishResource(CVMRM_INT_REGS(con), scratch);
	    } else {
                CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
				  rhsReg, CVMCPUALURhsTokenConstZero);
	    }
	    CVMJITaddCodegenComment((
                con, trapCheckComments[CVMJITIR_DIVIDE_BY_ZERO]));
            CVMCPUemitAbsoluteCallConditional(con, 
		(void*)CVMCCMruntimeThrowDivideByZeroGlue,
                CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK, CVMCPU_COND_EQ);
	    CVMJITcsBeginBlock(con);
	} else {
	    /* Dirty locals are not supported yet */
	    CVMassert(CVM_FALSE);
	}
    }

    /* Spill the outgoing registers if necessary: */
    CVMRMminorSpill(con, outSet);

#ifdef CVMCPU_HAS_64BIT_REGISTERS
    {
        if (argSize >= 3) {
            /* argSize >= 3 means the lhs argument is doubleword */
            CVMCPUemitMoveTo64BitRegister(con, CVMCPU_ARG1_REG, 
                                          CVMCPU_ARG1_REG);
            if (argSize == 4) {
                /* argSize = 4 means both lhs and rhs arguments 
                   are doubleword */
                CVMCPUemitMoveTo64BitRegister(con, CVMCPU_ARG2_REG,
                                              CVMCPU_ARG3_REG);
            } else {
                /* argSize is 3, and rhs is a 32-bit type. We need to move
                 * CVMCPU_ARG3_REG to CVMCPU_ARG2_REG to set up the second
                 * argument.
                 */
                CVMassert(argSize == 3);
                CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE,
                                       CVMCPU_ARG2_REG, CVMCPU_ARG3_REG,
                                       CVMJIT_NOSETCC);
            }
        }
    }
#endif

    /* Emit the call to the helper to compute the result: */
    CVMJITpushCodegenComment(con, comment);
    CVMJITpushSymbolName(con, symbolName);
    CVMCPUemitAbsoluteCall(con, helperAddress,
			   CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
    CVMJITcsBeginBlock(con);

#ifdef CVMCPU_HAS_64BIT_REGISTERS
    {
        if (resultSize == 2) {
            /* resultSize = 2 means the result is a doubleword type */
            CVMCPUemitMoveFrom64BitRegister(con, CVMCPU_RESULT1_REG,
                                            CVMCPU_RESULT1_REG);
        }
    }
#endif

    /* Release resources and publish the result: */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con), CVMCPU_RESULT1_REG,
				    resultSize);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}

/* Purpose: Emits a call to a Binary CCM helper which return a word result. */
static void
wordBinaryHelper(
    CVMJITCompilationContext *con,
    void* helperAddress,
    CVMJITIRNodePtr thisNode,
    CVMBool checkZero )
{
    binaryHelper(con, helperAddress, thisNode, checkZero, 2, 1);
}

/* Purpose: Emits a call to a Binary CCM helper which return a dword result. */
static void
longBinaryHelper(
    CVMJITCompilationContext *con,
    void *helperAddress,
    CVMJITIRNodePtr thisNode,
    CVMBool checkZero )
{
    binaryHelper(con, helperAddress, thisNode, checkZero, 4, 2);
}

/* Purpose: Emits a call to a Binary CCM helper which return a dword resul,
 * but whose 2nd argument is 32-bit, not 64-bit. */
static void
longBinaryHelper2(
    CVMJITCompilationContext *con,
    void *helperAddress,
    CVMJITIRNodePtr thisNode,
    CVMBool checkZero )
{
    binaryHelper(con, helperAddress, thisNode, checkZero, 3, 2);
}

/* Purpose: Emits a call to a Binary CCM helper which return a dword result. */
static void
longBinary2WordHelper(
    CVMJITCompilationContext *con,
    void *helperAddress,
    CVMJITIRNodePtr thisNode,
    CVMBool checkZero)
{
    binaryHelper(con, helperAddress, thisNode, checkZero, 4, 1);
}

/*
 * For nodes of the form:
 * NODE .... reg<width>
 * 
 * Pass the reg<width> value onto NODE
 */
static void
passLastEvaluated(CVMJITCompilationContext* con, CVMJITRMContext* rc, 
		  CVMJITIRNodePtr thisNode)
{
    /* Associate the last evaluated sub-node on to its parent */
    CVMRMResource* arg = popResource(con);
    CVMRMoccupyAndUnpinResource(rc, arg, thisNode);
    pushResource(con, arg);
}

/*
 * Inlining start
 */
static void
beginInlining(CVMJITCompilationContext* con, CVMJITIRNodePtr thisNode)
{
    CVMJITUnaryOp* unop = CVMJITirnodeGetUnaryOp(thisNode);
    CVMJITConstantAddr* constAddr;
    CVMJITMethodContext* mc;
    CVMUint16 pcStart = CVMJITcbufGetLogicalPC(con);
    CVMJITInliningInfoStackEntry* newEntry;
    
    CVMassert(CVMJITirnodeIsConstMC(unop->operand));
    constAddr = CVMJITirnodeGetConstantAddr(unop->operand);
    mc = constAddr->mc;
    /* Start a new inlining entry */
    CVMassert(con->inliningDepth < con->maxInliningDepth);
    newEntry = &con->inliningStack[con->inliningDepth++];
    /* Record the starting point and mb of this inlining record */
    /* The end point will be recorded by the corresponding END_INLINING node */
    newEntry->pcOffset1 = pcStart;
    newEntry->mc = mc;
    CVMJITprintCodegenComment(("Begin inlining of %C.%M (start pc=%d):",
			       CVMmbClassBlock(mc->mb), mc->mb, pcStart));
}

/*
 * We have finished inlined code. Record this PC.
 */
static void
endInlining(CVMJITCompilationContext* con, CVMJITIRNodePtr thisNode)
{
    CVMJITInliningInfoStackEntry* topEntry;
    CVMCompiledInliningInfoEntry* recordedEntry;
    CVMUint16 pcEnd = CVMJITcbufGetLogicalPC(con);
    CVMJITMethodContext *mc;
    CVMUint16 count = CVMJITirnodeGetNull(thisNode)->data;

    CVMassert(count > 0);
    while (count-- > 0) {
	CVMassert(con->inliningDepth > 0);
	topEntry = &con->inliningStack[--con->inliningDepth];
	mc = topEntry->mc;
	CVMassert(topEntry->pcOffset1 > 0);
	CVMassert(topEntry->mc != NULL);

	CVMassert(con->inliningInfoIdx < con->numInliningInfoEntries);
	/* Now commit another "permanent" entry */
	recordedEntry = &con->inliningInfo->entries[con->inliningInfoIdx++];
	recordedEntry->pcOffset1 = topEntry->pcOffset1;
	recordedEntry->mb = mc->mb;
	recordedEntry->pcOffset2 = pcEnd;
	recordedEntry->invokePC = mc->invokePC;
	recordedEntry->flags = 0;

	{
	    recordedEntry->firstLocal = mc->firstLocal;
	    if (CVMmbIs(mc->mb, SYNCHRONIZED)) {
		CVMassert(!CVMmbIs(mc->mb, STATIC));
		recordedEntry->syncObject = mc->syncObject;
	    } else {
		recordedEntry->syncObject = 0;
	    }
	}

	CVMJITprintCodegenComment(("End inlining of %C.%M (end pc=%d):",
				   CVMmbClassBlock(recordedEntry->mb),
				   recordedEntry->mb,
				   pcEnd));
    }
}

#ifdef CVM_DEBUG
static void 
printInliningInfo(CVMJITIRNode* thisNode, const char* what)
{
    CVMJITUnaryOp* unop = CVMJITirnodeGetUnaryOp(thisNode);
    CVMJITConstantAddr* constAddr;
    CVMMethodBlock* mb;
    
    if (CVMJITirnodeIsConstMB(unop->operand)) {
	constAddr = CVMJITirnodeGetConstantAddr(unop->operand);       
	mb = constAddr->mb;
	CVMJITprintCodegenComment(("%s of %C.%M:", what,
				   CVMmbClassBlock(mb), mb));
    } else {
	CVMJITprintCodegenComment(("%s (node id %d):", what,
				   CVMJITirnodeGetID(unop->operand)));
    }
}
#else
#define printInliningInfo(thisNode, what)
#endif
#line 1024 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

#define SEQUENCE_R_INHERITANCE(thisNode, rc) { \
    SET_ATTRIBUTE_TYPE(0, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
    goal_top[0].attributes[0].u.rs.target = CVMRM_GET_ANY_SET(rc);   \
    goal_top[0].attributes[0].u.rs.avoid  = CVMCPU_AVOID_NONE;       \
    goal_top[0].attributes[1] = goal_top[-1].curr_attribute[0];      \
}
#line 1057 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

#define SEQUENCE_L_INHERITANCE(thisNode, rc) { \
    SET_ATTRIBUTE_TYPE(1, CVMJIT_EXPRESSION_ATTRIBUTE_TARGET_AVOID); \
    goal_top[0].attributes[0] = goal_top[-1].curr_attribute[0];      \
    goal_top[0].attributes[1].u.rs.target = CVMRM_GET_ANY_SET(rc);   \
    goal_top[0].attributes[1].u.rs.avoid  = CVMCPU_AVOID_NONE;       \
}
#line 1115 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Gets a value from a static field of an object. */
static void getStaticField(CVMJITCompilationContext *con,
			   CVMJITRMContext* rc,
                           CVMJITIRNodePtr thisNode,
                           CVMRMregset target, CVMRMregset avoid,
                           CVMInt32 opcode, int fieldSize,
                           CVMBool isVolatile)
{
    /* NOTE: For a non-LVM build, the staticFieldSpec is the
       staticFieldAddress.  For an LVM build, the staticFieldSpec is the
       fieldblock of the static field. */

    /* fetch over static-field-ref over fb constant */
    CVMRMResource *staticField = popResource(con);
    CVMRMResource *dest = CVMRMgetResource(rc, target, avoid, fieldSize);

    /* %comment l026 */ 

    CVMRMpinResource(CVMRM_INT_REGS(con), staticField,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMJITcsSetGetStaticFieldInstruction(con);
    CVMCPUemitMemoryReferenceImmediate(con, opcode,
        CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(staticField), 0);

    if (isVolatile) {
        CVMCPUemitMemBarAcquire(con);
    }

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), staticField);
    CVMRMoccupyAndUnpinResource(rc, dest, thisNode);
    pushResource(con, dest);
}

#line 1418 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Converts a constant into reg32 value. */
static void
const2Reg32(CVMJITCompilationContext *con, CVMJITRMContext *rc,
	    CVMJITIRNodePtr thisNode, int target, int avoid)
{
    CVMInt32 constant;
    CVMRMResource *dest;
    if (CVMJITirnodeIsConstant32Node(thisNode)) {
        constant = CVMJITirnodeGetConstant32(thisNode)->j.i;
    } else {
        constant = CVMJITirnodeGetConstantAddr(thisNode)->vAddr;
    }
    dest = CVMRMbindResourceForConstant32(rc, constant);
    /* only pin eagerly if constant cannot be treated as an immediate */
    if (!CVMCPUalurhsIsEncodableAsImmediate(CVMCPU_ADD_OPCODE, constant) &&
	!CVMCPUalurhsIsEncodableAsImmediate(CVMCPU_SUB_OPCODE, constant))
    {
	CVMRMpinResourceEagerlyIfDesireable(rc, dest, target, avoid);
    }
    /* Need this in case this constant is a CSE */
    CVMRMoccupyAndUnpinResource(rc, dest, thisNode);
    pushResource(con, dest);
}

#line 1488 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

#define IDENT_SYNTHESIS(con, thisNode)			\
{							\
    if (!CVMJIT_JCS_DID_PHASE(IRGetState(thisNode),	\
	CVMJIT_JCS_STATE_SYNTHED))			\
    {							\
	/* same as DEFAULT_SYNTHESIS_CHAIN  */		\
	DEFAULT_SYNTHESIS1(con, thisNode);		\
    } else {						\
	/* Leaf node */					\
    }							\
}

#define IDENT_INHERITANCE(con, thisNode)		\
{							\
    CVMassert(!CVMJIT_DID_SEMANTIC_ACTION(thisNode));	\
    DEFAULT_INHERITANCE_CHAIN(con, thisNode);		\
}

#line 1588 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

#ifdef CVMJIT_INTRINSICS

/* Get absolute value of srcReg and set condition codes:
     adds    rDest, rSrc, #0
     neglt   rDest, rSrc
*/
static void emitAbsolute(CVMJITCompilationContext* con,
			 int destReg, int srcReg)
{
    CVMCPUemitBinaryALU(con, CVMCPU_ADD_OPCODE,
        destReg, srcReg, CVMCPUALURhsTokenConstZero, CVMJIT_SETCC);
#ifndef CVMCPU_HAS_ALU_SETCC
    CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_LT,
        srcReg, CVMCPUALURhsTokenConstZero);
#endif
    CVMCPUemitUnaryALUConditional(con, CVMCPU_NEG_OPCODE,
        destReg, srcReg, CVMJIT_NOSETCC, CVMCPU_COND_LT);
}

CVMJITRegsRequiredType
CVMJITRISCintrinsicDefaultGetRequired(CVMJITCompilationContext *con,
                                      CVMJITIRNode *intrinsicNode,
                                      CVMJITRegsRequiredType argsRequiredSet)
{
    return argsRequiredSet;
}

CVMRMregset
CVMJITRISCintrinsicDefaultGetArgTarget(CVMJITCompilationContext *con,
                                       int typeTag, CVMUint16 argNumber,
                                       CVMUint16 argWordIndex)
{
    return CVMRM_ANY_SET;
}

#ifdef CVMJIT_SIMPLE_SYNC_METHODS
CVMJITRegsRequiredType
CVMJITRISCintrinsicSimpleLockReleaseGetRequired(
    CVMJITCompilationContext *con,
    CVMJITIRNode *intrinsicNode,
    CVMJITRegsRequiredType argsRequiredSet)
{
#if CVM_FASTLOCK_TYPE == CVM_FASTLOCK_ATOMICOPS
    /* During the release, we may call CVMCCMruntimeSimpleSyncUnlock,
     * which requires ARG1 and ARG2
     */
    return argsRequiredSet | ARG1 | ARG2 | CVMCPU_AVOID_C_CALL;
#else
    return argsRequiredSet;
#endif
}


CVMRMregset
CVMJITRISCintrinsicSimpleLockReleaseGetArgTarget(
    CVMJITCompilationContext *con,
    int typeTag, CVMUint16 argNumber,
    CVMUint16 argWordIndex)
{
    CVMassert(argNumber == 0);
#if CVM_FASTLOCK_TYPE == CVM_FASTLOCK_ATOMICOPS
    /* During the release, we may call CVMCCMruntimeSimpleSyncUnlock,
     * which requires "this" to be in ARG2
     */
    return ARG2;
#else
    return CVMRM_ANY_SET;
#endif
}
#endif /* CVMJIT_SIMPLE_SYNC_METHODS */

/* intrinsic emitter for Thread.currentThread(). */
static void
java_lang_Thread_currentThread_EmitOperator(CVMJITCompilationContext *con,
                                            CVMJITIRNode *intrinsicNode)
{
    struct CVMJITCompileExpression_rule_computation_state *goal_top =
	(struct CVMJITCompileExpression_rule_computation_state *)
        (con->goal_top);
    CVMRMResource* dest =
	CVMRMgetResource(CVMRM_INT_REGS(con), GET_REGISTER_GOALS, 1);
    int destReg = CVMRMgetRegisterNumber(dest);
    int eeReg;

    /*
        ldr eeReg, [sp, #OFFSET_CVMCCExecEnv_ee]           @ Get ee.
        ldr rDest, [eeReg, #OFFSET_CVMExecEnv_threadICell] @ Get threadICell.
        ldr rDest, [rDest]                                 @ Get thread obj.
    */

#ifdef CVMCPU_EE_REG
    eeReg = CVMCPU_EE_REG;
#else
    eeReg = destReg;
    /* Get the ee from the ccee: */
    CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
    CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
        eeReg, offsetof(CVMCCExecEnv, eeX));
#endif
    /* Get the thread icell from the ee: */
    CVMJITaddCodegenComment((con, "destReg = ee->threadICell"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
        destReg, eeReg, offsetof(CVMExecEnv, threadICell));

    /* Get the thread object from the thread icell: */
    CVMJITaddCodegenComment((con, "destReg = *ee->threadICell"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
        destReg, destReg, 0);

    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, intrinsicNode);
    pushResource(con, dest);
}

static void
iabsEmitOperator(CVMJITCompilationContext *con, CVMJITIRNode *intrinsicNode)
{
    struct CVMJITCompileExpression_rule_computation_state *goal_top =
	(struct CVMJITCompileExpression_rule_computation_state *)
        (con->goal_top);
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest =
	CVMRMgetResource(CVMRM_INT_REGS(con), GET_REGISTER_GOALS, 1);
    CVMRMpinResource(CVMRM_INT_REGS(con), src,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    emitAbsolute(con, CVMRMgetRegisterNumber(dest),
                 CVMRMgetRegisterNumber(src));
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, intrinsicNode);
    pushResource(con, dest);
}

#ifdef CVMJIT_SIMPLE_SYNC_METHODS
#if CVM_FASTLOCK_TYPE == CVM_FASTLOCK_MICROLOCK && \
    CVM_MICROLOCK_TYPE == CVM_MICROLOCK_SWAP_SPINLOCK

/*
 * Intrinsic emitter for spinlock microlock version of CVM.simpleLockGrab().
 *
 * Grabs CVMglobals.objGlobalMicroLock using atomic swap. If it fails,
 * returns FALSE. If successful, checks if the object is already locked.
 * If locked, releases CVMglobals.objGlobalMicroLock and returns FALSE.
 * Otherwise returns TRUE.
 */

static void
simpleLockGrabEmitter(
    CVMJITCompilationContext * con,
    CVMJITIRNode *intrinsicNode)
{
    CVMRMResource* obj = popResource(con);
    CVMRMResource* objHdr;
    CVMRMResource* dest;
    CVMRMResource* microLock;
    int objRegID, objHdrRegID, destRegID, microLockRegID;
    int fixupPC1, fixupPC2; /* To patch the conditional branches */

    struct CVMJITCompileExpression_rule_computation_state *goal_top =
	(struct CVMJITCompileExpression_rule_computation_state *)
        (con->goal_top);

    dest = CVMRMgetResource(CVMRM_INT_REGS(con), GET_REGISTER_GOALS, 1);
    objHdr = CVMRMgetResource(CVMRM_INT_REGS(con),
			      CVMRM_ANY_SET, CVMRM_SAFE_SET, 1);
    CVMRMpinResource(CVMRM_INT_REGS(con), obj, CVMRM_ANY_SET, CVMRM_SAFE_SET);

    objRegID    = CVMRMgetRegisterNumber(obj);
    objHdrRegID = CVMRMgetRegisterNumber(objHdr);
    destRegID   = CVMRMgetRegisterNumber(dest);

    /* load microlock address into microLockRegID */
    CVMJITsetSymbolName((con, "&CVMglobals.objGlobalMicroLock"));
    microLock = CVMRMgetResourceForConstant32(
        CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	(CVMUint32)&CVMglobals.objGlobalMicroLock);
    microLockRegID = CVMRMgetRegisterNumber(microLock);
    /* preload the address to help caching */
    CVMJITaddCodegenComment((con, "tmp = CVMglobals.objGlobalMicroLock"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
				       destRegID, microLockRegID, 0);
    /* Get microlock LOCKED flag */
    CVMJITaddCodegenComment((con, "CVM_MICROLOCK_LOCKED"));
    CVMCPUemitLoadConstant(con, destRegID, CVM_MICROLOCK_LOCKED);
    /* atomic swap LOCKED into the microlock */
    CVMJITaddCodegenComment((con,
	"swp(CVMglobals.objGlobalMicroLock, CVM_MICROLOCK_LOCKED)"));
    CVMCPUemitAtomicSwap(con, destRegID, microLockRegID);
    CVMCPUemitMemBarAcquire(con);

    /* check if microlock is already locked */
    CVMJITaddCodegenComment((con, "check if microlock is locked"));
    CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
			      destRegID, CVM_MICROLOCK_LOCKED);

    /* branch if microlock already locked */
    CVMJITaddCodegenComment((con, "br failed if microlock is locked"));
    CVMCPUemitBranch(con, 0, CVMCPU_COND_EQ);
#ifdef CVMCPU_HAS_DELAY_SLOT
    fixupPC1 = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
    fixupPC1 = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif

#ifdef IAI_CODE_SCHEDULER_SCORE_BOARD
    fixupPC1 = CVMJITcbufGetLogicalInstructionPC(con);
#endif
    /* load the object header */
    CVMJITaddCodegenComment((con, "get obj.hdr.various32"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
				       objHdrRegID, objRegID,
				       CVMoffsetof(CVMObjectHeader,various32));
    /* assume not locked and set intrinsic result */
    CVMJITaddCodegenComment((con, "assume not locked: result = true"));
    CVMCPUemitLoadConstant(con, destRegID, CVM_TRUE);
    /* get sync bits from object header */
    CVMJITaddCodegenComment((con, "get obj sync bits"));
    CVMCPUemitBinaryALUConstant(con, CVMCPU_AND_OPCODE,
				objHdrRegID, objHdrRegID,
				CVM_SYNC_MASK, CVMJIT_NOSETCC);
    /* check if object is unlocked */
    CVMJITaddCodegenComment((con, "check if obj unlocked"));
    CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
			      objHdrRegID, CVM_LOCKSTATE_UNLOCKED);
    /* branch if object not locked */
    CVMJITaddCodegenComment((con, "br done if object is not locked"));
    CVMCPUemitBranch(con, 0, CVMCPU_COND_EQ);
#ifdef CVMCPU_HAS_DELAY_SLOT
    fixupPC2 = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
    fixupPC2 = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif

#ifdef IAI_CODE_SCHEDULER_SCORE_BOARD
    fixupPC2 = CVMJITcbufGetLogicalInstructionPC(con);
#endif
    /* Object is locked. Release microlock */
    CVMJITaddCodegenComment((con, "CVM_MICROLOCK_UNLOCKED"));
    CVMCPUemitLoadConstant(con, destRegID, CVM_MICROLOCK_UNLOCKED);
    CVMJITaddCodegenComment((con,
	"CVMglobals.objGlobalMicroLock = CVM_MICROLOCK_UNLOCKED"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
				       destRegID, microLockRegID, 0);
    /* Failure target. Make instrinsic return false. */
    CVMtraceJITCodegen(("\t\tfailed:\n"));
    CVMJITfixupAddress(con, fixupPC1, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);
    CVMCPUemitLoadConstant(con, destRegID, CVM_FALSE);
    /* "done" target. No change is made instrinc result. */
    CVMtraceJITCodegen(("\t\tdone:\n"));
    CVMJITfixupAddress(con, fixupPC2, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);

#ifdef CVM_DEBUG
    /* For Debug builds, we do the following:
     *
     * 1. Set the ee's microlock depth to 0 or 1 based on success.
     * 2. Set CVMglobals.jit.currentSimpleSyncMB to the Simple Sync
     *    mb we are currently generating code for.
     *
     * (1) is done so C code will assert if the microlock gets out
     * of balance. Note we don't assert in here in the generated code
     * because it is too ugly.
     *
     * (2) is done in case there is ever a problem, we can find out
     * the last Simple Sync method called by looking in CVMglobals.
     * It is disabled with #if 0 by default.
     */
    {
	/* 1. Set the ee's microlock depth to 0 or 1 based on success. */
	int eeReg;
#ifndef CVMCPU_EE_REG
	CVMRMResource *eeRes =
	    CVMRMgetResource(CVMRM_INT_REGS(con),
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
	eeReg = CVMRMgetRegisterNumber(eeRes);
	/* Get the ee: */
	CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
	CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE, eeReg,
					 CVMoffsetof(CVMCCExecEnv, eeX));
#else
	eeReg = CVMCPU_EE_REG;
#endif
	/* Set the ee's microlock depth. We just set it to the result
	 * of this intrinsic, which will be 0 or 1. */
	CVMJITaddCodegenComment((con, "ee->microLock = <result>"));
	CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
            destRegID, eeReg, offsetof(CVMExecEnv, microLock));
#ifndef CVMCPU_EE_REG
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), eeRes);
#endif
    }
    /* 
     * The following debugging code is disabled for now, but can be enabled
     * if Simple Sync methods are suspected of causing problems, like a
     * deadlock or assert.
     */
#if 0
    {
	/* Store the mb of the currently executing Simple Sync method into
	 * CVMglobals.jit.currentSimpleSyncMB. */
	CVMRMResource* currentSimpleSyncMBRes;
	CVMRMResource* simpleSyncMBRes;
	CVMJITMethodContext* mc = con->inliningStack[con->inliningDepth-1].mc;
	/* load CVMglobals.jit.currentSimpleSyncMB address into a register */
	CVMJITsetSymbolName((con, "&CVMglobals.jit.currentSimpleSyncMB"));
	currentSimpleSyncMBRes = CVMRMgetResourceForConstant32(
            CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	    (CVMUint32)&CVMglobals.jit.currentSimpleSyncMB);
	/* load the Simple Sync mb address into a register */
        CVMJITsetSymbolName((con, "mb %C.%M", mc->cb, mc->mb));
	simpleSyncMBRes = CVMRMgetResourceForConstant32(
            CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	    (CVMUint32)mc->mb);
	/* Store the Simple Sync mb into CVMglobals.jit.currentSimpleSyncMB. */
	CVMJITaddCodegenComment((con,
				 "CVMglobals.jit.currentSimpleSyncMB = %C.%M",
				 mc->cb, mc->mb));
	CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
	    CVMRMgetRegisterNumber(simpleSyncMBRes),
	    CVMRMgetRegisterNumber(currentSimpleSyncMBRes), 0);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), currentSimpleSyncMBRes);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), simpleSyncMBRes);
    }
    {
	/* Store the mb of the currently executing method into
	 * CVMglobals.jit.currentMB. */
	CVMRMResource* currentMBRes;
	CVMRMResource* mbRes;
	/* load CVMglobals.jit.currentMB address into a register */
	CVMJITsetSymbolName((con, "&CVMglobals.jit.currentMB"));
	currentMBRes = CVMRMgetResourceForConstant32(
            CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	    (CVMUint32)&CVMglobals.jit.currentMB);
	/* load the current mb address into a register */
        CVMJITsetSymbolName((con, "mb %C.%M",
			     CVMmbClassBlock(con->mb), con->mb));
	mbRes = CVMRMgetResourceForConstant32(
            CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	    (CVMUint32)con->mb);
	/* Store the Simple Sync mb into CVMglobals.jit.currentMB. */
	CVMJITaddCodegenComment((con,
				 "CVMglobals.jit.currentMB = %C.%M",
				 CVMmbClassBlock(con->mb), con->mb));
	CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
	    CVMRMgetRegisterNumber(mbRes),
	    CVMRMgetRegisterNumber(currentMBRes), 0);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), currentMBRes);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), mbRes);
    }
#endif /* 0 */
#endif /* CVM_DEBUG */

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), obj);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), objHdr);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), microLock);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, intrinsicNode);
    pushResource(con, dest);
}

/*
 * Intrinsic emitter for microlock version of CVM.simpleLockRelease().
 *
 * Stores CVM_MICROLOCK_UNLOCKED into CVMglobals.objGlobalMicroLock.
 */
static void
simpleLockReleaseEmitter(
    CVMJITCompilationContext * con,
    CVMJITIRNode *intrinsicNode)
{
    CVMRMResource* tmp;
    CVMRMResource* microLock;
    int tmpRegID, microLockRegID;

    popResource(con); /* pop the "this" argument, which we don't use */
    tmp = CVMRMgetResource(CVMRM_INT_REGS(con),
			   CVMRM_ANY_SET, CVMRM_SAFE_SET, 1);

    tmpRegID = CVMRMgetRegisterNumber(tmp);

    /* get CVM_MICROLOCK_UNLOCKED value */
    CVMJITaddCodegenComment((con, "CVM_MICROLOCK_UNLOCKED"));
    CVMCPUemitLoadConstant(con, tmpRegID, CVM_MICROLOCK_UNLOCKED);

#ifdef CVM_DEBUG
    /* Set the ee's microlock depth to 0, which is the same as
     * CVM_MICROLOCK_UNLOCKED. */
    {
	int eeReg;
#ifndef CVMCPU_EE_REG
	CVMRMResource *eeRes =
	    CVMRMgetResource(CVMRM_INT_REGS(con),
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
	eeReg = CVMRMgetRegisterNumber(eeRes);
	/* Get the ee: */
	CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
	CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE, eeReg,
					 CVMoffsetof(CVMCCExecEnv, eeX));
#else
	eeReg = CVMCPU_EE_REG;
#endif
	CVMassert(CVM_MICROLOCK_UNLOCKED == 0);
	CVMJITaddCodegenComment((con, "ee->microLock = 0"));
	CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
					   tmpRegID, eeReg,
					   offsetof(CVMExecEnv, microLock));
#ifndef CVMCPU_EE_REG
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), eeRes);
#endif
    }
#endif /* CVM_DEBUG */

    /* load microlock address into microLockRegID */
    CVMJITsetSymbolName((con, "&CVMglobals.objGlobalMicroLock"));
    microLock = CVMRMgetResourceForConstant32(CVMRM_INT_REGS(con),
				  CVMRM_ANY_SET, CVMRM_SAFE_SET,
				  (CVMUint32)&CVMglobals.objGlobalMicroLock);
    microLockRegID = CVMRMgetRegisterNumber(microLock);
    CVMCPUemitMemBarRelease(con);
    /* release the microlock */
    CVMJITaddCodegenComment((con,
	"CVMglobals.objGlobalMicroLock = CVM_MICROLOCK_UNLOCKED"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
				       tmpRegID, microLockRegID, 0);
    CVMCPUemitMemBar(con);

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), tmp);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), microLock);
}

#elif CVM_FASTLOCK_TYPE == CVM_FASTLOCK_ATOMICOPS

/*
 * Intrinsic emitter for fastlock CAS version of CVM.simpleLockGrab().
 *
 * Attempts to lock the object using the reserved 
 * ee->simpleSyncReservedOwnedMonitor. If the object is already locked,
 * returns FALSE. Otherwise returns TRUE.
 */

static void
simpleLockGrabEmitter(
    CVMJITCompilationContext * con,
    CVMJITIRNode *intrinsicNode)
{
    CVMRMResource* obj = popResource(con);
    CVMRMResource* objHdr;
    CVMRMResource* lockRec;
    CVMRMResource* dest;
    int objRegID, objHdrRegID, lockRecRegID, destRegID, eeRegID;
    int fixupPC1, fixupPC2; /* To patch the conditional branches */
#ifndef CVMCPU_EE_REG
    CVMRMResource *eeRes;
#endif

    struct CVMJITCompileExpression_rule_computation_state *goal_top =
	(struct CVMJITCompileExpression_rule_computation_state *)
        (con->goal_top);

    if (CVMRMisConstant(obj) && CVMRMgetConstant(obj) == 0) {
	/* We know the object is a null, so an NPE would be thrown before
	 * code generated here is executed. Therefore we don't need to
	 * generate anything. We'll stuff the obj resource into the
	 * intrinisic node just to keep JCS happy.
	 */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), obj, intrinsicNode);
	pushResource(con, obj);
	return;
    }

#ifndef CVMCPU_EE_REG
    eeRes = CVMRMgetResource(
        CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
#endif

    /* get all our resources ready */
    dest = CVMRMgetResource(CVMRM_INT_REGS(con), GET_REGISTER_GOALS, 1);
    CVMRMpinResource(CVMRM_INT_REGS(con), obj, CVMRM_ANY_SET, CVMRM_SAFE_SET);
    objHdr = CVMRMgetResource(CVMRM_INT_REGS(con),
			      CVMRM_ANY_SET, CVMRM_SAFE_SET, 1);
    lockRec = CVMRMgetResource(CVMRM_INT_REGS(con),
			      CVMRM_ANY_SET, CVMRM_SAFE_SET, 1);

    /* ... and get all the register numbers */
    objRegID    = CVMRMgetRegisterNumber(obj);
    objHdrRegID = CVMRMgetRegisterNumber(objHdr);
    lockRecRegID = CVMRMgetRegisterNumber(lockRec);
    destRegID   = CVMRMgetRegisterNumber(dest);

#ifdef CVM_DEBUG
    CVMJITprintCodegenComment(("DEBUG-ONLY CODE"));
    {
	/* Store the mb of the currently executing Simple Sync method into
	 * ee->currentSimpleSyncMB. */
	CVMRMResource* simpleSyncMBRes;
	CVMJITMethodContext* mc = con->inliningStack[con->inliningDepth-1].mc;
	/* load the Simple Sync mb address into a register */
        CVMJITsetSymbolName((con, "mb %C.%M", mc->cb, mc->mb));
	simpleSyncMBRes = CVMRMgetResourceForConstant32(
            CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	    (CVMUint32)mc->mb);
	/* Get the ee: */
#ifndef CVMCPU_EE_REG
	eeRegID = CVMRMgetRegisterNumber(eeRes);
	CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
	CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE, eeRegID,
					 CVMoffsetof(CVMCCExecEnv, eeX));
#else
	eeRegID = CVMCPU_EE_REG;
#endif
	/* Store the Simple Sync mb into ee->currentSimpleSyncMB. */
	CVMJITaddCodegenComment((con,
				 "ee->currentSimpleSyncMB = %C.%M",
				 mc->cb, mc->mb));
	CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
	    CVMRMgetRegisterNumber(simpleSyncMBRes), eeRegID,
	     CVMoffsetof(CVMExecEnv,currentSimpleSyncMB));
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), simpleSyncMBRes);
    }
    {
	/* Store the mb of the currently executing method into
	 * ee->currentMB. */
	CVMRMResource* mbRes;
	/* load the current mb address into a register */
        CVMJITsetSymbolName((con, "mb %C.%M",
			     CVMmbClassBlock(con->mb), con->mb));
	mbRes = CVMRMgetResourceForConstant32(
            CVMRM_INT_REGS(con), CVMRM_ANY_SET, CVMRM_SAFE_SET,
	    (CVMUint32)con->mb);
	/* Store the Simple Sync mb into ee->currentMB. */
	CVMJITaddCodegenComment((con,
				 "ee->currentMB = %C.%M",
				 CVMmbClassBlock(con->mb), con->mb));
	CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
	    CVMRMgetRegisterNumber(mbRes), 
            eeRegID, CVMoffsetof(CVMExecEnv,currentMB));
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), mbRes);
    }
    CVMJITprintCodegenComment(("END OF DEBUG-ONLY CODE"));
#endif /* CVM_DEBUG */

    /* load the object header */
    CVMJITaddCodegenComment((con, "get obj.hdr.various32"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
				       objHdrRegID, objRegID,
				       CVMoffsetof(CVMObjectHeader,various32));
    /* assume object locked and set intrinsic result */
    CVMJITaddCodegenComment((con, "assume locked: result = false"));
    CVMCPUemitLoadConstant(con, destRegID, CVM_FALSE);
    /* get sync bits from object header, borrow lockRecRegID for now */
    CVMJITaddCodegenComment((con, "get obj sync bits"));
    CVMCPUemitBinaryALUConstant(con, CVMCPU_AND_OPCODE,
				lockRecRegID, objHdrRegID,
				CVM_SYNC_MASK, CVMJIT_NOSETCC);
    /* check if object is unlocked */
    CVMJITaddCodegenComment((con, "check if obj unlocked"));
    CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
			      lockRecRegID, CVM_LOCKSTATE_UNLOCKED);
    /* branch if object is locked */
    CVMJITaddCodegenComment((con,"br simpleLockGrabDone if object is locked"));
    CVMCPUemitBranch(con, 0, CVMCPU_COND_NE);
#ifdef CVMCPU_HAS_DELAY_SLOT
    fixupPC1 = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
    fixupPC1 = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif

#ifdef IAI_CODE_SCHEDULER_SCORE_BOARD
    fixupPC1 = CVMJITcbufGetLogicalInstructionPC(con);
#endif
    /* Get the ee: */
#ifndef CVMCPU_EE_REG
    eeRegID = CVMRMgetRegisterNumber(eeRes);
    CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
    CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE, eeRegID,
				     CVMoffsetof(CVMCCExecEnv, eeX));
#else
    eeRegID = CVMCPU_EE_REG;
#endif
    /* Store the object pointer into the lock record */
    CVMJITaddCodegenComment((con,
	"ee->simpleSyncReservedOwnedMonitor.object = obj"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
        objRegID, eeRegID,
	offsetof(CVMExecEnv, simpleSyncReservedOwnedMonitor.object));
    /* Store the object header word into the lock record */
    CVMJITaddCodegenComment((con,
       "ee->simpleSyncReservedOwnedMonitor.u.fast.bits = obj->hdr.various32"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
        objHdrRegID, eeRegID,
	offsetof(CVMExecEnv, simpleSyncReservedOwnedMonitor.u.fast.bits));
    /* compute address of lock record */
    CVMJITaddCodegenComment((con, "compute address of lock record fast.bits"));
    CVMCPUemitBinaryALUConstant(con, CVMCPU_ADD_OPCODE,
	lockRecRegID, eeRegID,
	offsetof(CVMExecEnv, simpleSyncReservedOwnedMonitor),
	CVMJIT_NOSETCC);
    /* CAS the lock record pointer into the object header. The PC of the
     * failure branch is returned by CVMCPUemitAtomicCompareAndSwap. */
    CVMJITprintCodegenComment((
	"CAS the lock record pointer into the object header"));
    CVMJITsetSymbolName((con, "simpleLockGrabDone"));/* name of branch label */
    fixupPC2 = CVMCPUemitAtomicCompareAndSwap(con,
	objRegID, CVMoffsetof(CVMObjectHeader,various32),
        objHdrRegID, lockRecRegID);
    /* Success if we fall through to here. Return true. */
    CVMJITaddCodegenComment((con, "success: result = true"));
    CVMCPUemitLoadConstant(con, destRegID, CVM_TRUE);
    /* "done" target for failure branches. */
    CVMtraceJITCodegen(("\t\tsimpleLockGrabDone:\n"));
    CVMJITfixupAddress(con, fixupPC1, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);
    CVMJITfixupAddress(con, fixupPC2, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);

#ifndef CVMCPU_EE_REG
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), eeRes);
#endif
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), obj);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), objHdr);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lockRec);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, intrinsicNode);
    pushResource(con, dest);
}

/*
 * Intrinsic emitter for fastlock CAS version of  CVM.simpleLockRelease().
 *
 * Attempts to unlock the object by swapping in the old bits, expecting
 * the reserved ee->simpleSyncReservedOwnedMonitor pointer to be swapped
 * out. If the atomic CAS of these values failed, then in if defers
 * to the CVMCCMruntimeSimpleSyncUnlock() helper.
 */
static void
simpleLockReleaseEmitter(
    CVMJITCompilationContext * con,
    CVMJITIRNode *intrinsicNode)
{
    CVMRMResource* obj = popResource(con);
    CVMRMResource* objHdr;
    CVMRMResource* lockRec;
    CVMRMResource* eeRes;
    int objRegID, objHdrRegID, lockRecRegID, eeRegID;
    int fixupPC1, fixupPC2; /* To patch the conditional branches */

    if (CVMRMisConstant(obj) && CVMRMgetConstant(obj) == 0) {
	/* We know the object is a null, so an NPE would be thrown before
	 * code generated here is executed. Therefore we don't need to
	 * generate anything.
	 */
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), obj);
	return;
    }

    /* Make sure obj is in ARG2 in case we call C helper */
    if (CVMRMgetRegisterNumberUnpinned(obj) != CVMCPU_ARG2_REG) {
	CVMJITprintCodegenComment(("ARG2 = obj"));
    }
    CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), obj, CVMCPU_ARG2_REG);
    objRegID = CVMRMgetRegisterNumber(obj);
    CVMassert(objRegID == CVMCPU_ARG2_REG);

    /* Be sure to spill ARG1. We will load it with the ee later if needed.
     * ARG2 since it already contains obj. */
    CVMRMmajorSpill(con, ARG2, CVMRM_SAFE_SET);

    eeRes = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con), CVMCPU_ARG1_REG, 1);
    objHdr = CVMRMgetResource(CVMRM_INT_REGS(con),
			      CVMRM_ANY_SET, CVMRM_SAFE_SET, 1);
    lockRec = CVMRMgetResource(CVMRM_INT_REGS(con),
			      CVMRM_ANY_SET, CVMRM_SAFE_SET, 1);

    objHdrRegID = CVMRMgetRegisterNumber(objHdr);
    lockRecRegID = CVMRMgetRegisterNumber(lockRec);

#ifndef CVMCPU_EE_REG
    /* Get the ee: */
    eeRegID = CVMRMgetRegisterNumber(eeRes);
    CVMassert(eeRegID == CVMCPU_ARG1_REG);
    CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
    CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE, eeRegID,
				     CVMoffsetof(CVMCCExecEnv, eeX));
#else
    eeRegID = CVMCPU_EE_REG;
#endif

    /* load the object header */
    CVMJITaddCodegenComment((con, "get obj.hdr.various32"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
				       objHdrRegID, objRegID,
				       CVMoffsetof(CVMObjectHeader,various32));
    /* compute address of lock record */
    CVMJITaddCodegenComment((con, "compute address of lock record fast.bits"));
    CVMCPUemitBinaryALUConstant(con, CVMCPU_ADD_OPCODE,
	lockRecRegID, eeRegID,
	offsetof(CVMExecEnv, simpleSyncReservedOwnedMonitor),
	CVMJIT_NOSETCC);
    /* Note the lock state bits for "locked" must be 0 in order for the
     * CAS below to work without first checking the lock state bits. */
    CVMassert(CVM_LOCKSTATE_LOCKED == 0);
    /* get old object header word from lock record. Note, it will have been
     * overwritten if inflated. */
    CVMJITaddCodegenComment((con,
	"ee->simpleSyncReservedOwnedMonitor.u.fast.bits"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
        objHdrRegID, eeRegID,
	offsetof(CVMExecEnv, simpleSyncReservedOwnedMonitor.u.fast.bits));
    /* CAS the old header word back into the object header, requiring
       that the swapped out word be the unmodified lock record pointer */
    CVMJITprintCodegenComment((
	"CAS the old header word back into the object header, requiring"));
    CVMJITprintCodegenComment((
	"that the swapped out word be the unmodified lock record pointer"));
    CVMJITsetSymbolName((con, "simpleLockReleaseFailed")); /* branch label */
    fixupPC2 = CVMCPUemitAtomicCompareAndSwap(con,
	objRegID, CVMoffsetof(CVMObjectHeader,various32),
	lockRecRegID, objHdrRegID);
    /* If we get here, then success. br simpleLockReleaseDone. */
    CVMJITaddCodegenComment((con, "br simpleLockReleaseDone"));
    CVMCPUemitBranch(con, 0, CVMCPU_COND_AL);
#ifdef CVMCPU_HAS_DELAY_SLOT
    fixupPC1 = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
    fixupPC1 = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif

#ifdef IAI_CODE_SCHEDULER_SCORE_BOARD
    fixupPC1 = CVMJITcbufGetLogicalInstructionPC(con);
#endif
    /* failed: target for failed CAS */
    CVMtraceJITCodegen(("\t\tsimpleLockReleaseFailed:\n"));
    CVMJITfixupAddress(con, fixupPC2, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);

    /* Setup arguments for C helper that will do unlock */
#ifdef CVMCPU_EE_REG
    CVMJITaddCodegenComment((con, "ARG1 = ee"));
    CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE,
			   CVMCPU_ARG1_REG, CVMCPU_EE_REG, CVMJIT_NOSETCC);
#else
    CVMJITprintCodegenComment(("ee already in ARG1"));
#endif
    CVMJITprintCodegenComment(("obj already in ARG2"));

    /* Call C helper to do unlock */
    CVMJITaddCodegenComment((con, "call CVMCCMruntimeSimpleSyncUnlockGlue"));
    CVMJITsetSymbolName((con, "CVMCCMruntimeSimpleSyncUnlockGlue"));
    CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeSimpleSyncUnlock);
    CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeSimpleSyncUnlockGlue,
                           CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);
    CVMJITcsBeginBlock(con);
    CVMJITcaptureStackmap(con, 0);

    /* "simpleLockReleaseDone" target for branches. */
    CVMtraceJITCodegen(("\t\tsimpleLockReleaseDone:\n"));
    CVMJITfixupAddress(con, fixupPC1, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);
#ifdef CVM_DEBUG
    /* NULL out object pointer in the lock record */
    CVMJITprintCodegenComment(("DEBUG-ONLY CODE"));
#ifndef CVMCPU_EE_REG
    /* Get the ee. It could have been clobbered by the call to
       CVMCCMruntimeSimpleSyncUnlockGlue */
    CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
    CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE, eeRegID,
				     CVMoffsetof(CVMCCExecEnv, eeX));
#endif
    CVMCPUemitLoadConstant(con, objHdrRegID, 0);
    CVMJITaddCodegenComment((con,
	"ee->simpleSyncReservedOwnedMonitor.object = NULL"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
        objHdrRegID, eeRegID,
	offsetof(CVMExecEnv, simpleSyncReservedOwnedMonitor.object));
    CVMJITprintCodegenComment(("END OF DEBUG-ONLY CODE"));
#endif

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), obj);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lockRec); 
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), objHdr);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), eeRes);
}

#else
#error Unsupported locking type for CVMJIT_SIMPLE_SYNC_METHODS
#endif
#endif /* CVMJIT_SIMPLE_SYNC_METHODS */


static void
doIntMinMax(
    CVMJITCompilationContext * con,
    CVMBool	min,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* rhs = popResource(con);
    CVMRMResource* lhs = popResource(con);
    CVMRMResource* dest;
    int cclhs, ccrhs;
    int destRegID, lhsRegID, rhsRegID;
    if (min){
        cclhs = CVMCPU_COND_LE;
        ccrhs = CVMCPU_COND_GT;
    } else {
        cclhs = CVMCPU_COND_GE;
        ccrhs = CVMCPU_COND_LT;
    }
    /* TODO: Reduce instruction count by looking at refcounts. If either
     * the lhs or rhs have a refcount of 1 (and are not a local that we
     * want to keep in a register), then we can save a move by resuing
     * it as the dest register. For example:
     *    cmp    lhs, rhs
     *    movle  lhs, rhs
     * And then lhs is repurposed as the "dest" resource. Note, if we
     * reuse rhs, then we need to reverse the compare and reverse the mov.
     */
    dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
    CVMRMpinResource(CVMRM_INT_REGS(con), rhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(CVMRM_INT_REGS(con), lhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    destRegID = CVMRMgetRegisterNumber(dest);
    lhsRegID = CVMRMgetRegisterNumber(lhs);
    rhsRegID = CVMRMgetRegisterNumber(rhs);
#ifdef CVMCPU_HAS_CONDITIONAL_ALU_INSTRUCTIONS
    CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE, cclhs,
			      lhsRegID, rhsRegID);
    CVMCPUemitMoveRegisterConditional(con, CVMCPU_MOV_OPCODE, destRegID,
                                      lhsRegID, CVMJIT_NOSETCC, cclhs);
    CVMCPUemitMoveRegisterConditional(con, CVMCPU_MOV_OPCODE, destRegID,
                                      rhsRegID, CVMJIT_NOSETCC, ccrhs),
#else
    CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE, destRegID,
			   lhsRegID, CVMJIT_NOSETCC);
    CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE, ccrhs,
			      lhsRegID, rhsRegID);
    CVMCPUemitMoveRegisterConditional(con, CVMCPU_MOV_OPCODE, destRegID,
                                      rhsRegID, CVMJIT_NOSETCC, ccrhs),
#endif
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}

static void
imaxEmitOperator(CVMJITCompilationContext *con, CVMJITIRNode *intrinsicNode)
{
    struct CVMJITCompileExpression_rule_computation_state *goal_top =
	(struct CVMJITCompileExpression_rule_computation_state *)
        (con->goal_top);
    doIntMinMax(con, CVM_FALSE, intrinsicNode, GET_REGISTER_GOALS);
}

static void
iminEmitOperator(CVMJITCompilationContext *con, CVMJITIRNode *intrinsicNode)
{
    struct CVMJITCompileExpression_rule_computation_state *goal_top =
	(struct CVMJITCompileExpression_rule_computation_state *)
        (con->goal_top);
    doIntMinMax(con, CVM_TRUE, intrinsicNode, GET_REGISTER_GOALS);
}

const CVMJITIntrinsicEmitterVtbl
   CVMJITRISCintrinsicThreadCurrentThreadEmitter =
{
    CVMJITRISCintrinsicDefaultGetRequired,
    CVMJITRISCintrinsicDefaultGetArgTarget,
    java_lang_Thread_currentThread_EmitOperator,
};

const CVMJITIntrinsicEmitterVtbl CVMJITRISCintrinsicIAbsEmitter =
{
    CVMJITRISCintrinsicDefaultGetRequired,
    CVMJITRISCintrinsicDefaultGetArgTarget,
    iabsEmitOperator,
};

const CVMJITIntrinsicEmitterVtbl CVMJITRISCintrinsicIMaxEmitter =
{
    CVMJITRISCintrinsicDefaultGetRequired,
    CVMJITRISCintrinsicDefaultGetArgTarget,
    imaxEmitOperator,
};

const CVMJITIntrinsicEmitterVtbl CVMJITRISCintrinsicIMinEmitter =
{
    CVMJITRISCintrinsicDefaultGetRequired,
    CVMJITRISCintrinsicDefaultGetArgTarget,
    iminEmitOperator,
};

#ifdef CVMJIT_SIMPLE_SYNC_METHODS
const CVMJITIntrinsicEmitterVtbl CVMJITRISCintrinsicSimpleLockGrabEmitter =
{
    CVMJITRISCintrinsicDefaultGetRequired,
    CVMJITRISCintrinsicDefaultGetArgTarget,
    simpleLockGrabEmitter,
};

const CVMJITIntrinsicEmitterVtbl CVMJITRISCintrinsicSimpleLockReleaseEmitter =
{
    CVMJITRISCintrinsicSimpleLockReleaseGetRequired,
    CVMJITRISCintrinsicSimpleLockReleaseGetArgTarget,
    simpleLockReleaseEmitter,
};
#endif /* CVMJIT_SIMPLE_SYNC_METHODS */

#endif /* CVMJIT_INTRINSICS */

#line 2511 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

#define IS_POWER_OF_2(x) (((x) & ((x) - 1)) == 0)

/* Purpose: Attempts to apply strength reduction on an IMul by a constant. */
static void
doIMulByIConst32(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode,
                 CVMRMregset target, CVMRMregset avoid)
{
    CVMRMResource *lhs = popResource(con);
    CVMJITIRNode *constNode;
    CVMRMResource *dest;
    CVMInt32 value;
    CVMBool constantIsNegative = CVM_FALSE;

    constNode = CVMJITirnodeGetRightSubtree(thisNode);
    value = CVMJITirnodeGetConstant32(constNode)->j.i;

    if (value < 0) {
        value = -value;
        constantIsNegative = CVM_TRUE;
    }

    if (value == 0) {
        dest = CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), 0);

    } else if (value == 1) {
        if (constantIsNegative) {
            /* Reduce multiply into negate: */
            dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
            CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

            CVMJITaddCodegenComment((con, "imul by -1"));
            /*  neg  rDest, rLhs */
            CVMCPUemitUnaryALU(con, CVMCPU_NEG_OPCODE,
                CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(lhs),
                CVMJIT_NOSETCC);
        } else {
            dest = lhs;

            /* Reduce multiply into nothing: */
            CVMJITprintCodegenComment(("imul by 1: Do nothing."));
            CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
            pushResource(con, dest);
            return;
        }

    } else if (IS_POWER_OF_2(value) || IS_POWER_OF_2(value - 1)) {
        int lhsReg, destReg;
        CVMInt32 i;
        CVMBool isPowerOf2 = IS_POWER_OF_2(value);

        CVMJITaddCodegenComment((con, "imul by %d",
                                 CVMJITirnodeGetConstant32(constNode)->j.i));

        /* Reduce multiply into shifts and adds: */
        if (!isPowerOf2) {
            value--;
        }
        for (i = 1; i < 32; i++) {
            if ((1 << i) == value) {
                break;
            }
        }
        CVMassert((i < 32 && constantIsNegative) ||
                  (i < 31 && !constantIsNegative));

        dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
        CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        lhsReg = CVMRMgetRegisterNumber(lhs);
        destReg = CVMRMgetRegisterNumber(dest);
        if (constantIsNegative) {
            /* neg  rDest, rLhs */
            CVMCPUemitUnaryALU(con, CVMCPU_NEG_OPCODE, destReg, lhsReg,
			       CVMJIT_NOSETCC);

            if (isPowerOf2) {
                /* rDest = rDest << #i; */
                CVMCPUemitShiftByConstant(con, CVMCPU_SLL_OPCODE,
                                          destReg, destReg, i);
            } else {
                /* rDest = (rDest << #i) + rDest; */
                CVMCPUemitShiftAndAdd(con, CVMCPU_SLL_OPCODE,
				      destReg, destReg, destReg, i);
            }
        } else {
            if (isPowerOf2) {
                /* rDest = rLhs << #i; */
                CVMCPUemitShiftByConstant(con, CVMCPU_SLL_OPCODE,
                                          destReg, lhsReg, i);
            } else {
                /* rDest = (rLhs << #i) + rLhs; */
                CVMCPUemitShiftAndAdd(con, CVMCPU_SLL_OPCODE,
				      destReg, lhsReg, lhsReg, i);
            }
        }

    } else {
        /* Do multiply: */
	int destRegID;
	int lhsRegID;

        dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
	destRegID = CVMRMgetRegisterNumber(dest);
	CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	lhsRegID = CVMRMgetRegisterNumber(lhs);

        if (constantIsNegative) {
            value = -value;
        }
#ifdef CVMCPU_HAS_IMUL_IMMEDIATE
	if (CVMCPUalurhsIsEncodableAsImmediate(CVMCPU_MULL_OPCODE, value)) {
	    CVMCPUemitMulConstant(con, destRegID, lhsRegID, value);
	} else
#endif
	{
	    CVMRMResource *constRes =
		CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), value);
	    CVMRMpinResource(CVMRM_INT_REGS(con), constRes,
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    CVMCPUemitMul(con, CVMCPU_MULL_OPCODE,
			  destRegID, lhsRegID,
			  CVMRMgetRegisterNumber(constRes),
			  CVMCPU_INVALID_REG);
	    CVMRMrelinquishResource(CVMRM_INT_REGS(con), constRes);
	}
    }

    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    pushResource(con, dest);
}

/* Purpose: Attempts to apply strength reduction on an IDiv/IRem by a
            constant. */
static void
doIDivOrIRemByIConst32(CVMJITCompilationContext *con,
                       CVMJITIRNodePtr thisNode,
                       CVMRMregset target, CVMRMregset avoid,
                       CVMBool isIDiv)
{
    CVMJITIRNode *rhsNode = CVMJITirnodeGetRightSubtree(thisNode);
    CVMInt32 value = CVMJITirnodeGetConstant32(rhsNode)->j.i;
    CVMBool divisorIsNegative = CVM_FALSE;

    if (value < 0) {
        value = -value;
        divisorIsNegative = CVM_TRUE;
    }

    if (value == 0) {
        CVMRMResource *lhs = popResource(con);
        CVMJITprintCodegenComment(("idiv by 0"));
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeThrowDivideByZeroGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
        CVMJITcsBeginBlock(con);
        /* We push the incoming operand as the result because the rest of
           the code to be compiled still expects a result.  They won't know
           that we're going to throw a DivideByZeroException. */
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), lhs, thisNode);
        pushResource(con, lhs);
        return;

    } else if (value == 1) {
        CVMRMResource *lhs = popResource(con);
        CVMRMResource *dest;
        if (isIDiv) {
            if (divisorIsNegative) {
                /* Reduce divide into negate: */
                dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
                CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
				 CVMRM_ANY_SET, CVMRM_EMPTY_SET);

                CVMJITaddCodegenComment((con, "idiv by -1"));
                /*  neg  rDest, rLhs */
                CVMCPUemitUnaryALU(con, CVMCPU_NEG_OPCODE,
                    CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(lhs),
                    CVMJIT_NOSETCC);
                CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
                CVMRMunpinResource(CVMRM_INT_REGS(con), dest);
            } else {
                CVMJITprintCodegenComment(("idiv by 1: Do nothing"));
                dest = lhs;
            }
        } else {
            CVMJITprintCodegenComment(("irem by %d: Do nothing",
                CVMJITirnodeGetConstant32(rhsNode)->j.i));
            dest = CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), 0);
            CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
        }
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
        pushResource(con, dest);
        return;

    } else if (IS_POWER_OF_2((CVMUint32)value)) {
	/*
	 * For IDIV:
	 *    sra   rDest, rSrc, #31
	 *    srl   rDest, rDest, #32-log2(value)
	 *    add   rDest, rSrc, rDest
	 *    sra   rDest, rDest, #log2(value)
	 *    neg   rDest, rDest   <--- Only if divisor is negative
	 *
	 * For IREM:
	 *    sra   rDest, rSrc, #31
	 *    srl   rDest, rDest, #32-log2(value)
	 *    add   rDest, rSrc, rDest
	 *    bic   rDest, rDest, #(1<<log2(value))-1
	 *    sub   rDest, rSrc, rDest
	 *
	 * This is a fairly generic solution that will work reasonably
	 * well on most platforms. It appears to be optimal for ARM and
	 * PowerPC. Sparc and MIPS could do a bit better if they had
	 * their own emitter for this, so maybe we should consider adding
	 * an optional CVMCPUemitDivOrRemByPowerOf2() emitter.
	 */
        CVMRMResource *lhs = popResource(con);
        CVMRMResource *dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       target, avoid, 1);
        int lhsReg, destReg;
        CVMInt32 i;

        for (i = 1; i < 32; i++) {
            if ((1 << i) == value) {
                break;
            }
        }
        CVMassert((divisorIsNegative && i < 32) ||
                  (!divisorIsNegative && i < 31));

        CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        lhsReg = CVMRMgetRegisterNumber(lhs);
        destReg = CVMRMgetRegisterNumber(dest);

        CVMJITprintCodegenComment(("do %s by %d:", (isIDiv ? "idiv" : "irem"),
                                   CVMJITirnodeGetConstant32(rhsNode)->j.i));

	/* sra   rDest, rSrc, #31 */
	CVMCPUemitShiftByConstant(con, CVMCPU_SRA_OPCODE, destReg, lhsReg, 31);
	/* IAI-02 */
	/* rDest = (rDest << #32-log2(value)) + rSrc */
	CVMCPUemitShiftAndAdd(con, CVMCPU_SRL_OPCODE,
			      destReg, destReg, lhsReg, 32 - i);
        if (isIDiv) {
	    /* sra   rDest, rDest, #log2(value) */
	    CVMCPUemitShiftByConstant(con, CVMCPU_SRA_OPCODE,
				      destReg, destReg, i);
	    /* neg   rDest, rDest */
	    if (divisorIsNegative) {
                CVMCPUemitUnaryALU(con, CVMCPU_NEG_OPCODE,
				   destReg, destReg, CVMJIT_NOSETCC);
	    }
        } else {
	    /* bic   rDest, rDest, #log2(value) */
	    CVMCPUemitBinaryALUConstant(con, CVMCPU_BIC_OPCODE,
					destReg, destReg, (1<<i)-1,
					CVMJIT_NOSETCC);
	    /* sub   rDest, rSrc, rDest */
	    CVMCPUemitBinaryALURegister(con, CVMCPU_SUB_OPCODE,
					destReg, lhsReg, destReg,
					CVMJIT_NOSETCC);
        }

        CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
        pushResource(con, dest);
        return;

    } else {
        CVMRMResource *lhs;
        CVMRMResource *dest;
        CVMRMResource *temp;
        CVMRMResource *inverse;
        CVMInt32 i;
        CVMJavaLong magic;
        CVMJavaLong long1 = CVMlongConstOne();
        CVMInt32 inverseValue;
        int lhsReg, destReg, tmpReg, invReg;

        /* NOTE: i can never be less than 2 because we know that value > 2: */
        for (i = 2; i < 32; i++) {
            if ((CVMUint32)(1 << i) > (CVMUint32)value) {
                break;
            }
        }
        i--;

        CVMJITprintCodegenComment(("do %s by %d:", (isIDiv ? "idiv" : "irem"),
                                   CVMJITirnodeGetConstant32(rhsNode)->j.i));

        /* ulong magic = ( (ulong)1 << (32 + i)) - 1; */
        magic = CVMlongSub(CVMlongShl(long1, 32+i), long1);
        /* uint inverse = (uint)(magic / divisor) + 1; */
        inverseValue = CVMlong2Int(CVMlongDiv(magic, CVMint2Long(value))) + 1;

	/*
	 * If the inverseValue is even, then we can cheat and shift it
	 * right by one and shift right by one less later on. This allows
	 * us to avoid the add of the dividend later on.
	 */
	if ((inverseValue & 1) == 0) {
	    inverseValue >>= 1;
	    inverseValue &= 0x7fffffff;
	    i--;
	}

        lhs = popResource(con);
        dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
        temp = CVMRMgetResource(CVMRM_INT_REGS(con),
				CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
        inverse = CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con),
						 inverseValue);
        CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMRMpinResource(CVMRM_INT_REGS(con), inverse,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMRMpinResource(CVMRM_INT_REGS(con), temp,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        lhsReg = CVMRMgetRegisterNumber(lhs);
        destReg = CVMRMgetRegisterNumber(dest);
        tmpReg = CVMRMgetRegisterNumber(temp);
        invReg = CVMRMgetRegisterNumber(inverse);

	/* sra   destReg,lhsReg,#31 */
        CVMCPUemitShiftByConstant(con, CVMCPU_SRA_OPCODE, destReg, lhsReg, 31);
	/* mulh  tmpReg,lhsReg,invReg */
        CVMCPUemitMul(con, CVMCPU_MULH_OPCODE,
		      tmpReg, lhsReg, invReg, CVMCPU_INVALID_REG);
	/* We only need to add if the inverseValue is still negative */
	if ((inverseValue & 0x80000000) != 0) {
	    /* add   tmpReg,tmpReg,lhsReg */
	    CVMCPUemitBinaryALURegister(con, CVMCPU_ADD_OPCODE,
					tmpReg, tmpReg, lhsReg,
					CVMJIT_NOSETCC);
	}
	/* sra   tmpReg,tmpReg,#<i> */
        CVMCPUemitShiftByConstant(con, CVMCPU_SRA_OPCODE, tmpReg, tmpReg, i);
	/* reverse subtract for neg divisor so we don't need to emit a neg */
	if (isIDiv && divisorIsNegative) {
	    /* sub   destReg,destReg,tmpReg */
	    CVMCPUemitBinaryALURegister(con, CVMCPU_SUB_OPCODE,
					destReg, destReg, tmpReg,
					CVMJIT_NOSETCC);
	} else {
	    /* sub   destReg,tmpReg,destReg */
	    CVMCPUemitBinaryALURegister(con, CVMCPU_SUB_OPCODE,
					destReg, tmpReg, destReg,
					CVMJIT_NOSETCC);
	}

	/* compute remainder if necessary */
        if (!isIDiv) {
            /*
	     * mul tmpReg,destReg,<value>
	     * sub destReg,lhsReg,tmpReg
	     *
	     * TOOD: try to make use of doIMulByIConst32().
             */
#ifdef CVMCPU_HAS_IMUL_IMMEDIATE
	    if (CVMCPUalurhsIsEncodableAsImmediate(CVMCPU_MULL_OPCODE, value)){
		CVMCPUemitMulConstant(con, tmpReg, destReg, value);
	    } else
#endif
	    {
		CVMRMResource *divisor =
		    CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), value);
		CVMRMpinResource(CVMRM_INT_REGS(con), divisor,
				 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
		CVMCPUemitMul(con, CVMCPU_MULL_OPCODE,
			      tmpReg, destReg,
			      CVMRMgetRegisterNumber(divisor),
			      CVMCPU_INVALID_REG);
		CVMRMrelinquishResource(CVMRM_INT_REGS(con), divisor);
	    }
	    CVMCPUemitBinaryALURegister(con, CVMCPU_SUB_OPCODE,
					destReg, lhsReg, tmpReg,
					CVMJIT_NOSETCC);
        }

        CVMRMrelinquishResource(CVMRM_INT_REGS(con), temp);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), inverse);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
        pushResource(con, dest);
        return;
    }
}

#undef IS_POWER_OF_2

#line 3015 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

static void
shortenInt(
    CVMJITCompilationContext* con,
    int rightshiftop,
    int shiftwidth,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					   target, avoid, 1);
    int destregno = CVMRMgetRegisterNumber(dest);
    CVMRMpinResource(CVMRM_INT_REGS(con), src, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUemitShiftByConstant(con, CVMCPU_SLL_OPCODE, destregno,
                              CVMRMgetRegisterNumber(src), shiftwidth);
    CVMCPUemitShiftByConstant(con, rightshiftop, destregno,
                              destregno, shiftwidth);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    pushResource(con, dest);
}

#line 3197 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


static void
longBinaryOp(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid )
{
    CVMRMResource* rhs = popResource(con);
    CVMRMResource* lhs = popResource(con);
    CVMRMResource* dest;
    CVMRMpinResource(CVMRM_INT_REGS(con), rhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(CVMRM_INT_REGS(con), lhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 2);

    CVMCPUemitBinaryALU64(con, opcode, CVMRMgetRegisterNumber(dest),
        CVMRMgetRegisterNumber(lhs), CVMRMgetRegisterNumber(rhs));

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}

#line 3270 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

/* Purpose: Emits code for a shift operation with a const shiftAmount.  Also
            masks off the offset with 0x3f before shifting per VM spec.

   NOTE: It turns out that emitting these constant shifts inline does not
   speed things up when running kBench, Spec, or CM3.0 because the are
   used so infrequently. We could get rid if the ICONST_32 rhs rules and
   just let the reg32 rhs rules pin the constant to a register and call the
   shift helpers. However, it does speed up a loop that does nothing but
   shift of a 64-bit values by about 3X, so this implementation is left in
   just in case an application is run that will make heavy use of it.
*/
static void doInt64Shift(CVMJITCompilationContext *con,
			 int shiftOp1, int shiftOp2, int shiftOp3,
			 CVMJITIRNodePtr thisNode,
			 CVMRMregset target, CVMRMregset avoid)
{
    CVMRMResource *src;
    CVMRMResource *dest;
    CVMInt32 shiftOffset =
        CVMJITirnodeGetConstant32(CVMJITirnodeGetRightSubtree(thisNode))->j.i;
    int destHI;
    int destLO;
    int srcHI;
    int srcLO;

    shiftOffset &= 0x3f;  /* mask higher bits per vm spec */

    if (shiftOffset == 0) {
	passLastEvaluated(con, CVMRM_INT_REGS(con), thisNode);
	return;  /* no code to emit in this case */
    }

    dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 2);
    src = popResource(con);
    CVMRMpinResource(CVMRM_INT_REGS(con), src,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

#if CVM_ENDIANNESS == CVM_LITTLE_ENDIAN
    destLO = CVMRMgetRegisterNumber(dest);
    destHI = destLO + 1;
    srcLO = CVMRMgetRegisterNumber(src);
    srcHI = srcLO + 1;
#else
    destHI = CVMRMgetRegisterNumber(dest);
    destLO = destHI + 1;
    srcHI = CVMRMgetRegisterNumber(src);
    srcLO = srcHI + 1;
#endif

    /* We can do shifts of >=32 in fewer instructions */
    if (shiftOffset >= 32) {
	shiftOffset -= 32;
	if (shiftOp3 == CVMCPU_SRA_OPCODE) {        /* SRA */
	    CVMCPUemitShiftByConstant(con, shiftOp3, destHI, srcHI, 31);
	    CVMCPUemitShiftByConstant(con, shiftOp3, destLO, srcHI,
				      shiftOffset);
	} else if (shiftOp3 == CVMCPU_SRL_OPCODE) { /* SRL */
	    CVMCPUemitLoadConstant(con, destHI, 0);
	    CVMCPUemitShiftByConstant(con, shiftOp2, destLO, srcHI,
				      shiftOffset);
	} else {                                    /* SLL */
	    CVMCPUemitShiftByConstant(con, shiftOp3, destHI, srcLO,
				      shiftOffset);
	    CVMCPUemitLoadConstant(con, destLO, 0);
	}
    } else {
	CVMRMResource *scratchRes =
	    CVMRMgetResource(CVMRM_INT_REGS(con),
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
	int scratch;
	int reverseShiftSrc;
	int reverseShiftDest;
	CVMRMpinResource(CVMRM_INT_REGS(con), scratchRes,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	scratch = CVMRMgetRegisterNumber(scratchRes);
	/* shiftOp1 shifts the reverse direction of shiftOp2 and shiftOp3,
	 * which are the true direction of the shift */
	if (shiftOp1 == CVMCPU_SRL_OPCODE) {    /* SLL */
	    reverseShiftSrc = srcLO;
	    reverseShiftDest = destHI;
	} else {  				/* SRL and SRA */
	    reverseShiftSrc = srcHI;
	    reverseShiftDest = destLO;
	}
	CVMCPUemitShiftByConstant(con, shiftOp1,
				  scratch, reverseShiftSrc, 32-shiftOffset);
	CVMCPUemitShiftByConstant(con, shiftOp2, destLO, srcLO, shiftOffset);
	CVMCPUemitShiftByConstant(con, shiftOp3, destHI, srcHI, shiftOffset);
	CVMCPUemitBinaryALURegister(con, CVMCPU_OR_OPCODE,
				    reverseShiftDest, reverseShiftDest,
				    scratch, CVMJIT_NOSETCC);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), scratchRes);
    }

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}
#line 3513 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/*
 * The descriptive table for each kind of array access.
 * This table is indexed by typeid. So the order of CVM_TYPEID constants
 * matters.
 *
 * For each entry, we have an index shift amount, whether this is a
 * ref entry, the appropriate load opcode, and the appropriate store
 * opcode for an array element of this type.
 */
typedef struct ArrayElemInfo ArrayElemInfo;
struct ArrayElemInfo {
    int      shiftAmount;  /* 2<<shiftAmount == elemSize */
    int      size;         /* Resultant size in words */
    CVMBool  isRef;
    int      loadOpcode;
    int      storeOpcode;
#ifdef CVM_JIT_USE_FP_HARDWARE
    int      floatLoadOpcode;
    int      floatStoreOpcode;
#endif
};

#define CVM_ILLEGAL_OPCODE -1

#ifdef CVM_JIT_USE_FP_HARDWARE
#define CVM_NONE	, CVM_ILLEGAL_OPCODE, CVM_ILLEGAL_OPCODE 
#define CVM_FLDST_NONE	, CVM_ILLEGAL_OPCODE, CVM_ILLEGAL_OPCODE
#define CVM_FLDST32	, CVMCPU_FLDR32_OPCODE, CVMCPU_FSTR32_OPCODE
#define CVM_FLDST64	, CVMCPU_FLDR64_OPCODE, CVMCPU_FSTR64_OPCODE
#else
#define CVM_NONE
#define CVM_FLDST_NONE
#define CVM_FLDST32
#define CVM_FLDST64
#endif
 
typedef struct ScaledIndexInfo ScaledIndexInfo;
struct ScaledIndexInfo {
    /*
     * The data for the "super-class"
     */
    CVMJITIdentityDecoration  dec;

    CVMBool hasConstIndex;
    CVMInt32 index;
    CVMRMResource* indexReg;
    CVMRMResource* arrayBaseReg;
    int shiftAmount;
    const ArrayElemInfo* elemInfo;

    int baseRegID;
    CVMRMResource *slotAddrReg;
    int slotAddrOffset;
    
#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    CVMBool isIDENTITYOutofBoundsCheck;
#endif
};

const ArrayElemInfo typeidToArrayElemInfo[] = {
    /* CVM_TYPEID_NONE */      
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},   /* No arrays of this type */
    /* CVM_TYPEID_ENDFUNC */   
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},   /* No arrays of this type */
    /* CVM_TYPEID_VOID */      
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},   /* No arrays of this type */
    /* CVM_TYPEID_INT */       
    {2, 1, CVM_FALSE, CVMCPU_LDR32_OPCODE, CVMCPU_STR32_OPCODE CVM_FLDST_NONE},
    /* CVM_TYPEID_SHORT */     
    {1, 1, CVM_FALSE, CVMCPU_LDR16_OPCODE, CVMCPU_STR16_OPCODE CVM_FLDST_NONE},
    /* CVM_TYPEID_CHAR */      
    {1, 1, CVM_FALSE, CVMCPU_LDR16U_OPCODE,CVMCPU_STR16_OPCODE CVM_FLDST_NONE},
    /* CVM_TYPEID_LONG */      
    {3, 2, CVM_FALSE, CVMCPU_LDR64_OPCODE, CVMCPU_STR64_OPCODE CVM_FLDST_NONE},
    /* CVM_TYPEID_BYTE */      
    {0, 1, CVM_FALSE, CVMCPU_LDR8_OPCODE, CVMCPU_STR8_OPCODE CVM_FLDST_NONE},
    /* CVM_TYPEID_FLOAT */     
    {2, 1, CVM_FALSE, CVMCPU_LDR32_OPCODE, CVMCPU_STR32_OPCODE CVM_FLDST32},
    /* CVM_TYPEID_DOUBLE */    
    {3, 2, CVM_FALSE, CVMCPU_LDR64_OPCODE, CVMCPU_STR64_OPCODE CVM_FLDST64},
    /* CVM_TYPEID_BOOLEAN  This will look like a byte array */
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},
    /* CVM_TYPEID_OBJ */       
    {2, 1, CVM_TRUE, CVMCPU_LDR32_OPCODE, CVMCPU_STR32_OPCODE CVM_FLDST_NONE},
    /* CVMJIT_TYPEID_32BITS */      
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},   /* No arrays of this type */
    /* CVMJIT_TYPEID_64BITS */   
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},   /* No arrays of this type */
    /* CVMJIT_TYPEID_ADDRESS */      
    {-1, -1, CVM_FALSE, 0, 0 CVM_FLDST_NONE},   /* No arrays of this type */
    /* CVMJIT_TYPEID_UBYTE */       
    {0, 1, CVM_FALSE, CVMCPU_LDR8U_OPCODE, CVMCPU_STR8_OPCODE CVM_FLDST_NONE},
};


/* Purpose: Instantiates a ScaledIndexInfo data structure. */
static ScaledIndexInfo*
newScaledIndexInfo(CVMJITCompilationContext *con, CVMRMResource *indexReg,
                   CVMInt32 index, CVMBool isConstIndex)
{
    ScaledIndexInfo* sinfo = CVMJITmemNew(con, JIT_ALLOC_CGEN_OTHER,
                                          sizeof(ScaledIndexInfo));
    CVMJITidentityInitDecoration(con, &sinfo->dec,
				 CVMJIT_IDENTITY_DECORATION_SCALEDINDEX);
    sinfo->hasConstIndex = isConstIndex;
    sinfo->index = index;
    sinfo->indexReg = indexReg;
    if (indexReg != NULL) {
	/* incorporated into sinfo. Increment ref count */
	CVMRMincRefCount(con, indexReg); 
    }
    sinfo->shiftAmount = -1;
#ifdef CVM_DEBUG
    sinfo->baseRegID = CVMCPU_INVALID_REG;
    sinfo->slotAddrReg = NULL;
    sinfo->slotAddrOffset = 0;
#endif

#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    sinfo->isIDENTITYOutofBoundsCheck = CVM_FALSE;
#endif
    
    return sinfo;
}

/* Purpose: Pushes a register index type ScaledIndexInfo on to the codegen
            semantic stack. */
static void
pushScaledIndexInfoReg(CVMJITCompilationContext *con, CVMRMResource *indexReg)
{
    ScaledIndexInfo* sinfo = newScaledIndexInfo(con, indexReg, 0, CVM_FALSE);
    pushIConst32(con, (CVMInt32)sinfo);
}

/* Purpose: Pushes an immediate index type ScaledIndexInfo on to the codegen
            semantic stack. */
static void
pushScaledIndexInfoImmediate(CVMJITCompilationContext *con, CVMInt32 index)
{
    ScaledIndexInfo* sinfo = newScaledIndexInfo(con, NULL, index, CVM_TRUE);
    pushIConst32(con, (CVMInt32)sinfo);
}

/* Purpose: Pops a ScaledIndexInfo off of the codegen semantic stack. */
CVM_INLINE static ScaledIndexInfo*
popScaledIndexInfo(CVMJITCompilationContext *con)
{
    return (ScaledIndexInfo*)popIConst32(con);
}

/* Purpose: Pops a ScaledIndexInfo off of the codegen semantic stack. */
CVM_INLINE static void
pushScaledIndexInfo(CVMJITCompilationContext *con, ScaledIndexInfo* sinfo)
{
    pushIConst32(con, (CVMInt32)sinfo);
}

/* Purpose: Does setup for doing a scaled index operation.  This may entail:
            1. Allocating and pinning any scratch resources needed.
            2. Emitting some setup code to produce intermediate values to be
               used in a memory reference later to do the actual Java array
               element access. 

   computeSlotAddr tells setupScaledIndex() that the array slot address
   has to be computed unconditionally as a register-immediate pair.

   isRef tells setupScaledIndex() that a card table routine is going to
   be called on the array access, so the slot address has to be computed
   precisely.

*/
static void
setupScaledIndex(CVMJITCompilationContext *con, int opcode,
                 CVMRMResource *array, ScaledIndexInfo *sinfo,
		 CVMBool isRefStore)
{
    int arrayRegID = CVMRMgetRegisterNumber(array);

    sinfo->slotAddrReg = NULL;

    /* Need to compute effective address into a register: */
    if (sinfo->hasConstIndex) {
        CVMUint32 offset;

        /* Fold all constants into a single offset: */
        offset = (sinfo->index << sinfo->shiftAmount) + ARRAY_DATA_OFFSET;

        if (isRefStore) {
            CVMRMResource *scratch;
            int scratchRegID;
            /* If we get here, we're dealing with a obj ref store.  Hence,
               we will need to fully evaluate the effective address into a
               register and use a register offset type of memspec.  The fully
               evaluated effective address will be used later by GC card
               table marking code.
            */
            scratch = CVMRMgetResource(CVMRM_INT_REGS(con),
				       CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
            sinfo->slotAddrReg = scratch;
	    
            scratchRegID = CVMRMgetRegisterNumber(scratch);
            CVMCPUemitBinaryALUConstant(con, CVMCPU_ADD_OPCODE, scratchRegID,
					arrayRegID, offset, CVMJIT_NOSETCC);
            sinfo->baseRegID = scratchRegID;
            sinfo->slotAddrOffset = 0;
        } else {
            if (CVMCPUmemspecIsEncodableAsOpcodeSpecificImmediate(opcode,
                                                                  offset)) {
                /* If we get here, then we can fold the indexing and array
                   header offset all into a single immediate offset: */
                sinfo->slotAddrOffset = offset;
            } else {
                /* If we get here, then the indexing and array header offset
                   is too big to fit into a single immediate offset.  Instead,
                   we bind the constant index to a register and go emit the
                   code for handling a non-const index.

                   NOTE: Chances are that the index was already loaded
                   previously to do a bounds check.  Hence, we use
                   CVMRMbindResourceForConstant32() to take advantage of this
                   instead of attempting to load it explicitly.
                */

                sinfo->hasConstIndex = CVM_FALSE;
                sinfo->indexReg =
                    CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con),
						   sinfo->index);
                goto doNonConstIndex;
            }
	    sinfo->baseRegID = arrayRegID;
	    sinfo->slotAddrReg = array;
            /* incorporated into sinfo. Increment ref count */
	    CVMRMincRefCount(con, array);
        }

    } else {
	/* The index is not a constant but is in a register */
        CVMRMResource *scratch;
        int indexRegID;
        int scratchRegID;
doNonConstIndex:

        /* If we get here, then the index is not constant.  We will first
           compute an arrayIndex:
              subscript = base + (index << shiftAmount)

           Then we use an immediate offset type memspec to add the array
           header offset to the subscript to compute the actual effective
           address we want.
        */
        scratch = CVMRMgetResource(CVMRM_INT_REGS(con),
				   CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
        sinfo->slotAddrReg = scratch;
	
        scratchRegID = CVMRMgetRegisterNumber(scratch);
        CVMRMpinResource(CVMRM_INT_REGS(con), sinfo->indexReg,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        indexRegID = CVMRMgetRegisterNumber(sinfo->indexReg);

        /* scratchReg = baseReg + (indexReg << #shiftAmount): */
        CVMCPUemitComputeAddressOfArrayElement(con, CVMCPU_ADD_OPCODE,
            scratchRegID, arrayRegID, indexRegID, CVMCPU_SLL_OPCODE,
            sinfo->shiftAmount);

        if (isRefStore) {
            /* If we get here, we're dealing with a obj ref store.  Hence,
               we will need to fully evaluate the effective address into a
               register and use a register offset type of memspec.  The fully
               evaluated effective address will be used later by GC card
               table marking code.
            */
            CVMCPUemitBinaryALUConstant(con, CVMCPU_ADD_OPCODE,
                scratchRegID, scratchRegID, ARRAY_DATA_OFFSET, CVMJIT_NOSETCC);
            sinfo->slotAddrOffset = 0;
        } else {
            CVMassert(CVMCPUmemspecIsEncodableAsOpcodeSpecificImmediate(
                        opcode, ARRAY_DATA_OFFSET));
	    sinfo->slotAddrOffset = ARRAY_DATA_OFFSET;
        }
        sinfo->baseRegID = scratchRegID;
    }
}

/* Purpose: Gets the baseRegID that was setup by setupScaledIndex(). */
#define getScaledIndexBaseRegID(sinfo)      ((sinfo)->baseRegID)

/* Purpose: unpin resource components of a ScaledIndexInfo */
static void
unpinScaledIndex(CVMJITCompilationContext *con, ScaledIndexInfo *sinfo)
{
    /* We are done with this ScaledIndexInfo. Get rid of it and
       its associated componenets */
    if (!sinfo->hasConstIndex) {
	CVMRMunpinResource(CVMRM_INT_REGS(con), sinfo->indexReg);
    }
    if (sinfo->slotAddrReg != NULL) {
	CVMRMunpinResource(CVMRM_INT_REGS(con), sinfo->slotAddrReg);
    }
    if (sinfo->arrayBaseReg != NULL) {
	CVMRMunpinResource(CVMRM_INT_REGS(con), sinfo->arrayBaseReg);
    }
}

/* Purpose: unpin resource components of a ScaledIndexInfo */
static void
persistAndUnpinScaledIndex(CVMJITCompilationContext *con, 
			   ScaledIndexInfo *sinfo)
{
    /* We are done with this ScaledIndexInfo. Get rid of it and
       its associated componenets */
    if (!sinfo->hasConstIndex) {
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), sinfo->indexReg,
				    NULL);
    }
    if (sinfo->slotAddrReg != NULL) {
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), sinfo->slotAddrReg,
				    NULL);
    }
    if (sinfo->arrayBaseReg != NULL) {
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), sinfo->arrayBaseReg,
				    NULL);
    }
}

/* Purpose: Releases any scratch that may have been pinned by
            setupScaledIndex(). */
static void
relinquishScaledIndex(CVMJITCompilationContext *con, ScaledIndexInfo *sinfo)
{
    /* We should never decrement below 0. */
    CVMassert(CVMJITidentityGetDecorationRefCount(con, &sinfo->dec) > 0); 
    CVMJITidentityDecrementDecorationRefCount(con, &sinfo->dec);
    if (CVMJITidentityGetDecorationRefCount(con, &sinfo->dec) <= 0) {
	/* We are done with this ScaledIndexInfo. Get rid of it and
	   its associated componenets */
	if (!sinfo->hasConstIndex) {
	    CVMRMrelinquishResource(CVMRM_INT_REGS(con), sinfo->indexReg);
	}
	if (sinfo->slotAddrReg != NULL) {
	    CVMRMrelinquishResource(CVMRM_INT_REGS(con), sinfo->slotAddrReg);
	}
	if (sinfo->arrayBaseReg != NULL) {
	    CVMRMrelinquishResource(CVMRM_INT_REGS(con), sinfo->arrayBaseReg);
	}
    } else {
	/* Merely unpin everything */
	unpinScaledIndex(con, sinfo);
    }
}

/* Purpose: Emits code to do a load of a Java array element. */
static void
indexedLoad(
    CVMJITCompilationContext* con,
    CVMJITRMContext* rc,
    CVMJITIRNodePtr fetchNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource *dest;
    ScaledIndexInfo *sinfo = popScaledIndexInfo(con);
    CVMRMResource *arrayObj = popResource(con);
    CVMUint16 typeId;
    CVMJITIRNode* indexNode;
    const ArrayElemInfo* elemInfo;
    int size;
    int shiftAmount;
    int opcode;
#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    CVMJITIRNode* outOfBoundsCheckNode = NULL;
#endif

    CVMassert(CVMJITirnodeIsFetch(fetchNode));
    indexNode = CVMJITirnodeGetLeftSubtree(fetchNode);
    
    /* Make sure we are getting the correct tree shape */
    CVMassert(CVMJITirnodeIsIndex(indexNode));
    
    typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
    elemInfo = &typeidToArrayElemInfo[typeId];
    
    size = elemInfo->size;
    shiftAmount = elemInfo->shiftAmount;
#ifdef CVM_JIT_USE_FP_HARDWARE
    opcode = (rc == CVMRM_INT_REGS(con)) ? elemInfo->loadOpcode
				    : elemInfo->floatLoadOpcode;
#else
    opcode = elemInfo->loadOpcode;
#endif
    CVMassert(opcode != CVM_ILLEGAL_OPCODE);

    CVMassert(sinfo->shiftAmount == -1);
    sinfo->shiftAmount = shiftAmount;
    
    CVMJITprintCodegenComment((
        "Do load(arrayObj, index) (elem type=%d,%c):", typeId,
	typeId == CVMJIT_TYPEID_UBYTE ?
	   'b' : CVMbasicTypeSignatures[CVMterseTypeBasicTypes[typeId]]));

    dest = CVMRMgetResource(rc, target, avoid, size);
    CVMRMpinResource(CVMRM_INT_REGS(con), arrayObj,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

    setupScaledIndex(con, opcode, arrayObj, sinfo, CVM_FALSE);
    CVMJITcsSetGetArrayInstruction(con);

/* IAI - 12 */
#if defined(IAI_CS_EXCEPTION_ENHANCEMENT) && !defined(IAI_CS_EXCEPTION_ENHANCEMENT2) 
    CVMJITcsSetExceptionInstruction(con);
#endif
/* IAI - 12 */

#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    indexNode = CVMJITirnodeValueOf(indexNode);
    if(CVMJITirnodeIsBinaryNode(indexNode)) {
        outOfBoundsCheckNode = CVMJITirnodeValueOf(
              CVMJITirnodeGetRightSubtree(indexNode));
    }
    
    /*
     * The following code deals with the case which the bounds-check sharing 
     * between different node.
     * From the code scheduling point of view, the more formal way is to emit all the
     * array fetch/store instruction in conditional style.
     * There are two possible solution,
     * 1) Share the resource directly
     * 2) Setup the dependency rule between array fetch/store and bounds check instructions
     */
    if(outOfBoundsCheckNode && CVMJITirnodeIsBoundsCheckNode(outOfBoundsCheckNode)
               && !sinfo->isIDENTITYOutofBoundsCheck) {
         CVMJITprintCodegenComment(("Schedule conditinal load"));
         CVMCPUemitMemoryReferenceImmediateConditional(con, opcode, 
         CVMRMgetRegisterNumber(dest),
         getScaledIndexBaseRegID(sinfo), 
         sinfo->slotAddrOffset, CVMCPU_COND_HI);
    } else {
         CVMCPUemitMemoryReferenceImmediate(con, opcode, 
         CVMRMgetRegisterNumber(dest),
         getScaledIndexBaseRegID(sinfo), 
         sinfo->slotAddrOffset);
    }
#else
    CVMCPUemitMemoryReferenceImmediate(con, opcode, 
                                       CVMRMgetRegisterNumber(dest),
                                       getScaledIndexBaseRegID(sinfo), 
                                       sinfo->slotAddrOffset);
#endif

    
    /* We should not have set arrayObj in this sinfo instance. arrayObj
       is relinquished separately below */
    CVMassert(sinfo->arrayBaseReg == NULL);
    
    relinquishScaledIndex(con, sinfo);

    CVMRMoccupyAndUnpinResource(rc, dest, fetchNode);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), arrayObj);
    pushResource(con, dest);
}

/* Purpose: Emits code to do a load of a Java array element. */
static void
indexedAddr(
    CVMJITCompilationContext* con,
    CVMJITIRNodePtr indexNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    ScaledIndexInfo *sinfo = popScaledIndexInfo(con);
    CVMRMResource *arrayObj = popResource(con);
    CVMUint16 typeId;
    const ArrayElemInfo* elemInfo;
    int shiftAmount;
    int opcode;
    CVMBool canBeUsedInStore;

    /* Make sure we are getting the correct tree shape */
    CVMassert(CVMJITirnodeIsIndex(indexNode));
    
    typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
    elemInfo = &typeidToArrayElemInfo[typeId];
    
    shiftAmount = elemInfo->shiftAmount;
    opcode = elemInfo->loadOpcode;

    CVMassert(sinfo->shiftAmount == -1);
    sinfo->shiftAmount = shiftAmount;
    
    CVMJITprintCodegenComment((
        "Compute slot &arr[index] (elem type=%c):",
	CVMbasicTypeSignatures[CVMterseTypeBasicTypes[typeId]]));
    CVMRMpinResource(CVMRM_INT_REGS(con), arrayObj,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

    canBeUsedInStore = CVMJITirnodeBinaryNodeIs(indexNode, WRITE);
    
    /*
     * When we are setting up the scaled index, we don't know if it's
     * for a load or a store. So we conservatively set isRef, even though
     * we might not need the extra computation for a precise slot address
     * in case of a reference read (and not a write)
     */
    setupScaledIndex(con, opcode, arrayObj, sinfo,
		     canBeUsedInStore && elemInfo->isRef /* isRefStore */);

    /*
     * Remember some more in the sinfo
     */
    sinfo->arrayBaseReg = arrayObj;
    /* incorporated into sinfo. Increment ref count */
    CVMRMincRefCount(con, arrayObj);
    
    sinfo->elemInfo = elemInfo;

    /*
     * Make sure the sinfo resource components persist across rules
     */
    persistAndUnpinScaledIndex(con, sinfo);
    
    /*
     * And propagate the scaledIndex part further. sinfo->slotAddrReg
     * contains the slot address.
     */
    pushScaledIndexInfo(con, sinfo);

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), arrayObj);
}

#define ARRAY_LOAD_SYNTHESIS(con, p)   L_BINARY_SYNTHESIS((con), (p))
#define ARRAY_LOAD_INHERITANCE(con, p) L_BINARY_INHERITANCE((con), (p))

/* Purpose: reads a slot from an array */
static void 
fetchArraySlot(CVMJITCompilationContext *con,
	       CVMJITRMContext* rc,
	       CVMJITIRNodePtr fetchNode,
	       CVMRMregset target, CVMRMregset avoid)
{
    CVMRMResource *dest;
    ScaledIndexInfo *sinfo = popScaledIndexInfo(con);
    CVMRMResource *arraySlot = sinfo->slotAddrReg;
    const ArrayElemInfo* elemInfo = sinfo->elemInfo;
    int opcode;
#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    CVMJITIRNode* indexNode;
    CVMJITIRNode* outOfBoundsCheckNode = NULL;
#endif

    CVMassert(CVMJITirnodeIsFetch(fetchNode));
    
#ifdef CVM_JIT_USE_FP_HARDWARE
    opcode = (rc == CVMRM_INT_REGS(con)) ? elemInfo->loadOpcode
				    : elemInfo->floatLoadOpcode;
#else
    opcode = elemInfo->loadOpcode;
#endif
    CVMassert(opcode != CVM_ILLEGAL_OPCODE);

    dest = CVMRMgetResource(rc, target, avoid, elemInfo->size);
    CVMRMpinResource(CVMRM_INT_REGS(con), arraySlot,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

    CVMJITcsSetGetArrayInstruction(con);

/* IAI - 12 */
#if defined(IAI_CS_EXCEPTION_ENHANCEMENT) && !defined(IAI_CS_EXCEPTION_ENHANCEMENT2) 
    CVMJITcsSetExceptionInstruction(con);
#endif
/* IAI - 12 */

#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    indexNode = CVMJITirnodeValueOf(CVMJITirnodeGetLeftSubtree(
                    CVMJITirnodeValueOf(fetchNode)));

    if(CVMJITirnodeIsBinaryNode(indexNode)) {
        outOfBoundsCheckNode = CVMJITirnodeValueOf(
                    CVMJITirnodeGetRightSubtree(indexNode));
    }
    
    if(outOfBoundsCheckNode && outOfBoundsCheckNode->tag == BOUNDS_CHECK 
               && !sinfo->isIDENTITYOutofBoundsCheck) {
        CVMJITprintCodegenComment(("Schedule conditinal load"));
        CVMCPUemitMemoryReferenceImmediateConditional(con, opcode,
  				       CVMRMgetRegisterNumber(dest), 
  				       CVMRMgetRegisterNumber(arraySlot),
  				       sinfo->slotAddrOffset, CVMCPU_COND_HI);
    } else {
        CVMCPUemitMemoryReferenceImmediate(con, opcode,
   				       CVMRMgetRegisterNumber(dest), 
   				       CVMRMgetRegisterNumber(arraySlot),
   				       sinfo->slotAddrOffset);
    }
#else
    CVMCPUemitMemoryReferenceImmediate(con, opcode,
				       CVMRMgetRegisterNumber(dest), 
				       CVMRMgetRegisterNumber(arraySlot),
				       sinfo->slotAddrOffset);
#endif				       

    CVMRMoccupyAndUnpinResource(rc, dest, fetchNode);
    relinquishScaledIndex(con, sinfo);
    pushResource(con, dest);
}

#line 4163 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Fetches an instance field from an object. */
static void fetchField(CVMJITCompilationContext *con,
		       CVMJITRMContext* rc,
                       CVMJITIRNodePtr thisNode,
                       CVMRMregset target, CVMRMregset avoid,
                       CVMInt32 opcode, int fieldSize,
                       CVMBool isVolatile)
{
    CVMCPUMemSpec *fieldOffset = popMemSpec(con);
    CVMRMResource *objPtr = popResource(con);
    CVMRMResource *dest;
    dest = CVMRMgetResource(rc, target, avoid, fieldSize);
    CVMRMpinResource(CVMRM_INT_REGS(con), objPtr,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMJITaddCodegenComment((con, "= getfield(obj, fieldIdx);"));
    CVMCPUmemspecPinResource(CVMRM_INT_REGS(con), fieldOffset,
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMJITcsSetGetFieldInstruction(con);
    CVMJITcsSetExceptionInstruction(con);

    CVMCPUemitMemoryReference(con, opcode,
        CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(objPtr),
        CVMCPUmemspecGetToken(con, fieldOffset));
    if (isVolatile) {
        CVMCPUemitMemBarAcquire(con);
    }

    CVMCPUmemspecRelinquishResource(CVMRM_INT_REGS(con), fieldOffset);

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), objPtr);
    CVMRMoccupyAndUnpinResource(rc, dest, thisNode);
    pushResource(con, dest);
}

#define GETFIELD_SYNTHESIS(con, p)   L_BINARY_SYNTHESIS((con), (p))
#define GETFIELD_INHERITANCE(con, p) L_BINARY_INHERITANCE((con), (p))

#line 4262 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


#if (CVM_GCCHOICE == CVM_GC_GENERATIONAL)
/*
 * Emit barrier for write of srcReg into objReg+dataOffset=destAddr
 */
static void
emitMarkCardTable(CVMJITCompilationContext *con, 
		  int objAddrReg, int destAddrReg)
{
    /* Need three registers here. One to hold zero, the other
     * to hold the card table virtual base, and a third
     * to hold the slot address being written into:
     */

    CVMCodegenComment *comment;
    CVMRMResource* cardtableReg;
    CVMRMResource* markReg;
    int            markRegID;
#ifdef CVM_SEGMENTED_HEAP
    /* t0 and t1 will eventually become cardTable and zero registers */
    CVMRMResource* t0;
    CVMRMResource* t1;
    int fixupPC; /* To patch the conditional barrier branch */
    CVMUint32 lowerBound  = (CVMUint32)CVMglobals.youngGenLowerBound;
    CVMUint32 higherBound = (CVMUint32)CVMglobals.youngGenUpperBound;
#endif
#ifdef CVM_JAVASE_CLASS_HAS_REF_FIELD
    int fixupPC1;
#endif

    CVMJITpopCodegenComment(con, comment);

    /* In SE 1.5 and later version, the java.lang.Class has non-static
     * reference fields. If the Class instance is ROMized, we should 
     * not mark the card table.
     */
#ifdef CVM_JAVASE_CLASS_HAS_REF_FIELD
    {
        CVMRMResource *tmpRes = CVMRMgetResource(CVMRM_INT_REGS(con),
                          CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
        int tmpReg = CVMRMgetRegisterNumber(tmpRes);
        /* load object hdr.clas */
        CVMJITaddCodegenComment((con, "get obj.hdr.clas"));
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
                                       tmpReg, objAddrReg,
                                       CVMoffsetof(CVMObjectHeader, clas));
        /* check if the object is in ROM */
        CVMJITaddCodegenComment((con, "check if object is in ROM"));
        CVMCPUemitBinaryALUConstant(con, CVMCPU_AND_OPCODE,
                                    tmpReg, tmpReg,
                                    CVM_OBJECT_IN_ROM_MASK,
                                    CVMJIT_NOSETCC);
        CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_NE,
                                  tmpReg, 0);
        /* skip mark cardtable if the object is in ROM */
        CVMJITaddCodegenComment((con, "skip mark cardtable if it's in ROM"));
        CVMCPUemitBranch(con, 0, CVMCPU_COND_NE);
#ifdef CVMCPU_HAS_DELAY_SLOT
        fixupPC1 = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
        fixupPC1 = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), tmpRes);
    }
#endif

#ifdef CVM_SEGMENTED_HEAP
    CVMJITprintCodegenComment(("Check if barrier required:"));

    t0 = CVMRMgetResource(CVMRM_INT_REGS(con), CVMRM_ANY_SET, 
			  CVMRM_EMPTY_SET, 1);
    /* t0 = #lowerBound */
    CVMJITaddCodegenComment((con, "CVMglobals.youngGenLowerBound"));
    CVMJITsetSymbolName((con, "CVMglobals.youngGenLowerBound"));
    CVMCPUemitLoadConstant(con, CVMRMgetRegisterNumber(t0), lowerBound);
    t1 = CVMRMgetResource(CVMRM_INT_REGS(con), CVMRM_ANY_SET, 
			  CVMRM_EMPTY_SET, 1);
    /* t1 = objRef - t0 */
    CVMCPUemitBinaryALURegister(con, CVMCPU_SUB_OPCODE, 
				CVMRMgetRegisterNumber(t1),
				objAddrReg, 
				CVMRMgetRegisterNumber(t0),
				CVMJIT_NOSETCC);
    /* Compare(t1, #higherBound - #lowerBound) */
    CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_LO,
			      CVMRMgetRegisterNumber(t1),
			      higherBound - lowerBound);

    CVMJITaddCodegenComment((con, "br skipBarrier if less than (hi-lo)"));
    /* Branch to skipBarrier if unsigned less than */
    CVMCPUemitBranch(con, 0, CVMCPU_COND_LO);
#ifdef CVMCPU_HAS_DELAY_SLOT
    fixupPC = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
    fixupPC = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif

#ifdef IAI_CODE_SCHEDULER_SCORE_BOARD
    fixupPC = CVMJITcbufGetLogicalInstructionPC(con);
#endif /* IAI_CODE_SCHEDULER_SCORE_BOARD */
    
    CVMJITcsSetEmitInPlace(con);
    con->inConditionalCode = CVM_TRUE;
    CVMJITprintCodegenComment(("Will do barrier:"));
    CVMJITprintCodegenComment(("Compute segment addr = BIC(obj, 0xffff)"));

    /* t1 = BIC(objAddr, 0xffff) to compute the segment address */
    /*
     * WARNING: don't let CVMCPUemitBinaryALUConstant() load a
     * large constant into a register. This will change the
     * regman state, which is a no-no in conditionally executed
     * code. Instead, manually load the large constant here
     * using CVMCPUemitLoadConstant(), which won't affect the
     * regman state.
     */
    CVMCPUemitBinaryALUConstant(con, CVMCPU_BIC_OPCODE, 
				CVMRMgetRegisterNumber(t1),
				objAddrReg,
				SEGMENT_ALIGNMENT - 1, CVMJIT_NOSETCC);
	
    CVMJITaddCodegenComment((con, "seg->cardTableVirtualBase"));
    /* And from the segment address, the barrier address: */
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
       CVMRMgetRegisterNumber(t1),
       CVMRMgetRegisterNumber(t1),
       GC_SEGMENT_CTV_OFFSET);
    cardtableReg = t1;

    /* Now we can re-use t0 to store the CARD_DIRTY_BYTE mark value: */
    markReg = t0;
    markRegID = CVMRMgetRegisterNumber(markReg);
    CVMCPUemitLoadConstant(con, markRegID, (CVMInt32)CARD_DIRTY_BYTE);
    CVMJITcsClearEmitInPlace(con);
#else /* !CVM_SEGMENTED_HEAP case below */

    /* Easy to compute for the non-segmented case */
#if defined(CVM_AOT)
    if (CVMglobals.jit.isPrecompiling) {
        /*
         * We can't use the cardTableVirtualBase
         * as a constant since the cardtable is dynamically
         * allocated. Emit code to access the cardTableVirtualBase
         * indirectly.
         */
        cardtableReg = CVMRMgetResource(CVMRM_INT_REGS(con),
                                    CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
        CVMCPUemitLoadConstant(con, CVMRMgetRegisterNumber(cardtableReg),
            (CVMInt32)(&CVMglobals.gc.cardTableVirtualBase));
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
            CVMRMgetRegisterNumber(cardtableReg),
            CVMRMgetRegisterNumber(cardtableReg), 0);
    } else
#endif
   {
       CVMJITsetSymbolName((con, "cardTableVirtualBase"));
        cardtableReg =
            CVMRMgetResourceForConstant32(CVMRM_INT_REGS(con),
	        CVMRM_ANY_SET, CVMRM_EMPTY_SET,
                (CVMInt32)CVMglobals.gc.cardTableVirtualBase);
        /* Make sure we get these constants right: */
        CVMassert(CVMRMgetRegisterNumber(cardtableReg) != -1);
    }

    CVMJITsetSymbolName((con, "CARD_DIRTY_BYTE"));
    markReg = CVMRMgetResourceForConstant32(CVMRM_INT_REGS(con),
	          CVMRM_ANY_SET, CVMRM_EMPTY_SET, (CVMInt32)CARD_DIRTY_BYTE);
    markRegID = CVMRMgetRegisterNumber(markReg);

#endif /* !CVM_SEGMENTED_HEAP */

    CVMassert(markRegID != -1);

    /* Now we are ready to write into card table, as well as
       perform the actual write: */

    /* Do card table write: */
    CVMJITaddCodegenComment((con, "mark card table"));
    CVMCPUemitArrayElementReference(con, CVMCPU_STR8_OPCODE,
        markRegID, CVMRMgetRegisterNumber(cardtableReg),
        destAddrReg, CVMCPU_SRL_OPCODE, CVM_GENGC_CARD_SHIFT);

#ifdef CVM_SEGMENTED_HEAP
    CVMtraceJITCodegen(("\t\tskipBarrier:\n"));
    CVMJITfixupAddress(con, fixupPC, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);
    con->inConditionalCode = CVM_FALSE;
#endif

#ifdef CVM_JAVASE_CLASS_HAS_REF_FIELD
    CVMJITfixupAddress(con, fixupPC1,  CVMJITcbufGetLogicalPC(con),
                       CVMJIT_COND_BRANCH_ADDRESS_MODE);
#endif

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), markReg);
    
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), cardtableReg);

    CVMJITpushCodegenComment(con, comment);
}

/*
 * Given an evaluated FIELDREF's components on the stack, return the
 * object address portion
 */
static CVMRMResource*
extractObjectAddrFromFieldRef(CVMJITCompilationContext* con)
{
    CVMCPUALURhs* rhs = popALURhs(con);
    CVMRMResource* lhs = popResource(con);
    pushResource(con, lhs);
    pushALURhs(con, rhs);
    return lhs;
}
 
#else
#define emitMarkCardTable(con, destAddrReg)

/* Currently, the JIT code assumes that our GC choice is generational,
   since we have to emit a card table write barrier. So enforce this
   using an cpp error if the GC choice is not as expected. */
#error The dynamic compiler only works with generational GC
#endif

#define PUTFIELD_SYNTHESIS(con, thisNode) \
    L_BINARY_R_UNARY_SYNTHESIS((con), (thisNode))
#define PUTFIELD_INHERITANCE(con, thisNode) \
    L_BINARY_R_UNARY_INHERITANCE((con), (thisNode))

#line 4542 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Sets a value to an instance field of an object. */
static void setField(CVMJITCompilationContext *con,
		     CVMJITRMContext* rc,
                     CVMInt32 opcode,
                     CVMBool isVolatile)
{
    CVMRMResource *src = popResource(con);
    CVMCPUMemSpec *fieldOffset = popMemSpec(con);
    CVMRMResource *objPtr = popResource(con);

    if (isVolatile) {
        CVMCPUemitMemBarRelease(con);
    }

    CVMRMpinResource(CVMRM_INT_REGS(con), objPtr,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(rc, src,    rc->anySet, CVMRM_EMPTY_SET);

    CVMCPUmemspecPinResource(CVMRM_INT_REGS(con), fieldOffset,
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMJITcsSetPutFieldInstruction(con);
    CVMJITcsSetExceptionInstruction(con);

    CVMCPUemitMemoryReference(con, opcode,
        CVMRMgetRegisterNumber(src), CVMRMgetRegisterNumber(objPtr),
        CVMCPUmemspecGetToken(con, fieldOffset));
    if (isVolatile) {
        CVMCPUemitMemBar(con);
    }

    CVMCPUmemspecRelinquishResource(CVMRM_INT_REGS(con), fieldOffset);

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), objPtr);
    CVMRMrelinquishResource(rc, src);
}

#line 4658 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Emits code to do */
static void
emitArrayAssignabilityCheck(CVMJITCompilationContext* con,
                            CVMRMResource *aref, CVMRMResource *src)
{
    CVMRMResource* arrClass; /* Scratch register */
    CVMRMResource* rhsClass; /* Scratch register */
    int srcRegID;
    CVMCPUCondCode condCode;
#if !defined(CVMCPU_HAS_CONDITIONAL_LOADSTORE_INSTRUCTIONS) || !defined(CVMCPU_HAS_CONDITIONAL_CALL_INSTRUCTIONS)
    int fixupPC;
#endif

    /* This is an 'aastore'. Do the assignability check */
    CVMJITaddCodegenComment((con, "aastore assignability check"));
    arrClass = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
				        CVMCPU_ARG3_REG, 1);
    rhsClass = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_ARG4_REG, 1);

    /* Get the rhs of the assignment and the array reference
       into registers. Do not clobber ARG1-ARG4 */
    CVMRMpinResourceStrict(CVMRM_INT_REGS(con), src,
			   CVMRM_SAFE_SET, ~CVMRM_SAFE_SET);
    srcRegID = CVMRMgetRegisterNumber(src);
    CVMRMpinResourceStrict(CVMRM_INT_REGS(con), aref,
			   CVMRM_SAFE_SET, ~CVMRM_SAFE_SET);

    /*
     * We might eventually do a call. Assume we will
     */
    CVMRMminorSpill(con, ARG3|ARG4);

    /* OK, first off, check for the rhs being NULL. NULL is assignable
       to any array of references. */
    /* Compare to NULL, and skip assignability check if NULL */
    CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
		      srcRegID, CVMCPUALURhsTokenConstZero);

    /*
     * Call the helper after loading the cb of the array class and
     * rhs class into ARG3 and ARG4 (with the lower bits still set).
     * This is all done conditionally based on whether or not
     * the object is NULL.
     */

#if !defined(CVMCPU_HAS_CONDITIONAL_LOADSTORE_INSTRUCTIONS) || !defined(CVMCPU_HAS_CONDITIONAL_CALL_INSTRUCTIONS)
    condCode = CVMCPU_COND_AL;
    CVMJITaddCodegenComment((con, "br .skip"));
    CVMCPUemitBranch(con, 0, CVMCPU_COND_EQ);
#ifdef CVMCPU_HAS_DELAY_SLOT
    fixupPC = CVMJITcbufGetLogicalPC(con) - 2 * CVMCPU_INSTRUCTION_SIZE;
#else
    fixupPC = CVMJITcbufGetLogicalPC(con) - 1 * CVMCPU_INSTRUCTION_SIZE;
#endif

#ifdef IAI_CODE_SCHEDULER_SCORE_BOARD
    fixupPC = CVMJITcbufGetLogicalInstructionPC(con);
#endif /* IAI_CODE_SCHEDULER_SCORE_BOARD */
    con->inConditionalCode = CVM_TRUE;
    /*
     * The instructions which will be emitted by the following code will
     * include a compare and a branch instruction on platforms that don't
     * support both a conditional load and a conditional call.  For code
     * scheduling, it is quite hard to know the dependency of
     * the compare instruction and the instructions which follow that
     * compare instruction. For example:
     * Block 7:
     * ...
     * cmp a0, 0
     * bne l1:
     * mov a1, 0
     * l1:
     * str a1, local0
     * ...
     * All of this code is in same block, and a1 is not a register which
     * flows between blocks. It is hard to understand the dependency
     * among cmp, bne and mov instructions. The solution is to turn off code
     * scheduling under such case.
     */
    CVMJITcsSetEmitInPlace(con);
#else
    condCode = CVMCPU_COND_NE;
#endif

    /* LDRNE ARG3, [robj] */
    CVMJITaddCodegenComment((con, "Get the array class"));
    CVMCPUemitMemoryReferenceImmediateConditional(con, CVMCPU_LDR32_OPCODE,
        CVMCPU_ARG3_REG, CVMRMgetRegisterNumber(aref), OBJECT_CB_OFFSET,
        condCode);

    /* LDRNE ARG4, [src] */
    CVMJITaddCodegenComment((con, "Get the rhs class"));
    CVMCPUemitMemoryReferenceImmediateConditional(con, CVMCPU_LDR32_OPCODE,
        CVMCPU_ARG4_REG, srcRegID, OBJECT_CB_OFFSET, condCode);

    /* do helper call */
    CVMJITaddCodegenComment((con, "call %s", 
                             "CVMCCMruntimeCheckArrayAssignableGlue"));
    CVMJITsetSymbolName((con,"CVMCCMruntimeCheckArrayAssignableGlue"));
    CVMJITstatsRecordInc(con,
        CVMJIT_STATS_CVMCCMruntimeCheckArrayAssignable);
    CVMCPUemitAbsoluteCallConditional(con,
        (void*)CVMCCMruntimeCheckArrayAssignableGlue,
        CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH, condCode);
    CVMJITcsBeginBlock(con);
    CVMJITcaptureStackmap(con, 0);

#if !defined(CVMCPU_HAS_CONDITIONAL_LOADSTORE_INSTRUCTIONS) || !defined(CVMCPU_HAS_CONDITIONAL_CALL_INSTRUCTIONS)
    CVMtraceJITCodegen((".skip"));
    CVMJITfixupAddress(con, fixupPC, CVMJITcbufGetLogicalPC(con),
		       CVMJIT_COND_BRANCH_ADDRESS_MODE);
    con->inConditionalCode = CVM_FALSE;
#endif

/* IAI-07 */
#if !defined(CVMCPU_HAS_CONDITIONAL_LOADSTORE_INSTRUCTIONS) || !defined(CVMCPU_HAS_CONDITIONAL_CALL_INSTRUCTIONS)
    CVMJITcsClearEmitInPlace(con);
#endif

    /* And we are at done */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), arrClass);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhsClass);

    CVMJITprintCodegenComment(("aastore assignment check done"));
    CVMRMunpinResource(CVMRM_INT_REGS(con), aref);
    CVMRMunpinResource(CVMRM_INT_REGS(con), src);
}

/* Purpose: Emits code to do a store of a Java array element. */
static void
indexedStore(
    CVMJITCompilationContext* con,
    CVMJITRMContext* rc,
    CVMJITIRNodePtr thisNode)
{
    CVMBool isRefStore;
    CVMRMResource *src  = popResource(con);
    int srcRegID, arrayRegID;
    ScaledIndexInfo *sinfo = popScaledIndexInfo(con);
    CVMRMResource *aref = popResource(con);

    CVMUint16 typeId;
    CVMJITIRNode* indexNode;
    const ArrayElemInfo* elemInfo;
    int size;
    int shiftAmount;
    int opcode;

    indexNode = CVMJITirnodeGetLeftSubtree(thisNode);
    
    /* Make sure we are getting the correct tree shape */
    CVMassert(CVMJITirnodeIsIndex(indexNode));
    
    typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
    elemInfo = &typeidToArrayElemInfo[typeId];
    
    size = elemInfo->size;
    shiftAmount = elemInfo->shiftAmount;
#ifdef CVM_JIT_USE_FP_HARDWARE
    opcode = (rc == CVMRM_INT_REGS(con)) ? elemInfo->storeOpcode
				    : elemInfo->floatStoreOpcode;
#else
    opcode = elemInfo->storeOpcode;
#endif
    CVMassert(opcode != CVM_ILLEGAL_OPCODE);

    CVMassert(sinfo->shiftAmount == -1);
    sinfo->shiftAmount = shiftAmount;
    
    CVMJITprintCodegenComment((
        "Do store(arrayObj, index) (elem type=%c):",
	CVMbasicTypeSignatures[CVMterseTypeBasicTypes[typeId]]));

    CVMassert(elemInfo->isRef == CVMJITirnodeIsReferenceType(thisNode));
    isRefStore = CVMJITirnodeIsReferenceType(thisNode);
    if (isRefStore) {
        emitArrayAssignabilityCheck(con, aref, src);
    }

    CVMRMpinResource(rc, src,  rc->anySet, CVMRM_EMPTY_SET);
    CVMRMpinResource(CVMRM_INT_REGS(con), aref, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    srcRegID = CVMRMgetRegisterNumber(src);
    arrayRegID = CVMRMgetRegisterNumber(aref);

    {
        int baseRegID;
        setupScaledIndex(con, opcode, aref, sinfo, isRefStore);
        baseRegID = getScaledIndexBaseRegID(sinfo);
        CVMJITcsSetPutArrayInstruction(con);
        CVMCPUemitMemoryReferenceImmediate(con, opcode, srcRegID, baseRegID,
					   sinfo->slotAddrOffset);
        if (isRefStore) {
	    CVMassert(sinfo->slotAddrOffset == 0);
            emitMarkCardTable(con, arrayRegID, baseRegID);
        }
	/* We should not have set arrayObj in this sinfo instance. arrayObj
	   is relinquished separately below */
	CVMassert(sinfo->arrayBaseReg == NULL);
    
        relinquishScaledIndex(con, sinfo);
    }

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), aref);
    CVMRMrelinquishResource(rc, src);
}

/* Purpose: reads a slot from an array */
static void 
storeArraySlot(CVMJITCompilationContext *con,
	       CVMJITRMContext* rc,
	       CVMJITIRNodePtr thisNode)
{
    CVMRMResource* rhs = popResource(con);
    ScaledIndexInfo *sinfo = popScaledIndexInfo(con);
    CVMRMResource *arraySlot = sinfo->slotAddrReg;
    CVMRMResource *arrayRef;
    int opcode;
    const ArrayElemInfo* elemInfo = sinfo->elemInfo;
    
    arrayRef = sinfo->arrayBaseReg;
    if (elemInfo->isRef) {
	emitArrayAssignabilityCheck(con, arrayRef, rhs);
    }
#ifdef CVM_JIT_USE_FP_HARDWARE
    opcode = (rc == CVMRM_INT_REGS(con)) ? elemInfo->storeOpcode
				    : elemInfo->floatStoreOpcode;
#else
    opcode = elemInfo->storeOpcode;
#endif
    CVMassert(opcode != CVM_ILLEGAL_OPCODE);

    CVMRMpinResource(CVMRM_INT_REGS(con), arraySlot,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(rc, rhs, rc->anySet, CVMRM_EMPTY_SET);
    CVMJITcsSetPutArrayInstruction(con);
    CVMCPUemitMemoryReferenceImmediate(con, opcode,
				       CVMRMgetRegisterNumber(rhs),
				       CVMRMgetRegisterNumber(arraySlot),
				       sinfo->slotAddrOffset);
    if (elemInfo->isRef) {
	CVMassert(sinfo->slotAddrOffset == 0);
	CVMRMpinResource(CVMRM_INT_REGS(con), arrayRef,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	emitMarkCardTable(con, 
			  CVMRMgetRegisterNumber(arrayRef),
			  CVMRMgetRegisterNumber(arraySlot));
	CVMRMunpinResource(CVMRM_INT_REGS(con), arrayRef);
    }
    
    relinquishScaledIndex(con, sinfo);
    CVMRMrelinquishResource(rc, rhs);
}

#define ARRAY_STORE_SYNTHESIS(con, thisNode) \
    L_BINARY_R_UNARY_SYNTHESIS((con), (thisNode))
#define ARRAY_STORE_INHERITANCE(con, thisNode) \
    L_BINARY_R_UNARY_INHERITANCE((con), (thisNode))

#line 4990 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

static void
handleUsedNode(CVMJITCompilationContext* con, CVMJITRMContext* rc,
	       CVMJITIRNode* usedNode, int target, int avoid)
{
    CVMRMResource* rp = CVMJITirnodeGetUsedOp(usedNode)->resource;
    /* 
     * Don't eagerly load DEFINE of a USED at the same location. See
     * DEFINE_SYNTHESIS for details.
     */
    if (rp->expr->regsRequired != CVMCPU_AVOID_METHOD_CALL) {
	CVMRMpinResourceEagerlyIfDesireable(rc, rp, target, avoid);
    }
    CVMRMunpinResource(rc, rp);
    pushResource(con, rp);
}
#line 5014 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


static CVMRMResource*
invokeMethod(CVMJITCompilationContext *con, CVMJITRMContext* rc,
	     CVMJITIRNodePtr invokeNode)
{
    /*
     * Safe set is the empty set.  No registers are preserved.
     * To preserve some registers, we would need to save and restore them to
     * the Java stack (caller or callee frame) on transitions.
     */
    CVMRMResource *mbptr = popResource(con);
    CVMUint16 numberOfArgs = CVMJITirnodeGetBinaryOp(invokeNode)->data;
    CVMBool emittedDirectInvoke = CVM_FALSE;

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
    /* Make JSP point just past the last argument */
    CVMSMadjustJSP(con);
#endif


    /* mov  a1, mb */
    mbptr = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), mbptr,
				     CVMCPU_ARG1_REG);

    /* TODO: Possible improvements:
     * - factor target capacity into caller capacity and
     *   avoid stack overflow check.
     */
    if (CVMRMisConstant(mbptr)) {
	CVMMethodBlock* targetMb = (CVMMethodBlock *)mbptr->constant;
	int invokerIdx = CVMmbInvokerIdx(targetMb);
	if (CVMmbIs(targetMb, NATIVE) && 
	    invokerIdx != CVM_INVOKE_LAZY_JNI_METHOD)
	{
	    /* It's CNI or JNI invoke, so call it directly. */
	    CVMRMregset safeSet;
	    int logicalPC;

	    CVMassert(
	        CVMmbJitInvoker(targetMb) == (void*)CVMCCMinvokeCNIMethod ||
		CVMmbJitInvoker(targetMb) == (void*)CVMCCMinvokeJNIMethod);

	    if (invokerIdx == CVM_INVOKE_JNI_METHOD ||
		invokerIdx == CVM_INVOKE_JNI_SYNC_METHOD)
	    {
		/* JNI - NV registers are preserved */
		safeSet = CVMRM_SAFE_SET;
	    } else {
		CVMassert(invokerIdx == CVM_INVOKE_CNI_METHOD);
		safeSet = CVMRM_EMPTY_SET;
	    }

	    CVMRMmajorSpill(con, ARG1, safeSet);
	    emittedDirectInvoke = CVM_TRUE;

	    if(CVMmbJitInvoker(targetMb) == (void*)CVMCCMinvokeCNIMethod) {
		CVMJITaddCodegenComment((con, 
			"call CVMCCMinvokeCNIMethod() for %C.%M", 
			 CVMmbClassBlock(targetMb), targetMb));
	    } else {
		CVMJITaddCodegenComment((con, 
			"call CVMCCMinvokeJNIMethod() for %C.%M", 
			 CVMmbClassBlock(targetMb), targetMb));
	    }
	    
	    logicalPC = CVMJITcbufGetLogicalPC(con);
	    CVMCPUemitAbsoluteCall(con, CVMmbJitInvoker(targetMb), 
				   CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);
	    CVMJITcsSetEmitInPlace(con);
	    CVMJITcsClearEmitInPlace(con);
	    CVMJITcsBeginBlock(con);
	} 

#ifdef CVM_JIT_PATCHED_METHOD_INVOCATIONS
	else if (CVMglobals.jit.pmiEnabled && 
                 invokerIdx != CVM_INVOKE_LAZY_JNI_METHOD) {
	    int flags = CVMJITirnodeGetBinaryNodeFlag(invokeNode);
	    CVMBool isVirtual = (flags & CVMJITBINOP_VIRTUAL_INVOKE) != 0; 

	    /* CVMRMmajorSpill must be done before setting up patch record
	     * because it will record the logical PC.
	     */
	    CVMRMmajorSpill(con, ARG1, CVMRM_EMPTY_SET);
	    if (CVMJITPMIaddPatchRecord(con->mb, targetMb,
					CVMJITcbufGetPhysicalPC(con),
					isVirtual))
	    {
		/*
		 * Patch record was successfully added, so make this a direct
		 * method call.
		 */
#ifdef CVM_DEBUG_ASSERTS
		int logicalPC = CVMJITcbufGetLogicalPC(con);
#endif
		/*
		 * Now we need to add targetMb to the "callee list" of the
		 * method we are compiling. This is needed so the targetMb
		 * has an entry for this patchable call, and it needs to be
		 * removed when we are decompiled by calling
		 * CVMJITpatchTableRemovePatchRecords.
		 *
		 * We keep a count of the number of unique targetMbs (callees)
		 * for this method.
		 *
		 * WARNING: this must be done before emitting any code.
		 * Otherwise if compilation fails while emitting code,
		 * this callee will never get registered properly.
		 */
		{
		    CVMUint32 numCallees = (CVMUint32)con->callees[0];
		    CVMUint32 idx;
		    CVMBool found = CVM_FALSE;
		    /* Search list for this method or an empty slot. */
		    for (idx = 1; idx <= numCallees; idx++) {
			CVMMethodBlock* mb = 
			    (CVMMethodBlock*)con->callees[idx]; 
			if (mb == targetMb) {
			    found = CVM_TRUE;
			    break;
			}
		    }
		    if (!found) {
			/* This is a unique callee. Add to the "callee list" */
			numCallees++;
			CVMtraceJITPatchedInvokes((
			    "PMI: callee(%d) slot used 0x%x %C.%M\n",
			    numCallees,
			    targetMb, CVMmbClassBlock(targetMb), targetMb));
			CVMassert(numCallees <= con->numCallees);
			con->callees[0] = (CVMMethodBlock*)numCallees;
			con->callees[numCallees] = targetMb;
		    }
		}

		/* Do the direct method call, either to the compiled method
		   or to CVMCCMletInterpreterDoInvoke. */
		emittedDirectInvoke = CVM_TRUE;
		CVMJITcsSetEmitInPlace(con);
		if (CVMmbIsCompiled(targetMb)) {
		    int targetOffset = CVMmbStartPC(targetMb) 
			- CVMJITcbufLogicalToPhysical(con, 0);
		    CVMJITaddCodegenComment((con,
		        "patchable invoke to compiled %C.%M",
		         CVMmbClassBlock(targetMb), targetMb));
		    CVMCPUemitBranchLink(con, targetOffset);
		} else {
		    CVMJITaddCodegenComment((con, 
		        "patchable invoke to interpreted (%C.%M)",
		        CVMmbClassBlock(targetMb), targetMb));
		    CVMCPUemitAbsoluteCall(con, 
					   (void*)CVMCCMletInterpreterDoInvoke,
					   CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);
		}
#ifdef CVMCPU_HAS_DELAY_SLOT
		CVMassert(CVMJITcbufGetLogicalPC(con) ==
			  logicalPC + 2 * CVMCPU_INSTRUCTION_SIZE);
#else
		CVMassert(CVMJITcbufGetLogicalPC(con) ==
			  logicalPC + CVMCPU_INSTRUCTION_SIZE);
#endif
		CVMJITcsClearEmitInPlace(con);
		CVMJITcsBeginBlock(con);
	    }
        }
#endif /* CVM_JIT_PATCHED_METHOD_INVOCATIONS */
    }

    if (!emittedDirectInvoke) {
	/* emit indirect call to caller */
    	CVMRMmajorSpill(con, ARG1, CVMRM_EMPTY_SET);
    	CVMCPUemitInvokeMethod(con);
    }

    CVMJITcaptureStackmap(con, numberOfArgs);
    CVMSMpopParameters(con, numberOfArgs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), mbptr);

    /* Dump the constant pool here after a method invocation if we need to: */
    CVMRISCemitConstantPoolDumpWithBranchAroundIfNeeded(con);

    /* Bind the result resource to the invokeNode: */
    return CVMSMinvocation(con, rc, invokeNode);
}

#line 5232 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


#ifdef CVMJIT_INTRINSICS
#ifdef CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER
static CVMRMregset
pinIntrinsicArgs(CVMJITCompilationContext *con,
                 CVMCPUCallContext *callContext,
                 CVMJITIRNodePtr intrinsicNode,
                 CVMJITIntrinsic *irec,
		 CVMBool useRegArgs)
{
    CVMRMregset outgoingRegs = CVMRM_EMPTY_SET;
    int numberOfArgs = irec->numberOfArgs;

    /* Pin the args to respective registers: */
    if (numberOfArgs != 0) {
        CVMJITIRNode *iargNode = CVMJITirnodeGetLeftSubtree(intrinsicNode);
        struct CVMJITStackElement* sp;
        int i;
        con->cgsp -= numberOfArgs;
        sp = con->cgsp + 1;
        /* Pin the high args first because they may need to be written to
           the stack and use scratch registers while they are at it.  The
           low args will get pinned into arg registers: */
        for (i = 0; i < numberOfArgs; i++) {
            CVMRMResource *arg = sp[i].u.r;
            arg = CVMCPUCCALLpinArg(con, callContext, arg,
                        CVMJITgetTypeTag(iargNode),
                        CVMJIT_IARG_ARG_NUMBER(iargNode),
                        CVMJIT_IARG_WORD_INDEX(iargNode),
                        &outgoingRegs, useRegArgs);
            sp[i].u.r = arg;
            iargNode = CVMJITirnodeGetRightSubtree(iargNode);
        }
        CVMassert(iargNode->tag == CVMJIT_ENCODE_NULL_IARG);
    }
    return outgoingRegs;
}

static void
relinquishIntrinsicArgs(CVMJITCompilationContext *con,
                        CVMCPUCallContext *callContext,
                        CVMJITIRNodePtr intrinsicNode,
                        CVMJITIntrinsic *irec,
			CVMBool useRegArgs)
{
    int numberOfArgs = irec->numberOfArgs;

    /* Pin the args to respective registers: */
    if (numberOfArgs != 0) {
        CVMJITIRNode *iargNode = CVMJITirnodeGetLeftSubtree(intrinsicNode);
        struct CVMJITStackElement* sp;
        int i;
        sp = con->cgsp + 1;
        for (i = 0; i < numberOfArgs; i++) {
            CVMRMResource *arg = sp[i].u.r;
            CVMCPUCCALLrelinquishArg(con, callContext, arg,
                CVMJITgetTypeTag(iargNode), CVMJIT_IARG_ARG_NUMBER(iargNode),
                CVMJIT_IARG_WORD_INDEX(iargNode), useRegArgs);
            iargNode = CVMJITirnodeGetRightSubtree(iargNode);
        }
        CVMassert(iargNode->tag == CVMJIT_ENCODE_NULL_IARG);
    }
}

static void
invokeIntrinsicMethod(CVMJITCompilationContext *con,
                      CVMJITIRNodePtr intrinsicNode)
{
    CVMUint16 intrinsicID = CVMJITirnodeGetBinaryOp(intrinsicNode)->data;
    CVMJITIntrinsic *irec = &CVMglobals.jit.intrinsics[intrinsicID - 1];
    const CVMJITIntrinsicConfig *config = irec->config;
    CVMUint16 properties = config->properties;

    CVMJITprintCodegenComment(("Invoke INTRINSIC %C.%M:",
                                CVMmbClassBlock(irec->mb), irec->mb));
#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
    /* Make JSP point just past the last argument */
    CVMSMadjustJSP(con);
#endif

    if ((properties & CVMJITINTRINSIC_OPERATOR_ARGS) != 0) {
        const CVMJITIntrinsicEmitterVtbl *emitter;
        /* NOTE: The emitter is responsible for popping arguments of the
           stack, and pushing any result back on the stack as well: */
        emitter = (const CVMJITIntrinsicEmitterVtbl *)
                      config->emitterOrCCMRuntimeHelper;
        emitter->emitOperator(con, intrinsicNode);
	CVMJITprintCodegenComment(("End INTRINSIC %C.%M:",
				   CVMmbClassBlock(irec->mb), irec->mb));

    } else {
        int rtnType = CVMJITgetTypeTag(intrinsicNode);
        CVMBool okToDumpCP, okToBranchAroundCP;
        CVMRMResource *dest = NULL;
        CVMBool useJavaStack;
        CVMBool useRegArgs;
        CVMRMregset outgoingRegSet = 0;
        CVMCPUCallContext callContext;
        CVMRMResource *cceeRes = NULL;

        useJavaStack = ((properties & CVMJITINTRINSIC_JAVA_ARGS) != 0);
        useRegArgs = GET_REG_ARGS(properties);

        /* Flush the Java frame pointer to the Java stack if neccesary.  Do
           this before we pin any arguments because they can adjust the stack
           frame and hence make it difficult to compute the value of the ccee
           which may be needed for this flush operation: */
        if ((properties & CVMJITINTRINSIC_FLUSH_JAVA_STACK_FRAME) != 0) {
	    int eeReg;
#ifndef CVMCPU_EE_REG
	    CVMRMResource *eeRes =
		CVMRMgetResource(CVMRM_INT_REGS(con),
				 CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
	    eeReg = CVMRMgetRegisterNumber(eeRes);
	    /* Get the ee: */
	    CVMJITaddCodegenComment((con, "eeReg = ccee->ee"));
	    CVMCPUemitCCEEReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
		eeReg, CVMoffsetof(CVMCCExecEnv, eeX));
#else
	    eeReg = CVMCPU_EE_REG;
#endif
	    /* Store the JFP into the stack: */
	    CVMJITaddCodegenComment((con, "flush JFP to stack"));
	    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE,
                CVMCPU_JFP_REG, eeReg, offsetof(CVMExecEnv, interpreterStack) +
				       offsetof(CVMStack, currentFrame));
#ifndef CVMCPU_EE_REG
	    CVMRMrelinquishResource(CVMRM_INT_REGS(con), eeRes);
#endif
        }

        /* Load the ccee arg if necessary.  Do this before we pin any other
           arguments because they can adjust the stack frame and hence make
           it difficult to compute the value of the ccee: */
        if ((properties & CVMJITINTRINSIC_ADD_CCEE_ARG) != 0) {
            cceeRes =
                CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					 CVMCPU_ARG1_REG, 1);
            CVMCPUemitLoadCCEE(con, CVMCPU_ARG1_REG);
            outgoingRegSet |= ARG1;
        }

        /* Prepare the outgoing arguments: */
        if (useJavaStack) {
            outgoingRegSet |= CVMRM_EMPTY_SET;
        } else {
            CVMCPUCCALLinitArgs(con, &callContext, irec, CVM_FALSE,
				useRegArgs);
            outgoingRegSet |= pinIntrinsicArgs(con, &callContext,
                                  intrinsicNode, irec, useRegArgs);
        }

        /* Do spills if necessary: */
        if ((properties & CVMJITINTRINSIC_NEED_MINOR_SPILL) != 0) {
            CVMRMminorSpill(con, outgoingRegSet);
        } else if ((properties & CVMJITINTRINSIC_NEED_MAJOR_SPILL) != 0) {
	    /* NOTE: We need to remove the outgoing registers from the
	       safeSet because the intrinsic may alter those registers
	       without preserving them.  An example of this is in
	       intrinsics that does special hardware instructions that
	       may make use of the non-volatile registers, or assembly
	       functions that uses CVMJITINTRINSIC_REG_ARGS calling
	       conventions that may pass some arguments in non-volatile
	       registers.  For those cases, those outgoing non-volatile
	       registers are essentially treated like they are volatile.
	    */
	    CVMRMregset safeSet = CVMRM_SAFE_SET & ~outgoingRegSet;
            CVMRMmajorSpill(con, outgoingRegSet, safeSet);
        }

        okToDumpCP = ((properties & CVMJITINTRINSIC_CP_DUMP_OK) != 0);
        okToBranchAroundCP = okToDumpCP &&
            ((properties & CVMJITINTRINSIC_NEED_STACKMAP) == 0);

        CVMJITaddCodegenComment((con,
            "%C.%M helper", CVMmbClassBlock(irec->mb), irec->mb));
        if ((properties & CVMJITINTRINSIC_FLUSH_JAVA_STACK_FRAME) != 0) {
            /* Emit call to intrinsic helper: */
            CVMCPUemitFlushJavaStackFrameAndAbsoluteCall(con,
                config->emitterOrCCMRuntimeHelper,
                okToDumpCP, okToBranchAroundCP);
        } else {
            /* Emit call to intrinsic helper: */
            CVMCPUemitAbsoluteCall(con, config->emitterOrCCMRuntimeHelper,
                                   okToDumpCP, okToBranchAroundCP);
        }
        CVMJITcsBeginBlock(con);

        /* Capture stackmap if necessary: */
        if ((properties & CVMJITINTRINSIC_NEED_STACKMAP) != 0) {
            /* NOTE: Intrinsic helpers are always frameless.  Hence, the
               argSize we pass to CVMJITcaptureStackmap() is always 0 i.e.
               the caller is always responsible for scanning the args. */
            CVMJITcaptureStackmap(con, 0);
        }

        /* Release the ccee arg if necessary: */
        if ((properties & CVMJITINTRINSIC_ADD_CCEE_ARG) != 0) {
            CVMRMrelinquishResource(CVMRM_INT_REGS(con), cceeRes);
        }

        /* Setup the result resource if appropriate: */
        if (useJavaStack) {
            /* Pop the arguments of the Java stack: */
            CVMSMpopParameters(con, CVMmbArgsSize(irec->mb));
            /* Bind the result resource to the intrinsicNode: */
            dest = CVMSMinvocation(con, CVMRM_INT_REGS(con), intrinsicNode);
            if (rtnType != CVM_TYPEID_VOID) {
                pushResource(con, dest);
            } else {
                /* No resource will be bound for a void return type: */
                CVMassert(dest == NULL);
            }
        } else {
            if (rtnType != CVM_TYPEID_VOID) {
                int resultWords = ((rtnType == CVM_TYPEID_LONG) ||
                                   (rtnType == CVM_TYPEID_DOUBLE)) ? 2 : 1;
                dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
						CVMCPU_RESULT1_REG,
                                                resultWords);
                CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con),
					    dest, intrinsicNode);
            }
            relinquishIntrinsicArgs(con, &callContext, intrinsicNode,
				    irec, useRegArgs);
            CVMCPUCCALLdestroyArgs(con, &callContext, irec, CVM_FALSE,
				   useRegArgs);
            /* NOTE: We cannot must not push the dest resource until after
                     we relinquish the iargs.  This is because we're relying
                     on the stack to still hold the pointers to the iarg
                     resources.  Pushing the dest resource before we
                     relinquish the iargs would cause the pointer to the first
                     iarg resource to be trashed.
            */
            if (rtnType != CVM_TYPEID_VOID) {
                pushResource(con, dest);
            }
        }
    }
}
#endif /* CVM_NEED_INVOKE_INTRINSIC_METHOD_HELPER */
#endif /* CVMJIT_INTRINSICS */

#line 5602 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/* Purpose: Emits code to do a NULL check of an object reference. */
static void doNullCheck(CVMJITCompilationContext *con,
			CVMJITIRNodePtr thisNode,
			CVMRMregset target, CVMRMregset avoid)
{
#ifdef CVMJIT_TRAP_BASED_NULL_CHECKS
    /* 
     * The NULL check is performed by doing a fake read using the pointer.
     * If null, we get a trap, which we catch and deal with.
     */
	CVMRMResource* objRes;
	CVMRMResource* scratch;

	CVMRMsynchronizeJavaLocals(con);

	/* Get the resource for the object to be NULL-checked */
	objRes = popResource(con);
	CVMRMpinResource(CVMRM_INT_REGS(con), objRes, target, avoid);

	/* We are going to load into this register.
	   NOTE: We allocate the scratch after pinning the objRes because we
	         don't want to make sure that the objRes gets targetted to the
		 desired register first.
	*/
	scratch = CVMRMgetResource(CVMRM_INT_REGS(con),
				   CVMRM_ANY_SET, avoid, 1);
	
	CVMJITaddCodegenComment((con,
				 trapCheckComments[CVMJITIR_NULL_POINTER]));
	
	/* LDR Rscratch, [obj] */
        CVMJITcsSetExceptionInstruction(con);
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
            CVMRMgetRegisterNumber(scratch), CVMRMgetRegisterNumber(objRes),
            0);

	/* We are done with the NULL check side effect. Pass the object on */
	if (thisNode != NULL) {
	    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), objRes, thisNode);
	} else {
	    CVMRMunpinResource(CVMRM_INT_REGS(con), objRes);
	}
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), scratch);
	pushResource(con, objRes);
#else
	CVMRMResource* objRes;

	CVMRMsynchronizeJavaLocals(con);

	/* Get the resource for the object to be NULL-checked */
	objRes = popResource(con);
	CVMRMpinResource(CVMRM_INT_REGS(con), objRes, target, avoid);

	/* Compare to NULL */
        CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_EQ,
                                  CVMRMgetRegisterNumber(objRes), 0);
	CVMJITaddCodegenComment((con,
	    trapCheckComments[CVMJITIR_NULL_POINTER]));
        CVMCPUemitAbsoluteCallConditional(con, 
            CVMCCMruntimeThrowNullPointerExceptionGlue,
            CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK, CVMCPU_COND_EQ);
	CVMJITcsBeginBlock(con);

	/* We are done with the NULL check side effect. Pass the object on */
	if (thisNode != NULL) {
	    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), objRes, thisNode);
	} else {
	    CVMRMunpinResource(CVMRM_INT_REGS(con), objRes);
	}
	pushResource(con, objRes);
#endif
}

#line 5996 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

static void
branchToBlock(
    CVMJITCompilationContext* con,
    CVMCPUCondCode condcode,
    CVMJITIRBlock* target);

/* convert CVMJIT_XXX condition code to a CVMCPUCondCode */
static CVMCPUCondCode
mapCondCode(CVMUint16 condition) {
    switch(condition) {
        case CVMJIT_EQ: return CVMCPU_COND_EQ;
        case CVMJIT_NE: return CVMCPU_COND_NE;
        case CVMJIT_LE: return CVMCPU_COND_LE;
        case CVMJIT_GE: return CVMCPU_COND_GE;
        case CVMJIT_LT: return CVMCPU_COND_LT;
        case CVMJIT_GT: return CVMCPU_COND_GT;
        default: CVMassert(CVM_FALSE); return 0;
    }
}

static void
compare32cc(CVMJITCompilationContext *con,
	    CVMJITIRNodePtr thisNode, int opcode)
{
    CVMCPUALURhs* rhs = popALURhs(con);
    CVMRMResource* lhs = popResource(con);
    CVMJITConditionalBranch* branch = CVMJITirnodeGetCondBranchOp(thisNode);
    CVMJITIRBlock* target = branch->target;
    CVMCPUCondCode condCode = mapCondCode(branch->condition);
#ifdef IAI_VIRTUAL_INLINE_CB_TEST
    CVMRMregSandboxResources* sandboxRes = NULL;
#endif

#ifndef CVMCPU_HAS_COMPARE
    /* pin before calling CVMCPUemitCompare() */
    CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
#endif
    CVMRMpinResource(CVMRM_INT_REGS(con), lhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUalurhsPinResource(CVMRM_INT_REGS(con), opcode, rhs,
			    CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUemitCompare(con, opcode, condCode,
		      CVMRMgetRegisterNumber(lhs),
		      CVMCPUalurhsGetToken(con, rhs));
    CVMRMsynchronizeJavaLocals(con);
#ifdef CVMCPU_HAS_COMPARE
    /* no longer need resource used in CVMCPUemitCompare() */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMCPUalurhsRelinquishResource(CVMRM_INT_REGS(con), rhs);
    /* pin after calling CVMCPUemitCompare() */
    CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
#endif

#ifdef IAI_VIRTUAL_INLINE_CB_TEST
    if(target->mtIndex != CVMJIT_IAI_VIRTUAL_INLINE_CB_TEST_DEFAULT) {
        /* reserve registers for outofline MB test */
        sandboxRes = CVMRMgetRegSandboxResources(
            CVMRM_INT_REGS(con), target,
            CVMRM_ANY_SET, CVMRM_EMPTY_SET,
            CVMCPU_VIRTUAL_INLINE_OUTOFLINE_MB_TEST_REG_NUM);
    }
#endif

    branchToBlock(con, condCode, target);

#ifdef IAI_VIRTUAL_INLINE_CB_TEST
    if(target->mtIndex != CVMJIT_IAI_VIRTUAL_INLINE_CB_TEST_DEFAULT) {
        CVMJITcsBeginBlock(con);
	target->oolReturnAddress = CVMJITcbufGetLogicalPC(con);
        /* relinquish the sandbox resources */
        CVMRMrelinquishRegSandboxResources(CVMRM_INT_REGS(con),
                                           sandboxRes);
    }
#endif

#ifndef CVMCPU_HAS_COMPARE
    /* no longer need resource used in CVMCPUemitCompare() */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMCPUalurhsRelinquishResource(CVMRM_INT_REGS(con), rhs);
#endif
    CVMRMunpinAllIncomingLocals(con, target);
}

static void
compare64cc(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode)
{
    CVMRMResource* rhs = popResource(con);
    CVMRMResource* lhs = popResource(con);
    CVMJITConditionalBranch* branch = CVMJITirnodeGetCondBranchOp(thisNode);
    CVMJITIRBlock* target = branch->target;
    CVMCPUCondCode condCode = mapCondCode(branch->condition);

#ifndef CVMCPU_HAS_COMPARE
    /* pin before calling CVMCPUemitCompare() */
    CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
#endif
    CVMRMpinResource(CVMRM_INT_REGS(con), lhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(CVMRM_INT_REGS(con), rhs, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    condCode = CVMCPUemitCompare64(con, CVMCPU_CMP64_OPCODE, condCode,
                    CVMRMgetRegisterNumber(lhs), CVMRMgetRegisterNumber(rhs));
    CVMRMsynchronizeJavaLocals(con);
#ifdef CVMCPU_HAS_COMPARE
    /* no longer need resource used in CVMCPUemitCompare() */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    /* pin after calling CVMCPUemitCompare() */
    CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
#endif

    branchToBlock(con, condCode, target);

#ifndef CVMCPU_HAS_COMPARE
    /* no longer need resource used in CVMCPUemitCompare() */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
#endif
    CVMRMunpinAllIncomingLocals(con, target);
}

#ifdef CVM_NEED_DO_FCMP_HELPER

/* Purpose: Emits code to compare 2 floats by calling a helper function. */
static void
fcomparecc(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode,
	   CVMBool needBranch, CVMBool needSetcc)
{
    CVMJITConditionalBranch* branch = NULL;
    CVMRMResource *rhs = popResource(con);
    CVMRMResource *lhs = popResource(con);
    CVMUint32 nanResult;
    int flags;
    
    /* If needBranch is TRUE, then we know this is CVMJITConditionalBranch.
     * Otherwise is is a CVMJITBinaryOp. */
    if (needBranch) {
	branch = CVMJITirnodeGetCondBranchOp(thisNode);
	flags = branch->flags;
    } else {
	flags = CVMJITirnodeGetBinaryNodeFlag(thisNode);
    }

    if (flags & CVMJITCMPOP_UNORDERED_LT) {
        nanResult = -1;
    } else {
        nanResult = 1;
    }

    /* Pin the input to the first two arguments because the helper expects it
       there: */
    lhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), lhs, CVMCPU_ARG1_REG);
    rhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), rhs, CVMCPU_ARG2_REG);

    /* Spill the outgoing registers if necessary: */
    CVMRMminorSpill(con, ARG1|ARG2);

    CVMJITaddCodegenComment((con, "do fcmp"));
    CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFCmp);
    CVMCPUemitLoadConstant(con, CVMCPU_ARG3_REG, nanResult);

    /* Emit the call to the helper to compute the result: */
    CVMJITaddCodegenComment((con, "call CVMCCMruntimeFCmp"));
    CVMJITsetSymbolName((con, "CVMCCMruntimeFCmp"));
    CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeFCmp,
                           CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
    CVMJITcsBeginBlock(con);

    /* if the needBranch is true, then we need to convert the {-1,0,1}
     * into a boolean condition code and do a conditional branch */
    if (needBranch) {
	CVMJITConditionalBranch* branch =
	    CVMJITirnodeGetCondBranchOp(thisNode);
	CVMCPUCondCode cc = mapCondCode(branch->condition);
	if (needSetcc) {
	    CVMJITaddCodegenComment((con, "set condition code"));
	    CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, cc,
			      CVMCPU_RESULT1_REG, CVMCPUALURhsTokenConstZero);
	}
	CVMRMsynchronizeJavaLocals(con);
	CVMRMpinAllIncomingLocals(con, branch->target, CVM_FALSE);
	branchToBlock(con, cc, branch->target);
	CVMRMunpinAllIncomingLocals(con, branch->target);
    }

    /* Release resources and publish the result: */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
}

#endif /* CVM_NEED_DO_FCMP_HELPER */

#ifdef CVM_NEED_DO_DCMP_HELPER

/* Purpose: Emits code to compare 2 doubles by calling a helper. */
static void
dcomparecc(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode,
	   CVMBool needBranch, CVMBool needSetcc)
{
    CVMJITConditionalBranch* branch = NULL;
    CVMRMResource *rhs = popResource(con);
    CVMRMResource *lhs = popResource(con);
    CVMUint32 nanResult;
    int flags;
    
    /* If needBranch is TRUE, then we know this is CVMJITConditionalBranch.
       Otherwise is is a CVMJITBinaryOp. */
    if (needBranch) {
	branch = CVMJITirnodeGetCondBranchOp(thisNode);
	flags = branch->flags;
    } else {
	flags = CVMJITirnodeGetBinaryNodeFlag(thisNode);
    }

    if (flags & CVMJITCMPOP_UNORDERED_LT) {
        nanResult = -1;
    } else {
        nanResult = 1;
    }

    /* Pin the input to the first two arguments because the helper expects it
       there: */
    lhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), lhs, CVMCPU_ARG1_REG);
    rhs = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), rhs, CVMCPU_ARG3_REG);

    /* Spill the outgoing registers if necessary: */
    CVMRMminorSpill(con, ARG1|ARG2|ARG3|ARG4);

#ifdef CVMCPU_HAS_64BIT_REGISTERS
    {
        /* Both arguments are doubleword */
        CVMCPUemitMoveTo64BitRegister(con, CVMCPU_ARG1_REG, 
                                      CVMCPU_ARG1_REG);
        CVMCPUemitMoveTo64BitRegister(con, CVMCPU_ARG2_REG,
                                      CVMCPU_ARG3_REG);
    }
#endif

    /* Emit the call to the helper to compute the result: */
    if (nanResult == -1) {
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDCmpl);
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDCmpl"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDCmpl"));
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeDCmpl,
                               CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
    } else {
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDCmpg);
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDCmpg"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDCmpg"));
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeDCmpg,
                               CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
    }
    CVMJITcsBeginBlock(con);

    /* if the needBranch is true, then we need to convert the {-1,0,1}
     * into a boolean condition code and do a conditional branch */
    if (needBranch) {
	CVMCPUCondCode cc = mapCondCode(branch->condition);
	if (needSetcc) {
	    CVMJITaddCodegenComment((con, "set condition code"));
	    CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, cc,
			      CVMCPU_RESULT1_REG, CVMCPUALURhsTokenConstZero);
	}
	CVMRMsynchronizeJavaLocals(con);
	CVMRMpinAllIncomingLocals(con, branch->target, CVM_FALSE);
	branchToBlock(con, cc, branch->target);
	CVMRMunpinAllIncomingLocals(con, branch->target);
    }

    /* Release resources and publish the result: */
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
}

#endif /* CVM_NEED_DO_DCMP_HELPER */

#line 6352 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


#ifdef CVMJIT_PATCH_BASED_GC_CHECKS
void
patchInstruction(CVMJITCompilationContext* con)
{
    CVMCPUInstruction* patchedInstructions = (CVMCPUInstruction*)
	(((CVMUint32)&con->gcCheckPcs->pcEntries[con->gcCheckPcsSize]
	  + sizeof(CVMCPUInstruction) - 1)
	 & ~(sizeof(CVMCPUInstruction)-1));
    CVMUint32      pcOffset;

    CVMassert(con->gcCheckPcsIndex < con->gcCheckPcsSize);

    patchedInstructions[con->gcCheckPcsIndex] =
	*(CVMCPUInstruction*)CVMJITcbufGetPhysicalPC(con);
    /* save away this offset in the gcCheckPcs table */
    pcOffset = CVMJITcbufGetLogicalPC(con) + 
	(con->codeEntry - con->codeBufAddr);
    CVMassert(pcOffset <= 0xffff); /* Make sure it fits */
    con->gcCheckPcs->pcEntries[con->gcCheckPcsIndex++] = pcOffset;
}
#endif /* CVMJIT_PATCH_BASED_GC_CHECKS */

static void
loadIncomingLocals(CVMJITCompilationContext* con,
		   CVMJITIRBlock *b, CVMBool spilledPhis)
{
#ifdef CVM_JIT_REGISTER_LOCALS
    /*
     * Backwards branch targets need to explicitly load all locals so OSR
     * will work. However, normally we skip this by setting up
     * b->logicalAddress to point after the loading of the locals.
     *
     * Note that we also need to load incoming locals after doing a gc to
     * reload ref locals.
     */
    if (b->incomingLocalsCount > 0) {
	CVMassert(spilledPhis == 0);
	/* This is the address we will OSR to */
	b->loadLocalsLogicalAddress = CVMJITcbufGetLogicalPC(con);
	CVMtraceJITCodegen((
            "\tL%d:\t%d:\t@ entry point when locals need to be loaded\n",
            CVMJITirblockGetBlockID(b), CVMJITcbufGetLogicalPC(con)));
	CVMRMpinAllIncomingLocals(con, b, CVM_TRUE);
	CVMRMunpinAllIncomingLocals(con, b);
    }
#endif /* CVM_JIT_REGISTER_LOCALS */
}

/*
 * At block entry, if we know this is the target of a backwards branch,
 * check for GC rendezvous request.
 */
void
CVMJITcheckGC(CVMJITCompilationContext* con, CVMJITIRBlock *b)
{
#if !defined(CVMJIT_TRAP_BASED_GC_CHECKS) && !defined(CVMJIT_PATCH_BASED_GC_CHECKS)
    /*
     * Generate code to do a gc rendezvous inline if one has been requested.
     */
    CVMBool spilledPhis;
    CVMRMResource* scratch;
    int scratchReg;
    int cvmGlobalsReg;

    CVMJITcsSetEmitInPlace(con);

    scratch = CVMRMgetResource(CVMRM_INT_REGS(con),
			       CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
    scratchReg = CVMRMgetRegisterNumber(scratch);
    
    /* This is where branches to the block will branch to. */
    CVMtraceJITCodegen(("\tL%d:\t%d:\t@ entry point for branches\n",
        CVMJITirblockGetBlockID(b), CVMJITcbufGetLogicalPC(con)));

    /* this is where we branch to when a gc is needed (do_gc label) */
    b->gcLogicalAddress = CVMJITcbufGetLogicalPC(con);

    /* spill phis */
    spilledPhis = CVMRMspillPhis(con, CVMRM_SAFE_SET);

    CVMJITprintCodegenComment(("Do GC Check:"));

#ifdef CVMCPU_CVMGLOBALS_REG
    cvmGlobalsReg = CVMCPU_CVMGLOBALS_REG;
#else
    /* load CVMglobals */
    cvmGlobalsReg = scratchReg;
    CVMJITaddCodegenComment((con, "CVMglobals"));
    CVMJITsetSymbolName((con, "CVMglobals"));
    CVMCPUemitLoadConstant(con, scratchReg, (CVMInt32)&CVMglobals);
#endif

    /* load CVMglobals.cstate[CVM_GC_SAFE].request */
    CVMJITaddCodegenComment((con, "CVMglobals.cstate[CVM_GC_SAFE].request;"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
	scratchReg, cvmGlobalsReg,
	offsetof(CVMGlobalState, cstate[CVM_GC_SAFE].request));

    /* check if gc requested */
    CVMJITaddCodegenComment((con, "If GC is requested,"));
    CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_NE,
			      scratchReg, 0);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), scratch);

    /* gc if requested */
    CVMJITaddCodegenComment((con, "CVMCCMruntimeGCRendezvousGlue"));
    CVMCPUemitAbsoluteCallConditional(con, CVMCCMruntimeGCRendezvousGlue,
				      CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK,
				      CVMCPU_COND_NE);

    /* Get a stackmap. This is a GC point */
    CVMJITcaptureStackmap(con, 0); 

    /* reload phis */
    if (spilledPhis) {
	CVMRMreloadPhis(con, CVMRM_SAFE_SET);
    }

    /* load incoming locals if necessary */
    loadIncomingLocals(con, b, spilledPhis);

    b->logicalAddress = CVMJITcbufGetLogicalPC(con);
    CVMJITcsClearEmitInPlace(con);

    return;
#else /* !CVMJIT_TRAP_BASED_GC_CHECKS && !CVMJIT_PATCH_BASED_GC_CHECKS */

#if defined(CVM_AOT) || defined(CVM_MTASK)
    /*
     * If either AOT or MTASK is enabled, use trap based GC for
     * precompiled/AOT code to avoid patching for the read only
     * code. For dynamic compiled code use patch based GC for
     * performance purpose.
     */
    if (CVMglobals.jit.isPrecompiling)
#endif
#ifdef CVMJIT_TRAP_BASED_GC_CHECKS
    {
        /*
          Just generate an instruction that will cause a trap (crash) when
          a gc is requested. There a 3 cases to worry about:

          (1) Trap-based Incoming Locals:
          ---------------------------
          bk->loadLocalsLogicalAddress:
          gc_return:
            <stackmap>
            load incoming locals
          bk->gcLogicalAddress:
            ldr  rGC,offset(gc_return)(rGC)
          bk->logicalAddress:
      
          -Trap redirects execution to CVMCCMruntimeGCRendezvousGlue,
           and sets LR to gc_return. The offset to gc_return is encoded
           in the ldr trap instruction.
      
          (2) Trap-based Incoming Phis:
          -------------------------
          do_gc:
            spill phis
            CVMCCMruntimeGCRendezvousGlue
            <stackmap>
            load phis
          bk->gcLogicalAddress:
            ldr  rGC,-offset(do_gc)(rGC)
          bk->logicalAddress:
    
          -Trap redirects execution to do_gc based on offset embedded in the
           trap instruction. Note that the offset is negative to indicate
           that execution needs to resume at the offset rather than just
           setting LR to the offset.
     
          (3) Trap-based Normal:
          ------------------
            <stackmap>
          bk->gcLogicalAddress:
            ldr  rGC,0(rGC)
          bk->logicalAddress:

          -Trap redirects execution to CVMCCMruntimeGCRendezvousGlue, 
           and sets LR to the trap instruction. The offset of 0 is 
           encoded the ldr trap instruction, so the trap handler knows 
           to set LR to be the same as the trap instruction.
        */

        CVMBool spilledPhis;
        CVMInt32 gcLogicalAddress;

        /*this is where we branch to when a gc is needed (do_gc label)*/
        gcLogicalAddress = CVMJITcbufGetLogicalPC(con);
        CVMJITcsSetEmitInPlace(con);

        /* spill phis */
        spilledPhis = CVMRMspillPhis(con, CVMRM_SAFE_SET);

        /* We only need an inlined unconditional gcRendezvous if there are
         * incoming phis.
         */
        if (spilledPhis) {
  	    /* unconditional gc  */
	    CVMJITaddCodegenComment((con, "CVMCCMruntimeGCRendezvousGlue"));
	    CVMCPUemitAbsoluteCall(con, CVMCCMruntimeGCRendezvousGlue,
			           CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);
        }

        if (CVMJITcbufGetLogicalPC(con) == 
	    CVMJITgetPreviousStackmapLogicalPC(con)) {
	    /* 6314307: If the previous block ended with a method call, then
             * this current pc already has a stackmap that includes the
	     * arguments to that call. When we generate a 2nd stackmap for
	     * this pc below, it won't end up getting used. If this
	     * gcCheck results in a gc, then non-existent stack items will
	     * be scanned. We need to force an extra instruction before
	     * the gcCheck instruction so the 2nd stackmap will be found.
	     */
	    CVMCPUemitNop(con);
	    /* we don't want GC to return to the nop or it will use the
	       wrong stackmap */
  	    CVMassert(gcLogicalAddress ==
		      CVMJITcbufGetLogicalPC(con) - CVMCPU_INSTRUCTION_SIZE);
	    gcLogicalAddress += CVMCPU_INSTRUCTION_SIZE;
        }

        /* Get a stackmap. This is a GC point */
        CVMJITcaptureStackmap(con, 0);

        /* reload phis */
        if (spilledPhis) {
	    CVMRMreloadPhis(con, CVMRM_SAFE_SET);
        }
    
        /* load incoming locals if necessary */
        loadIncomingLocals(con, b, spilledPhis);

#ifdef CVMCPU_HAS_VOLATILE_GC_REG
        /* rGC is not setup in an NV register. Get it from the ccee. Note
         * that it is normally setup before the backwards branch, which will
         * branch after this code. This one is in case we GC, in which case
         * rGC will be trashed before we re-enter the block.
         */
        CVMJITaddCodegenComment((con, "rGC = ccee->gcTrapAddr"));
        CVMCPUemitCCEEReferenceImmediate(con,
				     CVMCPU_LDR32_OPCODE, CVMCPU_GC_REG,
				     offsetof(CVMCCExecEnv, gcTrapAddr));
#endif

        /* This is where branches to the block will branch to. Note that 
         * we branch after the rGC setup above. In order to reduce load 
         * latency issues, we require that rGC be setup before doing the 
         * branch.
         */
        CVMtraceJITCodegen(("\tL%d:\t%d:\t@ entry point for branches\n",
			b->blockID, CVMJITcbufGetLogicalPC(con)));
        b->gcLogicalAddress = CVMJITcbufGetLogicalPC(con);

        {
	    /* Emit the instruction that will trap when a gc is
	     * requested. Note that the offset is the distance to the
	     * instructions above to handle the actual gcRendezvous. The SEGV
	     * handler will use this value to redirect execution to the above
	     * code.
	     */
	    int gcOffset = b->gcLogicalAddress - gcLogicalAddress;
	    CVMassert(gcOffset <=
		      sizeof(void*) * CVMJIT_MAX_GCTRAPADDR_WORD_OFFSET);
	    if (spilledPhis) {
                /* encode that there are incoming phis */
	        gcOffset = -gcOffset;
	    }
	    CVMJITaddCodegenComment((con, "gc trap instruction"));
	    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
					   CVMCPU_GC_REG, CVMCPU_GC_REG,
					   gcOffset);
            CVMassert((*((CVMCPUInstruction*)CVMJITcbufGetPhysicalPC(con)-1) &
                CVMCPU_GCTRAP_INSTRUCTION_MASK) == CVMCPU_GCTRAP_INSTRUCTION);
        }

        b->logicalAddress = CVMJITcbufGetLogicalPC(con);

        CVMJITcsClearEmitInPlace(con);
        return;
    }
#endif /* CVMJIT_TRAP_BASED_GC_CHECKS */

#if defined(CVM_AOT) || defined(CVM_MTASK)
    else
#endif

#ifdef CVMJIT_PATCH_BASED_GC_CHECKS
    {
        /*
         * Generate a call to do the rendezvous, but also patch over the call
         * so normally it is not made until a gc is requested. This way there
         * is no overhead involved at the gc checkpoint.
         */
        int spilled;

        CVMJITcsSetEmitInPlace(con);
#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        CVMassert(con->SMjspOffset == 0);
#endif

        /*
          There are 3 forms of backwards branch targets we worry about:

          (1) Patch-based Incoming Locals:
          -------------------------------
          do_gc:
              gcRendezvous    <-- patched until a GC is needed
          bk->loadLocalsLogicalAddress:
          load incoming locals
          bk->logicalAddress:
      
          (2) Patch-based Incoming Phis:
          ------------------------------
          do_gc:
              spill phis
              gcRendezvous    <-- patched until a GC is needed
              load phis
          bk->logicalAddress:
      
          (3) Patch-based Normal:
          -----------------------
          do_gc:
              gcRendezvous    <-- patched until a GC is needed
          bk->logicalAddress:
      
          When patching gcRendezvous, we do 1 of two things:
          -If there is no delay slot, gcRendezvous is patched by rewinding
           and overwriting it with whatever instruction would naturally
           come next.
          -If there is delay slot, gcRendezvous is patched with a nop.
      
          -All branches to the block are to bk->logicalAddress, thus avoiding
           any gcRendezvous overhead.
          -When doing a fallthrough, an explicit branch to bk->logicalAddress
           is used, unless there is no phis or incoming locals, in which case
           a fallthrough is allowed.
      
          -If there are incoming locals, the first MAP_PC node in the block
           is remapped to bk->loadLocalsLogicalAddress for OSR. There is no 
           OSR when there are incoming phis, and no remapping is needed in 
           the normal case.
         */

        CVMtraceJITCodegen(("\tL%d:\t%d:\t@ Patchable GC Check point\n",
             CVMJITirblockGetBlockID(b), CVMJITcbufGetLogicalPC(con)));

        /* this is where we branch to when a gc is needed (do_gc label) */
        b->gcLogicalAddress = CVMJITcbufGetLogicalPC(con);

        spilled = CVMRMspillPhisCount(con, CVMRM_SAFE_SET);

        /*
         * Need to spill phis before doing a gc and reload them after the gc
         * in case either they are ref phis or are in volatile registers.
         * See case (2) above.
         */
        if (spilled > 0) {
	    CVMJITprintCodegenComment(("******* PHI spill %C.%M\n",
				       CVMmbClassBlock(con->mb), con->mb));
	    CVMRMspillPhis(con, CVMRM_SAFE_SET);
        }

        if (CVMJITcbufGetLogicalPC(con) == 
	    CVMJITgetPreviousStackmapLogicalPC(con)) {
	    /* If the previous block ended with a method call, then this
	     * current pc already has a stackmap that includes the
	     * arguments to that call. When we generate a 2nd stackmap for
	     * this pc below, it won't end up getting used. If this
	     * gcCheck results in a gc, then non-existent stack items will
	     * be scanned. We need to force an extra instruction before
	     * the gcCheck instruction so the 2nd stackmap will be found.
	     */
   	    /*
	     * NOTE: We could do better here. Currently we don't always
	     * flush JSP when making method calls, so the stackmap is our
	     * only indicator of how much we need to scan. If we always
	     * flush JSP, then the scanner can just use the previous
	     * stackmap and stop scanning when topOfStack is reached. This
	     * is a fix that was made in 1.0, and also allows us to
	     * remove another fix involving exception handling when
	     * TOS isn't flushed. See bug #4732902.  
	    */
	    CVMCPUemitNop(con);
        }

        /* 1. Emit the gc rendezvous instruction. Tell the emitter that
         *    we are going to rewind the code buffer. */
        CVMCPUemitGcCheck(con, CVM_TRUE);
        /* 2. rewind to just before the rendezvous instruction */
        CVMJITcbufRewind(con, sizeof(CVMCPUInstruction));
        /* 3. save away the gc rendezvous instruction. */
        patchInstruction(con);

#ifdef CVMCPU_NUM_NOPS_FOR_GC_PATCH
        {
            int i;
            /* We only patch one instruction, which is the branch or call to
             * CVMCCMruntimeGCRendezvousGlue. For platforms like Sparc that
             * have a delay slot after a call(or branch), we put nop's at
             * the patch location and also one instruction after patch, so
             * when GC is patched we can guarantee that the delay slot does 
             * not contain any unexpected instruction. On platforms that 
             * require multiple instructions to do the call (like powerpc 
             * when we don't copy to the code cache) we also need to 
             * generate a nop so we don't attempt to return to the call 
             * instruction, but instead return to the instruction after it.
             * Otherwise if it gets repatched before we return, we executed
             * the call instruction without executing the code to setup the
             * call properly.
             */
            for (i = 0; i < CVMCPU_NUM_NOPS_FOR_GC_PATCH; i++) {
                CVMCPUemitNop(con);
            }
        }
#endif

        /* Get a stackmap. This is a GC point */
        CVMJITcaptureStackmap(con, 0);

        /* reload the spilled phis. See case (2) above. */
        if (spilled > 0) {
	    CVMRMreloadPhis(con, CVMRM_SAFE_SET);
        }

        /* load incoming locals if necessary */
        loadIncomingLocals(con, b, spilled > 0);

        /*
         * This is where branches to the block will branch to. Note that
         * we are smart here and make branches to this block branch after
         * the loading of locals, since the locals will already be loaded
         */
        CVMtraceJITCodegen(("\tL%d:\t%d:\t@ entry point for branches\n",
            CVMJITirblockGetBlockID(b), CVMJITcbufGetLogicalPC(con)));
        b->logicalAddress = CVMJITcbufGetLogicalPC(con);
        CVMJITcsClearEmitInPlace(con);
    }
#endif /* CVMJIT_PATCH_BASED_GC_CHECKS */
#endif /* !CVMJIT_TRAP_BASED_GC_CHECKS && !CVMJIT_PATCH_BASED_GC_CHECKS */
}

static void
branchToBlock(
    CVMJITCompilationContext* con,
    CVMCPUCondCode condcode,
    CVMJITIRBlock* target)
{
     CVMJITFixupElement** fixupList = NULL;
     CVMUint32 targetAddress = target->logicalAddress;

     CVMJITcsSetEmitInPlace(con);

    /*
     * If we are branching to a block that we haven't compiled yet,
     * then add the instruction to the list that must be fixed up
     * once the target block is compiled.
     */
    if (!(target->flags & CVMJITIRBLOCK_ADDRESS_FIXED)) {
	if (condcode == CVMCPU_COND_AL) {
	    fixupList = &(target->branchFixupList);
	} else {
	    fixupList = &(target->condBranchFixupList);
	}
    }

#if !defined(CVMJIT_PATCH_BASED_GC_CHECKS) && !defined(CVMJIT_TRAP_BASED_GC_CHECKS)
    if (CVMJITirblockIsBackwardBranchTarget(target)) {
	targetAddress = target->gcLogicalAddress;
    }
    CVMJITaddCodegenComment((con, "branch to block L%d",
                            CVMJITirblockGetBlockID(target)));
    CVMCPUemitBranchNeedFixup(con, targetAddress, condcode, fixupList);
#else /* !CVMJIT_TRAP_BASED_GC_CHECKS && !CVMJIT_PATCH_BASED_GC_CHECKS */

#if defined(CVM_AOT) || defined(CVM_MTASK)
    /* Compiling AOT/warmup code, use trap based GC */
    if (CVMglobals.jit.isPrecompiling)
#endif
#ifdef CVMJIT_TRAP_BASED_GC_CHECKS
    {
        if (CVMJITirblockIsBackwardBranchTarget(target)) {
	    targetAddress = target->gcLogicalAddress;
#ifdef CVMCPU_HAS_VOLATILE_GC_REG
	    /* rGC is not setup in an NV register. Get it from the ccee: */
	    CVMJITaddCodegenComment((con, "rGC = ccee->gcTrapAddr"));
	    CVMCPUemitCCEEReferenceImmediate(con,
					 CVMCPU_LDR32_OPCODE, CVMCPU_GC_REG,
					 offsetof(CVMCCExecEnv, gcTrapAddr));
#endif
        }
        CVMJITaddCodegenComment(
            (con, "branch to block L%d", CVMJITirblockGetBlockID(target)));
        CVMCPUemitBranchNeedFixup(con, targetAddress, condcode, fixupList);
    }
#endif
#if defined(CVM_AOT) || defined(CVM_MTASK)
    else
#endif
#ifdef CVMJIT_PATCH_BASED_GC_CHECKS
    {
        /* 
         * We need to do a patchable branch to backwards branch targets so we
         * don't execute gcRendezvous code everytime.
         */
        if (CVMJITirblockIsBackwardBranchTarget(target))
        {
            CVMCPUInstruction* patchedInstructions = (CVMCPUInstruction*)
                (((CVMUint32)&con->gcCheckPcs->pcEntries[con->gcCheckPcsSize]
                  + sizeof(CVMCPUInstruction) - 1)
                  & ~(sizeof(CVMCPUInstruction)-1));

            CVMassert(con->gcCheckPcsIndex < con->gcCheckPcsSize);
	    CVMJITaddCodegenComment(
                (con, "branch to block L%d", CVMJITirblockGetBlockID(target)));

            if (!(target->flags & CVMJITIRBLOCK_ADDRESS_FIXED)) {
                /* This must be a forward branch. */
                CVMCPUemitBranchNeedFixup(con, targetAddress,
                                          condcode, fixupList);
                patchedInstructions[con->gcCheckPcsIndex] = 0;
                con->gcCheckPcs->pcEntries[con->gcCheckPcsIndex++] = 0;
            } else {
                int rewindOffset = 0;
	        int branchPC = CVMJITcbufGetLogicalPC(con);
	        int patchPC = 0;

	        CVMassert(target->gcLogicalAddress != 0);
	        CVMassert(target->gcLogicalAddress <= targetAddress);

                /* No need to patch the backwards branch if 
                 * (targetAddress == target->gcLogicalAddress)
                 */
                if (targetAddress != target->gcLogicalAddress) {
                    CVMJITprintCodegenComment(
                        ("Patchable backwards branch:")); 
	            /* first emit branch to instructions that will gc */
                    CVMCPUemitBranch(con, 
                         target->gcLogicalAddress, condcode);

#ifdef CVMCPU_HAS_DELAY_SLOT
	            /* NOTE: This is not going to work for SH4. The
	               CVMCCMruntimeGCRendezvousGlue() call is going to take
                       more than two instruction, including the nop in the 
                       delay slot. We need an HPI #define for the number 
                       of instructions. Also, the patchInstruction() call 
                       below needs to be done for each instruction used in 
                       the branch, instead of assuming there is just one.
	            */
	            patchPC = CVMJITcbufGetLogicalPC(con) -
		        2 * CVMCPU_INSTRUCTION_SIZE;
#else
	            patchPC = CVMJITcbufGetLogicalPC(con) -
		        1 * CVMCPU_INSTRUCTION_SIZE;
#endif

                    /* If the platform has branch delay slot (such as SPARC
                     * 'call' instruction), space is reserved for the GC 
                     * check patching instruction at the beginning of a 
                     * backward branch target (see checkGC()).
                     *
                     * Normally, the reserved space is skipped when branching
                     * to the target for performance reason. When GC is 
                     * requested, the branch instruction will be patched to 
                     * the beginning of the target, so that GCRendezvous 
                     * can be invoked.
                     */
	            /* NOTE: This is not going to work for SH4. The two 
                       backwards branches we emit (one above and then the 
                       2nd a bit further down) might take a different 
                       number of instructions, given the SH4's limited 
                       conditional branch range (256 bytes). The 2nd might 
                       be in range and the 1st not (since it is a bit
		       further away). The fix is to count the number of 
                       instructions the first branch took, and if the 2nd 
                       takes fewer, fill it out with nops.
	            */
                    rewindOffset = CVMJITcbufGetLogicalPC(con) - patchPC;
                    CVMJITcbufRewind(con, rewindOffset);
                    /* save away the branch instruction. */
	            patchInstruction(con);
                    /* reemit branch */
	            CVMJITcbufRewind(con, patchPC - branchPC);
                } else {
                    /* no patch for this backwards branch. */
                    patchedInstructions[con->gcCheckPcsIndex] = 0;
                    con->gcCheckPcs->pcEntries[con->gcCheckPcsIndex++] = 0;
                }
	        /* now emit branch to normal block entry which won't gc */
	        CVMJITaddCodegenComment((con, "branch to block L%d, skip GC",
                                         CVMJITirblockGetBlockID(target)));
                CVMCPUemitBranch(con, targetAddress, condcode);
                if (targetAddress != target->gcLogicalAddress) {
	            if (CVMJITcbufGetLogicalPC(con) - patchPC !=
                                                        rewindOffset) {
		        CVMJITerror(con, CANNOT_COMPILE,
                            "CVMJIT: Inconsistent backwards branch instructions");
	            }
                }
            }
         } else {
	    CVMJITaddCodegenComment(
                (con, "branch to block L%d", CVMJITirblockGetBlockID(target)));
	    CVMCPUemitBranchNeedFixup(con, targetAddress,
                                      condcode, fixupList);
         }
     }
#endif /* CVMJIT_PATCH_BASED_GC_CHECKS */
#endif /* !CVMJIT_TRAP_BASED_GC_CHECKS && !CVMJIT_PATCH_BASED_GC_CHECKS */
     CVMJITcsClearEmitInPlace(con);
}

static void
jsrToBlock(
    CVMJITCompilationContext* con,
    CVMJITIRBlock* target)
{
    CVMJITaddCodegenComment((con, "jsr to block L%d",
                             CVMJITirblockGetBlockID(target)));
    if (!(target->flags & CVMJITIRBLOCK_ADDRESS_FIXED)) {
        CVMCPUemitBranchLinkNeedFixup(con, target->logicalAddress,
                                      &(target->branchFixupList));
    } else {
        CVMCPUemitBranchLink(con, target->logicalAddress);
    }
}

#line 7073 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

typedef struct {
    CVMUint16 low;
    CVMUint16 high;
    CVMUint16 prevIndex;
    CVMInt32 logicalAddress; /* address that branches to this node */
} CVMLookupSwitchStackItem;
 
static void
pushLookupNode(CVMLookupSwitchStackItem* stack, CVMUint8* tos, 
               CVMUint16 low, CVMUint16 high, CVMUint16 prevIndex,
	       CVMInt32 logicalAddress)
{
    CVMLookupSwitchStackItem* item = &stack[*tos];
    CVMtraceJITCodegen(("---> Pushing #%d (low=%d index=%d high=%d prev=%d)\n",
                       *tos, low, (low + high)/2, high, prevIndex));
    item->low   = low;
    item->high  = high;
    item->prevIndex  = prevIndex;
    item->logicalAddress = logicalAddress;
    ++(*tos);
}

static CVMLookupSwitchStackItem*
popLookupNode(CVMLookupSwitchStackItem stack[], CVMUint8* tos)
{
    CVMLookupSwitchStackItem* item;
    if (*tos == 0) {
	return 0;
    }
    --(*tos);
    item = &stack[*tos];
    CVMtraceJITCodegen(("<--- Popping #%d (low=%d index=%d high=%d prev=%d)\n",
                       *tos, item->low, (item->low + item->high) / 2,
                       item->high, item->prevIndex));
    return item;
}
#line 7456 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


static void
emitBoundsCheck(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode,
		CVMBool isConstIndex, CVMInt32 constIndex)
{
    CVMRMResource* arrayLength = popResource(con);
    CVMRMResource* arrayIndex = NULL;

    CVMRMpinResource(CVMRM_INT_REGS(con), arrayLength,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

    /* compare arrayLength to the index */
    if (isConstIndex) {
	/* index is the constant passed in */
	CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_LS,
				  CVMRMgetRegisterNumber(arrayLength),
				  constIndex);
    } else {
	/* array index is the resource on the stack */
	arrayIndex = popResource(con);
	CVMRMpinResource(CVMRM_INT_REGS(con), arrayIndex,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_LS,
				  CVMRMgetRegisterNumber(arrayLength),
				  CVMRMgetRegisterNumber(arrayIndex));
    }
    
    /* trap if array bounds check failed */
    CVMRMsynchronizeJavaLocals(con);
    CVMJITaddCodegenComment((con,trapCheckComments[CVMJITIR_ARRAY_INDEX_OOB]));


/* IAI - 12 */
#if defined(IAI_CS_EXCEPTION_ENHANCEMENT) && !defined(IAI_CS_EXCEPTION_ENHANCEMENT2) 
    CVMJITcsSetExceptionInstruction(con);
#endif
/* IAI - 12 */


#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
    CVMJITcsSetArrayIndexOutofBoundsBranch(con);
#endif

    CVMCPUemitAbsoluteCallConditional(con,
        (void*)CVMCCMruntimeThrowArrayIndexOutOfBoundsExceptionGlue,
        CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK, CVMCPU_COND_LS);

/* IAI - 12 */
#ifndef IAI_CS_EXCEPTION_ENHANCEMENT
    CVMJITcsBeginBlock(con);
#endif
/* IAI - 12 */

    /* push the arrayIndex resource on the stack */
    if (isConstIndex) {
        pushIConst32(con, constIndex);
    } else {
	pushResource(con, arrayIndex); 
	CVMRMunpinResource(CVMRM_INT_REGS(con), arrayIndex);
    }

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), arrayLength);
}
#line 7590 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

void
emitReturn(CVMJITCompilationContext* con, CVMJITRMContext* rp, int size) {
        void* helper;
        CVMRMResource *src;
	if (size == 0) {
	    src = NULL;
	} else {
	    src  = popResource(con);
	    CVMassert(size == src->size);
	    CVMRMpinResource(rp, src,
			     rp->anySet, CVMRM_EMPTY_SET);
	    CVMRMstoreReturnValue(rp, src);
	}
	CVMCPUemitPopFrame(con, size);
	if (CVMmbIs(con->mb, SYNCHRONIZED)) {
	    CVMJITaddCodegenComment((con, "goto CVMCCMreturnFromSyncMethod"));
	    CVMJITsetSymbolName((con, "CVMCCMreturnFromSyncMethod"));
	    helper = (void*)CVMCCMreturnFromSyncMethod;
	} else {
	    CVMJITaddCodegenComment((con, "goto CVMCCMreturnFromMethod"));
	    CVMJITsetSymbolName((con, "CVMCCMreturnFromMethod"));
	    helper = (void*)CVMCCMreturnFromMethod;
	}
        /* Emit the one-way ticket home: */
        CVMCPUemitReturn(con, helper);
	CVMJITresetSymbolName(con);
	if (src != NULL) {
	    CVMRMrelinquishResource(rp, src);
	}
	CVMJITcsBeginBlock(con);
	CVMJITdumpRuntimeConstantPool(con, CVM_FALSE);
}
#line 7659 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


/*
 * instanceof and checkcast generate very similar code sequences:
 * though they call different helper functions, in other respects they
 * differ by only one instruction.
 */
static void
instanceTest(
    CVMJITCompilationContext*	con,
    CVMJITIRNodePtr             thisNode,
    CVMBool                     isInstanceOf)
{
    CVMRMResource *cbRes = popResource(con);
    CVMRMResource *src = popResource(con);
    int srcReg;
    void *helperFunction;
#ifdef CVM_TRACE_JIT
    char *helperFunctionName;
#endif
#ifdef IAI_CACHEDCONSTANT_INLINING
    CVMInt32 branchPC;
    CVMInt32 targetPC;
#endif

    if (isInstanceOf){
        CVMJITprintCodegenComment(("Do instanceof:"));
        helperFunction = (void*)CVMCCMruntimeInstanceOfGlue;
#ifdef CVM_TRACE_JIT
        helperFunctionName = "CVMCCMruntimeInstanceOfGlue";
#endif
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeInstanceOf);
    } else {
        CVMJITprintCodegenComment(("Do checkcast:"));
        helperFunction = (void*)CVMCCMruntimeCheckCastGlue;
#ifdef CVM_TRACE_JIT
        helperFunctionName = "CVMCCMruntimeCheckCastGlue";
#endif
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeCheckCast);
    }

    CVMJITprintCodegenComment(("Note: ARG3=cb to check against"));

    /* Make sure we flush dirty locals if we might exit without
       a "major" spill */
    CVMRMsynchronizeJavaLocals(con);

    cbRes = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), cbRes,
				     CVMCPU_ARG3_REG);
    CVMRMpinResourceStrict(CVMRM_INT_REGS(con), src,
			   CVMRM_SAFE_SET, ~CVMRM_SAFE_SET);
    srcReg = CVMRMgetRegisterNumber(src);

    /* A "major" spill is not needed here.  The helpers return
       without becoming gc-safe except when an exception is thrown,
       and in that case, we only care about the locals which are always
       flushed to memory anyway. */
    CVMRMminorSpill(con, ARG3);

    /* do shortcut tests: null can cast to anything, but
     * isn't instanceof anything
     * We can do the test and set ARG1 to 0 in one instruction.
     */
    CVMJITaddCodegenComment((con, "set cc to \"eq\" if object is null"));
#ifdef CVMCPU_HAS_ALU_SETCC
    CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE, CVMCPU_ARG1_REG, srcReg,
			   CVMJIT_SETCC);
#else
    CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE, CVMCPU_ARG1_REG, srcReg,
			   CVMJIT_NOSETCC);
    /* NOTE: normally we would emit a compare instruction here after
     * having called an ALU emitter with CVMJIT_SETCC. Howver, the setcc is
     * for the benefit of the assembler glue, so platforms without SETCC
     * support will just need to do the compare in the glue code.
     */
#endif

    CVMJITcsSetEmitInPlace(con);
#ifdef IAI_CACHEDCONSTANT
    /* Add cached constant into constant entry list and also
     * generate code to load the address of the constant into ARG4 */
    CVMJITsetSymbolName((con, "%s cachedCb at offset %d",
			 (isInstanceOf ? "instanceof" : "checkcast"),
			 CVMJITcbufGetLogicalPC(con)));
    CVMJITgetRuntimeCachedConstant32(con);
#endif /* IAI_CACHEDCONSTANT */

#ifdef IAI_CACHEDCONSTANT_INLINING	
    /*
     * Inline quick tests normally found in checkcastglue and instanceofglue.
     */

    /* Generate branch around inline check if object is NULL. This will
     * be patched below when we know the address of the target. */
    branchPC = CVMJITcbufGetLogicalPC(con);
    targetPC = branchPC;
    CVMJITaddCodegenComment((con, 
	(isInstanceOf ? "beq .instanceofDone" : "beq .checkcastDone")));
    CVMCPUemitBranch(con, targetPC, CVMCPU_COND_EQ);

    /* ldr ARG2, [ARG1]  @ a2 = object.cb */
    CVMJITaddCodegenComment((con, "a2 = object.cb"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
	CVMCPU_ARG2_REG, CVMCPU_ARG1_REG, 0);

    /* ldr ARG1, [ARG4] @ load the guess cb */
    CVMJITaddCodegenComment((con, "load the guess cb"));
    CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
	CVMCPU_ARG1_REG, CVMCPU_ARG4_REG, 0);

    /* bic ARG2, ARG2, #3 @ mask off low bits of object cb*/
    CVMJITaddCodegenComment((con, "mask off low bits of object cb"));
    CVMCPUemitBinaryALUConstant(con, CVMCPU_BIC_OPCODE,
	CVMCPU_ARG2_REG, CVMCPU_ARG2_REG, 0x3, CVMJIT_NOSETCC);

    /* cmp ARG1, ARG2 @ see if guess is correct */
    CVMJITaddCodegenComment((con, "see if guess is correct"));
    CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE, 
    	CVMCPU_COND_NE, CVMCPU_ARG1_REG, CVMCPU_ARG2_REG);

    if (isInstanceOf) {
	/* mov RESULT1, #1 */
	CVMJITaddCodegenComment((con, "assume guess is correct"));
	CVMCPUemitLoadConstant(con, CVMCPU_RESULT1_REG, 1);
    }

    /* call the helper glue if cachedCB is not equal to objectCB */
    CVMJITaddCodegenComment((con, "call %s if guess not correct",
			     helperFunctionName));
    CVMJITsetSymbolName((con, helperFunctionName));
    CVMCPUemitAbsoluteCallConditional(con, helperFunction,
                        CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH, CVMCPU_COND_NE);
#else /* !IAI_CACHEDCONSTANT_INLINING */
    /* call the helper glue with the cc set to "eq" if the object is null */
    CVMJITaddCodegenComment((con, "call %s", helperFunctionName));
    CVMJITsetSymbolName((con, helperFunctionName));
    CVMCPUemitAbsoluteCall(con, helperFunction, 
			   CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
#endif /* !IAI_CACHEDCONSTANT_INLINING */

#ifndef IAI_CACHEDCONSTANT
    /* reserve a word for the guessCB */
    CVMJITaddCodegenComment((con, "guessCB i.e. last successful cast CB"));
    CVMJITemitWord(con, 0);
#endif /* IAI_CACHEDCONSTANT */

    CVMtraceJITCodegen(("\t\t.%s\n",
			isInstanceOf ? "instanceofDone:" : "checkcastDone:"));

#ifdef IAI_CACHEDCONSTANT_INLINING
    /* patch branch around inline check when object is null */
    targetPC = CVMJITcbufGetLogicalPC(con);
    CVMJITcbufPushFixup(con, branchPC);
    CVMJITaddCodegenComment((con, 
	(isInstanceOf ? "beq .instanceofDone" : "beq .checkcastDone")));
    CVMCPUemitBranch(con, targetPC, CVMCPU_COND_EQ);
    CVMJITcbufPop(con);
#endif

    CVMJITcaptureStackmap(con, 0);
    CVMJITcsClearEmitInPlace(con);

    CVMRMrelinquishResource(CVMRM_INT_REGS(con), cbRes);

    if (!isInstanceOf) {
        /* For checkcast, return the original object:*/
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), src, thisNode);
        pushResource(con, src);
    } else {
        CVMRMResource *dest;
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
        dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
        pushResource(con, dest);
    }
    CVMJITcsBeginBlock(con);
}

#line 7898 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"


static CVMRMResource*
resolveConstant(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode,
		CVMRMregset target, CVMRMregset avoid)
{
    /*
     * For most cases:
     *          @ do a major spill first
     *
     * startpc:
     *          ldr     Rx, cachedConstant(PC)
     *          ldr     ARG3, =cpIndex    @ or: mov ARG3, =cpIndex.
     *          bl      =helper
     * cachedConstant:
     *          .word   -1
     * resolveReturn1:
     *          ldr     Rx, cachedConstant(PC)
     * resolveReturn2:
     *          @ capture stackmap here
     *
     * The helper will store the resolved constant at cachedConstant,
     * which it locates by the value in the link register.
     *
     * If static initialization of a class is needed, the helper will
     * do the initialization in a non-recursive way via the
     * interpreter loop, which will resume execution at resolveReturn1
     * when initialization is complete.
     *
     * If static initialization is currently being done by the current
     * thread, then the helper just returns immediately to
     * resolveReturn1.
     *
     * If static initialization is not needed, then the *second*
     * instruction of the above is patched to be the following:
     *
     *          b resolveReturn2
     *
     * NOTE: It's possible to do the above without the first ldr. However
     * there are two advantages to doing it this. The first is that by
     * doing the ldr and then the branch, you avoid a processor stall
     * if the first instruction after resolveReturn2 references the
     * cachedConstant, which it normally does. The 2nd is that we can't
     * safely patch the first instruction generated, because it might
     * be the first instruction of a basic block, and therefor may get
     * patched to handle a gc-rendezvous.
     *
     * If the platform does not support PC relative load, the 
     * `ldr Rx, cachedConstant(PC)' instructions can be implemented as:
     *
     *          ldr      Rx, =physicalPC + offsetToCachecConstantPC 
     *          ldr      Rx, [Rx, #0] 
     *
     *	For vtbl offset resolution, the pattern is a little different:
     *          @ do a major spill first
     *
     * startpc:
     *          ldr     Rx, cachedConstant(PC)
     *          ldr     ARG3, =cpIndex    @ or: mov ARG3, =cpIndex.
     *          bl      =helper
     * resolveReturn0:
     *          mov	rdest, RESULT1
     *		b	haveMB
     * cachedConstant:
     *          .word   -1
     * resolveReturn1:
     *          ldr     Rx, cachedConstant(PC)
     * resolveReturn2:
     *          @ capture stackmap here
     *
     * Since this sort of resolution cannot require running a static
     * initializer, we don't have any reason to consider it. However,
     * this resolution can yield an MB that does not have a vtable
     * offset. In this case, the helper will return the MB pointer directly
     * and return to resolveReturn0. In this case no patching will get
     * done and we'll continue to call the helper each time. Since both
     * the instructions at resolveReturn1 will need to be patched, we
     * push the address on the stack for patching in the FETCH_MB_FROM_VTABLE
     * rule. This special odd case never happens in real life, so
     * there is no real performance penalty here (just the code expansion).
     */

    CVMRMResource *dest;
    int destRegNo;
    int loadCachedConstantPC;
    int cachedConstantOffset;
    void *helper = NULL;
#ifdef CVM_TRACE_JIT
    char *helperName = NULL;
#endif
    CVMConstantPool* cp;
    CVMJITIRNode*    cpIndexNode;
    CVMUint16        cpIndex;
    CVMBool          needsCheckInit;
    CVMBool	     resolveVtblOffset = CVM_FALSE;
    CVMAddr	     returnMbPC = 0;

#undef setHelper

#ifdef CVM_TRACE_JIT
#define setHelper(x)				\
    helper = (void*)x##Glue;				\
    helperName = #x; 				\
    CVMJITstatsRecordInc(con, CVMJIT_STATS_##x)
#else
#define setHelper(x)				\
    helper = (void*)x##Glue;				\
    CVMJITstatsRecordInc(con, CVMJIT_STATS_##x)
#endif

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
    /* Make JSP point just past the last argument currently on the stack */
    CVMSMadjustJSP(con);
#endif


    cp = CVMcbConstantPool(CVMmbClassBlock(con->mb));
    cpIndexNode = CVMJITirnodeGetLeftSubtree(thisNode);
    cpIndex = CVMJITirnodeGetConstant32(cpIndexNode)->j.i;
    needsCheckInit =
       ((CVMJITirnodeGetUnaryNodeFlag(thisNode) & CVMJITUNOP_CLASSINIT) != 0);

    if (needsCheckInit) {
        switch(CVMJITirnodeGetTag(cpIndexNode) >> CVMJIT_SHIFT_OPCODE) {
            case CVMJIT_CONST_NEW_CB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a class:"));
                setHelper(CVMCCMruntimeResolveNewClassBlockAndClinit);
                break;
            case CVMJIT_CONST_GETSTATIC_FB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a static field:"));
                setHelper(CVMCCMruntimeResolveGetstaticFieldBlockAndClinit);
                break;
            case CVMJIT_CONST_PUTSTATIC_FB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a static field:"));
                setHelper(CVMCCMruntimeResolvePutstaticFieldBlockAndClinit);
                break;
            case CVMJIT_CONST_STATIC_MB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a static method:"));
                setHelper(CVMCCMruntimeResolveStaticMethodBlockAndClinit);
                break;
            default:
                CVMassert(CVM_FALSE); /* Unexpected irnode tag. */
        }
    } else {
        switch(CVMJITirnodeGetTag(cpIndexNode) >> CVMJIT_SHIFT_OPCODE) {
            case CVMJIT_CONST_CB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a class:"));
                setHelper(CVMCCMruntimeResolveClassBlock);
                break;
            case CVMJIT_CONST_ARRAY_CB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving an array class:"));
                setHelper(CVMCCMruntimeResolveArrayClassBlock);
                break;
            case CVMJIT_CONST_GETFIELD_FB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving an instance field:"));
                setHelper(CVMCCMruntimeResolveGetfieldFieldOffset);
                break;
            case CVMJIT_CONST_PUTFIELD_FB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving an instance field:"));
                setHelper(CVMCCMruntimeResolvePutfieldFieldOffset);
                break;
            case CVMJIT_CONST_SPECIAL_MB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a 'special' method:"));
                setHelper(CVMCCMruntimeResolveSpecialMethodBlock);
                break;
            case CVMJIT_CONST_INTERFACE_MB_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving an 'interface' method:"));
                setHelper(CVMCCMruntimeResolveMethodBlock);
                break;
            case CVMJIT_CONST_METHOD_TABLE_INDEX_UNRESOLVED:
                CVMJITprintCodegenComment(("Resolving a method table index:"));
                setHelper(CVMCCMruntimeResolveMethodTableOffset);
		resolveVtblOffset = CVM_TRUE;
                break;
            default:
                CVMassert(CVM_FALSE); /* Unexpected irnode tag. */
        }
    }

    /* Note: It is important to spill and evict every register because we may
       not return from the helper via a normal route.  It's almost like an
       exception being thrown except that we may return to this method and
       expect its state to be preserved just as if we haven't done the
       resolution yet. */
    CVMRMmajorSpill(con, CVMRM_EMPTY_SET, CVMRM_EMPTY_SET);
    CVMJITcsSetEmitInPlace(con);
    loadCachedConstantPC = CVMJITcbufGetLogicalPC(con);

    /*
     * Load the cached constant. We don't know exactly where it will be
     * yet, so we'll need to come back and patch this instruction later.
     */
    dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
    destRegNo = CVMRMgetRegisterNumber(dest);
    CVMJITaddCodegenComment((con, "load cachedConstant"));
    CVMCPUemitMemoryReferencePCRelative(con, CVMCPU_LDR32_OPCODE,
					destRegNo, 0);

    /* 
     * Set ARG3 = cpIndex. This instruction eventually gets patched to branch
     * around the call to the helper once class initialization is done.
     */
    CVMJITaddCodegenComment((con, "ARG3 = cpIndex"));
    CVMJITsetSymbolName((con, "cpIndex"));
    CVMCPUemitLoadConstant(con, CVMCPU_ARG3_REG, (CVMInt32)cpIndex);

    /*
     * Do the helper call. Note that we can't allow the contant pool to be
     * dumped or else the helper glue won't be able to locate the instruction
     * to patch.
     */
    CVMJITaddCodegenComment((con, "call %s", helperName));
    CVMJITsetSymbolName((con, helperName));
    CVMCPUemitAbsoluteCall(con, helper, CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);

    if (resolveVtblOffset){
	returnMbPC = CVMJITcbufGetLogicalPC(con);
	pushAddress(con, returnMbPC);
	/* these instructions to get patched later */
	CVMJITaddCodegenComment((con, "return MB directly"));
        CVMJITcsSetEmitInPlace(con);
	CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE, CVMCPU_RESULT1_REG,
			       CVMCPU_RESULT1_REG, CVMJIT_NOSETCC);
	CVMCPUemitBranch(con, 0, CVMCPU_COND_AL);
        CVMJITcsClearEmitInPlace(con);
    }

    /* fixup earlier load now that we know the pc of the cachedConstant */
    cachedConstantOffset = CVMJITcbufGetLogicalPC(con) - loadCachedConstantPC;
    CVMJITcbufPushFixup(con, loadCachedConstantPC);
    CVMJITcsSetEmitInPlace(con);
    CVMJITaddCodegenComment((con, "load cachedConstant"));
    CVMCPUemitMemoryReferencePCRelative(con, CVMCPU_LDR32_OPCODE,
					destRegNo, cachedConstantOffset);
    CVMJITcsClearEmitInPlace(con);
    CVMJITcbufPop(con);

    /* emit the cachedConstant word */
    CVMJITaddCodegenComment((con, "cachedConstant"));
    CVMJITemitWord(con, -1);

    CVMJITcaptureStackmap(con, 0); /* This is the return PC in the frame. */

    /*
     * Load the cached constant, which is in the word before the
     * current instruction.
     */
    CVMJITaddCodegenComment((con, "load cachedConstant"));
    CVMCPUemitMemoryReferencePCRelative(con, CVMCPU_LDR32_OPCODE, destRegNo,
					-(CVMInt32)sizeof(CVMUint32));
    CVMJITcsClearEmitInPlace(con);
    CVMJITcsBeginBlock(con);

    /* Occupy the target register with the resolved value: */
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    return dest;
}

static void
doCheckInit(CVMJITCompilationContext *con, CVMJITIRNodePtr thisNode)
{
    /*
     *          @ do a major spill first
     *
     *          ldr     ARG3, =cb      @ load cb to intialize
     *          bl      =helper        @ call helper to intialize cb
     *          @ capture stackmap here
     *
     *          @ If the class is aready initialized, then the helper will
     *          @ patch the first instruction above, and we end up with the 
     *          @ following:
     *
     *          b       checkinitDone
     *          bl      =helper        @ call helper to intialize cb
     * checkinitDone:
     */

    CVMClassBlock *cb =
        CVMJITirnodeGetConstantAddr(CVMJITirnodeGetLeftSubtree(thisNode))->cb;

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
    /* Make JSP point just past the last argument currently on the stack */
    CVMSMadjustJSP(con);
#endif

    /* Note: It is important to spill and evict every register because we may
       not return from the helper via a normal route.  It's almost like an
       exception being thrown except that we may return to this method and
       expect its state to be preserved just as if we haven't done the
       checkinit yet. */
    CVMRMmajorSpill(con, CVMRM_EMPTY_SET, CVMRM_EMPTY_SET);

    CVMJITprintCodegenComment(("Do checkinit:"));

    /*
     * Load the cb into an argument register for the helper. This instruction
     * eventually gets patched to branch around the call to the helper once
     * the class has been intialized.
     */
    CVMJITaddCodegenComment((con, "ARG3 = %C", cb));
    CVMJITsetSymbolName((con, "%C", cb));
    CVMCPUemitLoadConstant(con, CVMCPU_ARG3_REG, (CVMUint32)cb);

    /* 
     * Do the helper call. Note that we can't allow the contant pool to be
     * dumped or else the helper glue won't be able to locate the instruction
     * to patch.
     */
    CVMJITaddCodegenComment((con,
			     "call CVMCCMruntimeRunClassInitializerGlue"));
    CVMJITsetSymbolName((con, "CVMCCMruntimeRunClassInitializerGlue"));
    CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeRunClassInitializer);
    CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeRunClassInitializerGlue,
                           CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);
    CVMJITcsBeginBlock(con);
    CVMJITcaptureStackmap(con, 0); /* This is the return PC in the frame. */

    CVMJITprintCodegenComment(("checkinitDone:"));
}

#line 8286 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

#ifdef CVMJIT_COUNT_VIRTUAL_INLINE_STATS
 /* Count and report the hits and misses for each invocation of an
    inlined virtual invoke.
 */
typedef struct CVMMethodInfoStat CVMMethodInfoStat;
struct CVMMethodInfoStat {
    CVMMethodInfoStat *next;
    CVMMethodBlock *mb;
    CVMUint32 hit;
    CVMUint32 miss;
};

CVMMethodInfoStat *virtualInlineHitMissList = NULL;

static CVMMethodInfoStat*
findMethodInfo(CVMMethodBlock *mb)
{
    CVMMethodInfoStat *info = virtualInlineHitMissList;
    while (info != NULL) {
        if (info->mb == mb) {
            return info;
        }
        info = info->next;
    }
    return NULL;
}

static void
insertMethodInfo(CVMMethodBlock *mb, CVMBool isHit)
{
    CVMMethodInfoStat *info = calloc(1, sizeof(CVMMethodInfoStat));
    info->mb = mb;
    if (isHit) {
        info->hit++;
    } else {
        info->miss++;
    }
    info->next = virtualInlineHitMissList;
    virtualInlineHitMissList = info;
}


static void
reportVirtualInlineHitMiss(CVMMethodBlock *mb, CVMBool isHit)
{
    CVMExecEnv *ee = CVMgetEE();
    CVMMethodInfoStat *info;

    /*
    CVMconsolePrintf("Virt/Interf INLINE: %s %C.%M\n",
                     (isHit ? "Hit " : "Miss"),
                     CVMmbClassBlock(mb), mb);
    */
    CVMsysMicroLock(ee, CVM_CODE_MICROLOCK);
    info = findMethodInfo(mb);
    if (info != NULL) {
        if (isHit) {
            info->hit++;
        } else {
            info->miss++;
        }
    } else {
        insertMethodInfo(mb, isHit);
    }
    CVMsysMicroUnlock(ee, CVM_CODE_MICROLOCK);
}

void CVMJITprintVirtualInlineHitMiss()
{
    CVMExecEnv *ee = CVMgetEE();
    CVMMethodInfoStat *info;

    CVMsysMicroLock(ee, CVM_CODE_MICROLOCK);
    info = virtualInlineHitMissList;
    CVMconsolePrintf("Virtual/Interface Inline Hit Miss stats:\n");
    while (info != NULL) {
	CVMUint32 hit = info->hit;
        CVMUint32 miss = info->miss;
        CVMUint32 total = hit + miss;
        CVMconsolePrintf("   h %d (%2.2f%%): m %d (%2.2f%%): %C.%M\n",
			hit, (float)hit/(float)total * 100.0,
			miss, (float)miss/(float)total * 100.0,
 			CVMmbClassBlock(info->mb), info->mb);
        info = info->next;
    }
    CVMconsolePrintf("END of stats\n");
    CVMsysMicroUnlock(ee, CVM_CODE_MICROLOCK);
}

#endif /* CVMJIT_COUNT_VIRTUAL_INLINE_STATS */
#line 230 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

static CVMBool
canDoFloatLoadstore(const ArrayElemInfo* ei, CVMBool isStore){
    int opcode =  (isStore) ? ei->floatStoreOpcode : ei->floatLoadOpcode;
    return (opcode != CVM_ILLEGAL_OPCODE);
}

#line 593 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

static void
floatBinaryOp(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    int size,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* rhs = popResource(con);
    CVMRMResource* lhs = popResource(con);
    CVMRMResource* dest;
    int lhsRegNo = CVMRMgetRegisterNumberUnpinned(lhs);
    CVMJITRMContext* rc = CVMRM_FP_REGS(con);

    CVMRMpinResource(rc, rhs, ~target, target);

    /* If the dest node has a regHint and the register number is the same as
     * the register the lhs is already loaded into, then reuse the lhs
     * register as the dest register. This is common when locals are
     * incremented.
     */
    if (thisNode->decorationType == CVMJIT_REGHINT_DECORATION &&
	lhsRegNo != -1 &&
	(1U << lhsRegNo) == target &&
	CVMRMgetRefCount(rc, lhs) == 1)
    {
	/* relinquish first so dirty resources are not spilled */
	CVMRMrelinquishResource(rc, lhs);
	lhs = NULL;
	dest = CVMRMgetResourceSpecific(rc, lhsRegNo, size);
	CVMassert(lhsRegNo == CVMRMgetRegisterNumber(dest));
    } else {
	/* Pin early so following CVMRMgetResource does not cause spill */
	CVMRMpinResource(rc, lhs, ~target, target);
	dest = CVMRMgetResource(rc, target, avoid, size);
	lhsRegNo = CVMRMgetRegisterNumber(lhs);
    }

    CVMCPUemitBinaryFP(con, opcode,
		       CVMRMgetRegisterNumber(dest), lhsRegNo,
		       CVMRMgetRegisterNumber(rhs));
    if (lhs != NULL) {
	CVMRMrelinquishResource(rc, lhs);
    }
    CVMRMrelinquishResource(rc, rhs);
    CVMRMoccupyAndUnpinResource(rc, dest, thisNode);
    pushResource(con, dest);
}

static void
floatUnaryOp(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    int size,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest = CVMRMgetResource(CVMRM_FP_REGS(con),
					   target, avoid, size);
    CVMRMpinResource(CVMRM_FP_REGS(con), src,
		     CVMRM_FP_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUemitUnaryFP(con, opcode, CVMRMgetRegisterNumber(dest),
        CVMRMgetRegisterNumber(src));
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), src);
    CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), dest, thisNode);
    pushResource(con, dest);
}
#line 700 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

#ifdef CVM_NEED_COMPARE_FLOATS_HELPER

/* convert CVMJIT_XXX condition code to a CVMCPUCondCode */
static CVMCPUCondCode
mapFloatCondCode(CVMUint16 condition) {
    switch(condition) {
        case CVMJIT_EQ: return CVMCPU_COND_FEQ;
        case CVMJIT_NE: return CVMCPU_COND_FNE;
        case CVMJIT_LE: return CVMCPU_COND_FLE;
        case CVMJIT_GE: return CVMCPU_COND_FGE;
        case CVMJIT_LT: return CVMCPU_COND_FLT;
        case CVMJIT_GT: return CVMCPU_COND_FGT;
        default: CVMassert(CVM_FALSE); return 0;
    }
}

static void
compareFloats(
    CVMJITCompilationContext *con,
    CVMJITIRNodePtr thisNode,
    int opcode)
{
    CVMRMResource* rhs = popResource(con);
    CVMRMResource* lhs = popResource(con);
    CVMJITConditionalBranch* branch = CVMJITirnodeGetCondBranchOp(thisNode);
    CVMJITIRBlock* target = branch->target;
    CVMCPUCondCode condCode = mapFloatCondCode(branch->condition);

#ifndef CVMCPU_HAS_COMPARE
    /* pin before calling CVMCPUemitFCompare() */
    CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
#endif
    CVMRMpinResource(CVMRM_FP_REGS(con), lhs,
		     CVMRM_FP_ANY_SET, CVMRM_EMPTY_SET);
    CVMRMpinResource(CVMRM_FP_REGS(con), rhs,
		     CVMRM_FP_ANY_SET, CVMRM_EMPTY_SET);
    if (branch->flags & CVMJITCMPOP_UNORDERED_LT)
	condCode |= CVMCPU_COND_UNORDERED_LT;
    CVMCPUemitFCompare(con, opcode, condCode,
		      CVMRMgetRegisterNumber(lhs),
		      CVMRMgetRegisterNumber(rhs));

    CVMRMsynchronizeJavaLocals(con);
#ifdef CVMCPU_HAS_COMPARE
    /* no longer need resource used in CVMCPUemitCompare() */
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), rhs);
    /* pin after calling CVMCPUemiFtCompare() */
    CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
#endif
    branchToBlock(con, condCode, target);

#ifndef CVMCPU_HAS_COMPARE
    /* no longer need resource used in CVMCPUemitCompare() */
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), lhs);
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), rhs);
#endif
    CVMRMunpinAllIncomingLocals(con, target);
}

#endif
#line 40 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"

static void
moveIntToFPRegs(
    CVMJITCompilationContext* con,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest;

    CVMassert( (src->size == 2)
		||  (CVMJITgetOpcode(thisNode) == 
		    (CVMJIT_CONST_JAVA_NUMERIC32 << CVMJIT_SHIFT_OPCODE))
		|| (CVMJITgetTypeTag(thisNode) == CVM_TYPEID_FLOAT)
		|| (CVMJITgetTypeTag(thisNode) == CVMJIT_TYPEID_32BITS));

    if (src->size == 1) {
        dest = CVMRMgetResource(CVMRM_FP_REGS(con), target, avoid, 1);
        CVMRMpinResource(CVMRM_INT_REGS(con), src,
                         CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMCPUemitUnaryFP(con, CVMMIPS_MTC1_OPCODE, 
                          CVMRMgetRegisterNumber(dest),
                          CVMRMgetRegisterNumber(src));
    } else {
        dest = CVMRMcloneResource(CVMRM_INT_REGS(con), src,
			      CVMRM_FP_REGS(con), target, avoid);
    }
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    /*
     * unpin without occupying
     * to occupy would cause big trouble if the thing is an
     * IDENT.
     */
    CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), dest, NULL);
    pushResource(con, dest);
}

static void
moveFPToIntRegs(
    CVMJITCompilationContext* con,
    CVMJITIRNodePtr thisNode,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest;

    CVMassert( (src->size == 2)
		||  (CVMJITgetOpcode(thisNode) == 
		    (CVMJIT_CONST_JAVA_NUMERIC32 << CVMJIT_SHIFT_OPCODE))
		|| (CVMJITgetTypeTag(thisNode) == CVM_TYPEID_FLOAT));

    if (src->size == 1) {
        dest = CVMRMgetResource(CVMRM_INT_REGS(con), target, avoid, 1);
        CVMRMpinResource(CVMRM_FP_REGS(con), src, 
                         CVMRM_FP_ANY_SET, CVMRM_EMPTY_SET);
        CVMCPUemitUnaryFP(con, CVMMIPS_MFC1_OPCODE,
                          CVMRMgetRegisterNumber(dest),
                          CVMRMgetRegisterNumber(src));
    } else {
        dest = CVMRMcloneResource(CVMRM_FP_REGS(con), src, 
                                  CVMRM_INT_REGS(con),
			          target, avoid);
    }
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), src);
    /*
     * unpin without occupying
     * to occupy would cause big trouble if the thing is an
     * FIDENT.
     */
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, NULL);
    pushResource(con, dest);
}

static void
mipsIntToFloat(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    int size,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest = CVMRMgetResource(CVMRM_FP_REGS(con),
					   target, avoid, size);
    CVMRMpinResource(CVMRM_INT_REGS(con), src,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUemitUnaryFP(con, opcode, CVMRMgetRegisterNumber(dest),
        CVMRMgetRegisterNumber(src));
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), dest, thisNode);
    pushResource(con, dest);
}

#if 0 /* not currently used */
static void
mipsFloatToInt(
    CVMJITCompilationContext* con,
    int opcode,
    CVMJITIRNodePtr thisNode,
    int size,
    CVMRMregset target,
    CVMRMregset avoid)
{
    CVMRMResource* src = popResource(con);
    CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					   target, avoid, size);
    CVMRMpinResource(CVMRM_FP_REGS(con), src,
		     CVMRM_FP_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUemitUnaryFP(con, opcode, CVMRMgetRegisterNumber(dest),
        CVMRMgetRegisterNumber(src));
    CVMRMrelinquishResource(CVMRM_FP_REGS(con), src);
    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, thisNode);
    pushResource(con, dest);
}
#endif
#include "../linux-mips-/./generated/javavm/include/jit/jitcodegen.h"
extern const unsigned char	CVMJITCompileExpression_IDENT32_transition[205];
extern const unsigned char	CVMJITCompileExpression_ASSIGN_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_ASSIGN_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_ASSIGN_transition[12][5];
extern const unsigned char	CVMJITCompileExpression_FETCH32_transition[205];
extern const unsigned char	CVMJITCompileExpression_FETCH64_transition[205];
extern const unsigned char	CVMJITCompileExpression_IMUL32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IMUL32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IMUL32_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_IDIV32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IDIV32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IDIV32_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_IREM32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IREM32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IREM32_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_SLL32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SLL32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SLL32_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_SRL32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRL32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRL32_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_SRA32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRA32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRA32_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_SLL64_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SLL64_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SLL64_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_SRL64_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRL64_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRL64_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_SRA64_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRA64_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_SRA64_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_BCOND_INT_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_BCOND_INT_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_BCOND_INT_transition[2][3];
extern const unsigned char	CVMJITCompileExpression_BOUNDS_CHECK_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_BOUNDS_CHECK_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_BOUNDS_CHECK_transition[3][2];
extern const unsigned char	CVMJITCompileExpression_IRETURN_transition[205];
extern const unsigned char	CVMJITCompileExpression_LRETURN_transition[205];
extern const unsigned char	CVMJITCompileExpression_VINTRINSIC_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_VINTRINSIC_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_VINTRINSIC_transition[3][2];
extern const unsigned char	CVMJITCompileExpression_INTRINSIC32_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_INTRINSIC32_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_INTRINSIC32_transition[3][2];
extern const unsigned char	CVMJITCompileExpression_INTRINSIC64_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_INTRINSIC64_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_INTRINSIC64_transition[3][2];
extern const unsigned char	CVMJITCompileExpression_IARG_l_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IARG_r_map[ 205 ];
extern const unsigned char	CVMJITCompileExpression_IARG_transition[3][2];
extern const char CVMJITCompileExpression_isdag[ 267 ];
#define CVMJIT_JCS_MATCH_DAG
#define	CVMJIT_JCS_STATE_MASK(v)	(v & 0x0fff)
#define	CVMJIT_JCS_STATE_MATCHED	0x1000
#define CVMJIT_JCS_STATE_SYNTHED	0x2000
#define CVMJIT_JCS_STATE_SUBACTED 0x4000
#define CVMJIT_JCS_STATE_ACTED	0x8000
#define CVMJIT_JCS_SET_STATE(p, flag)	IRRecordState(p, IRGetState(p)|flag)
#define CVMJIT_JCS_DID_PHASE(v, flag)	(((v) & (flag)) != 0)
#define CVMJIT_DID_SEMANTIC_ACTION(p)	(CVMJIT_JCS_DID_PHASE(IRGetState(p), CVMJIT_JCS_STATE_ACTED))

int
CVMJITCompileExpression_match( CVMJITIRNodePtr p, CVMJITCompilationContext* con)
{
	unsigned char l = 255, r = 255, result;
	CVMJITIRNodePtr left_p, right_p;
	int opcode, n_operands;
#ifdef CVMJIT_JCS_MATCH_DAG
	int stateno;
#endif
	struct CVMJITCompileExpression_match_computation_state * top_state;
	INITIALIZE_MATCH_STACK;
    new_node:
#ifdef CVMJIT_JCS_MATCH_DAG
	stateno = IRGetState(p);
	if (stateno >= CVMJIT_JCS_STATE_MATCHED){
		goto skip_match;
	}
#endif
	opcode = CVMJITgetMassagedIROpcode(con, p);
	switch( opcode ){
	case NEW_ARRAY_REF:
	case MULTI_NEW_ARRAY_REF:
	case ASSIGN:
	case IINVOKE:
	case LINVOKE:
	case VINVOKE:
	case IPARAMETER:
	case LPARAMETER:
	case FETCH_MB_FROM_VTABLE:
	case FETCH_MB_FROM_ITABLE:
	case FETCH_MB_FROM_VTABLE_OUTOFLINE:
	case MB_TEST_OUTOFLINE:
	case INDEX:
	case FIELDREFOBJ:
	case FIELDREF32:
	case FIELDREF64:
	case FIELDREF64VOL:
	case ISEQUENCE_R:
	case LSEQUENCE_R:
	case VSEQUENCE_R:
	case ISEQUENCE_L:
	case LSEQUENCE_L:
	case VSEQUENCE_L:
	case IADD32:
	case ISUB32:
	case IMUL32:
	case IDIV32:
	case IREM32:
	case AND32:
	case OR32:
	case XOR32:
	case SLL32:
	case SRL32:
	case SRA32:
	case IADD64:
	case ISUB64:
	case IMUL64:
	case IDIV64:
	case IREM64:
	case AND64:
	case OR64:
	case XOR64:
	case SLL64:
	case SRL64:
	case SRA64:
	case LCMP:
	case FADD:
	case FSUB:
	case FMUL:
	case FDIV:
	case FREM:
	case FCMPL:
	case FCMPG:
	case DADD:
	case DSUB:
	case DMUL:
	case DDIV:
	case DREM:
	case DCMPL:
	case DCMPG:
	case BCOND_INT:
	case BCOND_LONG:
	case BCOND_FLOAT:
	case BCOND_DOUBLE:
	case BOUNDS_CHECK:
	case CHECKCAST:
	case INSTANCEOF:
	case CHECKINIT:
	case VINTRINSIC:
	case INTRINSIC32:
	case INTRINSIC64:
	case IARG:
	case FPARAMETER:
	case DPARAMETER:
	case FINVOKE:
	case DINVOKE:
	case FSEQUENCE_R:
	case DSEQUENCE_R:
	case FSEQUENCE_L:
	case DSEQUENCE_L:
		left_p = CVMJITirnodeGetLeftSubtree(p);
		right_p = CVMJITirnodeGetRightSubtree(p);
		MATCH_PUSH( p, opcode, left_p, right_p, 0, 2 );
		break;
	case STATIC32:
	case STATIC64:
	case STATIC64VOL:
	case IDENT32:
	case IDENT64:
	case NEW_OBJECT:
	case NEW_ARRAY_BASIC:
	case DEFINE_VALUE32:
	case DEFINE_VALUE64:
	case GET_VTBL:
	case GET_ITBL:
	case FETCH_VCB:
	case FETCH32:
	case FETCH64:
	case ALENGTH:
	case NULLCHECK:
	case INEG32:
	case NOT32:
	case INT2BIT32:
	case INEG64:
	case FNEG:
	case DNEG:
	case I2B:
	case I2C:
	case I2S:
	case I2L:
	case I2F:
	case I2D:
	case L2I:
	case L2F:
	case L2D:
	case F2D:
	case F2I:
	case F2L:
	case D2F:
	case D2I:
	case D2L:
	case TABLESWITCH:
	case LOOKUPSWITCH:
	case RET:
	case IRETURN:
	case LRETURN:
	case ATHROW:
	case MONITOR_ENTER:
	case MONITOR_EXIT:
	case FIDENT:
	case DIDENT:
	case FDEFINE:
	case DDEFINE:
		left_p = CVMJITirnodeGetLeftSubtree(p);
		right_p = NULL;
		MATCH_PUSH( p, opcode, left_p, NULL, 0, 1 );
		break;
	default:
		goto assign_state; /* leaves need not worry */
	}
	top_state = GET_MATCH_STACK_TOP;
    continue_processing:
	if ( top_state->which_submatch < top_state->n_submatch ){
		p = top_state->subtrees[top_state->which_submatch++];
		goto new_node;
	}
	/* done with subtrees, go assign state.*/
	MATCH_POP( p, opcode, left_p, right_p, n_operands )
	switch ( n_operands ){
	    case 2:
		r = CVMJIT_JCS_STATE_MASK(IRGetState( right_p ));
		/* FALLTHRU */
	    case 1:
		l = CVMJIT_JCS_STATE_MASK(IRGetState( left_p ));
	}
    assign_state:
	/* IN THE FUTURE, DO THIS ALL DIFFERENT */
	/* now use appropriate transition function */
	switch ( opcode ){
	case LOCAL32:
		result = 1;
		break;
	case LOCAL64:
		result = 2;
		break;
	case STATIC32:
		result = 26;
		break;
	case STATIC64:
		result = 27;
		break;
	case STATIC64VOL:
		result = 28;
		break;
	case IDENT32:
		result = CVMJITCompileExpression_IDENT32_transition[l];
		break;
	case IDENT64:
		result = 31;
		break;
	case EXCEPTION_OBJECT:
		result = 3;
		break;
	case NEW_OBJECT:
		result = 32;
		break;
	case NEW_ARRAY_REF:
		result = 33;
		break;
	case MULTI_NEW_ARRAY_REF:
		result = 34;
		break;
	case NEW_ARRAY_BASIC:
		result = 35;
		break;
	case DEFINE_VALUE32:
		result = 36;
		break;
	case DEFINE_VALUE64:
		result = 37;
		break;
	case LOAD_PHIS:
		result = 4;
		break;
	case RELEASE_PHIS:
		result = 5;
		break;
	case USED32:
		result = 6;
		break;
	case USED64:
		result = 7;
		break;
	case ICONST_32:
		result = 8;
		break;
	case ICONST_64:
		result = 9;
		break;
	case STRING_ICELL_CONST:
		result = 10;
		break;
	case METHOD_BLOCK:
		result = 11;
		break;
	case CLASS_BLOCK:
		result = 12;
		break;
	case ASSIGN:
		result = CVMJITCompileExpression_ASSIGN_transition[CVMJITCompileExpression_ASSIGN_l_map[l]][CVMJITCompileExpression_ASSIGN_r_map[r]];
		break;
	case IINVOKE:
		result = 42;
		break;
	case LINVOKE:
		result = 43;
		break;
	case VINVOKE:
		result = 44;
		break;
	case IPARAMETER:
		result = 45;
		break;
	case LPARAMETER:
		result = 46;
		break;
	case NULL_PARAMETER:
		result = 13;
		break;
	case GET_VTBL:
		result = 47;
		break;
	case GET_ITBL:
		result = 48;
		break;
	case FETCH_MB_FROM_VTABLE:
		result = 187;
		break;
	case FETCH_MB_FROM_ITABLE:
		result = 188;
		break;
	case FETCH_VCB:
		result = 49;
		break;
	case FETCH_MB_FROM_VTABLE_OUTOFLINE:
		result = 50;
		break;
	case MB_TEST_OUTOFLINE:
		result = 51;
		break;
	case FETCH32:
		result = CVMJITCompileExpression_FETCH32_transition[l];
		break;
	case FETCH64:
		result = CVMJITCompileExpression_FETCH64_transition[l];
		break;
	case INDEX:
		result = 52;
		break;
	case ALENGTH:
		result = 53;
		break;
	case NULLCHECK:
		result = 54;
		break;
	case FIELDREFOBJ:
		result = 55;
		break;
	case FIELDREF32:
		result = 56;
		break;
	case FIELDREF64:
		result = 57;
		break;
	case FIELDREF64VOL:
		result = 58;
		break;
	case ISEQUENCE_R:
		result = 59;
		break;
	case LSEQUENCE_R:
		result = 60;
		break;
	case VSEQUENCE_R:
		result = 61;
		break;
	case ISEQUENCE_L:
		result = 62;
		break;
	case LSEQUENCE_L:
		result = 63;
		break;
	case VSEQUENCE_L:
		result = 64;
		break;
	case INEG32:
		result = 65;
		break;
	case NOT32:
		result = 66;
		break;
	case INT2BIT32:
		result = 67;
		break;
	case IADD32:
		result = 68;
		break;
	case ISUB32:
		result = 69;
		break;
	case IMUL32:
		result = CVMJITCompileExpression_IMUL32_transition[CVMJITCompileExpression_IMUL32_l_map[l]][CVMJITCompileExpression_IMUL32_r_map[r]];
		break;
	case IDIV32:
		result = CVMJITCompileExpression_IDIV32_transition[CVMJITCompileExpression_IDIV32_l_map[l]][CVMJITCompileExpression_IDIV32_r_map[r]];
		break;
	case IREM32:
		result = CVMJITCompileExpression_IREM32_transition[CVMJITCompileExpression_IREM32_l_map[l]][CVMJITCompileExpression_IREM32_r_map[r]];
		break;
	case AND32:
		result = 76;
		break;
	case OR32:
		result = 77;
		break;
	case XOR32:
		result = 78;
		break;
	case SLL32:
		result = CVMJITCompileExpression_SLL32_transition[CVMJITCompileExpression_SLL32_l_map[l]][CVMJITCompileExpression_SLL32_r_map[r]];
		break;
	case SRL32:
		result = CVMJITCompileExpression_SRL32_transition[CVMJITCompileExpression_SRL32_l_map[l]][CVMJITCompileExpression_SRL32_r_map[r]];
		break;
	case SRA32:
		result = CVMJITCompileExpression_SRA32_transition[CVMJITCompileExpression_SRA32_l_map[l]][CVMJITCompileExpression_SRA32_r_map[r]];
		break;
	case INEG64:
		result = 85;
		break;
	case IADD64:
		result = 86;
		break;
	case ISUB64:
		result = 87;
		break;
	case IMUL64:
		result = 88;
		break;
	case IDIV64:
		result = 89;
		break;
	case IREM64:
		result = 90;
		break;
	case AND64:
		result = 91;
		break;
	case OR64:
		result = 92;
		break;
	case XOR64:
		result = 93;
		break;
	case SLL64:
		result = CVMJITCompileExpression_SLL64_transition[CVMJITCompileExpression_SLL64_l_map[l]][CVMJITCompileExpression_SLL64_r_map[r]];
		break;
	case SRL64:
		result = CVMJITCompileExpression_SRL64_transition[CVMJITCompileExpression_SRL64_l_map[l]][CVMJITCompileExpression_SRL64_r_map[r]];
		break;
	case SRA64:
		result = CVMJITCompileExpression_SRA64_transition[CVMJITCompileExpression_SRA64_l_map[l]][CVMJITCompileExpression_SRA64_r_map[r]];
		break;
	case LCMP:
		result = 100;
		break;
	case FNEG:
		result = 101;
		break;
	case FADD:
		result = 102;
		break;
	case FSUB:
		result = 103;
		break;
	case FMUL:
		result = 104;
		break;
	case FDIV:
		result = 105;
		break;
	case FREM:
		result = 106;
		break;
	case FCMPL:
		result = 107;
		break;
	case FCMPG:
		result = 108;
		break;
	case DNEG:
		result = 109;
		break;
	case DADD:
		result = 110;
		break;
	case DSUB:
		result = 111;
		break;
	case DMUL:
		result = 112;
		break;
	case DDIV:
		result = 113;
		break;
	case DREM:
		result = 114;
		break;
	case DCMPL:
		result = 115;
		break;
	case DCMPG:
		result = 116;
		break;
	case I2B:
		result = 117;
		break;
	case I2C:
		result = 118;
		break;
	case I2S:
		result = 119;
		break;
	case I2L:
		result = 120;
		break;
	case I2F:
		result = 121;
		break;
	case I2D:
		result = 122;
		break;
	case L2I:
		result = 123;
		break;
	case L2F:
		result = 124;
		break;
	case L2D:
		result = 125;
		break;
	case F2D:
		result = 126;
		break;
	case F2I:
		result = 127;
		break;
	case F2L:
		result = 128;
		break;
	case D2F:
		result = 129;
		break;
	case D2I:
		result = 130;
		break;
	case D2L:
		result = 131;
		break;
	case TABLESWITCH:
		result = 132;
		break;
	case LOOKUPSWITCH:
		result = 133;
		break;
	case BCOND_INT:
		result = CVMJITCompileExpression_BCOND_INT_transition[CVMJITCompileExpression_BCOND_INT_l_map[l]][CVMJITCompileExpression_BCOND_INT_r_map[r]];
		break;
	case BCOND_LONG:
		result = 135;
		break;
	case BCOND_FLOAT:
		result = 136;
		break;
	case BCOND_DOUBLE:
		result = 137;
		break;
	case BOUNDS_CHECK:
		result = CVMJITCompileExpression_BOUNDS_CHECK_transition[CVMJITCompileExpression_BOUNDS_CHECK_l_map[l]][CVMJITCompileExpression_BOUNDS_CHECK_r_map[r]];
		break;
	case GOTO:
		result = 14;
		break;
	case RETURN:
		result = 15;
		break;
	case JSR:
		result = 16;
		break;
	case JSR_RETURN_ADDRESS:
		result = 17;
		break;
	case RET:
		result = 140;
		break;
	case IRETURN:
		result = CVMJITCompileExpression_IRETURN_transition[l];
		break;
	case LRETURN:
		result = CVMJITCompileExpression_LRETURN_transition[l];
		break;
	case ATHROW:
		result = 145;
		break;
	case CHECKCAST:
		result = 146;
		break;
	case INSTANCEOF:
		result = 147;
		break;
	case MONITOR_ENTER:
		result = 148;
		break;
	case MONITOR_EXIT:
		result = 149;
		break;
	case RESOLVE_REF:
		result = 18;
		break;
	case CHECKINIT:
		result = 150;
		break;
	case MAP_PC:
		result = 19;
		break;
	case BEGININLINING:
		result = 20;
		break;
	case VENDINLINING:
		result = 21;
		break;
	case OUTOFLINEINVOKE:
		result = 22;
		break;
	case VINTRINSIC:
		result = CVMJITCompileExpression_VINTRINSIC_transition[CVMJITCompileExpression_VINTRINSIC_l_map[l]][CVMJITCompileExpression_VINTRINSIC_r_map[r]];
		break;
	case INTRINSIC32:
		result = CVMJITCompileExpression_INTRINSIC32_transition[CVMJITCompileExpression_INTRINSIC32_l_map[l]][CVMJITCompileExpression_INTRINSIC32_r_map[r]];
		break;
	case INTRINSIC64:
		result = CVMJITCompileExpression_INTRINSIC64_transition[CVMJITCompileExpression_INTRINSIC64_l_map[l]][CVMJITCompileExpression_INTRINSIC64_r_map[r]];
		break;
	case IARG:
		result = CVMJITCompileExpression_IARG_transition[CVMJITCompileExpression_IARG_l_map[l]][CVMJITCompileExpression_IARG_r_map[r]];
		break;
	case NULL_IARG:
		result = 23;
		break;
	case FPARAMETER:
		result = 159;
		break;
	case DPARAMETER:
		result = 160;
		break;
	case FINVOKE:
		result = 161;
		break;
	case DINVOKE:
		result = 162;
		break;
	case FIDENT:
		result = 163;
		break;
	case DIDENT:
		result = 164;
		break;
	case FDEFINE:
		result = 165;
		break;
	case DDEFINE:
		result = 166;
		break;
	case FUSED:
		result = 24;
		break;
	case DUSED:
		result = 25;
		break;
	case FSEQUENCE_R:
		result = 167;
		break;
	case DSEQUENCE_R:
		result = 168;
		break;
	case FSEQUENCE_L:
		result = 169;
		break;
	case DSEQUENCE_L:
		result = 170;
		break;
	default:
#if defined(CVM_DEBUG) || defined(CVM_TRACE_JIT)
		con->errNode = p;
#endif
		return -1;
        }
	IRRecordState(p, result|CVMJIT_JCS_STATE_MATCHED);
#ifdef CVMJIT_JCS_MATCH_DAG
    skip_match:
#endif
	if ( MATCH_STACK_EMPTY ){
	    return 0;
	}
	top_state = GET_MATCH_STACK_TOP;
	goto continue_processing;
}

enum goals { goal_BAD , goal_effect, goal_root, goal_reg32, goal_reg64, goal_aluRhs, goal_memSpec, goal_parameters, goal_iconst32Index, goal_arrayIndex, goal_arraySubscript, goal_arrayAssignmentRhs32, goal_invoke32_result, goal_invoke64_result, goal_iargs, goal_intrinsicMB, goal_voffMemSpec, goal_param32, goal_param64, goal_freg32, goal_freg64, goal_finvoke32_result, goal_finvoke64_result, goal_fparam32, goal_fparam64 };

extern const short	CVMJITCompileExpression_rule_to_use[205][25];
extern const struct CVMJITCompileExpression_action_pairs_t CVMJITCompileExpression_action_pairs[];
extern const struct CVMJITCompileExpression_rule_action CVMJITCompileExpression_rule_actions[];
int
CVMJITCompileExpression_synthesis( CVMJITIRNodePtr CVMJITCompileExpression_thing_p, CVMJITCompilationContext* con)
{
	INITIALIZE_STACK_STATS(SYNTHESIS)
	int ruleno, subgoals_todo, goal, i;
	unsigned int stateno;
	const struct CVMJITCompileExpression_action_pairs_t *app = NULL;
	struct CVMJITCompileExpression_rule_computation_state*  goal_top;
	CVMJITIRNodePtr *submatch_roots = NULL;
	INITIALIZE_GOAL_STACK;
	goal = goal_root;
start_rule:
	stateno = IRGetState(CVMJITCompileExpression_thing_p);
	ruleno = CVMJITCompileExpression_rule_to_use[ CVMJIT_JCS_STATE_MASK(stateno) ][ goal ];
#ifdef CVMJIT_JCS_MATCH_DAG
	if (CVMJIT_JCS_DID_PHASE(stateno, CVMJIT_JCS_STATE_SYNTHED)){
	    if (CVMJITCompileExpression_isdag[ruleno]){
		/*
		CVMconsolePrintf(">Synth: skipping node %d rule %d\n", CVMJITCompileExpression_thing_p->nodeID, ruleno);
		*/
		goto skip_synth;
	    }
	}
#endif
	if (ruleno == 0 ) {
#if defined(CVM_DEBUG) || defined(CVM_TRACE_JIT)
	    con->errNode = CVMJITCompileExpression_thing_p;
#endif
	    return -1;
	}
	subgoals_todo = CVMJITCompileExpression_rule_actions[ruleno].n_subgoals;
	app = &CVMJITCompileExpression_action_pairs[CVMJITCompileExpression_rule_actions[ruleno].action_pair_index];
	goal_top->p = CVMJITCompileExpression_thing_p;
	goal_top->ruleno = ruleno;
	goal_top->subgoals_todo = subgoals_todo;
	goal_top->app = app;
	submatch_roots = goal_top->submatch_roots;
	goal_top->curr_submatch_root = submatch_roots;
	for (i=0; i<subgoals_todo; i++){
	    switch (app[i].pathno ){
		case 0:
		    submatch_roots[i]=CVMJITCompileExpression_thing_p;
		    break;
		case 1:
		    submatch_roots[i]=CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
		    break;
		case 2:
		    submatch_roots[i]=CVMJITirnodeGetLeftSubtree(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p));
		    break;
		case 3:
		    submatch_roots[i]=CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
		    break;
		case 4:
		    submatch_roots[i]=CVMJITirnodeGetRightSubtree(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p));
		    break;
		case 5:
		    submatch_roots[i]=CVMJITirnodeGetLeftSubtree(CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p));
		    break;
		}
	}
continue_rule:
	if ( goal_top->subgoals_todo > 0 ){
		app = (goal_top->app)++;
		goal_top->subgoals_todo -= 1;
		CVMJITCompileExpression_thing_p = *goal_top->curr_submatch_root++;
		goal = app->subgoal;
		goal_top += 1;
		statsPushStack(SYNTHESIS);
		validateStack((goal_top < GOAL_STACK_TOP), Synthesis);
		goto start_rule;
	}
	CVMJITCompileExpression_thing_p = goal_top->p;
	submatch_roots = goal_top->submatch_roots;
	ruleno = goal_top->ruleno;
#ifdef CVMJIT_JCS_MATCH_DAG
    skip_synth:
#endif
	switch( ruleno ){
	case 0:
#if defined(CVM_DEBUG) || defined(CVM_TRACE_JIT)
		con->errNode = CVMJITCompileExpression_thing_p;
#endif
		return -1;
	case 6:
#line 398 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 29:
#line 1177 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 31:
#line 1199 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 32:
#line 1271 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 33:
#line 1308 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 34:
#line 1371 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 38:
#line 1509 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    IDENT_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 39:
#line 1529 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    IDENT_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 40:
#line 1541 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    IDENT_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 41:
#line 1566 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    IDENT_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 52:
#line 2928 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 54:
#line 2945 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 56:
#line 2963 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 57:
#line 2971 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 58:
#line 2980 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 59:
#line 2989 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 60:
#line 2998 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 61:
#line 3007 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 66:
#line 3077 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 67:
#line 3086 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 69:
#line 3109 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 70:
#line 3118 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 72:
#line 3144 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 73:
#line 3153 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 74:
#line 3162 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 75:
#line 3171 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 76:
#line 3180 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 77:
#line 3189 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 85:
#line 3253 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 86:
#line 3262 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 90:
#line 3423 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 91:
#line 3432 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 92:
#line 3441 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 94:
#line 3461 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 95:
#line 3469 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 96:
#line 3478 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 97:
#line 3487 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 98:
#line 3496 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 99:
#line 3505 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 103:
#line 4140 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    ARRAY_LOAD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 104:
#line 4146 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    ARRAY_LOAD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 107:
#line 4205 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    GETFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 108:
#line 4219 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    GETFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 109:
#line 4233 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    GETFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 110:
#line 4243 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 111:
#line 4494 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    PUTFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 112:
#line 4583 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    PUTFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 113:
#line 4597 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    PUTFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 114:
#line 4607 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 121:
#line 4948 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    ARRAY_STORE_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 122:
#line 4954 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    ARRAY_STORE_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 123:
#line 4966 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
DEFINE_SYNTHESIS(con, CVMJITCompileExpression_thing_p) 
	break;
	case 124:
#line 4974 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
DEFINE_SYNTHESIS(con, CVMJITCompileExpression_thing_p) 
	break;
	case 129:
#line 5205 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 130:
#line 5212 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 131:
#line 5221 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 137:
#line 5499 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p); 
	break;
	case 138:
#line 5514 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p); 
	break;
	case 139:
#line 5529 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p); 
	break;
	case 140:
#line 5544 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p); 
	break;
	case 141:
#line 5559 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p); 
	break;
	case 142:
#line 5574 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p); 
	break;
	case 145:
#line 5685 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 147:
#line 5793 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 148:
#line 5827 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 149:
#line 5878 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 166:
#line 6281 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 167:
#line 6287 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 168:
#line 6294 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 169:
#line 6304 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 170:
#line 6316 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 171:
#line 6329 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 172:
#line 6341 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 186:
#line 7841 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 187:
#line 7847 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 190:
#line 8220 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 191:
#line 8226 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 193:
#line 8242 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_AVOID_C_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 204:
#line 121 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    GETFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 207:
#line 156 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    GETFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 208:
#line 166 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 209:
#line 175 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
SET_AVOID_METHOD_CALL(CVMJITCompileExpression_thing_p); 
	break;
	case 218:
#line 240 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    ARRAY_LOAD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 220:
#line 274 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    ARRAY_LOAD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 225:
#line 352 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    PUTFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 228:
#line 384 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    ARRAY_STORE_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 231:
#line 435 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    PUTFIELD_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 234:
#line 458 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    ARRAY_STORE_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 236:
#line 495 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    IDENT_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 237:
#line 514 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    IDENT_SYNTHESIS(con, CVMJITCompileExpression_thing_p); 
	break;
	case 242:
#line 571 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
DEFINE_SYNTHESIS(con, CVMJITCompileExpression_thing_p) 
	break;
	case 243:
#line 578 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
DEFINE_SYNTHESIS(con, CVMJITCompileExpression_thing_p) 
	break;
	case 2:
	case 3:
	case 8:
	case 10:
	case 12:
	case 27:
	case 28:
	case 42:
	case 43:
	case 44:
	case 51:
	case 53:
	case 55:
	case 62:
	case 63:
	case 64:
	case 65:
	case 68:
	case 71:
	case 78:
	case 87:
	case 88:
	case 89:
	case 105:
	case 106:
	case 116:
	case 117:
	case 118:
	case 144:
	case 173:
	case 174:
	case 176:
	case 180:
	case 183:
	case 184:
	case 185:
	case 188:
	case 189:
	case 203:
	case 206:
	case 219:
	case 221:
	case 224:
	case 227:
	case 230:
	case 233:
	case 250:
	case 255:
	case 264:
	case 265:
	case 266:
	case 267:
		DEFAULT_SYNTHESIS1(con, CVMJITCompileExpression_thing_p);
		break;
	case 1:
	case 14:
	case 16:
	case 100:
	case 101:
	case 115:
	case 136:
	case 150:
	case 151:
	case 155:
	case 156:
	case 157:
	case 158:
	case 159:
	case 160:
	case 161:
	case 162:
	case 210:
	case 211:
	case 214:
	case 215:
	case 216:
	case 217:
	case 222:
	case 223:
	case 238:
	case 239:
	case 240:
	case 241:
	case 260:
	case 261:
	case 262:
	case 263:
		DEFAULT_SYNTHESIS_CHAIN(con, CVMJITCompileExpression_thing_p);
		break;
	case 4:
	case 5:
	case 9:
	case 11:
	case 13:
	case 17:
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 79:
	case 80:
	case 81:
	case 82:
	case 83:
	case 84:
	case 102:
	case 119:
	case 120:
	case 133:
	case 134:
	case 146:
	case 153:
	case 154:
	case 163:
	case 164:
	case 165:
	case 175:
	case 196:
	case 197:
	case 198:
	case 199:
	case 212:
	case 213:
	case 226:
	case 229:
	case 232:
	case 235:
	case 246:
	case 247:
	case 248:
	case 249:
	case 251:
	case 252:
	case 253:
	case 254:
	case 256:
	case 257:
	case 258:
	case 259:
		DEFAULT_SYNTHESIS2(con, CVMJITCompileExpression_thing_p);
		break;
	}
#ifdef CVMJIT_JCS_MATCH_DAG
	CVMJIT_JCS_SET_STATE(CVMJITCompileExpression_thing_p, CVMJIT_JCS_STATE_SYNTHED);
#endif
	if ( !GOAL_STACK_EMPTY ){
		statsPopStack(SYNTHESIS);
		goal_top -= 1;
		goto continue_rule;
	}
	return 0;
}

int
CVMJITCompileExpression_action( CVMJITIRNodePtr CVMJITCompileExpression_thing_p, CVMJITCompilationContext* con)
{
	INITIALIZE_STACK_STATS(ACTION)
	int ruleno, subgoals_todo = 0, goal, i;
	unsigned int stateno;
	const struct CVMJITCompileExpression_action_pairs_t *app = NULL;
	struct CVMJITCompileExpression_rule_computation_state*  goal_top;
	CVMJITIRNodePtr *submatch_roots = NULL;
	INITIALIZE_GOAL_STACK;
	goal = goal_root;
start_rule:
	/* Get the rule which can convert the current IRNode from its current
	   state into a form that is expected by the desired goal: */
	stateno = IRGetState(CVMJITCompileExpression_thing_p);
	ruleno = CVMJITCompileExpression_rule_to_use[ CVMJIT_JCS_STATE_MASK(stateno) ][ goal ];
#ifdef CVMJIT_JCS_MATCH_DAG
	if (CVMJIT_JCS_DID_PHASE(stateno, CVMJIT_JCS_STATE_SUBACTED)){
	    if (CVMJITCompileExpression_isdag[ruleno]){
		/*
		CVMconsolePrintf(">Action: skipping node %d rule %d\n", CVMJITCompileExpression_thing_p->nodeID, ruleno);
		*/
		goto skip_action_recursion;
	    }
	}
#endif
	if (ruleno != 0 ) {
            /* Initialize the current goal: */
	    subgoals_todo = CVMJITCompileExpression_rule_actions[ruleno].n_subgoals;
	    app = &CVMJITCompileExpression_action_pairs[CVMJITCompileExpression_rule_actions[ruleno].action_pair_index];
	    goal_top->p = CVMJITCompileExpression_thing_p;
	    goal_top->ruleno = ruleno;
	    goal_top->subgoals_todo = subgoals_todo;
	    goal_top->app = app;
	}
	submatch_roots = goal_top->submatch_roots;
	goal_top->curr_submatch_root = submatch_roots;
	for (i=0; i<subgoals_todo; i++){
	    switch (app[i].pathno ){
		case 0:
		    submatch_roots[i]=CVMJITCompileExpression_thing_p;
		    break;
		case 1:
		    submatch_roots[i]=CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
		    break;
		case 2:
		    submatch_roots[i]=CVMJITirnodeGetLeftSubtree(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p));
		    break;
		case 3:
		    submatch_roots[i]=CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
		    break;
		case 4:
		    submatch_roots[i]=CVMJITirnodeGetRightSubtree(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p));
		    break;
		case 5:
		    submatch_roots[i]=CVMJITirnodeGetLeftSubtree(CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p));
		    break;
		}
	}
	    goal_top->curr_attribute = goal_top->attributes;
	switch( ruleno ){
	case 0:
#if defined(CVM_DEBUG) || defined(CVM_TRACE_JIT)
		con->errNode = CVMJITCompileExpression_thing_p;
#endif
		goto error_return_branch_island;
	case 2:
#line 304 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
ASSIGN_INHERITANCE(con, CVMJITCompileExpression_thing_p) 
	break;
	case 3:
#line 324 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
ASSIGN_INHERITANCE(con, CVMJITCompileExpression_thing_p) 
	break;
	case 6:
#line 398 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG3, ARG1) 
	break;
	case 17:
#line 1033 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        SEQUENCE_R_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_INT_REGS(con)); 
	break;
	case 18:
#line 1038 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	SEQUENCE_R_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_INT_REGS(con)); 
	break;
	case 19:
#line 1044 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	SEQUENCE_R_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_INT_REGS(con)); 
	break;
	case 20:
#line 1066 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	SEQUENCE_L_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_INT_REGS(con)); 
	break;
	case 21:
#line 1071 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	SEQUENCE_L_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_INT_REGS(con)); 
	break;
	case 22:
#line 1077 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	SEQUENCE_L_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_INT_REGS(con)); 
	break;
	case 29:
#line 1177 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 31:
#line 1199 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG2); 
	break;
	case 32:
#line 1271 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG3, ARG2); 
	break;
	case 33:
#line 1308 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2_0(CVMJITCompileExpression_thing_p, ARG3); 
	break;
	case 34:
#line 1371 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG2); 
	break;
	case 38:
#line 1510 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
IDENT_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 39:
#line 1530 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
IDENT_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 40:
#line 1542 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
IDENT_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 41:
#line 1567 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
IDENT_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 52:
#line 2928 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 54:
#line 2945 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 56:
#line 2963 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 57:
#line 2971 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 58:
#line 2980 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 59:
#line 2989 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 60:
#line 2998 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 61:
#line 3007 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
	case 66:
#line 3077 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 67:
#line 3086 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 69:
#line 3109 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 70:
#line 3118 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 72:
#line 3144 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 73:
#line 3153 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 74:
#line 3162 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 75:
#line 3171 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 76:
#line 3180 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 77:
#line 3189 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 85:
#line 3253 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 86:
#line 3262 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 90:
#line 3423 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 91:
#line 3432 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 92:
#line 3441 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 94:
#line 3461 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 95:
#line 3469 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 96:
#line 3478 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 97:
#line 3487 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 98:
#line 3496 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 99:
#line 3505 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 103:
#line 4141 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
ARRAY_LOAD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 104:
#line 4147 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
ARRAY_LOAD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 107:
#line 4206 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
GETFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 108:
#line 4220 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
GETFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 109:
#line 4234 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
GETFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 110:
#line 4244 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
	case 111:
#line 4495 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
PUTFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 112:
#line 4584 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
PUTFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 113:
#line 4598 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
PUTFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 114:
#line 4608 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
PUTFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p);
    SET_TARGET3_WO_INHERITANCE(CVMJITCompileExpression_thing_p, ARG3, ARG4, ARG1); 
	break;
	case 121:
#line 4949 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
ARRAY_STORE_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 122:
#line 4955 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
ARRAY_STORE_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 129:
#line 5205 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 130:
#line 5212 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 131:
#line 5221 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 132:
#line 5478 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
END_TARGET_IARG(con, CVMJITCompileExpression_thing_p); 
	break;
	case 133:
#line 5479 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_IARG(con, CVMJITCompileExpression_thing_p); 
	break;
	case 134:
#line 5480 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_IARG(con, CVMJITCompileExpression_thing_p); 
	break;
	case 137:
#line 5500 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p);
    
	break;
	case 138:
#line 5515 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p);
    
	break;
	case 139:
#line 5530 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p);
    
	break;
	case 140:
#line 5545 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p);
    
	break;
	case 141:
#line 5560 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p);
    
	break;
	case 142:
#line 5575 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET_INTRINSIC_CALL(con, CVMJITCompileExpression_thing_p);
    
	break;
	case 149:
#line 5879 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET2(CVMJITCompileExpression_thing_p, ARG2, ARG3); 
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 166:
#line 6281 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 167:
#line 6287 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 168:
#line 6294 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 169:
#line 6304 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
	case 170:
#line 6316 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG2); 
	break;
	case 171:
#line 6329 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 172:
#line 6341 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2(CVMJITCompileExpression_thing_p, ARG1, ARG3); 
	break;
	case 185:
#line 7640 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET1(CVMJITCompileExpression_thing_p, ARG3); 
	break;
	case 186:
#line 7841 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG3); 
	break;
	case 187:
#line 7847 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG3); 
	break;
	case 188:
#line 7853 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET1(CVMJITCompileExpression_thing_p, ARG3); 
	break;
	case 189:
#line 7876 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
SET_TARGET1(CVMJITCompileExpression_thing_p, ARG3); 
	break;
	case 196:
#line 51 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

	SEQUENCE_R_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_FP_REGS(con)); 
	break;
	case 197:
#line 56 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

	SEQUENCE_R_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_FP_REGS(con)); 
	break;
	case 198:
#line 64 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

	SEQUENCE_L_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_FP_REGS(con)); 
	break;
	case 199:
#line 69 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

        SEQUENCE_L_INHERITANCE(CVMJITCompileExpression_thing_p, CVMRM_FP_REGS(con)); 
	break;
	case 204:
#line 122 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
GETFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 207:
#line 157 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
GETFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 208:
#line 166 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 209:
#line 175 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    SET_TARGET2_1(CVMJITCompileExpression_thing_p, ARG1); 
	break;
	case 218:
#line 241 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
ARRAY_LOAD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 220:
#line 275 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
ARRAY_LOAD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 224:
#line 332 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
ASSIGN_INHERITANCE(con, CVMJITCompileExpression_thing_p) 
	break;
	case 225:
#line 353 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
PUTFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 228:
#line 385 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
ARRAY_STORE_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 230:
#line 415 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
ASSIGN_INHERITANCE(con, CVMJITCompileExpression_thing_p) 
	break;
	case 231:
#line 436 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
PUTFIELD_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 234:
#line 459 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
ARRAY_STORE_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 236:
#line 496 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
IDENT_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 237:
#line 515 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
IDENT_INHERITANCE(con, CVMJITCompileExpression_thing_p); 
	break;
	case 8:
	case 10:
	case 12:
	case 27:
	case 28:
	case 42:
	case 43:
	case 44:
	case 51:
	case 53:
	case 55:
	case 62:
	case 63:
	case 64:
	case 65:
	case 68:
	case 71:
	case 78:
	case 87:
	case 88:
	case 89:
	case 105:
	case 106:
	case 116:
	case 117:
	case 118:
	case 123:
	case 124:
	case 144:
	case 147:
	case 173:
	case 174:
	case 176:
	case 180:
	case 183:
	case 184:
	case 193:
	case 203:
	case 206:
	case 219:
	case 221:
	case 227:
	case 233:
	case 242:
	case 243:
	case 250:
	case 255:
	case 264:
	case 265:
	case 266:
	case 267:
		DEFAULT_INHERITANCE1(con, CVMJITCompileExpression_thing_p);
		break;
	case 1:
	case 14:
	case 16:
	case 100:
	case 101:
	case 115:
	case 136:
	case 150:
	case 151:
	case 155:
	case 156:
	case 157:
	case 158:
	case 159:
	case 160:
	case 161:
	case 162:
	case 210:
	case 211:
	case 214:
	case 215:
	case 216:
	case 217:
	case 222:
	case 223:
	case 238:
	case 239:
	case 240:
	case 241:
	case 260:
	case 261:
	case 262:
	case 263:
		DEFAULT_INHERITANCE_CHAIN(con, CVMJITCompileExpression_thing_p);
		break;
	case 4:
	case 5:
	case 9:
	case 11:
	case 13:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 79:
	case 80:
	case 81:
	case 82:
	case 83:
	case 84:
	case 102:
	case 119:
	case 120:
	case 145:
	case 146:
	case 148:
	case 153:
	case 154:
	case 163:
	case 164:
	case 165:
	case 175:
	case 212:
	case 213:
	case 226:
	case 229:
	case 232:
	case 235:
	case 246:
	case 247:
	case 248:
	case 249:
	case 251:
	case 252:
	case 253:
	case 254:
	case 256:
	case 257:
	case 258:
	case 259:
		DEFAULT_INHERITANCE2(con, CVMJITCompileExpression_thing_p);
		break;
	}
continue_rule:
	if ( goal_top->subgoals_todo > 0 ){
		app = (goal_top->app)++;
		goal_top->subgoals_todo -= 1;
		CVMJITCompileExpression_thing_p = *goal_top->curr_submatch_root++;
		goal = app->subgoal;
		goal_top += 1;
		statsPushStack(ACTION);
		validateStack((goal_top < GOAL_STACK_TOP), Action);
		goto start_rule;
	}
	CVMJITCompileExpression_thing_p = goal_top->p;
	submatch_roots = goal_top->submatch_roots;
	ruleno = goal_top->ruleno;
#ifdef CVMJIT_JCS_MATCH_DAG
	CVMJIT_JCS_SET_STATE(CVMJITCompileExpression_thing_p, CVMJIT_JCS_STATE_SUBACTED);
    skip_action_recursion:
#endif
#ifdef CVMJITCompileExpression_DEBUG
	if (CVMJITCompileExpression_DEBUG)
	    fprintf(stderr, "Applying rule %d to node %d\n",
		ruleno, id(CVMJITCompileExpression_thing_p) );
#endif

	switch( ruleno ){
	case 0:
#if defined(CVM_DEBUG) || defined(CVM_TRACE_JIT)
		con->errNode = CVMJITCompileExpression_thing_p;
#endif
error_return_branch_island:
		return -1;
	case 1:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  1] root: effect ", CVMJITCompileExpression_thing_p);
#line 295 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 2:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  2] root: ASSIGN LOCAL32 reg32 ", CVMJITCompileExpression_thing_p);
#line 304 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	CVMJITIRNode*  localNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMJITIRNode*  rhsNode = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	CVMJITLocal*   lhs = CVMJITirnodeGetLocal(localNode);
	CVMBool        isRef = CVMJITirnodeIsReferenceType(CVMJITCompileExpression_thing_p);
	int target;

	if (rhsNode->decorationType == CVMJIT_REGHINT_DECORATION) {
	    target = 1U << rhsNode->decorationData.regHint;
	} else {
	    target = CVMRM_ANY_SET;
	}

	CVMRMpinResource(CVMRM_INT_REGS(con), rhs, target, CVMRM_EMPTY_SET);
	CVMRMstoreJavaLocal(CVMRM_INT_REGS(con), rhs, 1, isRef, lhs->localNo);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 3:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  3] root: ASSIGN LOCAL64 reg64 ", CVMJITCompileExpression_thing_p);
#line 324 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource * rhs = popResource(con);
	CVMJITLocal   * lhs = CVMJITirnodeGetLocal(
	    CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p));
	CVMRMpinResource(CVMRM_INT_REGS(con), rhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	CVMRMstoreJavaLocal(CVMRM_INT_REGS(con), rhs, 2, CVM_FALSE,
			    lhs->localNo);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 4:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  4] root: ASSIGN STATIC32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 376 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do putstatic:"));
        CVMJITaddCodegenComment((con,
            "putstatic(staticFieldAddr, value{I|F|O})"));
        isVolatile =
            ((CVMJITirnodeGetUnaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITUNOP_VOLATILE_FIELD) != 0);
        setStaticField(con, CVMRM_INT_REGS(con), CVMCPU_STR32_OPCODE,
                       isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 5:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  5] root: ASSIGN STATIC64 reg32 reg64 ", CVMJITCompileExpression_thing_p);
#line 389 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do putstatic:"));
        CVMJITaddCodegenComment((con,
            "putstatic(staticFieldAddr, value{L|D})"));
        setStaticField(con, CVMRM_INT_REGS(con), CVMCPU_STR64_OPCODE,
                       CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 6:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  6] root: ASSIGN STATIC64VOL reg32 reg64 ", CVMJITCompileExpression_thing_p);
#line 399 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	CVMRMResource* lhs = popResource(con);

	/* Swap the arguments because the runtime helper function will expect
	   the 64-bit source value to come first followed by the static field
	   address: */
	pushResource(con, rhs);
	pushResource(con, lhs);

        CVMJITprintCodegenComment(("Do volatile putstatic:"));
        CVMJITaddCodegenComment((con, "call CVMCCMruntimePutstatic64Volatile"));
        CVMJITsetSymbolName((con, "CVMCCMruntimePutstatic64Volatile"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimePutstatic64Volatile);

	/* Call the helper function: */
        longBinaryHelper2(con, (void*)CVMCCMruntimePutstatic64Volatile, CVMJITCompileExpression_thing_p,
			  CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 7:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  7] aluRhs: ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 419 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMInt32 constant;
        if (CVMJITirnodeIsConstant32Node(CVMJITCompileExpression_thing_p)) {
            constant = CVMJITirnodeGetConstant32(CVMJITCompileExpression_thing_p)->j.i;
        } else {
            constant = CVMJITirnodeGetConstantAddr(CVMJITCompileExpression_thing_p)->vAddr;
        }
        pushALURhsConstant(con, constant);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 8:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  8] reg32: SLL32 reg32 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 514 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        doIntShift(con, CVMCPU_SLL_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 9:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[  9] reg32: SLL32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 518 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        doRegShift(con, CVMCPU_SLL_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 10:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 10] reg32: SRL32 reg32 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 522 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        doIntShift(con, CVMCPU_SRL_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 11:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 11] reg32: SRL32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 526 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        doRegShift(con, CVMCPU_SRL_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 12:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 12] reg32: SRA32 reg32 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 530 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        doIntShift(con, CVMCPU_SRA_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 13:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 13] reg32: SRA32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 534 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        doRegShift(con, CVMCPU_SRA_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 14:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 14] aluRhs: reg32 ", CVMJITCompileExpression_thing_p);
#line 538 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/*
	 * a simple matter of bookkeeping.
	 * may be able to (may need to!) delete this rule.
	 * it probably leads to ambiguity.
	 */
	CVMRMResource* operand = popResource(con);
        pushALURhsResource(con, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 15:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 15] memSpec: ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 548 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        pushMemSpecImmediate(con, CVMJITirnodeGetConstant32(CVMJITCompileExpression_thing_p)->j.i);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 16:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 16] memSpec: reg32 ", CVMJITCompileExpression_thing_p);
#line 553 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        /* If a numeric constant is too large, it will be converted into an
           ICONST_32 which can be mapped into a reg32.  This rule will provide
           a means to use that reg32 as an offset. */
        CVMRMResource *operand = popResource(con);
        pushMemSpecRegister(con, CVM_TRUE, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 17:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 17] reg32: ISEQUENCE_R effect reg32 ", CVMJITCompileExpression_thing_p);
#line 1034 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 18:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 18] reg64: LSEQUENCE_R effect reg64 ", CVMJITCompileExpression_thing_p);
#line 1039 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 19:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 19] reg32: VSEQUENCE_R effect reg32 ", CVMJITCompileExpression_thing_p);
#line 1045 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 20:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 20] reg32: ISEQUENCE_L reg32 effect ", CVMJITCompileExpression_thing_p);
#line 1067 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 21:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 21] reg64: LSEQUENCE_L reg64 effect ", CVMJITCompileExpression_thing_p);
#line 1072 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 22:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 22] reg32: VSEQUENCE_L reg32 effect ", CVMJITCompileExpression_thing_p);
#line 1078 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 23:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 23] root: BEGININLINING ", CVMJITCompileExpression_thing_p);
#line 1081 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    beginInlining(con, CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 24:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 24] effect: VENDINLINING ", CVMJITCompileExpression_thing_p);
#line 1088 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    endInlining(con, CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 25:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 25] reg32: LOCAL32 ", CVMJITCompileExpression_thing_p);
#line 1092 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMJITLocal*   l = CVMJITirnodeGetLocal( CVMJITCompileExpression_thing_p );
	CVMBool isRef = CVMJITirnodeIsReferenceType(CVMJITCompileExpression_thing_p);
	CVMRMResource* dest = 
	    CVMRMbindResourceForLocal(CVMRM_INT_REGS(con), 1,
				      isRef, l->localNo);
	CVMRMpinResourceEagerlyIfDesireable(CVMRM_INT_REGS(con),
					    dest, GET_REGISTER_GOALS);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 26:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 26] reg64: LOCAL64 ", CVMJITCompileExpression_thing_p);
#line 1104 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMJITLocal*   l = CVMJITirnodeGetLocal( CVMJITCompileExpression_thing_p );
	CVMRMResource* dest =
	    CVMRMbindResourceForLocal(CVMRM_INT_REGS(con), 2,
				      CVM_FALSE, l->localNo);
	CVMRMpinResourceEagerlyIfDesireable(CVMRM_INT_REGS(con),
					    dest, GET_REGISTER_GOALS);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 27:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 27] reg32: FETCH32 STATIC32 reg32 ", CVMJITCompileExpression_thing_p);
#line 1153 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do getstatic:"));
        CVMJITaddCodegenComment((con,
            "value{I|F|O} = getstatic(staticFieldAddr);"));
        isVolatile =
            ((CVMJITirnodeGetUnaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITUNOP_VOLATILE_FIELD) != 0);
        getStaticField(con, CVMRM_INT_REGS(con),
		       CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS, CVMCPU_LDR32_OPCODE, 1,
                       isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 28:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 28] reg64: FETCH64 STATIC64 reg32 ", CVMJITCompileExpression_thing_p);
#line 1167 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do getstatic:"));
        CVMJITaddCodegenComment((con,
            "value{L|D} = getstatic(staticFieldAddr);"));
        getStaticField(con, CVMRM_INT_REGS(con),
		       CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS, CVMCPU_LDR64_OPCODE, 2,
                       CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 29:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 29] reg64: FETCH64 STATIC64VOL reg32 ", CVMJITCompileExpression_thing_p);
#line 1178 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do volatile getstatic:"));
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeGetstatic64Volatile"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeGetstatic64Volatile"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeGetstatic64Volatile);
        unaryHelper(con, (void*)CVMCCMruntimeGetstatic64Volatile, CVMJITCompileExpression_thing_p, ARG1, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 30:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 30] reg32: EXCEPTION_OBJECT ", CVMJITCompileExpression_thing_p);
#line 1186 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/* It appears in ARG1, so we initially bind it to ARG1. Note that
	 * the front end guarantees that the first thing done is an
	 * ASSIGN of the exception object, so we don't need to worry
	 * about it being trashed before we get here.
	 */
	CVMRMResource* dest =
	    CVMRMgetResourceSpecific(CVMRM_INT_REGS(con), CVMCPU_ARG1_REG, 1);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 31:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 31] reg32: NEW_OBJECT reg32 ", CVMJITCompileExpression_thing_p);
#line 1200 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource* cbRes = popResource(con);
	CVMRMResource* dest;

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        /* Make JSP point just past the last argument currently on the stack */
        CVMSMadjustJSP(con);
#endif

        CVMJITprintCodegenComment(("Do new:"));
        cbRes = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), cbRes,
					 CVMCPU_ARG2_REG);
        CVMRMmajorSpill(con, ARG2, CVMRM_SAFE_SET);
	dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
#ifdef CVM_TRACE_JIT
        if (CVMRMisConstant(cbRes)) {
	    CVMJITprintCodegenComment(("cb: %C", cbRes->constant));
	}
#endif

#ifdef IAI_NEW_GLUE_CALLING_CONVENTION_IMPROVEMENT
	/* Load the class instanceSize and accessFlags into ARG3 and ARG4.
	 * If the class is resolved and linked, then these are known at
	 * compile time and can be treated as constants. Otherwise they
	 * are loaded from the cb.
	 */
        if (CVMRMisConstant(cbRes) &&
            CVMcbCheckRuntimeFlag((CVMClassBlock*)(cbRes->constant), LINKED))
	{
	    CVMClassBlock* cb = (CVMClassBlock*)(cbRes->constant);
	    /* get class instanceSize into ARG3 */
	    CVMJITaddCodegenComment((con, "CVMcbInstanceSize(cb)"));
	    CVMCPUemitLoadConstant(con, CVMCPU_ARG3_REG,
				   CVMcbInstanceSize(cb));
	    /* get class accessFlags into ARG4 */
	    CVMJITaddCodegenComment((con, "CVMcbAccessFlags(cb)"));
	    CVMCPUemitLoadConstant(con, CVMCPU_ARG4_REG,
				   CVMcbAccessFlags(cb));
        } else {
	    /* load class instanceSize from cb into ARG3 */
	    CVMJITaddCodegenComment((con, "CVMcbInstanceSize(cb)"));
            CVMCPUemitMemoryReferenceImmediate(con,
                CVMCPU_LDR16U_OPCODE,
                CVMCPU_ARG3_REG, CVMCPU_ARG2_REG,
		offsetof(CVMClassBlock, instanceSizeX));
	    /* load class accessFlags from cb into ARG4 */
	    CVMJITaddCodegenComment((con, "CVMcbAccessFlags(cb)"));
	    CVMCPUemitMemoryReferenceImmediate(con,
                CVMCPU_LDR8_OPCODE,
                CVMCPU_ARG4_REG, CVMCPU_ARG2_REG,
		offsetof(CVMClassBlock, accessFlagsX));
        }
#endif

        CVMJITaddCodegenComment((con, "call CVMCCMruntimeNewGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeNewGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeNew);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeNewGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);
	CVMJITcaptureStackmap(con, 0);
	/*
	 * Return value is RESULT1
	 */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), cbRes);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 32:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 32] reg32: NEW_ARRAY_REF reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 1272 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        CVMRMResource *dimension = popResource(con);
        CVMRMResource *cbRes = popResource(con);

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        /* Make JSP point just past the last argument currently on the stack */
        CVMSMadjustJSP(con);
#endif

        CVMJITprintCodegenComment(("Do anewarray:"));
        cbRes = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), cbRes,
					 CVMCPU_ARG3_REG);
        dimension = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), dimension,
					     CVMCPU_ARG2_REG);

        CVMRMmajorSpill(con, ARG2|ARG3, CVMRM_SAFE_SET);
	dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);

        CVMJITaddCodegenComment((con, "call CVMCCMruntimeANewArrayGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeANewArrayGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeANewArray);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeANewArrayGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);
	CVMJITcaptureStackmap(con, 0);
	
	/* Return value is in RESULT1 */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), cbRes);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), dimension);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 33:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 33] reg32: MULTI_NEW_ARRAY_REF reg32 parameters ", CVMJITCompileExpression_thing_p);
#line 1309 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest;
	CVMJITIRNode* paramnode;
	int nDimensions;
        CVMRMResource *cbRes = popResource(con);
        CVMRMResource *dimensionsDepth;

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        /* Make JSP point just past the last argument currently on the stack */
        CVMSMadjustJSP(con);
#endif

        CVMJITprintCodegenComment(("Do multianewarray:"));
        cbRes = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), cbRes,
					 CVMCPU_ARG3_REG);

        CVMRMmajorSpill(con, ARG3, CVMRM_SAFE_SET);
	dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);

	/*
	 * number of dimensions is simply the number of PARAMETER nodes
	 * stacked under this one.
	 */
	paramnode = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
        nDimensions = CVMJITirnodeGetBinaryOp(CVMJITCompileExpression_thing_p)->data;

        /* Load the number of dimensions into ARG2: */
        CVMJITaddCodegenComment((con, "number of dimensions"));
        dimensionsDepth = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
						   CVMCPU_ARG2_REG, 1);
        CVMCPUemitLoadConstant(con, CVMCPU_ARG2_REG, nDimensions);

        /* Address of the first of them is JSP-4*nDimensions: */
        CVMJITaddCodegenComment((con, "&dimensions"));
        /* arg4 = jsp - (dimensionsDepth << 2): */
        CVMCPUemitComputeAddressOfArrayElement(con, CVMCPU_SUB_OPCODE,
            CVMCPU_ARG4_REG, CVMCPU_JSP_REG,
            CVMRMgetRegisterNumber(dimensionsDepth), CVMCPU_SLL_OPCODE, 2);
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeMultiANewArrayGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeMultiANewArrayGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeMultiANewArray);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeMultiANewArrayGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);
	CVMJITcaptureStackmap(con, 0);

	/* pop dimensions from operand stack */
        CVMJITaddCodegenComment((con, "pop dimensions off the stack"));
        CVMCPUemitBinaryALUConstant(con, CVMCPU_SUB_OPCODE,
            CVMCPU_JSP_REG, CVMCPU_JSP_REG, 4*nDimensions, CVMJIT_NOSETCC);
        /* Tell stackman to pop the dimensions: */
        CVMSMpopParameters(con, nDimensions);

	/* Return value is in RESULT1 */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), cbRes);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), dimensionsDepth);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 34:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 34] reg32: NEW_ARRAY_BASIC reg32 ", CVMJITCompileExpression_thing_p);
#line 1372 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest;
	CVMRMResource* dimension = popResource(con);
        CVMBasicType typeCode;
	CVMClassBlock* arrCB;
	CVMUint32 elementSize;
	
#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        /* Make JSP point just past the last argument currently on the stack */
        CVMSMadjustJSP(con);
#endif

        /* Map the IR node opcode to the type code: */
        typeCode = (CVMJITgetOpcode(CVMJITCompileExpression_thing_p) >> CVMJIT_SHIFT_OPCODE)
                    - CVMJIT_NEW_ARRAY_BOOLEAN + CVM_T_BOOLEAN;

	/* This is known at compile time for arrays of basic types */
	arrCB = (CVMClassBlock*)CVMbasicTypeArrayClassblocks[typeCode];
	elementSize = CVMbasicTypeSizes[typeCode];

        CVMJITprintCodegenComment(("Do newarray:"));
        dimension = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), dimension,
					     CVMCPU_ARG2_REG);
	CVMRMmajorSpill(con, ARG2, CVMRM_SAFE_SET);
	dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);

        CVMJITaddCodegenComment((con, "%C", arrCB));
        CVMJITsetSymbolName((con, "cb %C", arrCB));
        CVMCPUemitLoadConstant(con, CVMCPU_ARG3_REG, (int)arrCB);
        CVMJITaddCodegenComment((con, "array element size"));
        CVMCPUemitLoadConstant(con, CVMCPU_ARG1_REG, elementSize);
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeNewArrayGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeNewArrayGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeNewArray);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeNewArrayGlue, 
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);
	CVMJITcaptureStackmap(con, 0);

	/* Return value is in RESULT1 */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), dimension);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 35:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 35] reg32: ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 1446 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        const2Reg32(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 36:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 36] reg32: STRING_ICELL_CONST ", CVMJITCompileExpression_thing_p);
#line 1449 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* stringICellResource;
	CVMUint32      stringICellReg;
	CVMRMResource* stringObjectResource =
	    CVMRMgetResource(CVMRM_INT_REGS(con), GET_REGISTER_GOALS, 1);
	CVMUint32      stringObjectReg =
	    CVMRMgetRegisterNumber(stringObjectResource);

	CVMStringICell* stringICell =
	    CVMJITirnodeGetConstantAddr(CVMJITCompileExpression_thing_p)->stringICell;
        CVMJITsetSymbolName((con, "StringICell"));
	stringICellResource = 
	    CVMRMgetResourceForConstant32(CVMRM_INT_REGS(con),
					  CVMRM_ANY_SET, CVMRM_EMPTY_SET,
                                          (CVMUint32)stringICell);
	stringICellReg = CVMRMgetRegisterNumber(stringICellResource);
        CVMJITaddCodegenComment((con, "StringObject from StringICell"));
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
            stringObjectReg, stringICellReg, 0);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con),
					stringObjectResource, CVMJITCompileExpression_thing_p);
	pushResource(con, stringObjectResource);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), stringICellResource);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 37:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 37] reg32: CLASS_BLOCK ", CVMJITCompileExpression_thing_p);
#line 1474 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        CVMClassBlock *cb = CVMJITirnodeGetConstantAddr(CVMJITCompileExpression_thing_p)->cb;
        CVMJITsetSymbolName((con, "cb %C", cb));
        dest =
	    CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), (CVMInt32)cb);
	CVMRMpinResourceEagerlyIfDesireable(CVMRM_INT_REGS(con),
					    dest, GET_REGISTER_GOALS);
        /* Need this in case this constant is a CSE */
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 38:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 38] reg32: IDENT32 reg32 ", CVMJITCompileExpression_thing_p);
#line 1510 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* src;
	if (!CVMJIT_DID_SEMANTIC_ACTION(CVMJITCompileExpression_thing_p)){
	    src = popResource(con);
	    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), src, CVMJITCompileExpression_thing_p);
	    /* CVMconsolePrintf("Initial evaluation of "); */
	} else {
	    src = CVMRMfindResource(CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	    /* CVMconsolePrintf("Reiteration of "); */
	    CVMassert(src != NULL);
	}
	/*
	CVMconsolePrintf("Fixed IDENT32 ID %d, resource 0x%x\n",
	    CVMJITCompileExpression_thing_p->nodeID, src);
	*/
	pushResource(con, src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 39:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 39] iconst32Index: IDENT32 iconst32Index ", CVMJITCompileExpression_thing_p);
#line 1530 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMInt32 idx;
	if (!CVMJIT_DID_SEMANTIC_ACTION(CVMJITCompileExpression_thing_p)){
	    idx = popIConst32(con);
	    CVMJITirnodeGetIdentOp(CVMJITCompileExpression_thing_p)->backendData = idx;
	} else {
	    idx = CVMJITirnodeGetIdentOp(CVMJITCompileExpression_thing_p)->backendData;
	}
	pushIConst32(con, idx);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 40:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 40] arrayIndex: IDENT32 arrayIndex ", CVMJITCompileExpression_thing_p);
#line 1542 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	ScaledIndexInfo* src;
	if (!CVMJIT_DID_SEMANTIC_ACTION(CVMJITCompileExpression_thing_p)){
	    src = popScaledIndexInfo(con);
	    CVMJITidentitySetDecoration(con, (CVMJITIdentityDecoration*)src,
					CVMJITCompileExpression_thing_p);
	} else {
	    src = (ScaledIndexInfo*)CVMJITidentityGetDecoration(con, CVMJITCompileExpression_thing_p);
	    CVMassert((src == NULL) ||
		      CVMJITidentityDecorationIs(con, CVMJITCompileExpression_thing_p, SCALEDINDEX));
	    /* CVMconsolePrintf("Reiteration of "); */
	    CVMassert(src != NULL);
	}
	/*
	CVMconsolePrintf("IDENT32 ID %d, resource 0x%x\n",
	    CVMJITCompileExpression_thing_p->nodeID, src);
	*/
	
#ifdef IAI_CS_EXCEPTION_ENHANCEMENT2
        src->isIDENTITYOutofBoundsCheck = CVM_TRUE;
#endif
	pushScaledIndexInfo(con, src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 41:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 41] reg64: IDENT64 reg64 ", CVMJITCompileExpression_thing_p);
#line 1567 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* src;
	if (!CVMJIT_DID_SEMANTIC_ACTION(CVMJITCompileExpression_thing_p)){
	    src = popResource(con);
	    CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), src, CVMJITCompileExpression_thing_p);
	} else {
	    src = CVMRMfindResource(CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	    CVMassert(src != NULL);
	}
	pushResource(con, src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 42:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 42] reg32: INEG32 reg32 ", CVMJITCompileExpression_thing_p);
#line 1579 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	wordUnaryOp(con, CVMCPU_NEG_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 43:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 43] reg32: NOT32 reg32 ", CVMJITCompileExpression_thing_p);
#line 1582 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	wordUnaryOp(con, CVMCPU_NOT_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 44:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 44] reg32: INT2BIT32 reg32 ", CVMJITCompileExpression_thing_p);
#line 1585 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

	wordUnaryOp(con, CVMCPU_INT2BIT_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 45:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 45] reg32: IADD32 reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 2498 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        wordBinaryOp(con, CVMCPU_ADD_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 46:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 46] reg32: ISUB32 reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 2500 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        wordBinaryOp(con, CVMCPU_SUB_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 47:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 47] reg32: AND32 reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 2502 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        wordBinaryOp(con, CVMCPU_AND_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 48:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 48] reg32: OR32 reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 2504 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        wordBinaryOp(con, CVMCPU_OR_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 49:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 49] reg32: XOR32 reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 2506 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        wordBinaryOp(con, CVMCPU_XOR_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 50:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 50] reg32: IMUL32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2906 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	CVMRMResource* lhs = popResource(con);
	CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       GET_REGISTER_GOALS, 1);
	CVMRMpinResource(CVMRM_INT_REGS(con), lhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	CVMRMpinResource(CVMRM_INT_REGS(con), rhs,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMCPUemitMul(con, CVMCPU_MULL_OPCODE, CVMRMgetRegisterNumber(dest),
	    CVMRMgetRegisterNumber(lhs), CVMRMgetRegisterNumber(rhs),
	    CVMCPU_INVALID_REG);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), lhs);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), rhs);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 51:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 51] reg32: IMUL32 reg32 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 2924 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        doIMulByIConst32(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 52:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 52] reg32: IDIV32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2929 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeIDiv"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeIDiv"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeIDiv);
	/* only do a "checkZero" if using the default C helper */
#ifdef CVMCCM_HAVE_PLATFORM_SPECIFIC_IDIV
        wordBinaryHelper(con, (void*)CVMCCMruntimeIDiv, CVMJITCompileExpression_thing_p, CVM_FALSE);
#else
        wordBinaryHelper(con, (void*)CVMCCMruntimeIDiv, CVMJITCompileExpression_thing_p, CVM_TRUE);
#endif
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 53:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 53] reg32: IDIV32 reg32 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 2941 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        doIDivOrIRemByIConst32(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS, CVM_TRUE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 54:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 54] reg32: IREM32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2946 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeIRem"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeIRem"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeIRem);
	/* only do a "checkZero" if using the default C helper */
#ifdef CVMCCM_HAVE_PLATFORM_SPECIFIC_IREM
        wordBinaryHelper(con, (void*)CVMCCMruntimeIRem, CVMJITCompileExpression_thing_p, CVM_FALSE);
#else
        wordBinaryHelper(con, (void*)CVMCCMruntimeIRem, CVMJITCompileExpression_thing_p, CVM_TRUE);
#endif
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 55:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 55] reg32: IREM32 reg32 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 2958 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        doIDivOrIRemByIConst32(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 56:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 56] reg32: FNEG reg32 ", CVMJITCompileExpression_thing_p);
#line 2963 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeFNeg"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeFNeg"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFNeg);
        unaryHelper(con, (void*)CVMCCMruntimeFNeg, CVMJITCompileExpression_thing_p, ARG1, 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 57:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 57] reg32: FADD reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2972 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeFAdd"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeFAdd"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFAdd);
        wordBinaryHelper(con, (void*)CVMCCMruntimeFAdd, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 58:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 58] reg32: FSUB reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2981 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeFSub"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeFSub"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFSub);
        wordBinaryHelper(con, (void*)CVMCCMruntimeFSub, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 59:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 59] reg32: FMUL reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2990 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeFMul"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeFMul"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFMul);
        wordBinaryHelper(con, (void*)CVMCCMruntimeFMul, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 60:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 60] reg32: FDIV reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 2999 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeFDiv"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeFDiv"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFDiv);
        wordBinaryHelper(con, (void*)CVMCCMruntimeFDiv, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 61:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 61] reg32: FREM reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 3008 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeFRem"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeFRem"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeFRem);
        wordBinaryHelper(con, (void*)CVMCCMruntimeFRem, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 62:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 62] reg32: I2C reg32 ", CVMJITCompileExpression_thing_p);
#line 3042 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do i2c:"));
        shortenInt(con, CVMCPU_SRL_OPCODE, 16, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 63:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 63] reg32: I2S reg32 ", CVMJITCompileExpression_thing_p);
#line 3048 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do i2s:"));
        shortenInt(con, CVMCPU_SRA_OPCODE, 16, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 64:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 64] reg32: I2B reg32 ", CVMJITCompileExpression_thing_p);
#line 3054 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do i2b:"));
        shortenInt(con, CVMCPU_SRA_OPCODE, 24, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 65:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 65] reg32: ALENGTH reg32 ", CVMJITCompileExpression_thing_p);
#line 3060 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       GET_REGISTER_GOALS, 1);
	CVMRMResource* src = popResource(con);
	CVMRMpinResource(CVMRM_INT_REGS(con), src,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMJITaddCodegenComment((con, "arraylength"));
        CVMJITcsSetExceptionInstruction(con);
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
            CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(src),
            ARRAY_LENGTH_OFFSET);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 66:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 66] reg64: L2D reg64 ", CVMJITCompileExpression_thing_p);
#line 3078 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeL2D"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeL2D"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeL2D);
        unaryHelper(con, (void*)CVMCCMruntimeL2D, CVMJITCompileExpression_thing_p, ARG1|ARG2, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 67:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 67] reg32: L2F reg64 ", CVMJITCompileExpression_thing_p);
#line 3087 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeL2F"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeL2F"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeL2F);
        unaryHelper(con, (void*)CVMCCMruntimeL2F, CVMJITCompileExpression_thing_p, ARG1|ARG2, 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 68:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 68] reg32: L2I reg64 ", CVMJITCompileExpression_thing_p);
#line 3095 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       GET_REGISTER_GOALS, 1);
	CVMRMResource* src = popResource(con);
	CVMRMpinResource(CVMRM_INT_REGS(con), src,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMCPUemitLong2Int(con, CVMRMgetRegisterNumber(dest),
                           CVMRMgetRegisterNumber(src));
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 69:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 69] reg64: I2D reg32 ", CVMJITCompileExpression_thing_p);
#line 3110 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeI2D"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeI2D"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeI2D);
        unaryHelper(con, (void*)CVMCCMruntimeI2D, CVMJITCompileExpression_thing_p, ARG1, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 70:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 70] reg32: I2F reg32 ", CVMJITCompileExpression_thing_p);
#line 3119 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeI2F"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeI2F"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeI2F);
        unaryHelper(con, (void*)CVMCCMruntimeI2F, CVMJITCompileExpression_thing_p, ARG1, 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 71:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 71] reg64: I2L reg32 ", CVMJITCompileExpression_thing_p);
#line 3127 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       GET_REGISTER_GOALS, 2);
	CVMRMResource* src = popResource(con);
	int destreg, srcreg;

	CVMRMpinResource(CVMRM_INT_REGS(con), src,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	destreg = CVMRMgetRegisterNumber(dest);
	srcreg  = CVMRMgetRegisterNumber(src);
        CVMCPUemitInt2Long(con, destreg, srcreg);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 72:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 72] reg64: F2D reg32 ", CVMJITCompileExpression_thing_p);
#line 3145 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeF2D"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeF2D"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeF2D);
        unaryHelper(con, (void*)CVMCCMruntimeF2D, CVMJITCompileExpression_thing_p, ARG1, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 73:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 73] reg32: F2I reg32 ", CVMJITCompileExpression_thing_p);
#line 3154 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeF2I"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeF2I"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeF2I);
        unaryHelper(con, (void*)CVMCCMruntimeF2I, CVMJITCompileExpression_thing_p, ARG1, 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 74:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 74] reg64: F2L reg32 ", CVMJITCompileExpression_thing_p);
#line 3163 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeF2L"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeF2L"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeF2L);
        unaryHelper(con, (void*)CVMCCMruntimeF2L, CVMJITCompileExpression_thing_p, ARG1, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 75:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 75] reg32: D2F reg64 ", CVMJITCompileExpression_thing_p);
#line 3172 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeD2F"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeD2F"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeD2F);
        unaryHelper(con, (void*)CVMCCMruntimeD2F, CVMJITCompileExpression_thing_p, ARG1|ARG2, 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 76:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 76] reg32: D2I reg64 ", CVMJITCompileExpression_thing_p);
#line 3181 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeD2I"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeD2I"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeD2I);
        unaryHelper(con, (void*)CVMCCMruntimeD2I, CVMJITCompileExpression_thing_p, ARG1|ARG2, 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 77:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 77] reg64: D2L reg64 ", CVMJITCompileExpression_thing_p);
#line 3190 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeD2L"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeD2L"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeD2L);
        unaryHelper(con, (void*)CVMCCMruntimeD2L, CVMJITCompileExpression_thing_p, ARG1|ARG2, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 78:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 78] reg64: INEG64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3226 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *src = popResource(con);
        CVMRMResource *dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       GET_REGISTER_GOALS, 2);
        CVMRMpinResource(CVMRM_INT_REGS(con), src,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMCPUemitUnaryALU64(con, CVMCPU_NEG64_OPCODE,
            CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(src));
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 79:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 79] reg64: IADD64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3239 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        longBinaryOp(con, CVMCPU_ADD64_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 80:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 80] reg64: ISUB64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3241 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        longBinaryOp(con, CVMCPU_SUB64_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 81:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 81] reg64: AND64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3243 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        longBinaryOp(con, CVMCPU_AND64_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 82:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 82] reg64: OR64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3245 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        longBinaryOp(con, CVMCPU_OR64_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 83:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 83] reg64: XOR64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3247 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        longBinaryOp(con, CVMCPU_XOR64_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 84:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 84] reg64: IMUL64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3249 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        longBinaryOp(con, CVMCPU_MUL64_OPCODE, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 85:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 85] reg64: IDIV64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3254 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeLDiv"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLDiv"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeLDiv);
        longBinaryHelper(con, (void*)CVMCCMruntimeLDiv, CVMJITCompileExpression_thing_p, CVM_TRUE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 86:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 86] reg64: IREM64 reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3263 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeLRem"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLRem"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeLRem);
        longBinaryHelper(con, (void*)CVMCCMruntimeLRem, CVMJITCompileExpression_thing_p, CVM_TRUE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 87:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 87] reg64: SLL64 reg64 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 3372 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    /* 
     * Implmented as follows if (const32 & 0x3f < 32) :
     *	  srl scratch,srcLO,  #32-<imm>
     *	  sll destLO, srcLO,  #<imm>
     *	  sll destHI, srcHI,  #<imm>
     *	  or  destHI, destHI, scratch
     *
     * Implmented as follows if (const32 & 0x3f >= 32) :
     *	  sll destHI, srcLO,  #<imm>-32
     *    mov destLO, #0
     */
    doInt64Shift(con, CVMCPU_SRL_OPCODE, CVMCPU_SLL_OPCODE, CVMCPU_SLL_OPCODE,
		 CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 88:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 88] reg64: SRL64 reg64 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 3389 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    /* 
     * Implmented as follows if (const32 & 0x3f < 32) :
     *	  sll scratch,srcHI,  #32-<imm>
     *	  srl destLO, srcLO,  #<imm>
     *	  srl destHI, srcHI,  #<imm>
     *	  or  destLO, destLO, scratch
     *
     * Implmented as follows if (const32 & 0x3f >= 32) :
     *	  srl destLO, srcHI,  #<imm>-32
     *    mov destHI, #0
     */
    doInt64Shift(con, CVMCPU_SLL_OPCODE, CVMCPU_SRL_OPCODE, CVMCPU_SRL_OPCODE,
		 CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 89:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 89] reg64: SRA64 reg64 ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 3406 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    /* 
     * Implmented as follows if (const32 & 0x3f < 32) :
     *	  sll scratch,srcHI,  #32-<imm>
     *	  srl destLO, srcLO,  #<imm>
     *	  sra destHI, srcHI,  #<imm>
     *	  or  destLO, destLO, scratch
     *
     * Implmented as follows if (const32 & 0x3f >= 32) :
     *	  sra destLO, srcHI,  #<imm>-32
     *    sra destHI, srcHI,  31
     */
    doInt64Shift(con, CVMCPU_SLL_OPCODE, CVMCPU_SRL_OPCODE, CVMCPU_SRA_OPCODE,
		 CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 90:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 90] reg64: SLL64 reg64 reg32 ", CVMJITCompileExpression_thing_p);
#line 3424 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeLShl"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLShl"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeLShl);
        longBinaryHelper2(con, (void*)CVMCCMruntimeLShl, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 91:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 91] reg64: SRA64 reg64 reg32 ", CVMJITCompileExpression_thing_p);
#line 3433 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeLShr"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLShr"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeLShr);
        longBinaryHelper2(con, (void*)CVMCCMruntimeLShr, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 92:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 92] reg64: SRL64 reg64 reg32 ", CVMJITCompileExpression_thing_p);
#line 3442 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeLUshr"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLUshr"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeLUshr);
        longBinaryHelper2(con, (void*)CVMCCMruntimeLUshr, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 93:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 93] reg64: ICONST_64 ", CVMJITCompileExpression_thing_p);
#line 3449 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest = CVMRMgetResource(CVMRM_INT_REGS(con),
					       GET_REGISTER_GOALS, 2);
	int destregno = CVMRMgetRegisterNumber(dest);
	CVMJavaVal64 v64;
	CVMmemCopy64(v64.v, CVMJITirnodeGetConstant64(CVMJITCompileExpression_thing_p)->j.v);
        CVMCPUemitLoadLongConstant(con, destregno, &v64);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 94:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 94] reg64: DNEG reg64 ", CVMJITCompileExpression_thing_p);
#line 3461 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDNeg"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDNeg"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDNeg);
        unaryHelper(con, (void*)CVMCCMruntimeDNeg, CVMJITCompileExpression_thing_p, ARG1|ARG2, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 95:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 95] reg64: DADD reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3470 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDAdd"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDAdd"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDAdd);
        longBinaryHelper(con, (void*)CVMCCMruntimeDAdd, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 96:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 96] reg64: DSUB reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3479 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDSub"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDSub"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDSub);
        longBinaryHelper(con, (void*)CVMCCMruntimeDSub, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 97:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 97] reg64: DMUL reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3488 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDMul"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDMul"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDMul);
        longBinaryHelper(con, (void*)CVMCCMruntimeDMul, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 98:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 98] reg64: DDIV reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3497 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDDiv"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDDiv"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDDiv);
        longBinaryHelper(con, (void*)CVMCCMruntimeDDiv, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 99:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[ 99] reg64: DREM reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 3506 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeDRem"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeDRem"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeDRem);
        longBinaryHelper(con, (void*)CVMCCMruntimeDRem, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 100:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[100] arraySubscript: reg32 ", CVMJITCompileExpression_thing_p);
#line 4122 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* operand = popResource(con);
        pushScaledIndexInfoReg(con, operand);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 101:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[101] arraySubscript: iconst32Index ", CVMJITCompileExpression_thing_p);
#line 4129 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMInt32 index = popIConst32(con);
        pushScaledIndexInfoImmediate(con, index);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 102:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[102] arrayIndex: INDEX reg32 arraySubscript ", CVMJITCompileExpression_thing_p);
#line 4135 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        indexedAddr(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 103:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[103] reg64: FETCH64 INDEX reg32 arraySubscript ", CVMJITCompileExpression_thing_p);
#line 4141 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        indexedLoad(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 104:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[104] reg32: FETCH32 INDEX reg32 arraySubscript ", CVMJITCompileExpression_thing_p);
#line 4147 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        indexedLoad(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 105:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[105] reg64: FETCH64 arrayIndex ", CVMJITCompileExpression_thing_p);
#line 4152 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do *slotAddr64:"));
        fetchArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 106:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[106] reg32: FETCH32 arrayIndex ", CVMJITCompileExpression_thing_p);
#line 4158 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do *slotAddr32:"));
        fetchArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 107:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[107] reg32: FETCH32 FIELDREFOBJ reg32 memSpec ", CVMJITCompileExpression_thing_p);
#line 4206 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do getfield:"));
        CVMJITaddCodegenComment((con, "valueObj"));
        isVolatile =
            ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0);
        fetchField(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
		   GET_REGISTER_GOALS, CVMCPU_LDR32_OPCODE, 1,
                   isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 108:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[108] reg32: FETCH32 FIELDREF32 reg32 memSpec ", CVMJITCompileExpression_thing_p);
#line 4220 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do getfield:"));
        CVMJITaddCodegenComment((con, "value{I|F}"));
        isVolatile =
            ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0);
        fetchField(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
		   GET_REGISTER_GOALS, CVMCPU_LDR32_OPCODE, 1,
                   isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 109:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[109] reg64: FETCH64 FIELDREF64 reg32 memSpec ", CVMJITCompileExpression_thing_p);
#line 4234 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do getfield:"));
        CVMJITaddCodegenComment((con, "value{L|D}"));
        fetchField(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
		   GET_REGISTER_GOALS, CVMCPU_LDR64_OPCODE, 2,
                   CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 110:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[110] reg64: FETCH64 FIELDREF64VOL reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 4244 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource *fieldOffset;

	/* Note: we need to do the NULL check for this object reference because
	   the CCM helper can't do it: */
	fieldOffset = popResource(con); /* Leave the objRes on the stack. */
	doNullCheck(con, NULL, ARG1, ARG2);
	pushResource(con, fieldOffset); /* Put back the fieldOffset. */

	/* Now do the get field: */
        CVMJITprintCodegenComment(("Do volatile getfield:"));
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeGetfield64Volatile"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeGetfield64Volatile"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeGetfield64Volatile);
        binaryHelper(con, (void*)CVMCCMruntimeGetfield64Volatile, CVMJITCompileExpression_thing_p,
		     CVM_FALSE, 2, 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 111:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[111] root: ASSIGN FIELDREFOBJ reg32 aluRhs reg32 ", CVMJITCompileExpression_thing_p);
#line 4495 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *src = popResource(con);
        CVMRMResource *fieldAddr;
	CVMRMResource *objAddr;
        int srcRegID, fieldAddrRegID, objAddrRegID;

        CVMJITprintCodegenComment(("Do putfield:"));

        /* Compute the address of the field: */
        CVMJITaddCodegenComment((con, "fieldAddr = obj + fieldOffset;"));
	objAddr = extractObjectAddrFromFieldRef(con);
	CVMRMincRefCount(con, objAddr); /* One more use for the barrier */
        wordBinaryOp(con, CVMCPU_ADD_OPCODE, CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p),
                     CVMRM_ANY_SET, CVMRM_EMPTY_SET);

        /* Emit write along with barrier for reference writes: */
        fieldAddr = popResource(con);
        CVMRMpinResource(CVMRM_INT_REGS(con), src,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMRMpinResource(CVMRM_INT_REGS(con), fieldAddr,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        CVMRMpinResource(CVMRM_INT_REGS(con), objAddr,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        srcRegID = CVMRMgetRegisterNumber(src);
        fieldAddrRegID = CVMRMgetRegisterNumber(fieldAddr);
        objAddrRegID = CVMRMgetRegisterNumber(objAddr);

        CVMJITaddCodegenComment((con, "putfield(fieldAddr, valueObj);"));
        CVMJITcsSetPutFieldInstruction(con);
        CVMJITcsSetExceptionInstruction(con);
        if ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0) {
            CVMCPUemitMemBarRelease(con);
        }
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_STR32_OPCODE, srcRegID,
                                           fieldAddrRegID, 0);
        emitMarkCardTable(con, objAddrRegID, fieldAddrRegID);
        if ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0) {
            CVMCPUemitMemBar(con);
        }

        CVMRMrelinquishResource(CVMRM_INT_REGS(con), objAddr);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), fieldAddr);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 112:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[112] root: ASSIGN FIELDREF32 reg32 memSpec reg32 ", CVMJITCompileExpression_thing_p);
#line 4584 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do putfield:"));
        CVMJITaddCodegenComment((con,
            "putfield(obj, fieldOffset, value{I|F});"));
        isVolatile =
            ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0);
        setField(con, CVMRM_INT_REGS(con), CVMCPU_STR32_OPCODE,
                 isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 113:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[113] root: ASSIGN FIELDREF64 reg32 memSpec reg64 ", CVMJITCompileExpression_thing_p);
#line 4598 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do putfield:"));
        CVMJITaddCodegenComment((con,
            "putfield(obj, fieldOffset, value{L|D});"));
        setField(con, CVMRM_INT_REGS(con), CVMCPU_STR64_OPCODE,
                 CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 114:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[114] root: ASSIGN FIELDREF64VOL reg32 reg32 reg64 ", CVMJITCompileExpression_thing_p);
#line 4609 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{

	CVMRMResource* value = popResource(con);
	CVMRMResource* fieldOffset = popResource(con);
	CVMRMResource* obj; /* Leave the obj on the stack for the null check. */

	/* Note: we need to do the NULL check for this object reference because
	   the CCM helper can't do it: */
	doNullCheck(con, NULL, ARG3, ARG1|ARG2|ARG4);

	/* Now do the put field: */
	obj = popResource(con); /* OK, to pop the obj now. */

	value = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con),
					 value, CVMCPU_ARG1_REG);
	obj = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con),
				       obj, CVMCPU_ARG3_REG);
	fieldOffset = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con),
					       fieldOffset, CVMCPU_ARG4_REG);

	/* Spill the outgoing registers if necessary: */
	CVMRMminorSpill(con, ARG1|ARG2|ARG3|ARG4);

#ifdef CVMCPU_HAS_64BIT_REGISTERS
	/* Shuffle the 64-bit arg first: */
	CVMCPUemitMoveTo64BitRegister(con, CVMCPU_ARG1_REG, CVMCPU_ARG1_REG);
	/* Shuffle the remaining 32-bit args: */
	CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE,
			       CVMCPU_ARG2_REG, CVMCPU_ARG3_REG,
			       CVMJIT_NOSETCC);
	CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE,
			       CVMCPU_ARG3_REG, CVMCPU_ARG4_REG,
			       CVMJIT_NOSETCC);
#endif

        CVMJITprintCodegenComment(("Do volatile putfield:"));
        CVMJITaddCodegenComment((con, "call CVMCCMruntimePutfield64Volatile"));
        CVMJITsetSymbolName((con, "CVMCCMruntimePutfield64Volatile"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimePutfield64Volatile);

	CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimePutfield64Volatile,
			       CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
	
	/* Release resources and publish the result: */
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), value);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), obj);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), fieldOffset);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 115:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[115] arrayAssignmentRhs32: reg32 ", CVMJITCompileExpression_thing_p);
#line 4924 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 116:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[116] arrayAssignmentRhs32: I2S reg32 ", CVMJITCompileExpression_thing_p);
#line 4931 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 117:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[117] arrayAssignmentRhs32: I2B reg32 ", CVMJITCompileExpression_thing_p);
#line 4932 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 118:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[118] arrayAssignmentRhs32: I2C reg32 ", CVMJITCompileExpression_thing_p);
#line 4933 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 119:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[119] root: ASSIGN arrayIndex reg64 ", CVMJITCompileExpression_thing_p);
#line 4936 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("*slotAddr64 = reg:"));
        storeArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 120:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[120] root: ASSIGN arrayIndex arrayAssignmentRhs32 ", CVMJITCompileExpression_thing_p);
#line 4942 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("*slotAddr32 = reg:"));
        storeArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 121:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[121] root: ASSIGN INDEX reg32 arraySubscript reg64 ", CVMJITCompileExpression_thing_p);
#line 4949 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        indexedStore(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 122:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[122] root: ASSIGN INDEX reg32 arraySubscript arrayAssignmentRhs32 ", CVMJITCompileExpression_thing_p);
#line 4955 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        indexedStore(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 123:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[123] root: DEFINE_VALUE32 reg32 ", CVMJITCompileExpression_thing_p);
#line 4966 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource* src = popResource(con);
	if (!CVMRMstoreDefinedValue(con, CVMJITCompileExpression_thing_p, src, 1)) {
            CVMJITerror(con, DEFINE_USED_NODE_MISMATCH,
                        "CVMJIT: Cannot store defined value");
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 124:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[124] root: DEFINE_VALUE64 reg64 ", CVMJITCompileExpression_thing_p);
#line 4974 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource* src = popResource(con);
	if (!CVMRMstoreDefinedValue(con, CVMJITCompileExpression_thing_p, src, 2)) {
            CVMJITerror(con, DEFINE_USED_NODE_MISMATCH,
                        "CVMJIT: Cannot store defined value");
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 125:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[125] root: LOAD_PHIS ", CVMJITCompileExpression_thing_p);
#line 4982 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    CVMRMloadOrReleasePhis(con, CVMJITCompileExpression_thing_p, CVM_TRUE /* load */);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 126:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[126] root: RELEASE_PHIS ", CVMJITCompileExpression_thing_p);
#line 4986 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    CVMRMloadOrReleasePhis(con, CVMJITCompileExpression_thing_p, CVM_FALSE /* release */);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 127:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[127] reg32: USED32 ", CVMJITCompileExpression_thing_p);
#line 5008 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    handleUsedNode(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 128:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[128] reg64: USED64 ", CVMJITCompileExpression_thing_p);
#line 5011 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

    handleUsedNode(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 129:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[129] root: VINVOKE parameters reg32 ", CVMJITCompileExpression_thing_p);
#line 5206 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Invoke a method w/ a void return type"));
	invokeMethod(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
   };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 130:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[130] invoke32_result: IINVOKE parameters reg32 ", CVMJITCompileExpression_thing_p);
#line 5213 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest;
        CVMJITprintCodegenComment(("Invoke a method w/ a 32bit return type"));
	dest = invokeMethod(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
   };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 131:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[131] invoke64_result: LINVOKE parameters reg32 ", CVMJITCompileExpression_thing_p);
#line 5222 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        CVMJITprintCodegenComment(("Invoke a method w/ a 64bit return type"));
	dest = invokeMethod(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
   };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 132:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[132] iargs: NULL_IARG ", CVMJITCompileExpression_thing_p);
#line 5478 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 133:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[133] iargs: IARG reg32 iargs ", CVMJITCompileExpression_thing_p);
#line 5479 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 134:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[134] iargs: IARG reg64 iargs ", CVMJITCompileExpression_thing_p);
#line 5480 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 135:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[135] intrinsicMB: METHOD_BLOCK ", CVMJITCompileExpression_thing_p);
#line 5484 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 136:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[136] intrinsicMB: reg32 ", CVMJITCompileExpression_thing_p);
#line 5491 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *mbptr = popResource(con);
        /* Just throw the mbptr resource away because we don't really need
           it. */
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), mbptr);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 137:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[137] effect: VINTRINSIC parameters intrinsicMB ", CVMJITCompileExpression_thing_p);
#line 5501 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef CVMJIT_INTRINSICS
        FLUSH_GOAL_TOP(con);
        invokeIntrinsicMethod(con, CVMJITCompileExpression_thing_p);
#else
        /* Should not get here when intrinsics are not supported: */
        CVMassert(CVM_FALSE);
        CVMJITerror(con, CANNOT_COMPILE,
                    "CVMJIT: IR syntax error: Intrinsics not supported");
#endif /* CVMJIT_INTRINSICS */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 138:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[138] invoke32_result: INTRINSIC32 parameters intrinsicMB ", CVMJITCompileExpression_thing_p);
#line 5516 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef CVMJIT_INTRINSICS
        FLUSH_GOAL_TOP(con);
        invokeIntrinsicMethod(con, CVMJITCompileExpression_thing_p);
#else
        /* Should not get here when intrinsics are not supported: */
        CVMassert(CVM_FALSE);
        CVMJITerror(con, CANNOT_COMPILE,
                    "CVMJIT: IR syntax error: Intrinsics not supported");
#endif /* CVMJIT_INTRINSICS */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 139:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[139] invoke64_result: INTRINSIC64 parameters intrinsicMB ", CVMJITCompileExpression_thing_p);
#line 5531 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef CVMJIT_INTRINSICS
        FLUSH_GOAL_TOP(con);
        invokeIntrinsicMethod(con, CVMJITCompileExpression_thing_p);
#else
        /* Should not get here when intrinsics are not supported: */
        CVMassert(CVM_FALSE);
        CVMJITerror(con, CANNOT_COMPILE,
                    "CVMJIT: IR syntax error: Intrinsics not supported");
#endif /* CVMJIT_INTRINSICS */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 140:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[140] effect: VINTRINSIC iargs intrinsicMB ", CVMJITCompileExpression_thing_p);
#line 5546 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef CVMJIT_INTRINSICS
        FLUSH_GOAL_TOP(con);
        invokeIntrinsicMethod(con, CVMJITCompileExpression_thing_p);
#else
        /* Should not get here when intrinsics are not supported: */
        CVMassert(CVM_FALSE);
        CVMJITerror(con, CANNOT_COMPILE,
                    "CVMJIT: IR syntax error: Intrinsics not supported");
#endif /* CVMJIT_INTRINSICS */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 141:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[141] reg32: INTRINSIC32 iargs intrinsicMB ", CVMJITCompileExpression_thing_p);
#line 5561 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef CVMJIT_INTRINSICS
        FLUSH_GOAL_TOP(con);
        invokeIntrinsicMethod(con, CVMJITCompileExpression_thing_p);
#else
        /* Should not get here when intrinsics are not supported: */
        CVMassert(CVM_FALSE);
        CVMJITerror(con, CANNOT_COMPILE,
                    "CVMJIT: IR syntax error: Intrinsics not supported");
#endif /* CVMJIT_INTRINSICS */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 142:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[142] reg64: INTRINSIC64 iargs intrinsicMB ", CVMJITCompileExpression_thing_p);
#line 5576 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef CVMJIT_INTRINSICS
        FLUSH_GOAL_TOP(con);
        invokeIntrinsicMethod(con, CVMJITCompileExpression_thing_p);
#else
        /* Should not get here when intrinsics are not supported: */
        CVMassert(CVM_FALSE);
        CVMJITerror(con, CANNOT_COMPILE,
                    "CVMJIT: IR syntax error: Intrinsics not supported");
#endif /* CVMJIT_INTRINSICS */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 143:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[143] reg32: METHOD_BLOCK ", CVMJITCompileExpression_thing_p);
#line 5589 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        CVMMethodBlock *mb = CVMJITirnodeGetConstantAddr(CVMJITCompileExpression_thing_p)->mb;
        CVMJITsetSymbolName((con, "mb %C.%M", CVMmbClassBlock(mb), mb));
        dest =
	    CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), (CVMInt32)mb);
	CVMRMpinResourceEagerlyIfDesireable(CVMRM_INT_REGS(con),
					    dest, GET_REGISTER_GOALS);
	/* Need this in case this constant is a CSE */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 144:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[144] reg32: NULLCHECK reg32 ", CVMJITCompileExpression_thing_p);
#line 5680 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        doNullCheck(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 145:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[145] reg32: FETCH_MB_FROM_VTABLE GET_VTBL reg32 voffMemSpec ", CVMJITCompileExpression_thing_p);
#line 5686 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMCPUMemSpec *vtblIndex = popMemSpec(con);
	CVMAddr        fixupAddress = popAddress(con);
	CVMRMResource* objPtr = popResource(con);
	CVMRMResource* vtblBase =
	   CVMRMgetResource(CVMRM_INT_REGS(con),
			    CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
	CVMRMResource* dest;

        CVMJITprintCodegenComment(("Fetch mb from vtable:"));
	CVMRMpinResource(CVMRM_INT_REGS(con), objPtr,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);

	/* LDR rv, [robj] */
        CVMJITaddCodegenComment((con, "Get object.cb"));
        CVMJITcsSetExceptionInstruction(con);
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
            CVMRMgetRegisterNumber(vtblBase), CVMRMgetRegisterNumber(objPtr),
            OBJECT_CB_OFFSET);

	/* Mask off special bits from the class pointer */
	/* BIC rv, rv, #3 */
        CVMCPUemitBinaryALUConstant(con, CVMCPU_BIC_OPCODE,
				    CVMRMgetRegisterNumber(vtblBase),
				    CVMRMgetRegisterNumber(vtblBase), 0x3,
				    CVMJIT_NOSETCC);

	/* Now find the vtable pointer in the classblock */
	/* LDR rv, [rv + CB_VTBL_OFF] */
        CVMJITaddCodegenComment((con, "Get cb.vtbl"));
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
            CVMRMgetRegisterNumber(vtblBase),
            CVMRMgetRegisterNumber(vtblBase), CB_VTBL_OFFSET);

	/* Now vtblBase holds the vtable pointer. Do the rest. */
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), objPtr);
	dest = CVMRMgetResource(CVMRM_INT_REGS(con), GET_REGISTER_GOALS, 1);

        /* LDR rdest, [rv + vtblIndex*4] */
        CVMJITaddCodegenComment((con, "method = cb.vtbl[methodIdx]"));
        CVMCPUmemspecPinResource(CVMRM_INT_REGS(con), vtblIndex, CVMRM_ANY_SET,
				 CVMRM_EMPTY_SET);
        CVMCPUemitMemoryReference(con, CVMCPU_LDR32_OPCODE,
            CVMRMgetRegisterNumber(dest), CVMRMgetRegisterNumber(vtblBase),
            CVMCPUmemspecGetToken(con, vtblIndex));
        CVMCPUmemspecRelinquishResource(CVMRM_INT_REGS(con), vtblIndex);

	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), vtblBase,
		CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p));
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), vtblBase);
	/* if there is resolution code to fixup, do it here */
	if (fixupAddress != 0){
	    /* go back into code generated by resolveConstant
	     * and make it read:
	     *	mov	dest, CVMCPU_RESULT1_REG
	     *	b	<here>
	     */
	    CVMAddr loadMbBranchTarget = CVMJITcbufGetLogicalPC(con);
            CVMJITcbufPushFixup(con, fixupAddress);
            CVMJITcsSetEmitInPlace(con);
	    CVMCPUemitMoveRegister(con, CVMCPU_MOV_OPCODE, 
				   CVMRMgetRegisterNumber(dest),
				   CVMCPU_RESULT1_REG, CVMJIT_NOSETCC);
	    CVMCPUemitBranch(con, loadMbBranchTarget, CVMCPU_COND_AL);
            CVMJITcsClearEmitInPlace(con);
	    CVMJITcbufPop(con);
	}

	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 146:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[146] root: MB_TEST_OUTOFLINE reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 5759 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef IAI_VIRTUAL_INLINE_CB_TEST
    CVMCPUALURhs* candidateMB = popALURhs(con);
    CVMRMResource* mb = popResource(con);

    CVMRMpinResource(CVMRM_INT_REGS(con), mb,
		     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
    CVMCPUalurhsPinResource(CVMRM_INT_REGS(con),
                            CVMCPU_CMP_OPCODE, candidateMB,
                            CVMRM_ANY_SET, CVMRM_EMPTY_SET);

    /* cmp mb, candidateMB */
    CVMCPUemitCompare(con, CVMCPU_CMP_OPCODE, CVMCPU_COND_NE,
                      CVMRMgetRegisterNumber(mb),
                      CVMCPUalurhsGetToken(con, candidateMB));

    /* branch back to the inlined method if the MB test pass */
    CVMCPUemitBranch(con,
        con->currentCompilationBlock->oolReturnAddress, CVMCPU_COND_EQ);

    CVMCPUalurhsRelinquishResource(CVMRM_INT_REGS(con), candidateMB);
    CVMRMrelinquishResource(CVMRM_INT_REGS(con), mb);

    /* No longer need to restrict register usage within the block
     * after this point.
     */
    CVMRMremoveRegSandboxRestriction(CVMRM_INT_REGS(con),
                                     con->currentCompilationBlock);

    CVMJITcsBeginBlock(con);	
#endif
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 147:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[147] reg32: FETCH_VCB reg32 ", CVMJITCompileExpression_thing_p);
#line 5794 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef IAI_VIRTUAL_INLINE_CB_TEST
  	CVMRMResource* objPtr = popResource(con);
	CVMRMResource* dest;

        CVMJITprintCodegenComment(("Fetch vcb:"));
        CVMRMpinResource(CVMRM_INT_REGS(con), objPtr,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	dest =	CVMRMgetResource(CVMRM_INT_REGS(con),
                                 GET_REGISTER_GOALS, 1);

	/* LDR rv, [robj] */
        CVMJITaddCodegenComment((con, "Get object.cb"));
        CVMJITcsSetExceptionInstruction(con);
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
                                           CVMRMgetRegisterNumber(dest),
                                           CVMRMgetRegisterNumber(objPtr),
                                           OBJECT_CB_OFFSET);

 	/* Mask off special bits from the class pointer */
	/* BIC rv, rv, #3 */
        CVMCPUemitBinaryALUConstant(con, CVMCPU_BIC_OPCODE,
				    CVMRMgetRegisterNumber(dest),
				    CVMRMgetRegisterNumber(dest), 0x3,
				    CVMJIT_NOSETCC);

	CVMRMrelinquishResource(CVMRM_INT_REGS(con), objPtr);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
#endif
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 148:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[148] reg32: FETCH_MB_FROM_VTABLE_OUTOFLINE reg32 voffMemSpec ", CVMJITCompileExpression_thing_p);
#line 5828 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
#ifdef IAI_VIRTUAL_INLINE_CB_TEST
        CVMCPUMemSpec *vtblIndex = popMemSpec(con);
        CVMRMResource* cb;
        CVMRMResource* dest;
#ifdef CVM_DEBUG_ASSERTS	
        CVMAddr fixupAddress  = popAddress(con);
#else
        popAddress(con);
#endif
        /* The incoming CB should not be trashed because
         * it comes from phi, which is not part of the reserved
         * registers.
         */
        cb = popResource(con);

        CVMJITprintCodegenComment(("Fetch mb from vtable for outofline:"));
        CVMRMpinResource(CVMRM_INT_REGS(con), cb,
                         CVMRM_ANY_SET, CVMRM_EMPTY_SET);
        dest = CVMRMgetResource(CVMRM_INT_REGS(con),
                                CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);

	/* Now find the vtable pointer in the classblock */
	/* LDR rVTBL, [rCB + CB_VTBL_OFF] */
        CVMJITaddCodegenComment((con, "Get cb.vtbl"));
        CVMCPUemitMemoryReferenceImmediate(con, CVMCPU_LDR32_OPCODE,
                                           CVMRMgetRegisterNumber(dest),
                                           CVMRMgetRegisterNumber(cb),
                                           CB_VTBL_OFFSET);

        /* LDR rDest, [rVTBL + vtblIndex*4] */
        CVMJITaddCodegenComment((con, "method = cb.vtbl[methodIdx]"));
        CVMCPUmemspecPinResource(CVMRM_INT_REGS(con), vtblIndex, 
			         CVMRM_ANY_SET, CVMRM_EMPTY_SET);

        CVMCPUemitMemoryReference(con, CVMCPU_LDR32_OPCODE,
                                  CVMRMgetRegisterNumber(dest), 
                                  CVMRMgetRegisterNumber(dest),
                                  CVMCPUmemspecGetToken(con, vtblIndex));

        CVMCPUmemspecRelinquishResource(CVMRM_INT_REGS(con), vtblIndex);
	CVMassert(fixupAddress == 0);

        CVMRMrelinquishResource(CVMRM_INT_REGS(con), cb);
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
#endif 
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 149:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[149] reg32: FETCH_MB_FROM_ITABLE GET_ITBL reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 5879 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource* mbPtr = popResource(con);
	CVMRMResource* objPtr = popResource(con);
	CVMRMResource* dest;

        objPtr = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), objPtr,
					  CVMCPU_ARG2_REG);
        mbPtr = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), mbPtr,
					 CVMCPU_ARG3_REG);
        CVMRMmajorSpill(con, ARG2|ARG3, CVMRM_SAFE_SET);
	dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
        CVMJITprintCodegenComment(("Fetch mb from itable:"));

	/* call the helper glue */
        CVMJITaddCodegenComment((con,
				 "call CVMCCMruntimeLookupInterfaceMBGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLookupInterfaceMBGlue"));
        CVMJITstatsRecordInc(con,
            CVMJIT_STATS_CVMCCMruntimeLookupInterfaceMB);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeLookupInterfaceMBGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);

	/* reserve a word for the guess method */
        CVMJITaddCodegenComment((con, "interface lookup guess"));
        CVMJITemitWord(con, 0);
	CVMJITcaptureStackmap(con, 0);

	/* Return value is in RESULT1 */
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), objPtr);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), mbPtr);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 150:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[150] reg32: invoke32_result ", CVMJITCompileExpression_thing_p);
#line 5916 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/* force into a register */
	CVMRMResource* operand = popResource(con);
	CVMassert(CVMRMisJavaStackTopValue(operand));
	CVMRMpinResource(CVMRM_INT_REGS(con), operand, GET_REGISTER_GOALS);
	CVMRMunpinResource(CVMRM_INT_REGS(con), operand);
	pushResource(con, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 151:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[151] reg64: invoke64_result ", CVMJITCompileExpression_thing_p);
#line 5926 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        /* force into a register */
        CVMRMResource *operand = popResource(con);
	CVMassert(CVMRMisJavaStackTopValue(operand));
	CVMRMpinResource(CVMRM_INT_REGS(con), operand, GET_REGISTER_GOALS);
        CVMRMunpinResource(CVMRM_INT_REGS(con), operand);
        pushResource(con, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 152:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[152] parameters: NULL_PARAMETER ", CVMJITCompileExpression_thing_p);
#line 5935 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 153:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[153] parameters: IPARAMETER param32 parameters ", CVMJITCompileExpression_thing_p);
#line 5936 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 154:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[154] parameters: LPARAMETER param64 parameters ", CVMJITCompileExpression_thing_p);
#line 5937 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 155:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[155] param32: invoke32_result ", CVMJITCompileExpression_thing_p);
#line 5938 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/* Free! Already on Stack  */
	CVMRMResource *operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 156:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[156] param32: reg32 ", CVMJITCompileExpression_thing_p);
#line 5943 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource *operand = popResource(con);
	CVMSMpushSingle(con, CVMRM_INT_REGS(con), operand);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 157:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[157] param64: invoke64_result ", CVMJITCompileExpression_thing_p);
#line 5948 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        /* Free! Already on Stack  */
        CVMRMResource *operand = popResource(con);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 158:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[158] param64: reg64 ", CVMJITCompileExpression_thing_p);
#line 5953 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource *operand = popResource(con);
	CVMSMpushDouble(con, CVMRM_INT_REGS(con), operand);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 159:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[159] effect: reg32 ", CVMJITCompileExpression_thing_p);
#line 5960 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 160:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[160] root: invoke32_result ", CVMJITCompileExpression_thing_p);
#line 5964 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/* the 0 cost here is a fib, but must be < the cost of a deferred
	 * pop of invoke32_result into a reg32, so that this instruction
	 * gets emitted
	 */
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);

	CVMSMpopSingle(con, NULL, NULL);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 161:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[161] effect: reg64 ", CVMJITCompileExpression_thing_p);
#line 5975 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 162:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[162] root: invoke64_result ", CVMJITCompileExpression_thing_p);
#line 5979 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/* the 0 cost here is a fib, but must be < the cost of a deferred
	 * pop of invoke64_result into a reg64, so that this instruction
	 * gets emitted
	 */
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), operand);
    
	CVMSMpopDouble(con, NULL, NULL);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 163:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[163] root: BCOND_INT reg32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 6272 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        compare32cc(con, CVMJITCompileExpression_thing_p, CVMCPU_CMP_OPCODE);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 164:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[164] root: BCOND_INT reg32 INEG32 aluRhs ", CVMJITCompileExpression_thing_p);
#line 6275 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        compare32cc(con, CVMJITCompileExpression_thing_p, CVMCPU_CMN_OPCODE);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 165:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[165] root: BCOND_LONG reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 6278 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        compare64cc(con, CVMJITCompileExpression_thing_p);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 166:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[166] root: BCOND_FLOAT reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 6282 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        fcomparecc(con, CVMJITCompileExpression_thing_p,
		   CVM_TRUE /* needBranch */, CVM_TRUE /* needSetcc */);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
#ifdef CVM_CG_EXPAND_UNREACHED_RULE
	case 167:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[167] root: BCOND_DOUBLE reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 6288 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        dcomparecc(con, CVMJITCompileExpression_thing_p,
		   CVM_TRUE /* needBranch */, CVM_TRUE /* needSetcc */);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
#endif /*CVM_CG_EXPAND_UNREACHED_RULE*/
	case 168:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[168] reg32: LCMP reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 6295 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITaddCodegenComment((con, "call CVMCCMruntimeLCmp"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeLCmp"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeLCmp);
        longBinary2WordHelper(con, (void*)CVMCCMruntimeLCmp, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 169:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[169] reg32: FCMPL reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 6305 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        fcomparecc(con, CVMJITCompileExpression_thing_p,
		   CVM_FALSE /* needBranch */, CVM_FALSE /* needSetcc */);
        dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);  
        pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 170:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[170] reg32: FCMPG reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 6317 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        fcomparecc(con, CVMJITCompileExpression_thing_p,
		   CVM_FALSE /* needBranch */, CVM_FALSE /* needSetcc */);
        dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 171:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[171] reg32: DCMPL reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 6330 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        dcomparecc(con, CVMJITCompileExpression_thing_p,
		   CVM_FALSE /* needBranch */, CVM_FALSE /* needSetcc */);
        dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 172:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[172] reg32: DCMPG reg64 reg64 ", CVMJITCompileExpression_thing_p);
#line 6342 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *dest;
        dcomparecc(con, CVMJITCompileExpression_thing_p,
		   CVM_FALSE /* needBranch */, CVM_FALSE /* needSetcc */);
        dest = CVMRMgetResourceSpecific(CVMRM_INT_REGS(con),
					CVMCPU_RESULT1_REG, 1);
        CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 173:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[173] root: TABLESWITCH reg32 ", CVMJITCompileExpression_thing_p);
#line 6984 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMJITTableSwitch* ts = CVMJITirnodeGetTableSwitchOp(CVMJITCompileExpression_thing_p);
	int i;
	int low = ts->low;
        int high = ts->high;
        CVMRMResource* key = popResource(con);
        int keyRegNo;

        CVMJITprintCodegenComment(("tableswitch"));
	CVMRMsynchronizeJavaLocals(con);

    if (CVMRMisConstant(key) && CVMRMgetConstant(key) == 0) {
            /* We know the key is a constant 0. So this can be simplified. */
            if (low <= 0 && high >= 0) {
                CVMJITIRBlock *target = ts->tableList[0-low];
                branchToBlock(con, CVMCPU_COND_AL, target);
            } else {
                branchToBlock(con, CVMCPU_COND_AL, ts->defaultTarget);
            }
        } else {
	    /*
	     * After pinning the key resource, we are going to modify
	     * the register it gets pinned to. This causes problems because
	     * the resource may be associated with a local or constant
	     * that will be used later. It could also be a DEFINE node,
	     * which means its refcount > 1. This is handled below.
	     */
            CVMRMpinResource(CVMRM_INT_REGS(con), key,
			     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    if (CVMRMgetRefCount(CVMRM_INT_REGS(con), key) > 1) {
		/* If there is more than one reference to this resource,
		 * then make a clone and use the clone as the key. This
		 * way we can change it without affecting the other
		 * references.
		 */
		CVMRMResource* clone =
		    CVMRMcloneResourceStrict(CVMRM_INT_REGS(con), key,
					     CVMRM_ANY_SET, CVMRM_EMPTY_SET);
		CVMRMunpinResource(CVMRM_INT_REGS(con), key);
		key = clone;
		keyRegNo = CVMRMgetRegisterNumber(key);
	    } else {
		/* The refcount is 1, but there may be a cached local
		 * or constant referencing this resource. We must
		 * relinquish it and then grab it again to force any
		 * cached referenced to be decached.
		 */
		keyRegNo = CVMRMgetRegisterNumber(key);
		CVMRMrelinquishResource(CVMRM_INT_REGS(con), key);
		key =
		    CVMRMgetResourceSpecific(CVMRM_INT_REGS(con), keyRegNo, 1);
	    }

	    /* subtract "low" from "key" since branch table is 0-based */
            if (low != 0) {
	        int opcode;    /* add or sub */
   	        int absLow;    /* absolute value of low */
	        if (low > 0) {
                    opcode = CVMCPU_SUB_OPCODE;
		    absLow = low;
	        } else {
	            opcode = CVMCPU_ADD_OPCODE;
	    	    absLow = -low;
	        }
                CVMCPUemitBinaryALUConstant(con, opcode, keyRegNo, keyRegNo,
				   	    absLow, CVMJIT_NOSETCC);
            }

            /* compare (key - low) to (high - low) */
	    {
	        CVMUint32 highLow = high - low;
                CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE,
					  CVMCPU_COND_HI, keyRegNo, highLow);
	    }

	    /* emit branch for the default target */
            branchToBlock(con, CVMCPU_COND_HI, ts->defaultTarget);

	    CVMCPUemitTableSwitchBranch(con, keyRegNo);
            CVMRMrelinquishResource(CVMRM_INT_REGS(con), key);

	    /* emit branches for each table element */
	    for (i = 0; i < ts->nElements; i++) {
	        CVMJITIRBlock* target = ts->tableList[i];
                branchToBlock(con, CVMCPU_COND_AL, target);
	    }
        }
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 174:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[174] root: LOOKUPSWITCH reg32 ", CVMJITCompileExpression_thing_p);
#line 7112 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	/*
	 * There are really 4 ways to do a lookup switch based on
	 * whether you not you want to do a binary search and whether or not
	 * you want a table driven loop, or an unrolled loop.
	 *
	 *   1. binary search with loop
	 *   2. binary search with unrolled loop
	 *   3. linear search with loop
	 *   4. linear search with unrolled loop
	 *
	 * Here are advantages and disadvantages of all:
	 *
	 *   -Unrolled loops are always faster then their loop counterparts,
	 *    but for large tables they are almost 50% bigger. The breakeven
	 *    point for size seems to be around 10 entries.
	 *   -Unrolled loops require little if any loads from memory, whereas
	 *    loops require 1 or 2 loads, depending on how the branch is
	 *     handled.
	 *   -Unrolled loop only require a register for the key and possibly
	 *    for a large temporary constant. loops require more registers,
	 *    espcially when doing a binary search.
	 *   -(1) is faster than (3) for large data sets, but slower for small.
	 *   -(2) is almost always faster than (4). It's only disadvantage
	 *    to (4) is that it may cause more constant values to be loaded
	 *    into a register  rather than used immediate value, but this 
	 *    is rare.
	 *
	 * In summary, a binary search with an unrolled loop is fastest and 
	 * the most register efficient and is worth the extra space overhead
         * for large tables, espcially since most lookup tables tend to be on
	 * the smaller size, so this is what we chose.
	 *
	 * To generate the code we need to traverse the sorted lookupswitch
	 * table in a manner similar to the way a binary search would. The
	 * difference is that we aren't looking for anything, but instead
	 *  have to visit each node and generate code for it.
	 *
	 * For each node visited we do the following (it helps to visualize the
	 * table organized as a balanced binary tree):
	 *  1. generate compare code to see if this node is a match.
	 *  2. If there is a "left subtree", push a context for it so we can
	 *     generate code for it later and generate a "blt" to it.
	 *  3. Generate a "beq" to branch to the correct target for a match
	 *  4. If there is a "right subtree", make it the next node to visit.
	 *  5. If there is not a "right subtree", pop the topmost node off of
	 *     the stack and make it the next node to visit. If there is 
	 *     nothing on the stack then we are done.
	 *
	 * There's one more thing that makes the code a bit tricky. We don't
	 * use cmp to compare the key to the matchValue. We use sub instead
	 * (or add if the value is negative). The advantage of this is that if
	 * the table has a bunch of values in a narrow range, but all are 
	 * > 255, then we can avoid having to do an ldr for each one of them.
	 * Instead, we just ldr the first one we need to check, and the rest
	 * of the time we can just base the "compare" values on the difference
	 * between the matchValue of the current node and the matchValue of
	 * the previous node we visited.
	 */
	CVMJITLookupSwitch* ls = CVMJITirnodeGetLookupSwitchOp(CVMJITCompileExpression_thing_p);
        CVMRMResource* key = popResource(con);
	int keyreg;
        CVMRMResource* scratch;
	int scratchreg;
     	CVMUint16 low  = 0;	
    	CVMUint16 high = ls->nPairs - 1;
    	CVMUint16 index = (low + high) / 2;
    	CVMLookupSwitchStackItem stack[16]; /* enough for nPairs == 64k */
    	CVMUint8  tos = 0;
	CVMBool useCmp; /* use cmp rather than adds and subs */
        int branchToDefaultPC = -1;
#ifdef CVMCPU_HAS_ALU_SETCC
	CVMInt32  prevMatchValue = 0;
#endif
        CVMJITcsSetEmitInPlace(con);

        CVMJITprintCodegenComment(("lookupswitch"));
	CVMRMsynchronizeJavaLocals(con);

	/* if there are no pairs, then just branch to the target. */
	if (ls->nPairs == 0) {
            branchToBlock(con, CVMCPU_COND_AL, ls->defaultTarget);
	    goto done;
	}

#ifndef CVMCPU_HAS_ALU_SETCC
	useCmp = CVM_TRUE;
#else
	{
	    /* if (hi - lo < 0), then that means it is actually greater than
	     * MININT, so we need to use compares rather than adds/subs.
	     */
	    int lo = ls->lookupList[0].matchValue;
	    int hi = ls->lookupList[ls->nPairs - 1].matchValue;
	    useCmp = (lo < 0) && (hi > 0) && (hi - lo < 0);
	}
#endif

	if (CVMRMisConstant(key) && CVMRMgetConstant(key) == 0) {
            /* We know the key is a constant 0. So this can be simplified. */
            int i;
            for (i = 0; i < ls->nPairs; i++) {
                if (ls->lookupList[i].matchValue == 0) {
                    /* If we found a match, then branch to its handler. */
                    branchToBlock(con, CVMCPU_COND_AL, ls->lookupList[i].dest);
                    goto done;
                }
            }
            branchToBlock(con, CVMCPU_COND_AL, ls->defaultTarget);
            goto done;
        }

	/*
	 * After pinning the key resource, we are going to modify
	 * the register it gets pinned to. This causes problems because
	 * the resource may be associated with a local or constant
	 * that will be used later. It could also be a DEFINE node,
	 * which means its refcount > 1. This is handled below.
	 */
	CVMRMpinResource(CVMRM_INT_REGS(con), key,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	if (CVMRMgetRefCount(CVMRM_INT_REGS(con), key) > 1) {
	    /* If there is more than one reference to this resource,
	     * then make a clone and use the clone as the key. This
	     * way we can change it without affecting the other
	     * references.
	     */
	    CVMRMResource* clone =
		CVMRMcloneResourceStrict(CVMRM_INT_REGS(con), key,
					 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    CVMRMunpinResource(CVMRM_INT_REGS(con), key);
	    key = clone;
	    keyreg = CVMRMgetRegisterNumber(key);
	} else {
	    /* The refcount is 1, but there may be a cached local
	     * or constant referencing this resource. We must
	     * relinquish it and then grab it again to force any
	     * cached referenced to be decached.
	     */
	    keyreg = CVMRMgetRegisterNumber(key);
	    CVMRMrelinquishResource(CVMRM_INT_REGS(con), key);
	    key =
		CVMRMgetResourceSpecific(CVMRM_INT_REGS(con), keyreg, 1);
	}
	
	/*
	 * We may need a scratch register for loading large constant
	 * values into. Get it now (and allow for any necessary spills).
	 * because it will be to late once we start generating code (we
	 * don't want the regman state to change)
	 */
	scratch = CVMRMgetResource(CVMRM_INT_REGS(con),
				   CVMRM_ANY_SET, CVMRM_EMPTY_SET, 1);
	scratchreg = CVMRMgetRegisterNumber(scratch);
	
	
	/*
	 * On each iteration, look at the lookupswitch node at index,
	 * generate the appropriate code, push state for a "lower" node
	 * to be generated if necessary, and setup state for next "higher"
	 * node to be generated if necessary. We are all done what there
	 * are no nodes on the stack and no "higher" nodes left.
	 */
	con->inConditionalCode = CVM_TRUE;
	while (1) {
	    /*
	     * diff is the absolute value of the difference between this
	     * nodes matchValue and the previous nodes matchValue. It needs
	     * to be an absolute value and an uint because it may be larger
	     * than MAXINT.
	     */
#ifdef CVMCPU_HAS_ALU_SETCC
	    CVMUint16      prevIndex;
#endif
	    CVMInt32       matchValue = ls->lookupList[index].matchValue;
	    
	    CVMtraceJITCodegen((".match%d (low=%d index=%d high=%d)\n",
				index, low, index, high));
#ifdef CVMCPU_HAS_ALU_SETCC
	    if (!useCmp) {
		CVMUint32      diff;
		int            opcode; /* do we add or subtract diff */
		/* compute diff */
		if (prevMatchValue > matchValue) {
		    opcode = CVMCPU_ADD_OPCODE;
		    diff = prevMatchValue - matchValue;
		} else {
		    opcode = CVMCPU_SUB_OPCODE;
		    diff = matchValue - prevMatchValue;
		}
		
		/* add or subtract diff from key and setcc.
		 * WARNING: don't let CVMCPUemitBinaryALUConstant() load a
		 * large constant into a register. This will change the
		 * regman state, which is a no-no in conditionally executed
		 * code. Instead, manually load the large constant here
		 * using CVMCPUemitLoadConstant(), which won't affect the
		 * regman state.
		 */
		if (CVMCPUalurhsIsEncodableAsImmediate(opcode, diff)) {
		    CVMCPUemitBinaryALUConstant(con, opcode,
		        keyreg, keyreg, diff, CVMJIT_SETCC);
		} else {
		    CVMCPUemitLoadConstant(con, scratchreg, diff);
		    CVMCPUemitBinaryALURegister(con, opcode,
			keyreg, keyreg, scratchreg, CVMJIT_SETCC);
		}
	    }
#endif
	    
	    /*
	     * There are 3 states we can be in.
	     * 1. low < index < high, which means that if we don't have
	     *    a match, we need to deal both with the possiblity of
	     *    index being too big or too large.
	     * 2. low == index < high, which means that if we don't have
	     *    a match, we only need to deal with the possibility of
	     *    index being too small.
	     * 3. low == index == high, which means either we find a
	     *    a match this time or the lookup failed.
	     * NOTE: low < index == high is not possible.
	     */
	    if (low != high) {          /* (1) and (2) */
		if (index != low) {     /* (1) only */
		    if (useCmp) {
			/* WARNING: don't let regman know about constant */
			if (CVMCPUalurhsIsEncodableAsImmediate(
                                CVMCPU_CMP_OPCODE, matchValue)) {
			    CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE,
				CVMCPU_COND_LT, keyreg, matchValue);
			} else {
			    CVMCPUemitLoadConstant(con, scratchreg,
						   matchValue);
			    CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE,
			        CVMCPU_COND_LT, keyreg, scratchreg);
			}
		    }
		    /* If key is too small, branch to node that will handle
		     * a smaller match, but first push a context so we can
		     * generate code for that node later and also backpatch
		     * this reference to it.
		     */
		    CVMCPUemitBranch(con, 0, CVMCPU_COND_LT);
#ifdef CVMCPU_HAS_DELAY_SLOT
                    pushLookupNode(stack, &tos, low, index - 1, index,
                                   CVMJITcbufGetLogicalPC(con) -
				    2 * CVMCPU_INSTRUCTION_SIZE);
#else
                    pushLookupNode(stack, &tos, low, index - 1, index,
                                   CVMJITcbufGetLogicalPC(con) -
                                    1 * CVMCPU_INSTRUCTION_SIZE);
#endif
		}
		if (useCmp) {
		    /* WARNING: don't let regman know about constant */
		    if (CVMCPUalurhsIsEncodableAsImmediate(
                            CVMCPU_CMP_OPCODE, matchValue)) {
			CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE,
			    CVMCPU_COND_EQ, keyreg, matchValue);
		    } else {
			CVMCPUemitLoadConstant(con, scratchreg, matchValue);
			CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE,
		            CVMCPU_COND_EQ, keyreg, scratchreg);
		    }
		}
		/* If we found a match, then branch to its handler. */
		branchToBlock(con, CVMCPU_COND_EQ,
			      ls->lookupList[index].dest);
		/* Setup for node to handle the next higher range. */
#ifdef CVMCPU_HAS_ALU_SETCC
		prevIndex = index;
#endif
		low = index + 1;
	    } else {                     /* (3) low == index == high */
		CVMLookupSwitchStackItem* item;
		if (useCmp) {
		    /* WARNING: don't let regman know about constant */
		    if (CVMCPUalurhsIsEncodableAsImmediate(
                            CVMCPU_CMP_OPCODE, matchValue)) {
			CVMCPUemitCompareConstant(con, CVMCPU_CMP_OPCODE,
		            CVMCPU_COND_EQ, keyreg, matchValue);
		    } else {
			CVMCPUemitLoadConstant(con, scratchreg, matchValue);
			CVMCPUemitCompareRegister(con, CVMCPU_CMP_OPCODE,
		            CVMCPU_COND_EQ, keyreg, scratchreg);
		    }
		}
		
		/* If EQ we have a match */
		branchToBlock(con, CVMCPU_COND_EQ, ls->lookupList[index].dest);

                /* Else we have a lookup failure and need to branch to the
                   default target. Note we are only allowed one backward branch
                   to the default target in this lookupswitch, due to how
                   gcCheckPcsSize is computed by the front end. Therefore
                   if we've already generated a backward branch to the default,
                   target we'll just branch to this branch instead.
                */
                if (branchToDefaultPC == -1) {
                    if (CVMJITirblockIsBackwardBranchTarget(ls->defaultTarget))
                    {
                        branchToDefaultPC = CVMJITcbufGetLogicalPC(con);
                    }
                    branchToBlock(con, CVMCPU_COND_AL, ls->defaultTarget);
                } else {
                    CVMJITaddCodegenComment((con,
                        "branch to default target branch"));
		    CVMCPUemitBranch(con, branchToDefaultPC, CVMCPU_COND_AL);
                }
		
		/* See if there are still some more nodes to generate. */
		item = popLookupNode(stack, &tos);
		if (item == NULL) {
		    break;
		}
		
		/* Setup for the handling of the popped node */
		low = item->low;
		high = item->high;
#ifdef CVMCPU_HAS_ALU_SETCC
		prevIndex = item->prevIndex;
#endif
		
		/* backpatch the branch in the node that created this node. */
		CVMJITfixupAddress(con,
				   item->logicalAddress,
				   CVMJITcbufGetLogicalPC(con),
				   CVMJIT_COND_BRANCH_ADDRESS_MODE);
	    }
	    
	    /* setup index and prevMatchValue for the next node */
	    index = (low + high) / 2;
#ifdef CVMCPU_HAS_ALU_SETCC
	    prevMatchValue = ls->lookupList[prevIndex].matchValue;
#endif
	}
	con->inConditionalCode = CVM_FALSE;
	
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), scratch);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), key);
 done:;
        CVMJITcsClearEmitInPlace(con);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 175:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[175] reg32: BOUNDS_CHECK reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 7523 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        emitBoundsCheck(con, CVMJITCompileExpression_thing_p, CVM_FALSE, 0);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 176:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[176] iconst32Index: BOUNDS_CHECK ICONST_32 reg32 ", CVMJITCompileExpression_thing_p);
#line 7528 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMInt32 arrayIndex =
	    CVMJITirnodeGetConstant32(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p))->j.i;
        emitBoundsCheck(con, CVMJITCompileExpression_thing_p, CVM_TRUE, arrayIndex);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 177:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[177] iconst32Index: ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 7534 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"

        pushIConst32(con, CVMJITirnodeGetConstant32(CVMJITCompileExpression_thing_p)->j.i);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 178:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[178] root: JSR ", CVMJITCompileExpression_thing_p);
#line 7537 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITIRBlock* target = CVMJITirnodeGetBranchOp(CVMJITCompileExpression_thing_p)->branchLabel;
	CVMRMsynchronizeJavaLocals(con);

	/* We need a major spill because unlike goto, we will be returning. */
	CVMRMmajorSpill(con, CVMRM_EMPTY_SET, CVMRM_EMPTY_SET);

	CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);
	jsrToBlock(con, target);
	CVMRMunpinAllIncomingLocals(con, target);

	/* We no longer do checkGC() here, because we don't have the proper
           local state for RET at this point. checkGC() is now done at 
           the beginning of JSR return targets. */
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 179:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[179] reg32: JSR_RETURN_ADDRESS ", CVMJITCompileExpression_thing_p);
#line 7553 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* dest;

	/* Get a resource to put the JSR return address in. */
	dest = CVMRMgetResourceStrict(CVMRM_INT_REGS(con),  
	    CVMCPU_JSR_RETURN_ADDRESS_SET & CVMRM_ANY_SET,
	    ~(CVMCPU_JSR_RETURN_ADDRESS_SET & CVMRM_ANY_SET), 1);
	/* force the return address into dest */
        CVMCPUemitLoadReturnAddress(con, CVMRMgetRegisterNumber(dest));
	CVMRMoccupyAndUnpinResource(CVMRM_INT_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 180:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[180] root: RET reg32 ", CVMJITCompileExpression_thing_p);
#line 7566 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* src = popResource(con);
	CVMRMpinResource(CVMRM_INT_REGS(con), src,
			 CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	CVMCPUemitRegisterBranch(con, CVMRMgetRegisterNumber(src));
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
	CVMJITcsBeginBlock(con);
	CVMJITdumpRuntimeConstantPool(con, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 181:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[181] root: GOTO ", CVMJITCompileExpression_thing_p);
#line 7576 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMJITIRBlock* target = CVMJITirnodeGetBranchOp(CVMJITCompileExpression_thing_p)->branchLabel;

	CVMRMsynchronizeJavaLocals(con);
	CVMRMpinAllIncomingLocals(con, target, CVM_FALSE);

	branchToBlock(con, CVMCPU_COND_AL, target);

	CVMRMunpinAllIncomingLocals(con, target);
	CVMJITcsBeginBlock(con);

	CVMJITdumpRuntimeConstantPool(con, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 182:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[182] root: RETURN ", CVMJITCompileExpression_thing_p);
#line 7625 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        /* Emit the one-way ticket home: */
        emitReturn(con, NULL, 0);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 183:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[183] root: IRETURN reg32 ", CVMJITCompileExpression_thing_p);
#line 7630 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        /* Emit the one-way ticket home: */
        emitReturn(con, CVMRM_INT_REGS(con), 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 184:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[184] root: LRETURN reg64 ", CVMJITCompileExpression_thing_p);
#line 7635 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        /* Emit the one-way ticket home: */
        emitReturn(con, CVMRM_INT_REGS(con), 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 185:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[185] root: ATHROW reg32 ", CVMJITCompileExpression_thing_p);
#line 7640 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* src  = popResource(con);
	CVMRMsynchronizeJavaLocals(con);
        src = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), src,
				       CVMCPU_ARG3_REG);
	/* A "major" spill is not needed here.  We are leaving,
	  so we only care about the locals */
#if 0
	CVMRMmajorSpill(con, ARG3, CVMRM_SAFE_SET);
#endif
        CVMJITaddCodegenComment((con, "goto CVMCCMruntimeThrowObjectGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeThrowObjectGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeThrowObject);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeThrowObjectGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_CPBRANCHOK);
        CVMJITcsBeginBlock(con);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 186:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[186] reg32: CHECKCAST reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 7842 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        instanceTest(con, CVMJITCompileExpression_thing_p, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 187:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[187] reg32: INSTANCEOF reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 7848 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        instanceTest(con, CVMJITCompileExpression_thing_p, CVM_TRUE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 188:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[188] root: MONITOR_ENTER reg32 ", CVMJITCompileExpression_thing_p);
#line 7853 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* src  = popResource(con);

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        /* Make JSP point just past the last argument currently on the stack */
        CVMSMadjustJSP(con);
#endif

        src = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), src,
				       CVMCPU_ARG3_REG);
	CVMRMmajorSpill(con, ARG3, CVMRM_SAFE_SET);

        CVMJITaddCodegenComment((con, "call CVMCCMruntimeMonitorEnterGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeMonitorEnterGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeMonitorEnter);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeMonitorEnterGlue,
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);
	CVMJITcaptureStackmap(con, 0);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 189:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[189] effect: MONITOR_EXIT reg32 ", CVMJITCompileExpression_thing_p);
#line 7876 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMRMResource* src  = popResource(con);
        src = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con), src,
				       CVMCPU_ARG3_REG);

#ifndef CVMCPU_HAS_POSTINCREMENT_STORE
        /* Make JSP point just past the last argument currently on the stack */
        CVMSMadjustJSP(con);
#endif

	CVMRMmajorSpill(con, ARG3, CVMRM_SAFE_SET);

        CVMJITaddCodegenComment((con, "call CVMCCMruntimeMonitorExitGlue"));
        CVMJITsetSymbolName((con, "CVMCCMruntimeMonitorExitGlue"));
        CVMJITstatsRecordInc(con, CVMJIT_STATS_CVMCCMruntimeMonitorExit);
        CVMCPUemitAbsoluteCall(con, (void*)CVMCCMruntimeMonitorExitGlue, 
                               CVMJIT_CPDUMPOK, CVMJIT_NOCPBRANCH);
        CVMJITcsBeginBlock(con);
	CVMJITcaptureStackmap(con, 0);
	CVMRMrelinquishResource(CVMRM_INT_REGS(con), src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 190:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[190] reg32: RESOLVE_REF ", CVMJITCompileExpression_thing_p);
#line 8220 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        pushResource(con, resolveConstant(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS));
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 191:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[191] voffMemSpec: RESOLVE_REF ", CVMJITCompileExpression_thing_p);
#line 8226 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        CVMRMResource *operand = resolveConstant(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
        pushMemSpecRegister(con, CVM_TRUE, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 192:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[192] voffMemSpec: ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 8234 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	pushAddress(con, 0);
        pushMemSpecImmediate(con, CVMJITirnodeGetConstant32(CVMJITCompileExpression_thing_p)->j.i);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 193:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[193] reg32: CHECKINIT CLASS_BLOCK reg32 ", CVMJITCompileExpression_thing_p);
#line 8242 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
        doCheckInit(con, CVMJITCompileExpression_thing_p);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 194:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[194] effect: MAP_PC ", CVMJITCompileExpression_thing_p);
#line 8246 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
	CVMCompiledPcMapTable* pcMapTable = con->pcMapTable;
        CVMUint16 javaPc = CVMJITirnodeGetTypeNode(CVMJITCompileExpression_thing_p)->mapPcNode.javaPcToMap;
	CVMUint16 compiledPc = CVMJITcbufGetLogicalPC(con);

	CVMCompiledPcMap* pcMap = 
	   &(pcMapTable->maps[con->mapPcNodeCount++]);

	CVMassert(con->mapPcNodeCount <= pcMapTable->numEntries);

        if (compiledPc == con->currentCompilationBlock->logicalAddress) {
#ifdef CVM_JIT_REGISTER_LOCALS
	    /*
	     * If this is the MAP_PC node at the start of a basic block that
	     * is also a backwards branch target, then we need to set the
	     * compiledPc to the real start of the block that will load
	     * incoming locals. This is necessary for OSR to work.
	     */
	    if (con->currentCompilationBlock->loadLocalsLogicalAddress != 0) {
                compiledPc =
		    con->currentCompilationBlock->loadLocalsLogicalAddress;
	    }
#endif /* CVM_JIT_REGISTER_LOCALS */

            if (CVMJITirblockIsBackwardBranchTarget(
                            con->currentCompilationBlock))
            {
                CVMemitThreadSchedHook(con);
            }

        }

	pcMap->javaPc = javaPc;
	pcMap->compiledPc = compiledPc;

	CVMJITprintCodegenComment((
            "MAP_PC idepth=%d javaPc=%d compiledPc=%d",
	    con->inliningDepth, javaPc, compiledPc));
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 195:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[195] root: OUTOFLINEINVOKE ", CVMJITCompileExpression_thing_p);
#line 8379 "../../src/portlibs/jit/risc/jitgrammarrules.jcs"
{
    printInliningInfo(CVMJITCompileExpression_thing_p, "Out-of-line invocation");
#ifdef CVMJIT_COUNT_VIRTUAL_INLINE_STATS
    {
        CVMJITIRNode*    mbNode;
        CVMInt32 constant;
        CVMMethodBlock *mb;
        CVMBool isHit;
        CVMRMResource *mbRes;
        CVMRMResource *isHitRes;

        mbNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
        constant = CVMJITirnodeGetConstantAddr(mbNode)->vAddr;
        mb = (CVMMethodBlock *)(constant & ~0x1);
        isHit = (CVMBool)(constant & 0x1);

        CVMconsolePrintf("INLINING %s %C.%M in %C.%M\n",
			 isHit?"Hit ":"Miss", CVMmbClassBlock(mb), mb,
                         CVMmbClassBlock(con->mb), con->mb);

        mbRes =
	    CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), (CVMInt32)mb);
        isHitRes =
	    CVMRMbindResourceForConstant32(CVMRM_INT_REGS(con), isHit);
        mbRes = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con),
					 mbRes, CVMCPU_ARG1_REG);
        isHitRes = CVMRMpinResourceSpecific(CVMRM_INT_REGS(con),
					    isHitRes, CVMCPU_ARG2_REG);

        /* Spill the outgoing registers if necessary: */
        CVMRMminorSpill(con, ARG1|ARG2);

        CVMCPUemitAbsoluteCall(con, (void*)reportVirtualInlineHitMiss,
                               CVMJIT_NOCPDUMP, CVMJIT_NOCPBRANCH);

        CVMRMrelinquishResource(CVMRM_INT_REGS(con), mbRes);
        CVMRMrelinquishResource(CVMRM_INT_REGS(con), isHitRes);
    }
#endif /* CVMJIT_COUNT_VIRTUAL_INLINE_STATS */
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 196:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[196] freg32: FSEQUENCE_R effect freg32 ", CVMJITCompileExpression_thing_p);
#line 52 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 197:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[197] freg64: DSEQUENCE_R effect freg64 ", CVMJITCompileExpression_thing_p);
#line 57 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 198:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[198] freg32: FSEQUENCE_L freg32 effect ", CVMJITCompileExpression_thing_p);
#line 65 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 199:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[199] freg64: DSEQUENCE_L freg64 effect ", CVMJITCompileExpression_thing_p);
#line 70 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
    passLastEvaluated(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
};
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 200:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[200] freg32: ICONST_32 ", CVMJITCompileExpression_thing_p);
#line 75 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

        const2Reg32(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 201:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[201] freg64: ICONST_64 ", CVMJITCompileExpression_thing_p);
#line 78 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	/*
	 * Here we probably don't know the actual type of the
	 * constant value. There will be big trouble if long integers
	 * and double floats need different treatment.
	 */
	CVMRMResource* dest = CVMRMgetResource(CVMRM_FP_REGS(con),
					       GET_FLOAT_REGISTER_GOALS, 2);
	int destregno = CVMRMgetRegisterNumber(dest);
	CVMJavaVal64 v64;
	CVMmemCopy64(v64.v, CVMJITirnodeGetConstant64(CVMJITCompileExpression_thing_p)->j.v);
        CVMCPUemitLoadLongConstantFP(con, destregno, &v64);
	CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 202:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[202] freg32: LOCAL32 ", CVMJITCompileExpression_thing_p);
#line 95 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMJITLocal* l = CVMJITirnodeGetLocal( CVMJITCompileExpression_thing_p );
	CVMRMResource* dest =
	    CVMRMbindResourceForLocal(CVMRM_FP_REGS(con), 1,
				      CVM_FALSE, l->localNo);
	CVMRMpinResourceEagerlyIfDesireable(CVMRM_FP_REGS(con),
					    dest, GET_FLOAT_REGISTER_GOALS);
	CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 203:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[203] freg32: FETCH32 STATIC32 reg32 ", CVMJITCompileExpression_thing_p);
#line 107 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do getstatic:"));
        CVMJITaddCodegenComment((con,
            "value{I|F|O} = getstatic(staticFieldAddr);"));
        isVolatile =
            ((CVMJITirnodeGetUnaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITUNOP_VOLATILE_FIELD) != 0);
        getStaticField(con, CVMRM_FP_REGS(con),
		       CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS, CVMCPU_FLDR32_OPCODE, 1,
		       isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 204:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[204] freg32: FETCH32 FIELDREF32 reg32 memSpec ", CVMJITCompileExpression_thing_p);
#line 122 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do getfield:"));
        CVMJITaddCodegenComment((con, "value{I|F}"));
        isVolatile =
            ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0);
        fetchField(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p,
		   GET_FLOAT_REGISTER_GOALS, CVMCPU_FLDR32_OPCODE, 1,
		   isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 205:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[205] freg64: LOCAL64 ", CVMJITCompileExpression_thing_p);
#line 134 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMJITLocal* l = CVMJITirnodeGetLocal( CVMJITCompileExpression_thing_p );
	CVMRMResource* dest =
	    CVMRMbindResourceForLocal(CVMRM_FP_REGS(con), 2,
				      CVM_FALSE, l->localNo);
	CVMRMpinResourceEagerlyIfDesireable(CVMRM_FP_REGS(con),
					    dest, GET_FLOAT_REGISTER_GOALS);
	CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), dest, CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 206:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[206] freg64: FETCH64 STATIC64 reg32 ", CVMJITCompileExpression_thing_p);
#line 146 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do getstatic:"));
        CVMJITaddCodegenComment((con,
            "value{D} = getstatic(staticFieldAddr);"));
        getStaticField(con, CVMRM_FP_REGS(con),
		       CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS, CVMCPU_FLDR64_OPCODE, 2,
		       CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 207:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[207] freg64: FETCH64 FIELDREF64 reg32 memSpec ", CVMJITCompileExpression_thing_p);
#line 157 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do getfield:"));
        CVMJITaddCodegenComment((con, "value{D}"));
        fetchField(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p,
		   GET_FLOAT_REGISTER_GOALS, CVMCPU_FLDR64_OPCODE, 2,
		   CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 208:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[208] finvoke32_result: FINVOKE parameters reg32 ", CVMJITCompileExpression_thing_p);
#line 167 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* dest;
        CVMJITprintCodegenComment(("Invoke a method w/ a 32bit return type"));
	dest = invokeMethod(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	pushResource(con, dest);
   };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 209:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[209] finvoke64_result: DINVOKE parameters reg32 ", CVMJITCompileExpression_thing_p);
#line 176 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMRMResource *dest;
        CVMJITprintCodegenComment(("Invoke a method w/ a 64bit return type"));
	dest = invokeMethod(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
        pushResource(con, dest);
   };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 210:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[210] freg32: finvoke32_result ", CVMJITCompileExpression_thing_p);
#line 184 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        /* force into a register */
        CVMRMResource *operand = popResource(con);
	CVMassert(CVMRMisJavaStackTopValue(operand));
	CVMRMpinResource(CVMRM_FP_REGS(con), operand,
			 GET_FLOAT_REGISTER_GOALS);
        CVMRMunpinResource(CVMRM_FP_REGS(con), operand);
        pushResource(con, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 211:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[211] freg64: finvoke64_result ", CVMJITCompileExpression_thing_p);
#line 195 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        /* force into a register */
        CVMRMResource *operand = popResource(con);
        CVMassert(CVMRMisJavaStackTopValue(operand));
	CVMRMpinResource(CVMRM_FP_REGS(con), operand,
			 GET_FLOAT_REGISTER_GOALS);
        CVMRMunpinResource(CVMRM_FP_REGS(con), operand);
        pushResource(con, operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 212:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[212] parameters: FPARAMETER fparam32 parameters ", CVMJITCompileExpression_thing_p);
#line 205 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 213:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[213] parameters: DPARAMETER fparam64 parameters ", CVMJITCompileExpression_thing_p);
#line 206 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
;
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 214:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[214] fparam32: finvoke32_result ", CVMJITCompileExpression_thing_p);
#line 208 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	/* Free! Already on Stack  */
	CVMRMResource *operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 215:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[215] fparam32: freg32 ", CVMJITCompileExpression_thing_p);
#line 213 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource *operand = popResource(con);
	CVMSMpushSingle(con, CVMRM_FP_REGS(con), operand);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 216:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[216] fparam64: finvoke64_result ", CVMJITCompileExpression_thing_p);
#line 219 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        /* Free! Already on Stack  */
        CVMRMResource *operand = popResource(con);
        CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 217:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[217] fparam64: freg64 ", CVMJITCompileExpression_thing_p);
#line 224 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource *operand = popResource(con);
	CVMSMpushDouble(con, CVMRM_FP_REGS(con), operand);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 218:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[218] freg32: FETCH32 INDEX reg32 arraySubscript ", CVMJITCompileExpression_thing_p);
#line 241 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMJITIRNode* indexNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMUint16 typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
	const ArrayElemInfo* ei = &typeidToArrayElemInfo[typeId];
	if (canDoFloatLoadstore(ei, CVM_FALSE)){
	    indexedLoad(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	} else {
	    indexedLoad(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
			CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    moveIntToFPRegs(con, CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	}

    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 219:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[219] freg32: FETCH32 arrayIndex ", CVMJITCompileExpression_thing_p);
#line 257 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	ScaledIndexInfo *sinfo = popScaledIndexInfo(con); /* peek */
	const ArrayElemInfo* ei = sinfo->elemInfo;
	pushScaledIndexInfo(con, sinfo);
        CVMJITprintCodegenComment(("Do *slotAddr32:"));
	if (canDoFloatLoadstore(ei, CVM_FALSE)){
	    fetchArraySlot(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p,
			   GET_FLOAT_REGISTER_GOALS);
	}else{
	    fetchArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
			   CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    moveIntToFPRegs(con, CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	}

    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 220:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[220] freg64: FETCH64 INDEX reg32 arraySubscript ", CVMJITCompileExpression_thing_p);
#line 275 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMJITIRNode* indexNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMUint16 typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
	const ArrayElemInfo* ei = &typeidToArrayElemInfo[typeId];
	if (canDoFloatLoadstore(ei, CVM_FALSE)){
	    indexedLoad(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	} else {
	    indexedLoad(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
			CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    moveIntToFPRegs(con, CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	}

    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 221:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[221] freg64: FETCH64 arrayIndex ", CVMJITCompileExpression_thing_p);
#line 291 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	ScaledIndexInfo *sinfo = popScaledIndexInfo(con); /* peek */
	const ArrayElemInfo* ei = sinfo->elemInfo;
	pushScaledIndexInfo(con, sinfo);
        CVMJITprintCodegenComment(("Do *slotAddr32:"));
	if (canDoFloatLoadstore(ei, CVM_FALSE)){
	    fetchArraySlot(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p,
			   GET_FLOAT_REGISTER_GOALS);
	}else{
	    fetchArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p,
			   CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    moveIntToFPRegs(con, CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	}

    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 222:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[222] param32: freg32 ", CVMJITCompileExpression_thing_p);
#line 313 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource *operand = popResource(con);
	CVMSMpushSingle(con, CVMRM_FP_REGS(con), operand);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 223:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[223] param64: freg64 ", CVMJITCompileExpression_thing_p);
#line 318 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource *operand = popResource(con);
	CVMSMpushDouble(con, CVMRM_FP_REGS(con), operand);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 224:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[224] root: ASSIGN LOCAL32 freg32 ", CVMJITCompileExpression_thing_p);
#line 332 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	CVMJITIRNode*  localNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMJITIRNode*  rhsNode = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	CVMJITLocal*   lhs = CVMJITirnodeGetLocal(localNode);
	int target;

	if (rhsNode->decorationType == CVMJIT_REGHINT_DECORATION) {
	    target = 1U << rhsNode->decorationData.regHint;
	} else {
	    target = CVMRM_FP_ANY_SET;
	}

	CVMRMpinResource(CVMRM_FP_REGS(con), rhs, target, CVMRM_EMPTY_SET);
	CVMRMstoreJavaLocal(CVMRM_FP_REGS(con), rhs, 1,
			    CVM_FALSE, lhs->localNo);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), rhs);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 225:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[225] root: ASSIGN FIELDREF32 reg32 memSpec freg32 ", CVMJITCompileExpression_thing_p);
#line 353 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do putfield:"));
        CVMJITaddCodegenComment((con,
            "putfield(obj, fieldOffset, value{I|F});"));
        isVolatile =
            ((CVMJITirnodeGetBinaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITBINOP_VOLATILE_FIELD) != 0);
        setField(con, CVMRM_FP_REGS(con), CVMCPU_FSTR32_OPCODE,
                 isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 226:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[226] root: ASSIGN STATIC32 reg32 freg32 ", CVMJITCompileExpression_thing_p);
#line 366 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMBool isVolatile;
        CVMJITprintCodegenComment(("Do putstatic:"));
        CVMJITaddCodegenComment((con,
            "putstatic(staticFieldAddr, value{I|F|O})"));
        isVolatile =
            ((CVMJITirnodeGetUnaryNodeFlag(CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p)) &
             CVMJITUNOP_VOLATILE_FIELD) != 0);
        setStaticField(con, CVMRM_FP_REGS(con), CVMCPU_FSTR32_OPCODE,
		       isVolatile);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 227:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[227] root: IRETURN freg32 ", CVMJITCompileExpression_thing_p);
#line 378 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        /* Emit the one-way ticket home: */
        emitReturn(con, CVMRM_FP_REGS(con), 1);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 228:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[228] root: ASSIGN INDEX reg32 arraySubscript freg32 ", CVMJITCompileExpression_thing_p);
#line 385 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMJITIRNode* indexNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMUint16 typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
	const ArrayElemInfo* ei = &typeidToArrayElemInfo[typeId];
	if (canDoFloatLoadstore(ei, CVM_TRUE)){
	    indexedStore(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	} else {
	    CVMJITIRNode *f32 = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	    moveFPToIntRegs(con, f32, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    indexedStore(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 229:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[229] root: ASSIGN arrayIndex freg32 ", CVMJITCompileExpression_thing_p);
#line 399 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	ScaledIndexInfo *sinfo = popScaledIndexInfo(con); /* peek */
	const ArrayElemInfo* ei = sinfo->elemInfo;
	pushScaledIndexInfo(con, sinfo);
	pushResource(con, rhs);
        CVMJITprintCodegenComment(("*slotAddr32 = freg:"));
	if (canDoFloatLoadstore(ei, CVM_TRUE)){
	    storeArraySlot(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	}else{
	    CVMJITIRNode *f32 = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	    moveFPToIntRegs(con, f32, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    storeArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 230:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[230] root: ASSIGN LOCAL64 freg64 ", CVMJITCompileExpression_thing_p);
#line 415 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	CVMJITIRNode*  localNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMJITIRNode*  rhsNode = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	CVMJITLocal*   lhs = CVMJITirnodeGetLocal(localNode);
	int target;

	if (rhsNode->decorationType == CVMJIT_REGHINT_DECORATION) {
	    target = 1U << rhsNode->decorationData.regHint;
	} else {
	    target = CVMRM_FP_ANY_SET;
	}

	CVMRMpinResource(CVMRM_FP_REGS(con), rhs, target, CVMRM_EMPTY_SET);
	CVMRMstoreJavaLocal(CVMRM_FP_REGS(con), rhs, 2,
			    CVM_FALSE, lhs->localNo);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), rhs);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 231:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[231] root: ASSIGN FIELDREF64 reg32 memSpec freg64 ", CVMJITCompileExpression_thing_p);
#line 436 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do putfield:"));
        CVMJITaddCodegenComment((con,
            "putfield(obj, fieldOffset, value{L|D});"));
        setField(con, CVMRM_FP_REGS(con), CVMCPU_FSTR64_OPCODE, CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 232:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[232] root: ASSIGN STATIC64 reg32 freg64 ", CVMJITCompileExpression_thing_p);
#line 444 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMJITprintCodegenComment(("Do putstatic:"));
        CVMJITaddCodegenComment((con,
            "putstatic(staticFieldAddr, value{L|D})"));
        setStaticField(con, CVMRM_FP_REGS(con), CVMCPU_FSTR64_OPCODE,
		       CVM_FALSE);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 233:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[233] root: LRETURN freg64 ", CVMJITCompileExpression_thing_p);
#line 452 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        /* Emit the one-way ticket home: */
        emitReturn(con, CVMRM_FP_REGS(con), 2);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 234:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[234] root: ASSIGN INDEX reg32 arraySubscript freg64 ", CVMJITCompileExpression_thing_p);
#line 459 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMJITIRNode* indexNode = CVMJITirnodeGetLeftSubtree(CVMJITCompileExpression_thing_p);
	CVMUint16 typeId = CVMJITirnodeGetBinaryOp(indexNode)->data;
	const ArrayElemInfo* ei = &typeidToArrayElemInfo[typeId];
	if (canDoFloatLoadstore(ei, CVM_TRUE)){
	    indexedStore(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	} else {
	    CVMJITIRNode *f64 = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	    moveFPToIntRegs(con, f64, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    indexedStore(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 235:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[235] root: ASSIGN arrayIndex freg64 ", CVMJITCompileExpression_thing_p);
#line 473 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* rhs = popResource(con);
	ScaledIndexInfo *sinfo = popScaledIndexInfo(con); /* peek */
	const ArrayElemInfo* ei = sinfo->elemInfo;
	pushScaledIndexInfo(con, sinfo);
	pushResource(con, rhs);
        CVMJITprintCodegenComment(("*slotAddr32 = freg:"));
	if (canDoFloatLoadstore(ei, CVM_TRUE)){
	    storeArraySlot(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	}else{
	    CVMJITIRNode *f64 = CVMJITirnodeGetRightSubtree(CVMJITCompileExpression_thing_p);
	    moveFPToIntRegs(con, f64, CVMRM_ANY_SET, CVMRM_EMPTY_SET);
	    storeArraySlot(con, CVMRM_INT_REGS(con), CVMJITCompileExpression_thing_p);
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 236:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[236] freg32: FIDENT freg32 ", CVMJITCompileExpression_thing_p);
#line 496 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* src;
	if (!CVMJIT_DID_SEMANTIC_ACTION(CVMJITCompileExpression_thing_p)){
	    src = popResource(con);
	    CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), src, CVMJITCompileExpression_thing_p);
	    /* CVMconsolePrintf("Initial evaluation of "); */
	} else {
	    src = CVMRMfindResource(CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	    /* CVMconsolePrintf("Reiteration of "); */
	    CVMassert(src != NULL);
	}
	/*
	    CVMconsolePrintf("Float IDENT32 ID %d, resource 0x%x\n",
	    CVMJITCompileExpression_thing_p->nodeID, src);
	*/
	pushResource(con, src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 237:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[237] freg64: DIDENT freg64 ", CVMJITCompileExpression_thing_p);
#line 515 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* src;
	if (!CVMJIT_DID_SEMANTIC_ACTION(CVMJITCompileExpression_thing_p)){
	    src = popResource(con);
	    CVMRMoccupyAndUnpinResource(CVMRM_FP_REGS(con), src, CVMJITCompileExpression_thing_p);
	    /* CVMconsolePrintf("Initial evaluation of "); */
	} else {
	    src = CVMRMfindResource(CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p);
	    /* CVMconsolePrintf("Reiteration of "); */
	    CVMassert(src != NULL);
	}
	/*
	    CVMconsolePrintf("Float IDENT64 ID %d, resource 0x%x\n",
	    CVMJITCompileExpression_thing_p->nodeID, src);
	*/
	pushResource(con, src);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 238:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[238] effect: freg32 ", CVMJITCompileExpression_thing_p);
#line 534 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 239:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[239] root: finvoke32_result ", CVMJITCompileExpression_thing_p);
#line 538 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	/* the 0 cost here is a fib, but must be < the cost of a deferred
	 * pop of invoke32_result into a reg32, so that this instruction
	 * gets emitted
	 */
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);

	CVMSMpopSingle(con, NULL, NULL);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 240:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[240] effect: freg64 ", CVMJITCompileExpression_thing_p);
#line 549 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 241:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[241] root: finvoke64_result ", CVMJITCompileExpression_thing_p);
#line 553 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
	/* the 0 cost here is a fib, but must be < the cost of a deferred
	 * pop of invoke64_result into a reg64, so that this instruction
	 * gets emitted
	 */
	CVMRMResource* operand = popResource(con);
	CVMRMrelinquishResource(CVMRM_FP_REGS(con), operand);
    
	CVMSMpopDouble(con, NULL, NULL);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 242:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[242] root: FDEFINE freg32 ", CVMJITCompileExpression_thing_p);
#line 571 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMRMResource* src = popResource(con);
	if (!CVMRMstoreDefinedValue(con, CVMJITCompileExpression_thing_p, src, 1)) {
	    return -2;  /*  fail */
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 243:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[243] root: DDEFINE freg64 ", CVMJITCompileExpression_thing_p);
#line 578 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"
{
        CVMRMResource* src = popResource(con);
	if (!CVMRMstoreDefinedValue(con, CVMJITCompileExpression_thing_p, src, 2)) {
	    return -2;  /*  fail */
	}
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 244:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[244] freg32: FUSED ", CVMJITCompileExpression_thing_p);
#line 585 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    handleUsedNode(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 245:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[245] freg64: DUSED ", CVMJITCompileExpression_thing_p);
#line 588 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    handleUsedNode(con, CVMRM_FP_REGS(con), CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 246:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[246] freg32: FADD freg32 freg32 ", CVMJITCompileExpression_thing_p);
#line 666 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_FADD_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 247:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[247] freg32: FSUB freg32 freg32 ", CVMJITCompileExpression_thing_p);
#line 668 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_FSUB_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 248:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[248] freg32: FMUL freg32 freg32 ", CVMJITCompileExpression_thing_p);
#line 670 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_FMUL_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 249:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[249] freg32: FDIV freg32 freg32 ", CVMJITCompileExpression_thing_p);
#line 672 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_FDIV_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 250:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[250] freg32: FNEG freg32 ", CVMJITCompileExpression_thing_p);
#line 676 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatUnaryOp(con,  CVMCPU_FNEG_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 251:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[251] freg64: DADD freg64 freg64 ", CVMJITCompileExpression_thing_p);
#line 679 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_DADD_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 252:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[252] freg64: DSUB freg64 freg64 ", CVMJITCompileExpression_thing_p);
#line 681 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_DSUB_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 253:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[253] freg64: DMUL freg64 freg64 ", CVMJITCompileExpression_thing_p);
#line 683 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_DMUL_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 254:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[254] freg64: DDIV freg64 freg64 ", CVMJITCompileExpression_thing_p);
#line 685 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatBinaryOp(con, CVMCPU_DDIV_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 255:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[255] freg64: DNEG freg64 ", CVMJITCompileExpression_thing_p);
#line 689 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

    floatUnaryOp(con,  CVMCPU_DNEG_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 256:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[256] root: BCOND_FLOAT freg32 freg32 ", CVMJITCompileExpression_thing_p);
#line 764 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

	compareFloats(con, CVMJITCompileExpression_thing_p, CVMCPU_FCMP_OPCODE);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 257:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[257] root: BCOND_DOUBLE freg64 freg64 ", CVMJITCompileExpression_thing_p);
#line 767 "../../src/portlibs/jit/risc/jitfloatgrammarrules.jcs"

	compareFloats(con, CVMJITCompileExpression_thing_p, CVMCPU_DCMP_OPCODE);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 258:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[258] reg32: IDIV32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 35 "../../src/mips/javavm/runtime/jit/jitgrammarrules.jcs"

	wordBinaryOpWithReg32Rhs(con, CVMMIPS_DIV_OPCODE, CVMJITCompileExpression_thing_p,
	    GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 259:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[259] reg32: IREM32 reg32 reg32 ", CVMJITCompileExpression_thing_p);
#line 38 "../../src/mips/javavm/runtime/jit/jitgrammarrules.jcs"

	wordBinaryOpWithReg32Rhs(con, CVMMIPS_REM_OPCODE, CVMJITCompileExpression_thing_p,
	    GET_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 260:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[260] reg32: freg32 ", CVMJITCompileExpression_thing_p);
#line 160 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"
{
	moveFPToIntRegs(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 261:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[261] freg32: reg32 ", CVMJITCompileExpression_thing_p);
#line 164 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"
{
	moveIntToFPRegs(con, CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 262:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[262] reg64: freg64 ", CVMJITCompileExpression_thing_p);
#line 168 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"
{
	moveFPToIntRegs(con, CVMJITCompileExpression_thing_p, GET_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 263:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[263] freg64: reg64 ", CVMJITCompileExpression_thing_p);
#line 172 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"
{
	moveIntToFPRegs(con, CVMJITCompileExpression_thing_p, GET_FLOAT_REGISTER_GOALS);
    };
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 264:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[264] freg32: I2F reg32 ", CVMJITCompileExpression_thing_p);
#line 177 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"

    mipsIntToFloat(con,  CVMMIPS_I2F_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 265:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[265] freg64: I2D reg32 ", CVMJITCompileExpression_thing_p);
#line 181 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"

    mipsIntToFloat(con,  CVMMIPS_I2D_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 266:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[266] freg64: F2D freg32 ", CVMJITCompileExpression_thing_p);
#line 185 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"

    floatUnaryOp(con,  CVMMIPS_F2D_OPCODE, CVMJITCompileExpression_thing_p, 2, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	case 267:
	CVMJITdoStartOfCodegenRuleAction(con, ruleno, "[267] freg32: D2F freg64 ", CVMJITCompileExpression_thing_p);
#line 189 "../../src/mips/javavm/runtime/jit/jitfloatgrammarrules.jcs"

    floatUnaryOp(con,  CVMMIPS_D2F_OPCODE, CVMJITCompileExpression_thing_p, 1, GET_FLOAT_REGISTER_GOALS);
	CVMJITdoEndOfCodegenRuleAction(con);
	break;
	}
#ifdef CVMJIT_JCS_MATCH_DAG
	CVMJIT_JCS_SET_STATE(CVMJITCompileExpression_thing_p, CVMJIT_JCS_STATE_ACTED);
#endif
	if ( !GOAL_STACK_EMPTY ){
		statsPopStack(ACTION);
		goal_top -= 1;
		goal_top->curr_attribute +=1;
		goto continue_rule;
	}
	return 0;
}

#ifdef CVMJITCompileExpression_DEBUG
enum type { t_binary, t_unary, t_leaf, t_eot };

/* symbol table of node types */
struct symbol { int symval; char * symname; enum type t; };

static struct symbol debug_symboltable[] = {
	{ LOCAL32, "LOCAL32", t_leaf },
	{ LOCAL64, "LOCAL64", t_leaf },
	{ STATIC32, "STATIC32", t_unary },
	{ STATIC64, "STATIC64", t_unary },
	{ STATIC64VOL, "STATIC64VOL", t_unary },
	{ IDENT32, "IDENT32", t_unary },
	{ IDENT64, "IDENT64", t_unary },
	{ EXCEPTION_OBJECT, "EXCEPTION_OBJECT", t_leaf },
	{ NEW_OBJECT, "NEW_OBJECT", t_unary },
	{ NEW_ARRAY_REF, "NEW_ARRAY_REF", t_binary },
	{ MULTI_NEW_ARRAY_REF, "MULTI_NEW_ARRAY_REF", t_binary },
	{ NEW_ARRAY_BASIC, "NEW_ARRAY_BASIC", t_unary },
	{ DEFINE_VALUE32, "DEFINE_VALUE32", t_unary },
	{ DEFINE_VALUE64, "DEFINE_VALUE64", t_unary },
	{ LOAD_PHIS, "LOAD_PHIS", t_leaf },
	{ RELEASE_PHIS, "RELEASE_PHIS", t_leaf },
	{ USED32, "USED32", t_leaf },
	{ USED64, "USED64", t_leaf },
	{ ICONST_32, "ICONST_32", t_leaf },
	{ ICONST_64, "ICONST_64", t_leaf },
	{ STRING_ICELL_CONST, "STRING_ICELL_CONST", t_leaf },
	{ METHOD_BLOCK, "METHOD_BLOCK", t_leaf },
	{ CLASS_BLOCK, "CLASS_BLOCK", t_leaf },
	{ ASSIGN, "ASSIGN", t_binary },
	{ IINVOKE, "IINVOKE", t_binary },
	{ LINVOKE, "LINVOKE", t_binary },
	{ VINVOKE, "VINVOKE", t_binary },
	{ IPARAMETER, "IPARAMETER", t_binary },
	{ LPARAMETER, "LPARAMETER", t_binary },
	{ NULL_PARAMETER, "NULL_PARAMETER", t_leaf },
	{ GET_VTBL, "GET_VTBL", t_unary },
	{ GET_ITBL, "GET_ITBL", t_unary },
	{ FETCH_MB_FROM_VTABLE, "FETCH_MB_FROM_VTABLE", t_binary },
	{ FETCH_MB_FROM_ITABLE, "FETCH_MB_FROM_ITABLE", t_binary },
	{ FETCH_VCB, "FETCH_VCB", t_unary },
	{ FETCH_MB_FROM_VTABLE_OUTOFLINE, "FETCH_MB_FROM_VTABLE_OUTOFLINE", t_binary },
	{ MB_TEST_OUTOFLINE, "MB_TEST_OUTOFLINE", t_binary },
	{ FETCH32, "FETCH32", t_unary },
	{ FETCH64, "FETCH64", t_unary },
	{ INDEX, "INDEX", t_binary },
	{ ALENGTH, "ALENGTH", t_unary },
	{ NULLCHECK, "NULLCHECK", t_unary },
	{ FIELDREFOBJ, "FIELDREFOBJ", t_binary },
	{ FIELDREF32, "FIELDREF32", t_binary },
	{ FIELDREF64, "FIELDREF64", t_binary },
	{ FIELDREF64VOL, "FIELDREF64VOL", t_binary },
	{ ISEQUENCE_R, "ISEQUENCE_R", t_binary },
	{ LSEQUENCE_R, "LSEQUENCE_R", t_binary },
	{ VSEQUENCE_R, "VSEQUENCE_R", t_binary },
	{ ISEQUENCE_L, "ISEQUENCE_L", t_binary },
	{ LSEQUENCE_L, "LSEQUENCE_L", t_binary },
	{ VSEQUENCE_L, "VSEQUENCE_L", t_binary },
	{ INEG32, "INEG32", t_unary },
	{ NOT32, "NOT32", t_unary },
	{ INT2BIT32, "INT2BIT32", t_unary },
	{ IADD32, "IADD32", t_binary },
	{ ISUB32, "ISUB32", t_binary },
	{ IMUL32, "IMUL32", t_binary },
	{ IDIV32, "IDIV32", t_binary },
	{ IREM32, "IREM32", t_binary },
	{ AND32, "AND32", t_binary },
	{ OR32, "OR32", t_binary },
	{ XOR32, "XOR32", t_binary },
	{ SLL32, "SLL32", t_binary },
	{ SRL32, "SRL32", t_binary },
	{ SRA32, "SRA32", t_binary },
	{ INEG64, "INEG64", t_unary },
	{ IADD64, "IADD64", t_binary },
	{ ISUB64, "ISUB64", t_binary },
	{ IMUL64, "IMUL64", t_binary },
	{ IDIV64, "IDIV64", t_binary },
	{ IREM64, "IREM64", t_binary },
	{ AND64, "AND64", t_binary },
	{ OR64, "OR64", t_binary },
	{ XOR64, "XOR64", t_binary },
	{ SLL64, "SLL64", t_binary },
	{ SRL64, "SRL64", t_binary },
	{ SRA64, "SRA64", t_binary },
	{ LCMP, "LCMP", t_binary },
	{ FNEG, "FNEG", t_unary },
	{ FADD, "FADD", t_binary },
	{ FSUB, "FSUB", t_binary },
	{ FMUL, "FMUL", t_binary },
	{ FDIV, "FDIV", t_binary },
	{ FREM, "FREM", t_binary },
	{ FCMPL, "FCMPL", t_binary },
	{ FCMPG, "FCMPG", t_binary },
	{ DNEG, "DNEG", t_unary },
	{ DADD, "DADD", t_binary },
	{ DSUB, "DSUB", t_binary },
	{ DMUL, "DMUL", t_binary },
	{ DDIV, "DDIV", t_binary },
	{ DREM, "DREM", t_binary },
	{ DCMPL, "DCMPL", t_binary },
	{ DCMPG, "DCMPG", t_binary },
	{ I2B, "I2B", t_unary },
	{ I2C, "I2C", t_unary },
	{ I2S, "I2S", t_unary },
	{ I2L, "I2L", t_unary },
	{ I2F, "I2F", t_unary },
	{ I2D, "I2D", t_unary },
	{ L2I, "L2I", t_unary },
	{ L2F, "L2F", t_unary },
	{ L2D, "L2D", t_unary },
	{ F2D, "F2D", t_unary },
	{ F2I, "F2I", t_unary },
	{ F2L, "F2L", t_unary },
	{ D2F, "D2F", t_unary },
	{ D2I, "D2I", t_unary },
	{ D2L, "D2L", t_unary },
	{ TABLESWITCH, "TABLESWITCH", t_unary },
	{ LOOKUPSWITCH, "LOOKUPSWITCH", t_unary },
	{ BCOND_INT, "BCOND_INT", t_binary },
	{ BCOND_LONG, "BCOND_LONG", t_binary },
	{ BCOND_FLOAT, "BCOND_FLOAT", t_binary },
	{ BCOND_DOUBLE, "BCOND_DOUBLE", t_binary },
	{ BOUNDS_CHECK, "BOUNDS_CHECK", t_binary },
	{ GOTO, "GOTO", t_leaf },
	{ RETURN, "RETURN", t_leaf },
	{ JSR, "JSR", t_leaf },
	{ JSR_RETURN_ADDRESS, "JSR_RETURN_ADDRESS", t_leaf },
	{ RET, "RET", t_unary },
	{ IRETURN, "IRETURN", t_unary },
	{ LRETURN, "LRETURN", t_unary },
	{ ATHROW, "ATHROW", t_unary },
	{ CHECKCAST, "CHECKCAST", t_binary },
	{ INSTANCEOF, "INSTANCEOF", t_binary },
	{ MONITOR_ENTER, "MONITOR_ENTER", t_unary },
	{ MONITOR_EXIT, "MONITOR_EXIT", t_unary },
	{ RESOLVE_REF, "RESOLVE_REF", t_leaf },
	{ CHECKINIT, "CHECKINIT", t_binary },
	{ MAP_PC, "MAP_PC", t_leaf },
	{ BEGININLINING, "BEGININLINING", t_leaf },
	{ VENDINLINING, "VENDINLINING", t_leaf },
	{ OUTOFLINEINVOKE, "OUTOFLINEINVOKE", t_leaf },
	{ VINTRINSIC, "VINTRINSIC", t_binary },
	{ INTRINSIC32, "INTRINSIC32", t_binary },
	{ INTRINSIC64, "INTRINSIC64", t_binary },
	{ IARG, "IARG", t_binary },
	{ NULL_IARG, "NULL_IARG", t_leaf },
	{ FPARAMETER, "FPARAMETER", t_binary },
	{ DPARAMETER, "DPARAMETER", t_binary },
	{ FINVOKE, "FINVOKE", t_binary },
	{ DINVOKE, "DINVOKE", t_binary },
	{ FIDENT, "FIDENT", t_unary },
	{ DIDENT, "DIDENT", t_unary },
	{ FDEFINE, "FDEFINE", t_unary },
	{ DDEFINE, "DDEFINE", t_unary },
	{ FUSED, "FUSED", t_leaf },
	{ DUSED, "DUSED", t_leaf },
	{ FSEQUENCE_R, "FSEQUENCE_R", t_binary },
	{ DSEQUENCE_R, "DSEQUENCE_R", t_binary },
	{ FSEQUENCE_L, "FSEQUENCE_L", t_binary },
	{ DSEQUENCE_L, "DSEQUENCE_L", t_binary },
	{ 0, 0, t_eot} };

void
CVMJITCompileExpression_printtree( CVMJITIRNodePtr p )
{
	struct symbol * sp;
	char * symname;
	char buf[10];

	/* look up symbol */
	for ( sp = debug_symboltable; sp->t != t_eot; sp ++ )
		if (CVMJITgetMassagedIROpcode(con, p) == sp->symval) break;
	if ( sp->t == t_eot ){
		sprintf( buf, "<%d>", CVMJITgetMassagedIROpcode(con, p) );
		symname = buf;
	} else 
		symname = sp->symname;
	/* now print out what we got */
	fprintf( stderr, "(%s [%d] state %d ", symname, id(p), IRGetState(p));
	switch (sp->t){
	case t_binary:
		CVMJITCompileExpression_printtree( CVMJITirnodeGetLeftSubtree(p)  );
		CVMJITCompileExpression_printtree( CVMJITirnodeGetRightSubtree(p) );
		break;
	case t_unary:
		CVMJITCompileExpression_printtree( CVMJITirnodeGetLeftSubtree(p)  );
		break;
	}
	fputs( " ) ", stderr );
}
#endif

