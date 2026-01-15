using System;
using System.Collections.Generic;
using System.Text;

namespace RaneCompiler
{
    public enum TokenType
    {
        // Keywords
        Module, Namespace, Proc, Struct, Enum, Variant, Union, Type, Typealias, Alias,
        Const, Constexpr, Consteval, Contract, Requires, Async, Dedicate, Linear, Nonlinear,
        Match, Switch, Decide, While, For, If, Else, Return, Try, Catch, Finally, Throw,
        With, Defer, Mutex, Channel, Mmio, Capability, Admin, Protected, Public, Private,
        Goto, Label, Trap, Halt, Start, Node, Say,
        // Literals
        Identifier, IntegerLiteral, FloatLiteral, StringLiteral, True, False, Null,
        // Operators
        Plus, Minus, Star, Slash, Percent, Ampersand, Pipe, Caret, Xor, Shl, Shr, Sar,
        LeftShift, RightShift, Less, LessEqual, Greater, GreaterEqual, EqualEqual, NotEqual,
        Equal, AndAnd, OrOr, And, Or, Question, Colon, Bang, Tilde, Not,
        // Punctuation
        LeftParen, RightParen, LeftBracket, RightBracket, LeftBrace, RightBrace,
        Comma, Dot, Semicolon, Arrow, End, As, To, Into, From, Size,
        // Special
        EOF, Unknown
    }

    public class Token
    {
        public TokenType Type { get; }
        public string Lexeme { get; }
        public object? Literal { get; } // For literals like numbers or processed strings
        public int Line { get; }

        public Token(TokenType type, string lexeme, object? literal, int line)
        {
            Type = type;
            Lexeme = lexeme;
            Literal = literal;
            Line = line;
        }

        public override string ToString() => $"{Type} '{Lexeme}' at line {Line}";
    }

    public class LexerError
    {
        public string Message { get; }
        public int Line { get; }

        public LexerError(string message, int line)
        {
            Message = message;
            Line = line;
        }

        public override string ToString() => $"Error at line {Line}: {Message}";
    }

    public class RaneLexer
    {
        private readonly string _source;
        private int _start = 0;
        private int _current = 0;
        private int _line = 1;
        private readonly List<LexerError> _errors = new();
        private readonly Dictionary<string, TokenType> _keywords = new()
        {
            {"module", TokenType.Module}, {"namespace", TokenType.Namespace}, {"proc", TokenType.Proc},
            {"struct", TokenType.Struct}, {"enum", TokenType.Enum}, {"variant", TokenType.Variant},
            {"union", TokenType.Union}, {"type", TokenType.Type}, {"typealias", TokenType.Typealias},
            {"alias", TokenType.Alias}, {"const", TokenType.Const}, {"constexpr", TokenType.Constexpr},
            {"consteval", TokenType.Consteval}, {"contract", TokenType.Contract}, {"requires", TokenType.Requires},
            {"async", TokenType.Async}, {"dedicate", TokenType.Dedicate}, {"linear", TokenType.Linear},
            {"nonlinear", TokenType.Nonlinear}, {"match", TokenType.Match}, {"switch", TokenType.Switch},
            {"decide", TokenType.Decide}, {"while", TokenType.While}, {"for", TokenType.For},
            {"if", TokenType.If}, {"else", TokenType.Else}, {"return", TokenType.Return},
            {"try", TokenType.Try}, {"catch", TokenType.Catch}, {"finally", TokenType.Finally},
            {"throw", TokenType.Throw}, {"with", TokenType.With}, {"defer", TokenType.Defer},
            {"mutex", TokenType.Mutex}, {"channel", TokenType.Channel}, {"mmio", TokenType.Mmio},
            {"capability", TokenType.Capability}, {"admin", TokenType.Admin}, {"protected", TokenType.Protected},
            {"public", TokenType.Public}, {"private", TokenType.Private}, {"goto", TokenType.Goto},
            {"label", TokenType.Label}, {"trap", TokenType.Trap}, {"halt", TokenType.Halt},
            {"start", TokenType.Start}, {"node", TokenType.Node}, {"say", TokenType.Say},
            {"true", TokenType.True}, {"false", TokenType.False}, {"null", TokenType.Null},
            {"end", TokenType.End}, {"as", TokenType.As}, {"to", TokenType.To}, {"into", TokenType.Into},
            {"from", TokenType.From}, {"size", TokenType.Size}
        };

        public RaneLexer(string source)
        {
            _source = source;
        }

        public (List<Token>, List<LexerError>) ScanTokens()
        {
            var tokens = new List<Token>();
            while (!IsAtEnd())
            {
                _start = _current;
                var token = ScanToken();
                if (token != null) tokens.Add(token);
            }
            tokens.Add(new Token(TokenType.EOF, "", null, _line));
            return (tokens, _errors);
        }

        private Token? ScanToken()
        {
            char c = Advance();
            switch (c)
            {
                case '(': return new Token(TokenType.LeftParen, "(", null, _line);
                case ')': return new Token(TokenType.RightParen, ")", null, _line);
                case '[': return new Token(TokenType.LeftBracket, "[", null, _line);
                case ']': return new Token(TokenType.RightBracket, "]", null, _line);
                case '{': return new Token(TokenType.LeftBrace, "{", null, _line);
                case '}': return new Token(TokenType.RightBrace, "}", null, _line);
                case ',': return new Token(TokenType.Comma, ",", null, _line);
                case '.': return new Token(TokenType.Dot, ".", null, _line);
                case ';': return new Token(TokenType.Semicolon, ";", null, _line);
                case '+': return new Token(TokenType.Plus, "+", null, _line);
                case '-': 
                    if (Match('>')) return new Token(TokenType.Arrow, "->", null, _line);
                    return new Token(TokenType.Minus, "-", null, _line);
                case '*': return new Token(TokenType.Star, "*", null, _line);
                case '/': return new Token(TokenType.Slash, "/", null, _line);
                case '%': return new Token(TokenType.Percent, "%", null, _line);
                case '&': 
                    if (Match('&')) return new Token(TokenType.AndAnd, "&&", null, _line);
                    return new Token(TokenType.Ampersand, "&", null, _line);
                case '|': 
                    if (Match('|')) return new Token(TokenType.OrOr, "||", null, _line);
                    return new Token(TokenType.Pipe, "|", null, _line);
                case '^': return new Token(TokenType.Caret, "^", null, _line);
                case '<': 
                    if (Match('<')) return new Token(TokenType.LeftShift, "<<", null, _line);
                    if (Match('=')) return new Token(TokenType.LessEqual, "<=", null, _line);
                    return new Token(TokenType.Less, "<", null, _line);
                case '>': 
                    if (Match('>')) return new Token(TokenType.RightShift, ">>", null, _line);
                    if (Match('=')) return new Token(TokenType.GreaterEqual, ">=", null, _line);
                    return new Token(TokenType.Greater, ">", null, _line);
                case '=': 
                    if (Match('=')) return new Token(TokenType.EqualEqual, "==", null, _line);
                    return new Token(TokenType.Equal, "=", null, _line);
                case '!': 
                    if (Match('=')) return new Token(TokenType.NotEqual, "!=", null, _line);
                    return new Token(TokenType.Bang, "!", null, _line);
                case '~': return new Token(TokenType.Tilde, "~", null, _line);
                case '?': return new Token(TokenType.Question, "?", null, _line);
                case ':': return new Token(TokenType.Colon, ":", null, _line);
                case ' ': case '\r': case '\t': break; // Skip whitespace
                case '\n': _line++; break;
                case '"': return String();
                default:
                    if (IsDigit(c)) return Number();
                    if (IsAlpha(c)) return Identifier();
                    Error($"Unexpected character '{c}'");
                    return null;
            }
            return null;
        }

        private Token String()
        {
            var sb = new StringBuilder();
            while (Peek() != '"' && !IsAtEnd())
            {
                if (Peek() == '\n') _line++;
                if (Peek() == '\\')
                {
                    Advance(); // Consume \
                    char escape = Peek();
                    switch (escape)
                    {
                        case 'n': sb.Append('\n'); break;
                        case 't': sb.Append('\t'); break;
                        case 'r': sb.Append('\r'); break;
                        case '\\': sb.Append('\\'); break;
                        case '"': sb.Append('"'); break;
                        case '0': sb.Append('\0'); break; // Null char
                        default:
                            Error($"Invalid escape sequence '\\{escape}'");
                            sb.Append(escape); // Keep as is for now
                            break;
                    }
                    Advance(); // Consume the escaped char
                }
                else
                {
                    sb.Append(Advance());
                }
            }
            if (IsAtEnd())
            {
                Error("Unterminated string");
                return new Token(TokenType.Unknown, "Unterminated string", null, _line);
            }
            Advance(); // Closing "
            return new Token(TokenType.StringLiteral, _source.Substring(_start, _current - _start), sb.ToString(), _line);
        }

        private Token Number()
        {
            while (IsDigit(Peek()) || Peek() == '_' || Peek() == 'x' || Peek() == 'b' || (Peek() >= 'a' && Peek() <= 'f') || (Peek() >= 'A' && Peek() <= 'F')) Advance();
            string numStr = _source.Substring(_start, _current - _start);
            if (Peek() == '.' && IsDigit(PeekNext())) 
            {
                Advance(); // Consume .
                while (IsDigit(Peek())) Advance();
                numStr = _source.Substring(_start, _current - _start);
                if (double.TryParse(numStr.Replace("_", ""), out double value))
                {
                    return new Token(TokenType.FloatLiteral, numStr, value, _line);
                }
                else
                {
                    Error($"Invalid float literal '{numStr}'");
                    return new Token(TokenType.Unknown, numStr, null, _line);
                }
            }
            else
            {
                // Handle int literals: dec, hex (0x), bin (0b), with _
                string cleanNum = numStr.Replace("_", "");
                if (cleanNum.StartsWith("0x") || cleanNum.StartsWith("0X"))
                {
                    if (long.TryParse(cleanNum[2..], System.Globalization.NumberStyles.HexNumber, null, out long value))
                    {
                        return new Token(TokenType.IntegerLiteral, numStr, value, _line);
                    }
                }
                else if (cleanNum.StartsWith("0b") || cleanNum.StartsWith("0B"))
                {
                    try
                    {
                        long value = Convert.ToInt64(cleanNum[2..], 2);
                        return new Token(TokenType.IntegerLiteral, numStr, value, _line);
                    }
                    catch
                    {
                        // Fall through
                    }
                }
                else
                {
                    if (long.TryParse(cleanNum, out long value))
                    {
                        return new Token(TokenType.IntegerLiteral, numStr, value, _line);
                    }
                }
                Error($"Invalid integer literal '{numStr}'");
                return new Token(TokenType.Unknown, numStr, null, _line);
            }
        }

        private Token Identifier()
        {
            while (IsAlphaNumeric(Peek())) Advance();
            string text = _source.Substring(_start, _current - _start);
            TokenType type = _keywords.GetValueOrDefault(text, TokenType.Identifier);
            return new Token(type, text, null, _line);
        }

        private void Error(string message)
        {
            _errors.Add(new LexerError(message, _line));
        }

        private bool IsAtEnd() => _current >= _source.Length;
        private char Advance() => _source[_current++];
        private char Peek() => IsAtEnd() ? '\0' : _source[_current];
        private char PeekNext() => _current + 1 >= _source.Length ? '\0' : _source[_current + 1];
        private bool Match(char expected)
        {
            if (IsAtEnd() || _source[_current] != expected) return false;
            _current++;
            return true;
        }
        private static bool IsDigit(char c) => c >= '0' && c <= '9';
        private static bool IsAlpha(char c) => (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        private static bool IsAlphaNumeric(char c) => IsAlpha(c) || IsDigit(c);
    }
}
