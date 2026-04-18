# CloudPol — Cloud Policy as Code DSL Compiler

A **statically-typed, deterministic compiler** for a custom Cloud Policy DSL that translates human-readable access-control policies into executable AWS IAM JSON.

```
.pol source  →  Lexer  →  Parser+AST  →  Semantic Analysis
     →  MLIR Bridge  →  MLIR Passes  →  Code Generation  →  IAM JSON / Report
```

---

## Compiler Pipeline

| Phase | Step | Technology | Status |
|---|---|---|---|
| 1 | Lexical Analysis | Flex | ✅ Complete |
| 2 | Syntax Analysis + AST | Bison | ✅ Complete |
| 3 | Semantic Analysis | Custom (6 checks) | ✅ Complete |
| 4 | MLIR Bridge (AST→IR) | CloudPol Dialect (C) | ✅ Complete |
| 5 | MLIR Optimization Passes | Dead Elim, Norm, Folding | ✅ Complete |
| 6 | Backend Code Generation | IAM JSON + Markdown | ✅ Complete |
| 7 | CLI Interface | Full flag parser | ✅ Complete |

---

## Quick Start

### Prerequisites

```bash
sudo apt install flex bison gcc
```

### Build

```bash
make all
# → build/cloudpol
```

### DSL Example

```cloudpol
// Allow DevOps to upload to S3 — MFA required, trusted IP only
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

## CLI Usage

```
cloudpol [options] [input.pol]

Options:
  --emit-ast      Print AST (default: always on)
  --emit-mlir     Print MLIR dialect IR after bridge + passes
  --emit-iam      Emit AWS IAM policy JSON
  --emit-report   Emit Markdown policy report
  -o <file>       Write output to file (default: stdout)
  --no-semantic   Skip semantic analysis
  --no-passes     Skip MLIR optimization passes
  --help          Show help
  --version       Print version

Exit codes: 0=ok  1=syntax error  2=semantic error  3=codegen error
```

### Usage Examples

```bash
# Full pipeline with AST (default)
./build/cloudpol samples/valid_policy.pol

# Emit MLIR dialect IR
./build/cloudpol --emit-mlir samples/valid_policy.pol

# Emit AWS IAM JSON to file
./build/cloudpol --emit-iam -o policy.json samples/valid_policy.pol

# Emit Markdown policy report
./build/cloudpol --emit-report samples/valid_policy.pol

# Skip semantic analysis
./build/cloudpol --no-semantic --emit-mlir samples/valid_policy.pol

# Test semantic error detection
./build/cloudpol samples/semantic_errors.pol
```

### Makefile Convenience Targets

```bash
make run             # Full pipeline on valid_policy.pol
make emit-mlir       # Print MLIR dialect IR
make emit-iam        # Print IAM JSON
make emit-report     # Print Markdown report
make test-semantic   # Run semantic error test file
make lexer-test      # Build standalone lexer token printer
make clean           # Remove all build artifacts
```

---

## Project Structure

```
cd-cp/
├── include/
│   ├── ast.h              # AST node types
│   ├── tokens.h           # Token definitions (standalone lexer)
│   ├── semantic.h         # Semantic error taxonomy + API
│   ├── mlir.h             # CloudPol MLIR dialect IR types
│   ├── mlir_passes.h      # Pass manager API
│   ├── codegen.h          # Codegen format selector + API
│   └── cli.h              # CliOptions struct + entry point
├── src/
│   ├── policy_lexer.l     # Flex lexer specification
│   ├── policy_parser.y    # Bison grammar + main() shim
│   ├── ast.c              # AST free/print helpers
│   ├── aws_dict.h         # Mock AWS action dictionary (130+ actions)
│   ├── semantic.c         # 6 semantic checks (SC-01 to SC-06)
│   ├── mlir_bridge.c      # AST → CloudPol IR lowering
│   ├── mlir_passes.c      # 3 optimization passes
│   ├── codegen.c          # IAM JSON + Markdown report backends
│   └── cli.c              # CLI parser + full pipeline driver
├── samples/
│   ├── valid_policy.pol       # 13 valid statements
│   ├── invalid_policy.pol     # Syntax error tests
│   └── semantic_errors.pol    # Semantic error tests
├── docs/
│   ├── LANGUAGE_REFERENCE.md  # DSL syntax user guide
│   └── COMPILER_INTERNALS.md  # MLIR dialect spec, pass pipeline
├── Makefile
└── README.md
```

---

## Semantic Checks

| Code | Name | Description |
|---|---|---|
| SC-01 | `UNKNOWN_ACTION` | Action not in AWS mock dictionary |
| SC-02 | `BAD_ARN` | Resource not in `arn:*` format or `*` |
| SC-03 | `DUPLICATE_STMT` | Exact duplicate (role + action + resource + effect) |
| SC-04 | `CONFLICT_EFFECT` | Same triple with conflicting ALLOW/DENY |
| SC-05 | `TYPE_MISMATCH` | Wrong value type for `mfa`, `time`, or `ip` |
| SC-06 | `EMPTY_POLICY` | Zero statements in the policy |

---

## MLIR Optimization Passes

| Pass | Name | What it does |
|---|---|---|
| 1 | Dead Policy Elimination | Marks DENY ops dead when an `svc:*` ALLOW covers the same role |
| 2 | Effect Normalization | Reorders ops: DENY before ALLOW (AWS evaluation order) |
| 3 | Condition Folding | Detects impossible conditions (e.g. `time >= 25`) |

---

## DSL Language Reference

| Feature | Syntax |
|---|---|
| Allow statement | `ALLOW ROLE "name" ACTION "svc:op" ON RESOURCE "arn:...";` |
| Deny statement | `DENY ROLE "name" ACTION "svc:op" ON RESOURCE "arn:...";` |
| Conditional | `... WHERE <condition>;` |
| Attributes | `ip`, `time`, `mfa`, `region`, `tag` |
| Operators | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Logic | `AND`, `OR`, `NOT` |
| Comments | `// single-line`, `/* block */` |

See [`docs/LANGUAGE_REFERENCE.md`](docs/LANGUAGE_REFERENCE.md) for the full language guide.

---

## Research Foundation

| Finding | Impact |
|---|---|
| AI models score F-Measure 0.32 on `condition` entities | WHERE attributes are rigid and minimal |
| SMT-based fault localization achieves 84% repair rate | Semantic pass inspired by constraint solver concepts |
| Structured prompts outperform free-form for policy synthesis | Grammar-first DSL design |
