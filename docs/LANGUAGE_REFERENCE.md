# CloudPol Language Reference Guide

> **CloudPol** is a Domain-Specific Language (DSL) for expressing cloud access control policies in a human-readable, compiler-verifiable format. It is the intermediate language of the **Cloud Policy as Code DSL Compiler**.

---

## Table of Contents

1. [Overview](#1-overview)
2. [File Format](#2-file-format)
3. [Comments](#3-comments)
4. [Statement Structure](#4-statement-structure)
5. [Effects: ALLOW and DENY](#5-effects-allow-and-deny)
6. [Subject — ROLE](#6-subject--role)
7. [Action](#7-action)
8. [Resource](#8-resource)
9. [Conditions — WHERE Clause](#9-conditions--where-clause)
   - [Attributes](#91-attributes)
   - [Relational Operators](#92-relational-operators)
   - [Values](#93-values)
   - [Logical Operators](#94-logical-operators)
   - [Operator Precedence](#95-operator-precedence)
10. [Reserved Words](#10-reserved-words)
11. [Complete Examples](#11-complete-examples)
12. [Error Messages Reference](#12-error-messages-reference)

---

## 1. Overview

CloudPol policies are **declarative** — you describe *who* can do *what* on *which resource* and *under what conditions*, and the compiler enforces the rules deterministically. The compiler pipeline includes:

```
CloudPol source (.pol)
        ↓  Lex/Flex  (Step 2)
    Token stream
        ↓  Yacc/Bison  (Step 3)
  Abstract Syntax Tree (AST)
        ↓  SMT Solver / Z3  (Step 4)
   Validated, conflict-free AST
        ↓  Code Generator  (Step 5)
  AWS IAM JSON / Terraform HCL
```

---

## 2. File Format

- File extension: **`.pol`**
- Encoding: **UTF-8**
- Case-sensitive: **Yes** — all structural keywords must be **UPPERCASE**; attribute names (`ip`, `time`, `mfa`, `region`, `tag`) must be **lowercase**.
- One or more policy statements per file.
- Each statement is terminated by a **semicolon (`;`)**.

---

## 3. Comments

| Style | Syntax | Example |
|---|---|---|
| Single-line | `// text` | `// This allows DevOps to upload` |
| Block | `/* text */` | `/* Multi-line explanation */` |

Comments are stripped by the lexer and have no semantic meaning.

---

## 4. Statement Structure

Every CloudPol statement follows this rigid template:

```
<effect>  ROLE  "<role>"  ACTION  "<action>"
          ON  RESOURCE  "<resource>"
          [WHERE <condition>];
```

All five clauses (`ROLE`, `ACTION`, `ON RESOURCE`, and optionally `WHERE`) are **positional and required** except `WHERE`. The statement **must** end with `;`.

### Minimal Valid Statement

```cloudpol
ALLOW ROLE "Developer" ACTION "s3:GetObject" ON RESOURCE "arn:aws:s3:::assets";
```

### Full Statement with Condition

```cloudpol
ALLOW ROLE "DevOps"
      ACTION "s3:PutObject"
      ON RESOURCE "arn:aws:s3:::prod"
      WHERE ip == "10.0.0.0/8" AND mfa == TRUE;
```

---

## 5. Effects: ALLOW and DENY

| Keyword | Meaning |
|---|---|
| `ALLOW` | Explicitly grant the action |
| `DENY`  | Explicitly forbid the action |

> [!IMPORTANT]
> `DENY` always takes precedence over `ALLOW` when both target the same role, action, and resource. The SMT solver (Step 4) will detect and report this conflict precisely.

---

## 6. Subject — ROLE

```
ROLE "<role_name>"
```

- Must be a **double-quoted string literal**.
- Case-sensitive (matches your identity provider's role names exactly).
- Wildcards are **not supported** at the DSL level; supply them in resource ARNs.

**Examples:**

```cloudpol
ROLE "DevOps"
ROLE "SRE-Team"
ROLE "ExternalContractor"
```

---

## 7. Action

```
ACTION "<cloud_action>"
```

- Must be a **double-quoted string literal**.
- Follows the cloud provider's naming convention.
- Use `*` wildcards within the string for broad grants (handled downstream by the code generator).

**Examples:**

```cloudpol
ACTION "s3:PutObject"
ACTION "ec2:TerminateInstances"
ACTION "iam:CreateUser"
ACTION "s3:*"
```

---

## 8. Resource

```
ON RESOURCE "<resource_arn>"
```

- Must be a **double-quoted string literal**.
- Accepts full AWS ARNs or wildcard ARNs.

**Examples:**

```cloudpol
ON RESOURCE "arn:aws:s3:::prod-bucket"
ON RESOURCE "arn:aws:ec2:::instance/i-*"
ON RESOURCE "arn:aws:iam:::user/*"
ON RESOURCE "arn:aws:s3:::*"
```

---

## 9. Conditions — WHERE Clause

```
WHERE <condition>
```

The WHERE clause enforces contextual constraints. It is **optional** but strongly recommended for production policies.

### 9.1 Attributes

Attributes are the left-hand side of a condition. All attribute names are **lowercase**.

| Attribute | Type | Description |
|---|---|---|
| `ip`     | String  | Source IP address or CIDR block |
| `time`   | Number  | Hour of day (0–23, UTC) |
| `mfa`    | Boolean | Whether MFA was authenticated (`TRUE`/`FALSE`) |
| `region` | String  | AWS region name (e.g. `"us-east-1"`) |
| `tag`    | String  | Resource tag in `"key:value"` format |

> [!NOTE]
> Only these five attributes are recognized. Using any other identifier (e.g., `environment`, `IP`) will produce a lexer error. This is intentional — the constrained attribute set minimizes LLM generation errors.

### 9.2 Relational Operators

| Operator | Meaning |
|---|---|
| `==` | Equal to |
| `!=` | Not equal to |
| `<`  | Less than |
| `>`  | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

Numeric comparisons (`<`, `>`, `<=`, `>=`) are most meaningful with the `time` attribute. String comparisons use lexicographic ordering.

### 9.3 Values

| Value Type | Syntax | Example |
|---|---|---|
| String  | `"..."` | `"10.0.0.0/8"`, `"us-east-1"` |
| Integer | digits  | `22`, `0`, `6` |
| Boolean | `TRUE` or `FALSE` | `TRUE`, `FALSE` |

### 9.4 Logical Operators

| Operator | Syntax | Notes |
|---|---|---|
| AND | `cond AND cond` | Both conditions must be true |
| OR  | `cond OR cond`  | Either condition must be true |
| NOT | `NOT cond`      | Negates a single simple condition |

### 9.5 Operator Precedence

From **highest** to **lowest** binding power:

| Priority | Operator |
|---|---|
| 1 (highest) | `NOT` |
| 2           | `AND` |
| 3 (lowest)  | `OR`  |

Use parentheses (planned for Step 3) to override precedence explicitly.

#### Precedence Example

```cloudpol
WHERE ip == "10.0.0.1" OR mfa == TRUE AND region == "us-east-1"
-- Evaluates as:
-- ip == "10.0.0.1" OR (mfa == TRUE AND region == "us-east-1")
```

---

## 10. Reserved Words

The following identifiers are **reserved** and cannot be used as role/action/resource values outside of string literals.

| Category | Keywords |
|---|---|
| Effects | `ALLOW`, `DENY` |
| Structure | `ROLE`, `ACTION`, `ON`, `RESOURCE`, `WHERE` |
| Logic | `AND`, `OR`, `NOT` |
| Booleans | `TRUE`, `FALSE` |
| Attributes | `ip`, `time`, `mfa`, `region`, `tag` |

---

## 11. Complete Examples

### Example 1 — Simple Deny

**DSL**

```
DENY ROLE "Intern"
ACTION "iam:CreateUser"
ON RESOURCE "arn:aws:iam:::*";
```

**IAM Policy**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Deny",
      "Action": "iam:CreateUser",
      "Resource": "*"
    }
  ]
}
```

(Attach this to the `Intern` role)

---

### Example 2 — IP-restricted Access

**DSL**

```
ALLOW ROLE "DataEngineer"
ACTION "s3:GetObject"
ON RESOURCE "arn:aws:s3:::data-lake"
WHERE ip == "10.10.0.0/24";
```

**IAM Policy**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "s3:GetObject",
      "Resource": "arn:aws:s3:::data-lake/*",
      "Condition": {
        "IpAddress": {
          "aws:SourceIp": "10.10.0.0/24"
        }
      }
    }
  ]
}
```

---

### Example 3 — MFA Enforcement

**DSL**

```
ALLOW ROLE "SRE"
ACTION "ec2:TerminateInstances"
ON RESOURCE "arn:aws:ec2:::instance/*"
WHERE mfa == TRUE;
```

**IAM Policy**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "ec2:TerminateInstances",
      "Resource": "*",
      "Condition": {
        "Bool": {
          "aws:MultiFactorAuthPresent": "true"
        }
      }
    }
  ]
}
```

---

### Example 4 — Compound Condition

**DSL**

```
ALLOW ROLE "SecurityAuditor"
ACTION "cloudtrail:LookupEvents"
ON RESOURCE "arn:aws:cloudtrail:::trail/*"
WHERE ip == "192.168.1.0/24" AND mfa == TRUE;
```

**IAM Policy**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "cloudtrail:LookupEvents",
      "Resource": "*",
      "Condition": {
        "IpAddress": {
          "aws:SourceIp": "192.168.1.0/24"
        },
        "Bool": {
          "aws:MultiFactorAuthPresent": "true"
        }
      }
    }
  ]
}
```

---

### Example 5 — Time-based Restriction

**DSL**

```
DENY ROLE "Analyst"
ACTION "redshift:GetClusterCredentials"
ON RESOURCE "arn:aws:redshift:::cluster:analytics"
WHERE time >= 22 AND time <= 6;
```

IAM doesn’t support direct hour comparisons like this, so we approximate using UTC timestamps.

**IAM Policy**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Deny",
      "Action": "redshift:GetClusterCredentials",
      "Resource": "arn:aws:redshift:*:*:cluster:analytics",
      "Condition": {
        "DateGreaterThan": {
          "aws:CurrentTime": "2026-01-01T22:00:00Z"
        },
        "DateLessThan": {
          "aws:CurrentTime": "2026-01-02T06:00:00Z"
        }
      }
    }
  ]
}
```

Note: For recurring daily windows, AWS IAM alone is limited. You typically combine with **AWS Organizations SCPs, Lambda, or session policies**.

---

### Example 6 — NOT Condition

**DSL**

```
ALLOW ROLE "QAEngineer"
ACTION "lambda:InvokeFunction"
ON RESOURCE "arn:aws:lambda:::function:test-*"
WHERE NOT region == "ap-south-1";
```

**IAM Policy**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "lambda:InvokeFunction",
      "Resource": "arn:aws:lambda:*:*:function:test-*",
      "Condition": {
        "StringNotEquals": {
          "aws:RequestedRegion": "ap-south-1"
        }
      }
    }
  ]
}
```


---

## 12. Error Messages Reference

| Error Code | Message Pattern | Cause |
|---|---|---|
| `LEX-001` | `unexpected character '<c>'` | Non-printable or unsupported character in source |
| `LEX-002` | `unterminated string literal` | Opening `"` without a matching closing `"` |
| `LEX-003` | `unterminated block comment` | `/*` without a matching `*/` |
| `SYN-001` | `SYNTAX ERROR line X:Y — <msg>` | Grammar rule violated (e.g., missing `;`) |
| `SEM-001` | `undefined attribute '<name>'` | Attribute not in {ip, time, mfa, region, tag} |
| `SMT-001` | `logical conflict at statement N` | ALLOW and DENY overlap detected by Z3 solver |

> [!TIP]
> Every error message includes a **line:column** reference. Use your editor's "Go to Line" feature to jump directly to the offending token.
