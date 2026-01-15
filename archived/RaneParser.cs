using System;
using System.Collections.Generic;

namespace RaneCompiler
{
    // AST Node base class
    public abstract class AstNode
    {
        public int Line { get; }
        protected AstNode(int line) => Line = line;
    }

    // Top-level nodes
    public class ModuleNode : AstNode
    {
        public string Name { get; }
        public List<ImportNode> Imports { get; }
        public List<DeclNode> Declarations { get; }
        public ModuleNode(string name, List<ImportNode> imports, List<DeclNode> declarations, int line)
            : base(line) { Name = name; Imports = imports; Declarations = declarations; }
    }

    public class ImportNode : AstNode
    {
        public string Path { get; }
        public string? Alias { get; }
        public List<string>? Symbols { get; }
        public ImportNode(string path, string? alias, List<string>? symbols, int line)
            : base(line) { Path = path; Alias = alias; Symbols = symbols; }
    }

    public abstract class DeclNode : AstNode
    {
        protected DeclNode(int line) : base(line) { }
    }

    public class NamespaceDecl : DeclNode
    {
        public string Name { get; }
        public List<DeclNode> Declarations { get; }
        public NamespaceDecl(string name, List<DeclNode> declarations, int line)
            : base(line) { Name = name; Declarations = declarations; }
    }

    public class TypeDecl : DeclNode
    {
        public string Name { get; }
        public TypeExpr Type { get; }
        public TypeDecl(string name, TypeExpr type, int line) : base(line) { Name = name; Type = type; }
    }

    public class AliasDecl : DeclNode
    {
        public string Name { get; }
        public TypeExpr Type { get; }
        public AliasDecl(string name, TypeExpr type, int line) : base(line) { Name = name; Type = type; }
    }

    public class ConstDecl : DeclNode
    {
        public string Name { get; }
        public TypeExpr? Type { get; }
        public ExprNode Init { get; }
        public ConstDecl(string name, TypeExpr? type, ExprNode init, int line)
            : base(line) { Name = name; Type = type; Init = init; }
    }

    public class StructDecl : DeclNode
    {
        public string Name { get; }
        public List<string> Attrs { get; }
        public List<FieldNode> Fields { get; }
        public List<string> Derives { get; }
        public StructDecl(string name, List<string> attrs, List<FieldNode> fields, List<string> derives, int line)
            : base(line) { Name = name; Attrs = attrs; Fields = fields; Derives = derives; }
    }

    public class EnumDecl : DeclNode
    {
        public string Name { get; }
        public TypeExpr? ReprType { get; }
        public List<VariantNode> Variants { get; }
        public EnumDecl(string name, TypeExpr? reprType, List<VariantNode> variants, int line)
            : base(line) { Name = name; ReprType = reprType; Variants = variants; }
    }

    public class VariantDecl : DeclNode
    {
        public string Name { get; }
        public List<TypeParam> TypeParams { get; }
        public List<CaseNode> Cases { get; }
        public VariantDecl(string name, List<TypeParam> typeParams, List<CaseNode> cases, int line)
            : base(line) { Name = name; TypeParams = typeParams; Cases = cases; }
    }

    public class UnionDecl : DeclNode
    {
        public string Name { get; }
        public List<FieldNode> Fields { get; }
        public UnionDecl(string name, List<FieldNode> fields, int line) : base(line) { Name = name; Fields = fields; }
    }

    public class CapabilityDecl : DeclNode
    {
        public string Name { get; }
        public CapabilityDecl(string name, int line) : base(line) { Name = name; }
    }

    public class ContractDecl : DeclNode
    {
        public string Name { get; }
        public List<ParamNode> Params { get; }
        public ExprNode Ensures { get; }
        public ContractDecl(string name, List<ParamNode> @params, ExprNode ensures, int line)
            : base(line) { Name = name; Params = @params; Ensures = ensures; }
    }

    public class ProcDecl : DeclNode
    {
        public Visibility Visibility { get; }
        public List<string> Qualifiers { get; }
        public string Name { get; }
        public List<TypeParam> TypeParams { get; }
        public List<ParamNode> Params { get; }
        public TypeExpr RetType { get; }
        public List<string> Requires { get; }
        public BlockNode Body { get; }
        public ProcDecl(Visibility visibility, List<string> qualifiers, string name, List<TypeParam> typeParams,
                        List<ParamNode> @params, TypeExpr retType, List<string> requires, BlockNode body, int line)
            : base(line) { Visibility = visibility; Qualifiers = qualifiers; Name = name; TypeParams = typeParams;
                          Params = @params; RetType = retType; Requires = requires; Body = body; }
    }

    public enum Visibility { Public, Protected, Private, Admin }

    public class FieldNode : AstNode
    {
        public string Name { get; }
        public TypeExpr Type { get; }
        public FieldNode(string name, TypeExpr type, int line) : base(line) { Name = name; Type = type; }
    }

    public class VariantNode : AstNode
    {
        public string Name { get; }
        public ExprNode? Value { get; }
        public VariantNode(string name, ExprNode? value, int line) : base(line) { Name = name; Value = value; }
    }

    public class CaseNode : AstNode
    {
        public string Name { get; }
        public TypeExpr? Payload { get; }
        public CaseNode(string name, TypeExpr? payload, int line) : base(line) { Name = name; Payload = payload; }
    }

    public class ParamNode : AstNode
    {
        public string Name { get; }
        public TypeExpr Type { get; }
        public ParamNode(string name, TypeExpr type, int line) : base(line) { Name = name; Type = type; }
    }

    public class TypeParam : AstNode
    {
        public string Name { get; }
        public TypeParam(string name, int line) : base(line) { Name = name; }
    }

    // Type expressions
    public abstract class TypeExpr : AstNode
    {
        protected TypeExpr(int line) : base(line) { }
    }

    public class NamedType : TypeExpr
    {
        public string Name { get; }
        public NamedType(string name, int line) : base(line) { Name = name; }
    }

    public class GenericType : TypeExpr
    {
        public string Name { get; }
        public List<TypeExpr> Args { get; }
        public GenericType(string name, List<TypeExpr> args, int line) : base(line) { Name = name; Args = args; }
    }

    public class ArrayType : TypeExpr
    {
        public TypeExpr ElemType { get; }
        public ExprNode Size { get; }
        public ArrayType(TypeExpr elemType, ExprNode size, int line) : base(line) { ElemType = elemType; Size = size; }
    }

    public class TupleType : TypeExpr
    {
        public List<TypeExpr> ElemTypes { get; }
        public TupleType(List<TypeExpr> elemTypes, int line) : base(line) { ElemTypes = elemTypes; }
    }

    public class FunctionType : TypeExpr
    {
        public List<TypeExpr> ParamTypes { get; }
        public TypeExpr RetType { get; }
        public FunctionType(List<TypeExpr> paramTypes, TypeExpr retType, int line)
            : base(line) { ParamTypes = paramTypes; RetType = retType; }
    }

    // Statements
    public abstract class StmtNode : AstNode
    {
        protected StmtNode(int line) : base(line) { }
    }

    public class BlockNode : StmtNode
    {
        public List<StmtNode> Statements { get; }
        public BlockNode(List<StmtNode> statements, int line) : base(line) { Statements = statements; }
    }

    public class LetStmt : StmtNode
    {
        public string Name { get; }
        public TypeExpr? Type { get; }
        public ExprNode? Init { get; }
        public LetStmt(string name, TypeExpr? type, ExprNode? init, int line)
            : base(line) { Name = name; Type = type; Init = init; }
    }

    public class AssignStmt : StmtNode
    {
        public ExprNode Target { get; }
        public ExprNode Value { get; }
        public AssignStmt(ExprNode target, ExprNode value, int line) : base(line) { Target = target; Value = value; }
    }

    public class ReturnStmt : StmtNode
    {
        public ExprNode? Expr { get; }
        public ReturnStmt(ExprNode? expr, int line) : base(line) { Expr = expr; }
    }

    public class IfStmt : StmtNode
    {
        public ExprNode Condition { get; }
        public BlockNode ThenBlock { get; }
        public BlockNode? ElseBlock { get; }
        public IfStmt(ExprNode condition, BlockNode thenBlock, BlockNode? elseBlock, int line)
            : base(line) { Condition = condition; ThenBlock = thenBlock; ElseBlock = elseBlock; }
    }

    public class WhileStmt : StmtNode
    {
        public ExprNode Condition { get; }
        public BlockNode Body { get; }
        public WhileStmt(ExprNode condition, BlockNode body, int line) : base(line) { Condition = condition; Body = body; }
    }

    public class ForStmt : StmtNode
    {
        public string VarName { get; }
        public TypeExpr? VarType { get; }
        public ExprNode Init { get; }
        public ExprNode Condition { get; }
        public ExprNode Increment { get; }
        public BlockNode Body { get; }
        public ForStmt(string varName, TypeExpr? varType, ExprNode init, ExprNode condition, ExprNode increment, BlockNode body, int line)
            : base(line) { VarName = varName; VarType = varType; Init = init; Condition = condition; Increment = increment; Body = body; }
    }

    public class MatchStmt : StmtNode
    {
        public ExprNode Expr { get; }
        public List<MatchCase> Cases { get; }
        public BlockNode? Default { get; }
        public MatchStmt(ExprNode expr, List<MatchCase> cases, BlockNode? @default, int line)
            : base(line) { Expr = expr; Cases = cases; Default = @default; }
    }

    public class SwitchStmt : StmtNode
    {
        public ExprNode Expr { get; }
        public List<SwitchCase> Cases { get; }
        public BlockNode? Default { get; }
        public SwitchStmt(ExprNode expr, List<SwitchCase> cases, BlockNode? @default, int line)
            : base(line) { Expr = expr; Cases = cases; Default = @default; }
    }

    public class DecideStmt : StmtNode
    {
        public ExprNode Expr { get; }
        public List<DecideCase> Cases { get; }
        public BlockNode? Default { get; }
        public DecideStmt(ExprNode expr, List<DecideCase> cases, BlockNode? @default, int line)
            : base(line) { Expr = expr; Cases = cases; Default = @default; }
    }

    public class TryStmt : StmtNode
    {
        public BlockNode TryBlock { get; }
        public string? CatchVar { get; }
        public BlockNode? CatchBlock { get; }
        public BlockNode? FinallyBlock { get; }
        public TryStmt(BlockNode tryBlock, string? catchVar, BlockNode? catchBlock, BlockNode? finallyBlock, int line)
            : base(line) { TryBlock = tryBlock; CatchVar = catchVar; CatchBlock = catchBlock; FinallyBlock = finallyBlock; }
    }

    public class WithStmt : StmtNode
    {
        public ExprNode Resource { get; }
        public string Name { get; }
        public BlockNode Body { get; }
        public WithStmt(ExprNode resource, string name, BlockNode body, int line)
            : base(line) { Resource = resource; Name = name; Body = body; }
    }

    public class DeferStmt : StmtNode
    {
        public ExprNode Expr { get; }
        public DeferStmt(ExprNode expr, int line) : base(line) { Expr = expr; }
    }

    public class AsmStmt : StmtNode
    {
        public List<string> Instrs { get; }
        public AsmStmt(List<string> instrs, int line) : base(line) { Instrs = instrs; }
    }

    public class GotoStmt : StmtNode
    {
        public ExprNode Target { get; }
        public GotoStmt(ExprNode target, int line) : base(line) { Target = target; }
    }

    public class LabelStmt : StmtNode
    {
        public string Name { get; }
        public LabelStmt(string name, int line) : base(line) { Name = name; }
    }

    public class TrapStmt : StmtNode
    {
        public ExprNode? Code { get; }
        public TrapStmt(ExprNode? code, int line) : base(line) { Code = code; }
    }

    public class HaltStmt : StmtNode
    {
        public HaltStmt(int line) : base(line) { }
    }

    public class LockStmt : StmtNode
    {
        public ExprNode Mutex { get; }
        public BlockNode Body { get; }
        public LockStmt(ExprNode mutex, BlockNode body, int line) : base(line) { Mutex = mutex; Body = body; }
    }

    public class MatchCase : AstNode
    {
        public ExprNode Pattern { get; }
        public BlockNode Body { get; }
        public MatchCase(ExprNode pattern, BlockNode body, int line) : base(line) { Pattern = pattern; Body = body; }
    }

    public class SwitchCase : AstNode
    {
        public ExprNode Value { get; }
        public BlockNode Body { get; }
        public SwitchCase(ExprNode value, BlockNode body, int line) : base(line) { Value = value; Body = body; }
    }

    public class DecideCase : AstNode
    {
        public ExprNode Value { get; }
        public BlockNode Body { get; }
        public DecideCase(ExprNode value, BlockNode body, int line) : base(line) { Value = value; Body = body; }
    }

    // Expressions
    public abstract class ExprNode : AstNode
    {
        protected ExprNode(int line) : base(line) { }
    }

    public class LiteralExpr : ExprNode
    {
        public object Value { get; }
        public LiteralExpr(object value, int line) : base(line) { Value = value; }
    }

    public class VarExpr : ExprNode
    {
        public string Name { get; }
        public VarExpr(string name, int line) : base(line) { Name = name; }
    }

    public class UnaryExpr : ExprNode
    {
        public TokenType Op { get; }
        public ExprNode Expr { get; }
        public UnaryExpr(TokenType op, ExprNode expr, int line) : base(line) { Op = op; Expr = expr; }
    }

    public class BinaryExpr : ExprNode
    {
        public ExprNode Left { get; }
        public TokenType Op { get; }
        public ExprNode Right { get; }
        public BinaryExpr(ExprNode left, TokenType op, ExprNode right, int line)
            : base(line) { Left = left; Op = op; Right = right; }
    }

    public class TernaryExpr : ExprNode
    {
        public ExprNode Condition { get; }
        public ExprNode ThenExpr { get; }
        public ExprNode ElseExpr { get; }
        public TernaryExpr(ExprNode condition, ExprNode thenExpr, ExprNode elseExpr, int line)
            : base(line) { Condition = condition; ThenExpr = thenExpr; ElseExpr = elseExpr; }
    }

    public class CallExpr : ExprNode
    {
        public ExprNode Callee { get; }
        public List<ExprNode> Args { get; }
        public CallExpr(ExprNode callee, List<ExprNode> args, int line) : base(line) { Callee = callee; Args = args; }
    }

    public class IndexExpr : ExprNode
    {
        public ExprNode Target { get; }
        public ExprNode Index { get; }
        public IndexExpr(ExprNode target, ExprNode index, int line) : base(line) { Target = target; Index = index; }
    }

    public class FieldExpr : ExprNode
    {
        public ExprNode Target { get; }
        public string Field { get; }
        public FieldExpr(ExprNode target, string field, int line) : base(line) { Target = target; Field = field; }
    }

    public class CastExpr : ExprNode
    {
        public ExprNode Expr { get; }
        public TypeExpr Type { get; }
        public CastExpr(ExprNode expr, TypeExpr type, int line) : base(line) { Expr = expr; Type = type; }
    }

    public class StructInitExpr : ExprNode
    {
        public string TypeName { get; }
        public List<FieldInit> Fields { get; }
        public StructInitExpr(string typeName, List<FieldInit> fields, int line)
            : base(line) { TypeName = typeName; Fields = fields; }
    }

    public class VariantInitExpr : ExprNode
    {
        public string VariantName { get; }
        public string Case { get; }
        public ExprNode? Payload { get; }
        public VariantInitExpr(string variantName, string @case, ExprNode? payload, int line)
            : base(line) { VariantName = variantName; Case = @case; Payload = payload; }
    }

    public class TupleExpr : ExprNode
    {
        public List<ExprNode> Elems { get; }
        public TupleExpr(List<ExprNode> elems, int line) : base(line) { Elems = elems; }
    }

    public class TupleDestructureExpr : ExprNode
    {
        public List<string> Vars { get; }
        public ExprNode Tuple { get; }
        public TupleDestructureExpr(List<string> vars, ExprNode tuple, int line)
            : base(line) { Vars = vars; Tuple = tuple; }
    }

    public class FieldInit : AstNode
    {
        public string Name { get; }
        public ExprNode Value { get; }
        public FieldInit(string name, ExprNode value, int line) : base(line) { Name = name; Value = value; }
    }

    // Parser class
    public class RaneParser
    {
        private readonly List<Token> _tokens;
        private int _current = 0;
        private readonly List<ParseError> _errors = new();

        public RaneParser(List<Token> tokens)
        {
            _tokens = tokens;
        }

        public (ModuleNode?, List<ParseError>) Parse()
        {
            try
            {
                var module = ParseModule();
                if (_errors.Count > 0) return (null, _errors);
                return (module, _errors);
            }
            catch (ParseException)
            {
                return (null, _errors);
            }
        }

        private ModuleNode ParseModule()
        {
            Consume(TokenType.Module, "Expect 'module'");
            string name = Consume(TokenType.Identifier, "Expect module name").Lexeme;
            var imports = new List<ImportNode>();
            while (Match(TokenType.Import))
            {
                imports.Add(ParseImport());
            }
            var declarations = new List<DeclNode>();
            while (!IsAtEnd())
            {
                declarations.Add(ParseDeclaration());
            }
            return new ModuleNode(name, imports, declarations, Previous().Line);
        }

        private ImportNode ParseImport()
        {
            string path = Consume(TokenType.Identifier, "Expect import path").Lexeme;
            string? alias = null;
            if (Match(TokenType.As))
            {
                alias = Consume(TokenType.Identifier, "Expect alias").Lexeme;
            }
            List<string>? symbols = null;
            if (Match(TokenType.LeftParen))
            {
                symbols = new List<string>();
                do
                {
                    symbols.Add(Consume(TokenType.Identifier, "Expect symbol").Lexeme);
                } while (Match(TokenType.Comma));
                Consume(TokenType.RightParen, "Expect ')' after symbols");
            }
            return new ImportNode(path, alias, symbols, Previous().Line);
        }

        private DeclNode ParseDeclaration()
        {
            if (Match(TokenType.Namespace)) return ParseNamespace();
            if (Match(TokenType.Type)) return ParseTypeDecl();
            if (Match(TokenType.Typealias) || Match(TokenType.Alias)) return ParseAliasDecl();
            if (Match(TokenType.Const) || Match(TokenType.Constexpr) || Match(TokenType.Consteval)) return ParseConstDecl();
            if (Match(TokenType.Struct)) return ParseStructDecl();
            if (Match(TokenType.Enum)) return ParseEnumDecl();
            if (Match(TokenType.Variant)) return ParseVariantDecl();
            if (Match(TokenType.Union)) return ParseUnionDecl();
            if (Match(TokenType.Capability)) return ParseCapabilityDecl();
            if (Match(TokenType.Contract)) return ParseContractDecl();
            if (Match(TokenType.Proc)) return ParseProcDecl();
            throw Error(Peek(), "Expect declaration");
        }

        private NamespaceDecl ParseNamespace()
        {
            string name = Consume(TokenType.Identifier, "Expect namespace name").Lexeme;
            Consume(TokenType.Colon, "Expect ':' after namespace name");
            var declarations = new List<DeclNode>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                declarations.Add(ParseDeclaration());
            }
            Consume(TokenType.End, "Expect 'end' after namespace");
            return new NamespaceDecl(name, declarations, Previous().Line);
        }

        private TypeDecl ParseTypeDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect type name").Lexeme;
            Consume(TokenType.Equal, "Expect '=' after type name");
            var type = ParseType();
            return new TypeDecl(name, type, Previous().Line);
        }

        private AliasDecl ParseAliasDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect alias name").Lexeme;
            Consume(TokenType.Equal, "Expect '=' after alias name");
            var type = ParseType();
            return new AliasDecl(name, type, Previous().Line);
        }

        private ConstDecl ParseConstDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect const name").Lexeme;
            TypeExpr? type = null;
            if (Match(TokenType.Colon))
            {
                type = ParseType();
            }
            Consume(TokenType.Equal, "Expect '=' after const name");
            var init = ParseExpression();
            return new ConstDecl(name, type, init, Previous().Line);
        }

        private StructDecl ParseStructDecl()
        {
            var attrs = new List<string>();
            while (Match(TokenType.At))
            {
                attrs.Add(Consume(TokenType.Identifier, "Expect attribute").Lexeme);
            }
            string name = Consume(TokenType.Identifier, "Expect struct name").Lexeme;
            Consume(TokenType.Colon, "Expect ':' after struct name");
            var fields = new List<FieldNode>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                string fName = Consume(TokenType.Identifier, "Expect field name").Lexeme;
                var fType = ParseType();
                fields.Add(new FieldNode(fName, fType, Previous().Line));
            }
            Consume(TokenType.End, "Expect 'end' after struct");
            var derives = new List<string>();
            if (Match(TokenType.At) && Match(TokenType.Derive))
            {
                Consume(TokenType.LeftParen, "Expect '(' after derive");
                do
                {
                    derives.Add(Consume(TokenType.Identifier, "Expect derive").Lexeme);
                } while (Match(TokenType.Comma));
                Consume(TokenType.RightParen, "Expect ')' after derives");
            }
            return new StructDecl(name, attrs, fields, derives, Previous().Line);
        }

        private EnumDecl ParseEnumDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect enum name").Lexeme;
            TypeExpr? reprType = null;
            if (Match(TokenType.Colon))
            {
                reprType = ParseType();
            }
            Consume(TokenType.Colon, "Expect ':' after enum name");
            var variants = new List<VariantNode>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                string vName = Consume(TokenType.Identifier, "Expect variant name").Lexeme;
                ExprNode? value = null;
                if (Match(TokenType.Equal))
                {
                    value = ParseExpression();
                }
                variants.Add(new VariantNode(vName, value, Previous().Line));
            }
            Consume(TokenType.End, "Expect 'end' after enum");
            return new EnumDecl(name, reprType, variants, Previous().Line);
        }

        private VariantDecl ParseVariantDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect variant name").Lexeme;
            var typeParams = new List<TypeParam>();
            if (Match(TokenType.Less))
            {
                do
                {
                    string tpName = Consume(TokenType.Identifier, "Expect type param").Lexeme;
                    typeParams.Add(new TypeParam(tpName, Previous().Line));
                } while (Match(TokenType.Comma));
                Consume(TokenType.Greater, "Expect '>' after type params");
            }
            Consume(TokenType.Colon, "Expect ':' after variant name");
            var cases = new List<CaseNode>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                string cName = Consume(TokenType.Identifier, "Expect case name").Lexeme;
                TypeExpr? payload = null;
                if (Match(TokenType.Colon))
                {
                    payload = ParseType();
                }
                cases.Add(new CaseNode(cName, payload, Previous().Line));
            }
            Consume(TokenType.End, "Expect 'end' after variant");
            return new VariantDecl(name, typeParams, cases, Previous().Line);
        }

        private UnionDecl ParseUnionDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect union name").Lexeme;
            Consume(TokenType.Colon, "Expect ':' after union name");
            var fields = new List<FieldNode>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                string fName = Consume(TokenType.Identifier, "Expect field name").Lexeme;
                var fType = ParseType();
                fields.Add(new FieldNode(fName, fType, Previous().Line));
            }
            Consume(TokenType.End, "Expect 'end' after union");
            return new UnionDecl(name, fields, Previous().Line);
        }

        private CapabilityDecl ParseCapabilityDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect capability name").Lexeme;
            return new CapabilityDecl(name, Previous().Line);
        }

        private ContractDecl ParseContractDecl()
        {
            string name = Consume(TokenType.Identifier, "Expect contract name").Lexeme;
            Consume(TokenType.LeftParen, "Expect '(' after contract name");
            var @params = new List<ParamNode>();
            if (!Check(TokenType.RightParen))
            {
                do
                {
                    string pName = Consume(TokenType.Identifier, "Expect param name").Lexeme;
                    Consume(TokenType.Colon, "Expect ':' after param name");
                    var pType = ParseType();
                    @params.Add(new ParamNode(pName, pType, Previous().Line));
                } while (Match(TokenType.Comma));
            }
            Consume(TokenType.RightParen, "Expect ')' after params");
            Consume(TokenType.Colon, "Expect ':' after params");
            Consume(TokenType.Ensures, "Expect 'ensures'");
            var ensures = ParseExpression();
            Consume(TokenType.End, "Expect 'end' after contract");
            return new ContractDecl(name, @params, ensures, Previous().Line);
        }

        private ProcDecl ParseProcDecl()
        {
            var visibility = Visibility.Public;
            if (Match(TokenType.Public)) visibility = Visibility.Public;
            else if (Match(TokenType.Protected)) visibility = Visibility.Protected;
            else if (Match(TokenType.Private)) visibility = Visibility.Private;
            else if (Match(TokenType.Admin)) visibility = Visibility.Admin;

            var qualifiers = new List<string>();
            while (Match(TokenType.Inline) || Match(TokenType.Async) || Match(TokenType.Dedicate) || Match(TokenType.Linear) || Match(TokenType.Nonlinear))
            {
                qualifiers.Add(Previous().Lexeme);
            }

            string name = Consume(TokenType.Identifier, "Expect proc name").Lexeme;
            var typeParams = new List<TypeParam>();
            if (Match(TokenType.Less))
            {
                do
                {
                    string tpName = Consume(TokenType.Identifier, "Expect type param").Lexeme;
                    typeParams.Add(new TypeParam(tpName, Previous().Line));
                } while (Match(TokenType.Comma));
                Consume(TokenType.Greater, "Expect '>' after type params");
            }

            Consume(TokenType.LeftParen, "Expect '(' after proc name");
            var @params = new List<ParamNode>();
            if (!Check(TokenType.RightParen))
            {
                do
                {
                    string pName = Consume(TokenType.Identifier, "Expect param name").Lexeme;
                    Consume(TokenType.Colon, "Expect ':' after param name");
                    var pType = ParseType();
                    @params.Add(new ParamNode(pName, pType, Previous().Line));
                } while (Match(TokenType.Comma));
            }
            Consume(TokenType.RightParen, "Expect ')' after params");

            Consume(TokenType.Arrow, "Expect '->' after params");
            var retType = ParseType();

            var requires = new List<string>();
            if (Match(TokenType.Requires))
            {
                Consume(TokenType.LeftParen, "Expect '(' after requires");
                do
                {
                    requires.Add(Consume(TokenType.Identifier, "Expect capability").Lexeme);
                } while (Match(TokenType.Comma));
                Consume(TokenType.RightParen, "Expect ')' after requires");
            }

            var body = ParseBlock();
            return new ProcDecl(visibility, qualifiers, name, typeParams, @params, retType, requires, body, Previous().Line);
        }

        private TypeExpr ParseType()
        {
            if (Match(TokenType.LeftBracket))
            {
                var elemType = ParseType();
                Consume(TokenType.Semicolon, "Expect ';' in array type");
                var size = ParseExpression();
                Consume(TokenType.RightBracket, "Expect ']' after array size");
                return new ArrayType(elemType, size, Previous().Line);
            }
            if (Match(TokenType.LeftParen))
            {
                var elemTypes = new List<TypeExpr>();
                if (!Check(TokenType.RightParen))
                {
                    do
                    {
                        elemTypes.Add(ParseType());
                    } while (Match(TokenType.Comma));
                }
                Consume(TokenType.RightParen, "Expect ')' after tuple types");
                if (Match(TokenType.Arrow))
                {
                    var retType = ParseType();
                    return new FunctionType(elemTypes, retType, Previous().Line);
                }
                return new TupleType(elemTypes, Previous().Line);
            }
            string name = Consume(TokenType.Identifier, "Expect type name").Lexeme;
            var args = new List<TypeExpr>();
            if (Match(TokenType.Less))
            {
                do
                {
                    args.Add(ParseType());
                } while (Match(TokenType.Comma));
                Consume(TokenType.Greater, "Expect '>' after type args");
                return new GenericType(name, args, Previous().Line);
            }
            return new NamedType(name, Previous().Line);
        }

        private BlockNode ParseBlock()
        {
            var statements = new List<StmtNode>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                statements.Add(ParseStatement());
            }
            Consume(TokenType.End, "Expect 'end' after block");
            return new BlockNode(statements, Previous().Line);
        }

        private StmtNode ParseStatement()
        {
            if (Match(TokenType.Let)) return ParseLetStmt();
            if (Match(TokenType.Return)) return ParseReturnStmt();
            if (Match(TokenType.If)) return ParseIfStmt();
            if (Match(TokenType.While)) return ParseWhileStmt();
            if (Match(TokenType.For)) return ParseForStmt();
            if (Match(TokenType.Match)) return ParseMatchStmt();
            if (Match(TokenType.Switch)) return ParseSwitchStmt();
            if (Match(TokenType.Decide)) return ParseDecideStmt();
            if (Match(TokenType.Try)) return ParseTryStmt();
            if (Match(TokenType.With)) return ParseWithStmt();
            if (Match(TokenType.Defer)) return ParseDeferStmt();
            if (Match(TokenType.Asm)) return ParseAsmStmt();
            if (Match(TokenType.Goto)) return ParseGotoStmt();
            if (Match(TokenType.Label)) return ParseLabelStmt();
            if (Match(TokenType.Trap)) return ParseTrapStmt();
            if (Match(TokenType.Halt)) return ParseHaltStmt();
            if (Match(TokenType.Lock)) return ParseLockStmt();
            // Assignment
            if (Check(TokenType.Identifier) && CheckNext(TokenType.Equal))
            {
                var target = ParsePrimary();
                Consume(TokenType.Equal, "Expect '=' in assignment");
                var value = ParseExpression();
                return new AssignStmt(target, value, Previous().Line);
            }
            // Default to expression statement
            var expr = ParseExpression();
            return new ExprStmt(expr, expr.Line);
        }

        private LetStmt ParseLetStmt()
        {
            string name = Consume(TokenType.Identifier, "Expect variable name").Lexeme;
            TypeExpr? type = null;
            if (Match(TokenType.Colon))
            {
                type = ParseType();
            }
            ExprNode? init = null;
            if (Match(TokenType.Equal))
            {
                init = ParseExpression();
            }
            return new LetStmt(name, type, init, Previous().Line);
        }

        private ReturnStmt ParseReturnStmt()
        {
            ExprNode? expr = null;
            if (!Check(TokenType.End) && !Check(TokenType.Semicolon))
            {
                expr = ParseExpression();
            }
            return new ReturnStmt(expr, Previous().Line);
        }

        private IfStmt ParseIfStmt()
        {
            var condition = ParseExpression();
            var thenBlock = ParseBlock();
            BlockNode? elseBlock = null;
            if (Match(TokenType.Else))
            {
                elseBlock = ParseBlock();
            }
            return new IfStmt(condition, thenBlock, elseBlock, Previous().Line);
        }

        private WhileStmt ParseWhileStmt()
        {
            var condition = ParseExpression();
            var body = ParseBlock();
            return new WhileStmt(condition, body, Previous().Line);
        }

        private ForStmt ParseForStmt()
        {
            Consume(TokenType.Let, "Expect 'let' in for loop");
            string varName = Consume(TokenType.Identifier, "Expect loop variable").Lexeme;
            TypeExpr? varType = null;
            if (Match(TokenType.Colon))
            {
                varType = ParseType();
            }
            Consume(TokenType.Equal, "Expect '=' after loop var");
            var init = ParseExpression();
            Consume(TokenType.Semicolon, "Expect ';' after init");
            var condition = ParseExpression();
            Consume(TokenType.Semicolon, "Expect ';' after condition");
            var increment = ParseExpression();
            var body = ParseBlock();
            return new ForStmt(varName, varType, init, condition, increment, body, Previous().Line);
        }

        private MatchStmt ParseMatchStmt()
        {
            var expr = ParseExpression();
            Consume(TokenType.Colon, "Expect ':' after match expr");
            var cases = new List<MatchCase>();
            BlockNode? @default = null;
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                if (Match(TokenType.Case))
                {
                    var pattern = ParseExpression();
                    Consume(TokenType.Colon, "Expect ':' after case pattern");
                    var body = ParseBlock();
                    cases.Add(new MatchCase(pattern, body, Previous().Line));
                }
                else if (Match(TokenType.Default))
                {
                    Consume(TokenType.Colon, "Expect ':' after default");
                    @default = ParseBlock();
                }
            }
            Consume(TokenType.End, "Expect 'end' after match");
            return new MatchStmt(expr, cases, @default, Previous().Line);
        }

        private SwitchStmt ParseSwitchStmt()
        {
            var expr = ParseExpression();
            Consume(TokenType.Colon, "Expect ':' after switch expr");
            var cases = new List<SwitchCase>();
            BlockNode? @default = null;
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                if (Match(TokenType.Case))
                {
                    var value = ParseExpression();
                    Consume(TokenType.Colon, "Expect ':' after case value");
                    var body = ParseBlock();
                    cases.Add(new SwitchCase(value, body, Previous().Line));
                }
                else if (Match(TokenType.Default))
                {
                    Consume(TokenType.Colon, "Expect ':' after default");
                    @default = ParseBlock();
                }
            }
            Consume(TokenType.End, "Expect 'end' after switch");
            return new SwitchStmt(expr, cases, @default, Previous().Line);
        }

        private DecideStmt ParseDecideStmt()
        {
            var expr = ParseExpression();
            Consume(TokenType.Colon, "Expect ':' after decide expr");
            var cases = new List<DecideCase>();
            BlockNode? @default = null;
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                if (Match(TokenType.Case))
                {
                    var value = ParseExpression();
                    Consume(TokenType.Colon, "Expect ':' after case value");
                    var body = ParseBlock();
                    cases.Add(new DecideCase(value, body, Previous().Line));
                }
                else if (Match(TokenType.Default))
                {
                    Consume(TokenType.Colon, "Expect ':' after default");
                    @default = ParseBlock();
                }
            }
            Consume(TokenType.End, "Expect 'end' after decide");
            return new DecideStmt(expr, cases, @default, Previous().Line);
        }

        private TryStmt ParseTryStmt()
        {
            var tryBlock = ParseBlock();
            string? catchVar = null;
            BlockNode? catchBlock = null;
            if (Match(TokenType.Catch))
            {
                catchVar = Consume(TokenType.Identifier, "Expect catch var").Lexeme;
                catchBlock = ParseBlock();
            }
            BlockNode? finallyBlock = null;
            if (Match(TokenType.Finally))
            {
                finallyBlock = ParseBlock();
            }
            return new TryStmt(tryBlock, catchVar, catchBlock, finallyBlock, Previous().Line);
        }

        private WithStmt ParseWithStmt()
        {
            var resource = ParseExpression();
            Consume(TokenType.As, "Expect 'as' after resource");
            string name = Consume(TokenType.Identifier, "Expect name").Lexeme;
            var body = ParseBlock();
            return new WithStmt(resource, name, body, Previous().Line);
        }

        private DeferStmt ParseDeferStmt()
        {
            var expr = ParseExpression();
            return new DeferStmt(expr, Previous().Line);
        }

        private AsmStmt ParseAsmStmt()
        {
            Consume(TokenType.Colon, "Expect ':' after asm");
            var instrs = new List<string>();
            while (!Check(TokenType.End) && !IsAtEnd())
            {
                instrs.Add(Consume(TokenType.Identifier, "Expect asm instr").Lexeme);
            }
            Consume(TokenType.End, "Expect 'end' after asm");
            return new AsmStmt(instrs, Previous().Line);
        }

        private GotoStmt ParseGotoStmt()
        {
            var target = ParseExpression();
            return new GotoStmt(target, Previous().Line);
        }

        private LabelStmt ParseLabelStmt()
        {
            string name = Consume(TokenType.Identifier, "Expect label name").Lexeme;
            Consume(TokenType.Colon, "Expect ':' after label");
            return new LabelStmt(name, Previous().Line);
        }

        private TrapStmt ParseTrapStmt()
        {
            ExprNode? code = null;
            if (!Check(TokenType.End) && !Check(TokenType.Semicolon))
            {
                code = ParseExpression();
            }
            return new TrapStmt(code, Previous().Line);
        }

        private HaltStmt ParseHaltStmt()
        {
            return new HaltStmt(Previous().Line);
        }

        private LockStmt ParseLockStmt()
        {
            var mutex = ParseExpression();
            Consume(TokenType.Colon, "Expect ':' after lock");
            var body = ParseBlock();
            return new LockStmt(mutex, body, Previous().Line);
        }

        private ExprNode ParseExpression() => ParseTernary();

        private ExprNode ParseTernary()
        {
            var expr = ParseOr();
            if (Match(TokenType.Question))
            {
                var thenExpr = ParseTernary();
                Consume(TokenType.Colon, "Expect ':' in ternary");
                var elseExpr = ParseTernary();
                expr = new TernaryExpr(expr, thenExpr, elseExpr, expr.Line);
            }
            return expr;
        }

        private ExprNode ParseOr() => ParseBinaryOp(ParseAnd, TokenType.OrOr, TokenType.Or);

        private ExprNode ParseAnd() => ParseBinaryOp(ParseEquality, TokenType.AndAnd, TokenType.And);

        private ExprNode ParseEquality() => ParseBinaryOp(ParseComparison, TokenType.EqualEqual, TokenType.NotEqual);

        private ExprNode ParseComparison() => ParseBinaryOp(ParseTerm, TokenType.Less, TokenType.LessEqual, TokenType.Greater, TokenType.GreaterEqual);

        private ExprNode ParseTerm() => ParseBinaryOp(ParseFactor, TokenType.Plus, TokenType.Minus);

        private ExprNode ParseFactor() => ParseBinaryOp(ParseUnary, TokenType.Star, TokenType.Slash, TokenType.Percent);

        private ExprNode ParseBinaryOp(Func<ExprNode> next, params TokenType[] ops)
        {
            var expr = next();
            while (Match(ops))
            {
                var op = Previous().Type;
                var right = next();
                expr = new BinaryExpr(expr, op, right, expr.Line);
            }
            return expr;
        }

        private ExprNode ParseUnary()
        {
            if (Match(TokenType.Bang, TokenType.Tilde, TokenType.Minus, TokenType.Not))
            {
                var op = Previous().Type;
                var expr = ParseUnary();
                return new UnaryExpr(op, expr, Previous().Line);
            }
            return ParseCall();
        }

        private ExprNode ParseCall()
        {
            var expr = ParsePrimary();
            while (true)
            {
                if (Match(TokenType.LeftParen))
                {
                    var args = new List<ExprNode>();
                    if (!Check(TokenType.RightParen))
                    {
                        do
                        {
                            args.Add(ParseExpression());
                        } while (Match(TokenType.Comma));
                    }
                    Consume(TokenType.RightParen, "Expect ')' after args");
                    expr = new CallExpr(expr, args, Previous().Line);
                }
                else if (Match(TokenType.LeftBracket))
                {
                    var index = ParseExpression();
                    Consume(TokenType.RightBracket, "Expect ']' after index");
                    expr = new IndexExpr(expr, index, Previous().Line);
                }
                else if (Match(TokenType.Dot))
                {
                    string field = Consume(TokenType.Identifier, "Expect field name").Lexeme;
                    expr = new FieldExpr(expr, field, Previous().Line);
                }
                else if (Match(TokenType.As))
                {
                    var type = ParseType();
                    expr = new CastExpr(expr, type, Previous().Line);
                }
                else
                {
                    break;
                }
            }
            return expr;
        }

        private ExprNode ParsePrimary()
        {
            if (Match(TokenType.IntegerLiteral, TokenType.FloatLiteral, TokenType.StringLiteral, TokenType.True, TokenType.False, TokenType.Null))
            {
                return new LiteralExpr(Previous().Literal!, Previous().Line);
            }
            if (Match(TokenType.Identifier))
            {
                string name = Previous().Lexeme;
                if (Match(TokenType.Colon))
                {
                    // Struct init
                    var fields = new List<FieldInit>();
                    while (!Check(TokenType.End) && !IsAtEnd())
                    {
                        string fName = Consume(TokenType.Identifier, "Expect field name").Lexeme;
                        Consume(TokenType.Equal, "Expect '=' in field init");
                        var value = ParseExpression();
                        fields.Add(new FieldInit(fName, value, Previous().Line));
                    }
                    Consume(TokenType.End, "Expect 'end' after struct init");
                    return new StructInitExpr(name, fields, Previous().Line);
                }
                else if (Match(TokenType.Dot))
                {
                    // Variant init
                    string @case = Consume(TokenType.Identifier, "Expect case name").Lexeme;
                    ExprNode? payload = null;
                    if (Match(TokenType.LeftParen))
                    {
                        payload = ParseExpression();
                        Consume(TokenType.RightParen, "Expect ')' after payload");
                    }
                    return new VariantInitExpr(name, @case, payload, Previous().Line);
                }
                return new VarExpr(name, Previous().Line);
            }
            if (Match(TokenType.LeftParen))
            {
                var elems = new List<ExprNode>();
                if (!Check(TokenType.RightParen))
                {
                    do
                    {
                        elems.Add(ParseExpression());
                    } while (Match(TokenType.Comma));
                }
                Consume(TokenType.RightParen, "Expect ')' after tuple");
                if (elems.Count == 1) return elems[0]; // Not a tuple
                return new TupleExpr(elems, Previous().Line);
            }
            if (Match(TokenType.Let))
            {
                // Tuple destructure
                var vars = new List<string>();
                do
                {
                    vars.Add(Consume(TokenType.Identifier, "Expect var name").Lexeme);
                } while (Match(TokenType.Comma));
                Consume(TokenType.Equal, "Expect '=' in destructure");
                var tuple = ParseExpression();
                return new TupleDestructureExpr(vars, tuple, Previous().Line);
            }
            throw Error(Peek(), "Expect expression");
        }

        // Helper methods
        private bool Match(params TokenType[] types)
        {
            foreach (var type in types)
            {
                if (Check(type))
                {
                    Advance();
                    return true;
                }
            }
            return false;
        }

        private Token Consume(TokenType type, string message)
        {
            if (Check(type)) return Advance();
            throw Error(Peek(), message);
        }

        private bool Check(TokenType type) => !IsAtEnd() && Peek().Type == type;
        private bool CheckNext(TokenType type) => !IsAtEnd() && _current + 1 < _tokens.Count && _tokens[_current + 1].Type == type;
        private Token Advance() => _tokens[_current++];
        private bool IsAtEnd() => _current >= _tokens.Count;
        private Token Peek() => _tokens[_current];
        private Token Previous() => _tokens[_current - 1];

        private ParseException Error(Token token, string message)
        {
            _errors.Add(new ParseError(message, token.Line));
            return new ParseException();
        }
    }

    public class ParseError
    {
        public string Message { get; }
        public int Line { get; }
        public ParseError(string message, int line) { Message = message; Line = line; }
    }

    public class ParseException : Exception { }

    // Missing AST nodes
    public class ExprStmt : StmtNode
    {
        public ExprNode Expr { get; }
        public ExprStmt(ExprNode expr, int line) : base(line) { Expr = expr; }
    }
}
