/**
 * codegen.c — CloudPol Backend Code Generation
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 6: Lowers the CloudPol IR to two output formats:
 *
 *   FMT_IAM_JSON  — A valid AWS IAM policy JSON document.
 *   FMT_REPORT    — A human-readable Markdown policy report.
 *
 * IAM JSON format (AWS-compatible):
 * {
 *   "Version": "2012-10-17",
 *   "Statement": [
 *     {
 *       "Sid":       "CloudPol_<Role>_<idx>",
 *       "Effect":    "Allow" | "Deny",
 *       "Principal": {"AWS": "*"},
 *       "Action":    "<action>",
 *       "Resource":  "<resource>",
 *       "Condition": { ... }       // omitted when cond == "(none)"
 *     }
 *   ]
 * }
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/mlir.h"
#include "../include/codegen.h"

/* ═════════════════════════════════════════════════════════════
   Internal: JSON string escaping
═════════════════════════════════════════════════════════════ */
static void json_escape(FILE *out, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n",  out); break;
            case '\r': fputs("\\r",  out); break;
            case '\t': fputs("\\t",  out); break;
            default:   fputc(*s, out); break;
        }
    }
}

/* ═════════════════════════════════════════════════════════════
   Internal: condition string → IAM Condition block
   The full IAM condition DSL is a superset of what we support.
   We only generate supported IAM condition operators:
     IpAddress, DateGreaterThan, DateLessThan, Bool, StringEquals
═════════════════════════════════════════════════════════════ */

/*
 * Parse a single simple condition token "(attr op value)" and emit
 * the corresponding IAM JSON Condition block member.
 *
 * Returns 1 if a condition was emitted.
 */
static int emit_simple_condition_json(FILE *out, const char *attr,
                                      const char *op_sym, const char *val) {
    /* Map attribute + operator to IAM condition key */
    if (strcmp(attr, "ip") == 0) {
        /* IpAddress / NotIpAddress */
        const char *iam_op = (strcmp(op_sym, "!=") == 0)
                             ? "NotIpAddress" : "IpAddress";
        fprintf(out, "        \"%s\": { \"aws:SourceIp\": \"", iam_op);
        json_escape(out, val);
        fprintf(out, "\" }");
        return 1;
    }
    if (strcmp(attr, "mfa") == 0) {
        const char *bval = (strcmp(val, "TRUE") == 0) ? "true" : "false";
        fprintf(out, "        \"Bool\": { \"aws:MultiFactorAuthPresent\": \"%s\" }",
                bval);
        return 1;
    }
    if (strcmp(attr, "region") == 0) {
        const char *iam_op = (strcmp(op_sym, "!=") == 0)
                             ? "StringNotEquals" : "StringEquals";
        fprintf(out, "        \"%s\": { \"aws:RequestedRegion\": \"", iam_op);
        json_escape(out, val);
        fprintf(out, "\" }");
        return 1;
    }
    if (strcmp(attr, "time") == 0) {
        /* Map: >= → DateGreaterThanEquals, <= → DateLessThanEquals, etc. */
        const char *iam_op = "NumericEquals";
        if      (strcmp(op_sym, ">=") == 0) iam_op = "NumericGreaterThanEquals";
        else if (strcmp(op_sym, "<=") == 0) iam_op = "NumericLessThanEquals";
        else if (strcmp(op_sym, ">")  == 0) iam_op = "NumericGreaterThan";
        else if (strcmp(op_sym, "<")  == 0) iam_op = "NumericLessThan";
        else if (strcmp(op_sym, "!=") == 0) iam_op = "NumericNotEquals";
        fprintf(out, "        \"%s\": { \"cloudpol:HourOfDay\": \"", iam_op);
        json_escape(out, val);
        fprintf(out, "\" }");
        return 1;
    }
    if (strcmp(attr, "tag") == 0) {
        const char *iam_op = (strcmp(op_sym, "!=") == 0)
                             ? "StringNotEquals" : "StringEquals";
        fprintf(out,
            "        \"%s\": { \"aws:ResourceTag/Environment\": \"", iam_op);
        json_escape(out, val);
        fprintf(out, "\" }");
        return 1;
    }
    return 0;
}

/* Parse "(attr op val)" from cond_str and emit JSON condition.
   This is a best-effort single-term extractor; compound conditions
   are emitted as an IAM StringLike custom key (portable annotation). */
static void emit_condition_json(FILE *out, const char *cond_str) {
    if (!cond_str || strcmp(cond_str, "(none)") == 0) return;

    /* Try to parse a single simple condition: (attr op val) */
    char attr[64] = "", op_sym[8] = "", val[256] = "";
    /* Strip outer parens if present */
    const char *body = cond_str;
    if (*body == '(') body++;

    int matched = sscanf(body, "%63s %7s %255[^)]", attr, op_sym, val);
    if (matched == 3 && strlen(attr) > 0 && val[0] != '\0') {
        fprintf(out, ",\n      \"Condition\": {\n");
        if (!emit_simple_condition_json(out, attr, op_sym, val)) {
            /* Fallback: emit the raw cond_str as a StringLike comment */
            fprintf(out,
                "        \"StringLike\": { \"cloudpol:RawCondition\": \"");
            json_escape(out, cond_str);
            fprintf(out, "\" }");
        }
        fprintf(out, "\n      }");
    } else {
        /* Compound condition — emit as raw annotation */
        fprintf(out, ",\n      \"Condition\": {\n");
        fprintf(out,
            "        \"StringLike\": { \"cloudpol:RawCondition\": \"");
        json_escape(out, cond_str);
        fprintf(out, "\" }\n      }");
    }
}

/* ═════════════════════════════════════════════════════════════
   Backend 1: IAM JSON
═════════════════════════════════════════════════════════════ */
static int emit_iam_json(const CloudPolModule *mod, FILE *out) {
    fprintf(out, "{\n");
    fprintf(out, "  \"Version\": \"2012-10-17\",\n");
    if (mod->source_file)
        fprintf(out, "  \"_Source\": \"%s\",\n", mod->source_file);
    fprintf(out, "  \"Statement\": [\n");

    int first_stmt = 1;
    const CloudPolBlock *blk = mod->blocks;
    while (blk) {
        const CloudPolOp *op = blk->ops;
        int idx = 1;
        while (op) {
            /* Skip DEAD ops */
            if (op->flags & OP_FLAG_DEAD) { op = op->next; idx++; continue; }

            if (!first_stmt) fprintf(out, ",\n");
            first_stmt = 0;

            const char *effect_s =
                op->effect == EFFECT_ALLOW ? "Allow" : "Deny";

            /* Build a stable Sid: remove spaces from role name */
            char sid[128];
            int si = 0;
            const char *r = blk->role;
            while (*r && si < 120)
                sid[si++] = isalnum((unsigned char)*r) ? *r++ : (r++, '_');
            sid[si] = '\0';

            fprintf(out, "    {\n");
            fprintf(out, "      \"Sid\": \"CloudPol_%s_%d\",\n", sid, idx);
            fprintf(out, "      \"Effect\": \"%s\",\n", effect_s);
            fprintf(out, "      \"Principal\": {\"AWS\": \"*\"},\n");
            fprintf(out, "      \"Action\": \"");
            json_escape(out, op->action);
            fprintf(out, "\",\n");
            fprintf(out, "      \"Resource\": \"");
            json_escape(out, op->resource);
            fprintf(out, "\"");

            /* Condition block */
            emit_condition_json(out, op->cond_str);

            /* Cond-dead annotation (out-of-range) */
            if (op->flags & OP_FLAG_COND_DEAD)
                fprintf(out,
                    ",\n      \"_CloudPolNote\": \"condition always false\"");

            fprintf(out, "\n    }");
            op = op->next;
            idx++;
        }
        blk = blk->next;
    }

    fprintf(out, "\n  ]\n}\n");
    return 0;
}

/* ═════════════════════════════════════════════════════════════
   Backend 2: Markdown Policy Report
═════════════════════════════════════════════════════════════ */
static const char *relop_english(const char *op_sym) {
    if (strcmp(op_sym, "==") == 0) return "equals";
    if (strcmp(op_sym, "!=") == 0) return "does not equal";
    if (strcmp(op_sym, ">=") == 0) return "≥";
    if (strcmp(op_sym, "<=") == 0) return "≤";
    if (strcmp(op_sym, ">")  == 0) return ">";
    if (strcmp(op_sym, "<")  == 0) return "<";
    return op_sym;
}

static void emit_cond_human(FILE *out, const char *cond_str) {
    if (!cond_str || strcmp(cond_str, "(none)") == 0) {
        fprintf(out, "_unconditional_"); return;
    }
    char attr[64] = "", op_sym[8] = "", val[256] = "";
    const char *body = (*cond_str == '(') ? cond_str + 1 : cond_str;
    int m = sscanf(body, "%63s %7s %255[^)]", attr, op_sym, val);
    if (m == 3)
        fprintf(out, "`%s` %s `%s`", attr, relop_english(op_sym), val);
    else
        fprintf(out, "`%s`", cond_str);
}

static int emit_report(const CloudPolModule *mod, FILE *out) {
    fprintf(out, "# CloudPol Policy Report\n\n");
    if (mod->source_file)
        fprintf(out, "> **Source:** `%s`  \n", mod->source_file);
    fprintf(out, "> **Roles:** %d | **Total Statements:** %d\n\n",
            mod->block_count, mod->total_ops);
    fprintf(out, "---\n\n");

    const CloudPolBlock *blk = mod->blocks;
    while (blk) {
        fprintf(out, "## Role: `%s`\n\n", blk->role);
        fprintf(out, "| # | Effect | Action | Resource | Condition | Notes |\n");
        fprintf(out, "|---|--------|--------|----------|-----------|-------|\n");

        const CloudPolOp *op = blk->ops;
        int idx = 1;
        while (op) {
            const char *effect_s = op->effect == EFFECT_ALLOW ? "✅ ALLOW" : "❌ DENY";
            const char *note     = "";
            if (op->flags & OP_FLAG_DEAD)      note = "⚠️ Dead (eliminated)";
            if (op->flags & OP_FLAG_COND_DEAD) note = "⚠️ Cond always-false";

            fprintf(out, "| %d | %s | `%s` | `%s` | ",
                    idx++, effect_s, op->action, op->resource);
            emit_cond_human(out, op->cond_str);
            fprintf(out, " | %s |\n", note);
            op = op->next;
        }
        fprintf(out, "\n");
        blk = blk->next;
    }

    fprintf(out, "---\n");
    fprintf(out, "_Generated by CloudPol Compiler_\n");
    return 0;
}

/* ═════════════════════════════════════════════════════════════
   Public: codegen_emit
═════════════════════════════════════════════════════════════ */
int codegen_emit(const CloudPolModule *mod, CodegenFormat fmt, FILE *out) {
    if (!mod || !out) return 1;
    switch (fmt) {
        case FMT_IAM_JSON: return emit_iam_json(mod, out);
        case FMT_REPORT:   return emit_report(mod, out);
        default: return 1;
    }
}
