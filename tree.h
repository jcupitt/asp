/* Asp applicative language
 * J.Cupitt, November 86
 * Exports for the tree builder */

extern void InitParseTree();
extern struct ExpressionNode *NilTree;
extern struct ConstVal *BuildStringConst();
extern struct ConstVal *BuildNumConst();
extern struct ConstVal *BuildBoolConst();
extern struct ConstVal *BuildNilConst();
extern struct ConstVal *BuildCharConst();
extern struct ExpressionNode *BuildConst();
extern struct ExpressionNode *BuildVarRef();
extern struct ExpressionNode *BuildIf();
extern struct ExpressionNode *BuildUop();
extern struct ExpressionNode *BuildBiop();
extern struct ExpressionNode *BuildListGen1();
extern struct ExpressionNode *BuildListGen2();
extern struct ExpressionNode *BuildListGen3();
extern struct ExpressionNode *BuildListGen4();
extern void ApplyAll();
