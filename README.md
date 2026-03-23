# CloudPol — Cloud Policy as Code DSL Compiler

A **statically-typed, deterministic compiler** for a custom Cloud Policy DSL, augmented by an AI translation frontend.

```
English description  →  AI Engine  →  CloudPol DSL  →  Compiler  →  AWS IAM / Terraform
```

---

## Project Structure

```
cd-cp/
├── src/
│   ├── policy_lexer.l       # Step 2 — Flex lexer specification
│   └── policy_parser.y      # Step 3 — Bison parser (stub in Step 2)
├── include/
│   └── ast.h                # AST node type definitions
├── samples/
│   ├── valid_policy.pol      # Valid DSL examples covering all constructs
│   └── invalid_policy.pol    # Error test cases
├── docs/
│   ├── LANGUAGE_REFERENCE.md # User-facing language guide
│   └── GRAMMAR.md            # Formal BNF grammar & token table
├── Makefile
└── README.md
```

---

## DSL Quick-Start

```cloudpol
// Allow DevOps to upload to S3 from a trusted IP with MFA
ALLOW ROLE "DevOps"
      ACTION "s3:PutObject"
      ON RESOURCE "arn:aws:s3:::prod-artifacts"
      WHERE ip == "10.0.0.0/8" AND mfa == TRUE;

// Deny interns from creating IAM users globally
DENY ROLE "Intern"
     ACTION "iam:CreateUser"
     ON RESOURCE "arn:aws:iam:::*";
```

---

## Compiler Pipeline

| Step | Component | Technology | Status |
|---|---|---|---|
| 1 | DSL Grammar Design | BNF / CFG | ✅ Complete |
| 2 | Lexical Analysis | Flex | ✅ Complete |
| 3 | Syntax Analysis + AST | Bison | 🔧 In Progress |
| 4 | Semantic Analysis + SMT | Z3 Solver | ⬜ Planned |
| 5 | Code Generation | C / Templates | ⬜ Planned |
| 6 | AI Translation Engine | LLM + Few-Shot | ⬜ Planned |
| 7 | AI-Compiler Feedback Loop | HITL | ⬜ Planned |

---

## Build & Test

### Prerequisites

```bash
sudo apt install flex bison gcc
```

### Build the standalone lexer (Step 2 test)

```bash
make lexer-test
```

### Tokenize a sample policy

```bash
./build/policy_lexer < samples/valid_policy.pol
```

### Build the full parser (Step 2 + 3)

```bash
make all
```

### Clean

```bash
make clean
```

---

## Documentation

| Document | Description |
|---|---|
| [`docs/LANGUAGE_REFERENCE.md`](docs/LANGUAGE_REFERENCE.md) | User guide — syntax, examples, error messages |
| [`docs/GRAMMAR.md`](docs/GRAMMAR.md) | Formal BNF, token table, conflict analysis |

---

## Language Highlights

- **5 contextual attributes**: `ip`, `time`, `mfa`, `region`, `tag`
- **6 relational operators**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **3 logical operators**: `AND`, `OR`, `NOT` (with correct precedence)
- **Comments**: Single-line `//` and block `/* */`
- **Precise error reporting**: Line and column numbers on every error

---

## Research Foundation

This compiler design is grounded in published research:

| Finding | Impact on Design |
|---|---|
| AI models score F-Measure 0.32 on `condition` entities [[6]] | WHERE clause attributes are rigid and minimal |
| SMT-based fault localization achieves 84% repair rate [[2]] | Z3 integration planned in Step 4 |
| Structured prompts outperform free-form for policy synthesis [[3]] | Few-shot prompting with BNF in Step 6 |
| Explicit attribute provision improves LLM accuracy [[5]] | Token list injected into LLM context in Step 6 |
