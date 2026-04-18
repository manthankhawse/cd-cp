# CloudPol Compiler Internals

Technical reference for developers working on or studying the CloudPol DSL compiler.

---

## 1. Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                    CloudPol Compiler Pipeline                     │
│                                                                   │
│  .pol source text                                                 │
│       │                                                           │
│  ┌────▼────────┐                                                  │
│  │  Phase 1    │  policy_lexer.l  (Flex)                          │
│  │  LEXER      │  Emits typed token stream                        │
│  └────┬────────┘                                                  │
│       │ token stream                                              │
│  ┌────▼────────┐                                                  │
│  │  Phase 2    │  policy_parser.y  (Bison LALR(1))                │
│  │  PARSER     │  Builds ProgramNode AST                          │
│  └────┬────────┘                                                  │
│       │ ProgramNode*                                              │
│  ┌────▼────────┐                                                  │
│  │  Phase 3    │  semantic.c (6 checks)                           │
│  │  SEMANTIC   │  SemanticResult with error list                  │
│  └────┬────────┘                                                  │
│       │ ProgramNode* (validated)                                  │
│  ┌────▼────────┐                                                  │
│  │  Phase 4    │  mlir_bridge.c                                   │
│  │  MLIR BRIDGE│  AST → CloudPolModule (IR)                       │
│  └────┬────────┘                                                  │
│       │ CloudPolModule*                                           │
│  ┌────▼────────┐                                                  │
│  │  Phase 5    │  mlir_passes.c (3 passes)                        │
│  │  MLIR PASSES│  In-place IR transformation                      │
│  └────┬────────┘                                                  │
│       │ CloudPolModule* (optimized)                               │
│  ┌────▼────────┐  FMT_IAM_JSON   →  AWS IAM policy JSON           │
│  │  Phase 6    │  FMT_REPORT     →  Markdown policy report        │
│  │  CODEGEN    │  EMIT_MLIR flag →  CloudPol dialect text         │
│  └─────────────┘                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 2. CloudPol MLIR Dialect

The compiler uses a self-contained C implementation of a custom MLIR-inspired dialect. It mirrors LLVM MLIR's `Module/Block/Operation` hierarchy without the LLVM dependency.

### IR Hierarchy

```
CloudPolModule
  source_file: string
  block_count: int
  total_ops:   int
  blocks: CloudPolBlock[]
    role:      string       ← one block per unique role
    op_count:  int
    ops: CloudPolOp[]
      effect:      EFFECT_ALLOW | EFFECT_DENY
      action:      string   ← "svc:Action"
      resource:    string   ← ARN or "*"
      cond_str:    string   ← canonical parenthesised condition
      source_line: int
      flags:       bitmask  ← OP_FLAG_DEAD | OP_FLAG_COND_DEAD
```

### Textual IR Format

```mlir
module @cloudpol_policy {

  cloudpol.policy_block @DevOps {
    cloudpol.op ALLOW action="s3:PutObject"
               resource="arn:aws:s3:::prod-artifacts"
               cond=(none)
               line=9
  }

  cloudpol.policy_block @Admin {
    cloudpol.op ALLOW action="s3:*"
               resource="arn:aws:s3:::*"
               cond=(none)
               line=74
    cloudpol.op DENY action="s3:DeleteBucket"
               resource="arn:aws:s3:::*"
               cond=(none)
               line=78  // [DEAD — eliminated by pass]
  }

} // end module
```

### Condition String Serialisation

Conditions are serialised to a canonical form by `cond_to_str()` in `mlir_bridge.c`:

| AST Node | Canonical String |
|---|---|
| `ip == "10.0.0.1"` | `(ip == 10.0.0.1)` |
| `mfa == TRUE` | `(mfa == TRUE)` |
| `A AND B` | `((A) AND (B))` |
| `NOT (A)` | `(NOT (A))` |

---

## 3. MLIR Pass Pipeline

Passes run in `mlir_passes.c` via `run_passes(CloudPolModule*)`.

### Pass 1: Dead Policy Elimination

**Trigger:** A block contains an `ALLOW svc:*` op AND a `DENY svc:SpecificAction` op for the same resource.

**Action:** Sets `OP_FLAG_DEAD` on the DENY op.

**Rationale:** In AWS IAM, an explicit-deny overrides any allow. However, a wildcard `ALLOW svc:*` combined with a more specific `DENY svc:X` for the same `Resource` within the same role's block is flagged as contradictory — the dead DENY is kept in the IR but annotated so codegen skips it.

### Pass 2: Effect Normalization

**Action:** Within each block, reorders ops to place `DENY` ops before `ALLOW` ops.

**Rationale:** Matches AWS policy evaluation order (explicit-deny beats allow).

### Pass 3: Condition Constant Folding

**Trigger:** Detects impossible `time` condition values (e.g. `time >= 25`).

**Action:** Sets `OP_FLAG_COND_DEAD` and annotates the op.

---

## 4. Mock AWS Dictionary (`src/aws_dict.h`)

A compile-time static table of 130+ known AWS service actions across 15 services:
`s3`, `ec2`, `iam`, `lambda`, `dynamodb`, `rds`, `kms`, `sqs`, `sns`,
`cloudwatch`, `sts`, `secretsmanager`, `logs`, `glue`, `athena`,
`cloudtrail`, `redshift`

**Wildcard support:** `is_known_aws_action("s3:*")` returns 1 if any `s3:*` action exists in the table (service-prefix wildcard matching).

**ARN validation:** `is_valid_arn()` checks:
- Starts with `"arn:"` — 5+ colons required
- `"*"` is valid (global wildcard)

---

## 5. Code Generation Backends

### FMT_IAM_JSON

Emits a valid AWS IAM policy JSON document (`Version: 2012-10-17`).

Condition mapping:

| CloudPol Attribute | IAM Condition Key | IAM Operator |
|---|---|---|
| `ip ==` | `aws:SourceIp` | `IpAddress` |
| `ip !=` | `aws:SourceIp` | `NotIpAddress` |
| `mfa == TRUE` | `aws:MultiFactorAuthPresent` | `Bool: "true"` |
| `region ==` | `aws:RequestedRegion` | `StringEquals` |
| `region !=` | `aws:RequestedRegion` | `StringNotEquals` |
| `time >=` | `cloudpol:HourOfDay` | `NumericGreaterThanEquals` |
| `tag ==` | `aws:ResourceTag/Environment` | `StringEquals` |
| compound | `cloudpol:RawCondition` | `StringLike` fallback |

### FMT_REPORT

Emits a Markdown document with:
- Summary header (role count, total ops)
- Per-role table: `# | Effect | Action | Resource | Condition | Notes`
- ⚠️ annotations for `DEAD` and `COND-DEAD` ops

---

## 6. CLI Architecture

`cli.c` contains `parse_cli()` and `run_compiler()`.
`policy_parser.y` `main()` is a 2-line shim:
```c
int main(int argc, char *argv[]) {
    CliOptions opts = parse_cli(argc, argv);
    return run_compiler(opts);
}
```

`run_compiler()` drives all 7 phases in order, respecting `--no-semantic`, `--no-passes`, and the `EMIT_*` bitmask.

### Emit-mode Bitmask

| Flag | Bit |
|---|---|
| `EMIT_AST` | `0x01` |
| `EMIT_MLIR` | `0x02` |
| `EMIT_IAM` | `0x04` |
| `EMIT_REPORT` | `0x08` |

Multiple flags can be combined; all active outputs share the same `-o` file or stdout.

---

## 7. Error Code Reference

| Exit Code | Meaning |
|---|---|
| 0 | Compilation successful |
| 1 | Lexical or syntax error |
| 2 | Semantic analysis failure |
| 3 | Code generation / I/O failure |
