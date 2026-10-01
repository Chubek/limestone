#!/usr/bin/env ruby
# frozen_string_literal: true
#
# DSLtk-Generate.rb
#
# A parser-generator frontend for DSLtk.hpp.
#
# It reads an EBNF/PEG-style grammar (a `.g` file) and emits a self-contained
# C++ parser (default extension `.cc`, or whatever is passed to --out/-o) that
# depends only on DSLtk.hpp. Every regex-style terminal is matched through
# DSLtk's `dsl::pattern<...>::matches`; the grammar structure (sequencing,
# ordered choice, repetition, lookahead, recursion) is compiled to recursive
# C++ matcher functions.
#
# Usage:
#   ruby DSLtk-Generate.rb grammar.g            # -> grammar.cc
#   ruby DSLtk-Generate.rb grammar.g -o out.cc
#   ruby DSLtk-Generate.rb grammar.g --namespace my_ns --include "DSLtk.hpp"
#
# .g grammar format (see the header block emitted into generated files):
#
#   %namespace  my_parser        # C++ namespace for the generated parser
#   %include    "DSLtk.hpp"      # header include (repeatable)
#   %start      Program          # entry rule (defaults to first rule)
#   %skip       /[ \t\r\n]+/     # optional: text skipped before each terminal
#
#   Program   = Statement* EOF ;
#   Statement = "let" Ident "=" Expr ";" ;
#   Expr      = Term (("+" | "-") Term)* ;
#   Term      = Factor (("*" | "/") Factor)* ;
#   Factor    = Number | "(" Expr ")" ;
#   Ident     = /[A-Za-z_][A-Za-z0-9_]*/ ;
#   Number    = /[0-9]+/ ;
#
# Expression operators (PEG semantics, ordered choice):
#   a b        sequence
#   a | b      ordered choice (first match wins)
#   a*         zero or more            a+   one or more        a?   optional
#   &a         positive lookahead      !a   negative lookahead
#   ( ... )    grouping
#   "literal"  literal terminal (byte-exact, C-style escapes)
#   /regex/    pattern terminal, matched via dsl::pattern<...>::matches
#   Name       reference to another rule
#   EOF        built-in: matches only at end of input
#   EPSILON    built-in: always matches, consumes nothing
#
# Comments in .g: // line and /* block */.

require "optparse"

module DSLtkGen
  VERSION = "1.0.0"

  # ---- error type -----------------------------------------------------------
  class GrammarError < StandardError
    def initialize(msg, line = nil)
      super(line ? "line #{line}: #{msg}" : msg)
    end
  end

  # ---- AST nodes -------------------------------------------------------------
  Rule    = Struct.new(:name, :expr, :line)
  Choice  = Struct.new(:alts)          # array of expr
  Seq     = Struct.new(:items)         # array of expr
  Rep     = Struct.new(:op, :expr)     # op: "*","+","?"
  Look    = Struct.new(:op, :expr)     # op: "&","!"
  Ref     = Struct.new(:name)          # rule reference / builtin
  Lit     = Struct.new(:bytes)         # literal terminal (decoded String)
  Regex   = Struct.new(:src)           # pattern terminal (raw source)

  # ===========================================================================
  # Lexer for .g files
  # ===========================================================================
  class Lexer
    Token = Struct.new(:type, :value, :line)

    def initialize(src)
      @src = src
      @i = 0
      @line = 1
      @n = src.length
    end

    def tokens
      out = []
      loop do
        skip_trivia
        break if @i >= @n
        out << next_token
      end
      out << Token.new(:eof, nil, @line)
      out
    end

    private

    def peek(o = 0)
      @src[@i + o]
    end

    def advance
      c = @src[@i]
      @i += 1
      @line += 1 if c == "\n"
      c
    end

    def skip_trivia
      while @i < @n
        c = peek
        if c == " " || c == "\t" || c == "\r" || c == "\n"
          advance
        elsif c == "/" && peek(1) == "/"
          advance until @i >= @n || peek == "\n"
        elsif c == "/" && peek(1) == "*"
          advance; advance
          until @i >= @n || (peek == "*" && peek(1) == "/")
            advance
          end
          advance; advance if @i < @n # note: consumes */ ; if truncated stops
        else
          break
        end
      end
    end

    def next_token
      c = peek
      start_line = @line
      case c
      when '"'
        read_string(start_line)
      when "/"
        read_regex(start_line)
      when "%"
        read_directive(start_line)
      else
        if ident_start?(c)
          read_ident(start_line)
        elsif "=|()*+?&!;".include?(c)
          advance
          Token.new(:op, c, start_line)
        else
          raise GrammarError.new("unexpected character #{c.inspect}", start_line)
        end
      end
    end

    def ident_start?(c)
      c =~ /[A-Za-z_]/
    end

    def ident_char?(c)
      c =~ /[A-Za-z0-9_]/
    end

    def read_ident(line)
      s = +""
      s << advance while @i < @n && ident_char?(peek)
      Token.new(:ident, s, line)
    end

    def read_directive(line)
      advance # consume %
      s = +""
      s << advance while @i < @n && ident_char?(peek)
      raise GrammarError.new("empty directive after '%'", line) if s.empty?
      Token.new(:directive, s, line)
    end

    def read_string(line)
      advance # opening quote
      out = +""
      until @i >= @n || peek == '"'
        c = advance
        if c == "\\"
          out << decode_escape(line)
        else
          out << c
        end
      end
      raise GrammarError.new("unterminated string literal", line) if @i >= @n
      advance # closing quote
      Token.new(:string, out, line)
    end

    def decode_escape(line)
      e = advance
      case e
      when "n" then "\n"
      when "t" then "\t"
      when "r" then "\r"
      when "0" then "\0"
      when "\\" then "\\"
      when '"' then '"'
      when "/" then "/"
      when "x"
        h = +""
        2.times { h << advance if @i < @n && peek =~ /[0-9A-Fa-f]/ }
        raise GrammarError.new("bad \\x escape", line) if h.empty?
        h.to_i(16).chr
      else
        e # unknown escape: keep the char literally
      end
    end

    def read_regex(line)
      advance # opening /
      out = +""
      until @i >= @n || peek == "/"
        c = advance
        if c == "\\"
          nx = advance
          if nx == "/"
            out << "/"          # \/ -> literal slash inside the pattern
          else
            out << "\\" << nx   # keep other escapes for the pattern engine
          end
        else
          out << c
        end
      end
      raise GrammarError.new("unterminated /regex/ literal", line) if @i >= @n
      advance # closing /
      raise GrammarError.new("empty /regex/ literal", line) if out.empty?
      Token.new(:regex, out, line)
    end
  end

  # ===========================================================================
  # Parser for .g files -> directives + rules
  # ===========================================================================
  class GrammarParser
    def initialize(tokens)
      @toks = tokens
      @p = 0
      @directives = Hash.new { |h, k| h[k] = [] }
      @rules = []
    end

    def parse
      until at?(:eof)
        if cur.type == :directive
          parse_directive
        elsif cur.type == :ident
          @rules << parse_rule
        else
          err("expected a rule name or %directive, got #{describe(cur)}")
        end
      end
      { directives: @directives, rules: @rules }
    end

    private

    def cur;  @toks[@p]; end
    def at?(t); cur.type == t; end
    def advance; t = cur; @p += 1; t; end

    def expect_op(op)
      unless cur.type == :op && cur.value == op
        err("expected '#{op}', got #{describe(cur)}")
      end
      advance
    end

    def op?(op)
      cur.type == :op && cur.value == op
    end

    def describe(t)
      case t.type
      when :eof then "end of file"
      when :op then "'#{t.value}'"
      when :ident then "identifier '#{t.value}'"
      when :string then "string literal"
      when :regex then "/regex/ literal"
      when :directive then "%#{t.value}"
      else t.type.to_s
      end
    end

    def err(msg)
      raise GrammarError.new(msg, cur ? cur.line : nil)
    end

    def parse_directive
      d = advance # directive token
      name = d.value
      # value token: a string, regex or bare ident
      vtok = cur
      case vtok.type
      when :string, :regex, :ident
        advance
        @directives[name] << { kind: vtok.type, value: vtok.value }
      else
        err("directive %#{name} needs a value")
      end
    end

    def parse_rule
      name_tok = advance # ident
      line = name_tok.line
      expect_op("=")
      expr = parse_choice
      expect_op(";")
      Rule.new(name_tok.value, expr, line)
    end

    # Choice = Seq ( '|' Seq )*
    def parse_choice
      alts = [parse_seq]
      while op?("|")
        advance
        alts << parse_seq
      end
      alts.length == 1 ? alts.first : Choice.new(alts)
    end

    # Seq = Prefix+
    def parse_seq
      items = []
      items << parse_prefix while start_of_primary?
      err("empty sequence") if items.empty?
      items.length == 1 ? items.first : Seq.new(items)
    end

    def start_of_primary?
      return true if cur.type == :ident || cur.type == :string || cur.type == :regex
      return true if op?("(") || op?("&") || op?("!")
      false
    end

    # Prefix = ('&'|'!')? Postfix
    def parse_prefix
      if op?("&") || op?("!")
        o = advance.value
        Look.new(o, parse_postfix)
      else
        parse_postfix
      end
    end

    # Postfix = Primary ('*'|'+'|'?')?
    def parse_postfix
      prim = parse_primary
      if op?("*") || op?("+") || op?("?")
        o = advance.value
        Rep.new(o, prim)
      else
        prim
      end
    end

    def parse_primary
      t = cur
      case t.type
      when :ident
        advance
        Ref.new(t.value)
      when :string
        advance
        Lit.new(t.value)
      when :regex
        advance
        Regex.new(t.value)
      when :op
        if t.value == "("
          advance
          inner = parse_choice
          expect_op(")")
          inner
        else
          err("unexpected #{describe(t)}")
        end
      else
        err("unexpected #{describe(t)}")
      end
    end
  end

  # ===========================================================================
  # C++ emitter
  # ===========================================================================
  class Emitter
    BUILTINS = %w[EOF EPSILON].freeze

    def initialize(grammar, opts)
      @rules = grammar[:rules]
      @directives = grammar[:directives]
      @opts = opts
      @regex_terms = {}   # regex src -> function name
      @rule_names = @rules.map(&:name)
      validate!
    end

    def namespace
      cli = @opts[:namespace]
      return cli if cli
      d = @directives["namespace"].first
      d ? d[:value] : "generated_parser"
    end

    def includes
      list = @directives["include"].map { |d| d[:value] }
      list = ["DSLtk.hpp"] if list.empty?
      # always ensure DSLtk.hpp is present
      list << "DSLtk.hpp" unless list.include?("DSLtk.hpp")
      list.uniq
    end

    def start_rule
      d = @directives["start"].first
      name = d ? d[:value] : @rule_names.first
      unless @rule_names.include?(name)
        raise GrammarError.new("%start references unknown rule '#{name}'")
      end
      name
    end

    def skip_pattern
      d = @directives["skip"].first
      d ? d[:value] : nil
    end

    def validate!
      if @rules.empty?
        raise GrammarError.new("grammar contains no rules")
      end
      seen = {}
      @rules.each do |r|
        if seen[r.name]
          raise GrammarError.new("duplicate rule '#{r.name}'", r.line)
        end
        seen[r.name] = true
      end
      @rules.each { |r| check_refs(r.expr, r.line) }
    end

    def check_refs(node, line)
      case node
      when Choice then node.alts.each { |a| check_refs(a, line) }
      when Seq    then node.items.each { |i| check_refs(i, line) }
      when Rep    then check_refs(node.expr, line)
      when Look   then check_refs(node.expr, line)
      when Ref
        unless @rule_names.include?(node.name) || BUILTINS.include?(node.name)
          raise GrammarError.new("rule references unknown name '#{node.name}'", line)
        end
      end
    end

    # -- C++ literal string escaping -----------------------------------------
    def cpp_escape(str)
      str.bytes.map do |b|
        case b
        when 0x22 then '\\"'
        when 0x5c then "\\\\"
        when 0x0a then "\\n"
        when 0x0d then "\\r"
        when 0x09 then "\\t"
        else
          (b >= 0x20 && b < 0x7f) ? b.chr : format("\\%03o", b)
        end
      end.join
    end

    def regex_fn(src)
      @regex_terms[src] ||= "term_re_#{@regex_terms.size}"
    end

    # -- expression -> C++ boolean matcher expression -------------------------
    # Every emitted fragment is an immediately-invoked lambda `[&]{...}()`
    # capturing the enclosing `s` (string_view) and `p` (size_t&).
    def emit_expr(node)
      case node
      when Choice
        alts = node.alts.map { |a| emit_expr(a) }
        body = alts.map { |a| "sv=p; if (#{a}) return true; p=sv;" }.join("\n      ")
        "[&]()->bool{ std::size_t sv; #{body}\n      return false; }()"
      when Seq
        parts = node.items.map { |i| emit_expr(i) }
        checks = parts.map { |x| "if (!(#{x})) { p = st; return false; }" }.join("\n      ")
        "[&]()->bool{ std::size_t st = p; #{checks}\n      return true; }()"
      when Rep
        inner = emit_expr(node.expr)
        case node.op
        when "?"
          "[&]()->bool{ std::size_t sv = p; if (!(#{inner})) p = sv; return true; }()"
        when "*"
          "[&]()->bool{ for (;;) { std::size_t sv = p; if (!(#{inner})) { p = sv; break; } if (p == sv) break; } return true; }()"
        when "+"
          "[&]()->bool{ if (!(#{inner})) return false; for (;;) { std::size_t sv = p; if (!(#{inner})) { p = sv; break; } if (p == sv) break; } return true; }()"
        end
      when Look
        inner = emit_expr(node.expr)
        if node.op == "&"
          "[&]()->bool{ std::size_t sv = p; bool r = (#{inner}); p = sv; return r; }()"
        else
          "[&]()->bool{ std::size_t sv = p; bool r = (#{inner}); p = sv; return !r; }()"
        end
      when Ref
        case node.name
        when "EOF"     then "[&]()->bool{ skip_ws(s, p); return p >= s.size(); }()"
        when "EPSILON" then "true"
        else "rule_#{node.name}(s, p)"
        end
      when Lit
        %Q{match_lit(s, p, "#{cpp_escape(node.bytes)}", #{node.bytes.bytesize})}
      when Regex
        "#{regex_fn(node.src)}(s, p)"
      else
        raise "internal: unknown node #{node.class}"
      end
    end

    def emit
      out = +""
      out << header_comment
      includes.each { |h| out << %Q{#include "#{h}"\n} }
      out << "\n#include <cstddef>\n#include <string_view>\n\n"
      out << "namespace #{namespace} {\n\n"
      out << "using ::dsl::pattern;\n\n"
      out << runtime_support
      out << "\n// ---- forward declarations -------------------------------------------------\n"
      @rules.each { |r| out << "bool rule_#{r.name}(std::string_view s, std::size_t &p);\n" }
      # emit terminal fns after we know all of them (populate by walking rules)
      @rules.each { |r| walk_collect(r.expr) }
      out << "\n// ---- pattern terminals (via dsl::pattern<...>::matches) --------------------\n"
      @regex_terms.each do |src, fn|
        out << emit_regex_fn(src, fn)
      end
      out << "\n// ---- grammar rules --------------------------------------------------------\n"
      @rules.each { |r| out << emit_rule(r) }
      out << "\n"
      out << public_api
      out << "\n} // namespace #{namespace}\n"
      out
    end

    def walk_collect(node)
      case node
      when Choice then node.alts.each { |a| walk_collect(a) }
      when Seq    then node.items.each { |i| walk_collect(i) }
      when Rep    then walk_collect(node.expr)
      when Look   then walk_collect(node.expr)
      when Regex  then regex_fn(node.src)
      end
    end

    def emit_rule(r)
      body = emit_expr(r.expr)
      <<~CPP
        bool rule_#{r.name}(std::string_view s, std::size_t &p)
        {
          return #{body};
        }
      CPP
    end

    def emit_regex_fn(src, fn)
      esc = cpp_escape(src)
      <<~CPP
        // /#{src}/
        inline bool #{fn}(std::string_view s, std::size_t &p)
        {
          skip_ws(s, p);
          std::size_t avail = s.size() - p;
          // longest matching prefix at p (keep terminal patterns token-shaped)
          for (std::size_t len = avail; len > 0; --len)
            {
              if (pattern<"#{esc}">::matches(s.substr(p, len)))
                {
                  p += len;
                  return true;
                }
            }
          return false;
        }
      CPP
    end

    def runtime_support
      skip = skip_pattern
      skip_body =
        if skip
          esc = cpp_escape(skip)
          <<~CPP.chomp
            // %skip /#{skip}/
            for (;;)
              {
                std::size_t avail = s.size() - p;
                std::size_t best = 0;
                for (std::size_t len = avail; len > 0; --len)
                  if (pattern<"#{esc}">::matches(s.substr(p, len))) { best = len; break; }
                if (best == 0) break;
                p += best;
              }
          CPP
        else
          "(void)s; (void)p; // no %skip directive"
        end

      <<~CPP
        // ---- runtime support ------------------------------------------------------
        // Skips ignorable text (whitespace / comments) before each terminal, using
        // the %skip pattern when one was declared in the grammar.
        inline void skip_ws(std::string_view s, std::size_t &p)
        {
          #{skip_body.gsub("\n", "\n  ")}
        }

        // Byte-exact literal terminal match at position p (after skipping).
        inline bool match_lit(std::string_view s, std::size_t &p,
                              std::string_view lit, std::size_t len)
        {
          skip_ws(s, p);
          if (s.size() - p >= len && s.substr(p, len) == lit)
            {
              p += len;
              return true;
            }
          return false;
        }

        // Result of a top-level parse.
        struct ParseResult
        {
          bool ok;          ///< the start rule matched
          std::size_t pos;  ///< offset reached in the input
          bool full;        ///< matched AND consumed the entire input
        };
      CPP
    end

    def public_api
      start = start_rule
      <<~CPP
        // ---- public entry point ---------------------------------------------------
        inline ParseResult parse(std::string_view s)
        {
          std::size_t p = 0;
          bool ok = rule_#{start}(s, p);
          skip_ws(s, p);
          return ParseResult{ ok, p, ok && (p == s.size()) };
        }
      CPP
    end

    def header_comment
      <<~CPP
        // -----------------------------------------------------------------------------
        // Generated by DSLtk-Generate.rb v#{VERSION} -- DO NOT EDIT BY HAND.
        // Regenerate from the source grammar instead.
        //
        // Terminal /regex/ tokens are matched with dsl::pattern<...>::matches from
        // DSLtk.hpp. Grammar structure (sequence, ordered choice, repetition,
        // lookahead, recursion) is compiled to recursive matcher functions with PEG
        // (packrat-free, backtracking, first-match-wins) semantics.
        //
        // Entry point:  #{namespace}::parse(std::string_view) -> ParseResult
        // -----------------------------------------------------------------------------
      CPP
    end
  end

  # ===========================================================================
  # CLI driver
  # ===========================================================================
  class CLI
    def self.run(argv)
      opts = { out: nil, namespace: nil }
      parser = OptionParser.new do |o|
        o.banner = "Usage: DSLtk-Generate.rb GRAMMAR.g [options]"
        o.on("-o", "--out FILE", "output file (default: GRAMMAR.cc)") { |v| opts[:out] = v }
        o.on("--namespace NAME", "C++ namespace (overrides %namespace)") { |v| opts[:namespace] = v }
        o.on("-v", "--version", "print version and exit") do
          puts "DSLtk-Generate.rb #{VERSION}"
          exit 0
        end
        o.on("-h", "--help", "show this help") do
          puts o
          exit 0
        end
      end
      parser.parse!(argv)

      if argv.empty?
        warn parser.help
        exit 2
      end
      input = argv.shift

      unless File.file?(input)
        warn "error: no such grammar file: #{input}"
        exit 2
      end

      out = opts[:out] || derive_out(input)

      begin
        src = File.read(input, mode: "rb").force_encoding("UTF-8")
        tokens = Lexer.new(src).tokens
        grammar = GrammarParser.new(tokens).parse
        code = Emitter.new(grammar, opts).emit
        File.write(out, code)
      rescue GrammarError => e
        warn "#{input}: #{e.message}"
        exit 1
      end

      puts "wrote #{out} (#{File.size(out)} bytes) from #{input}"
    end

    def self.derive_out(input)
      base = input.sub(/\.g\z/, "")
      base = input if base == input # no .g extension
      "#{base}.cc"
    end
  end
end

DSLtkGen::CLI.run(ARGV) if $PROGRAM_NAME == __FILE__
