/**
 * aws_dict.h — Mock AWS Action Dictionary + ARN Validator
 * Cloud Policy as Code DSL Compiler — CloudPol
 *
 * Step 4: Provides a curated table of known AWS service actions and a
 * lightweight ARN format validator.  Both are intentionally kept in-header
 * (static inline / static const) so that semantic.c is the only TU that
 * needs to include this file — no separate compilation unit is required.
 *
 * Covered AWS services (representative subset, not exhaustive):
 *   s3, ec2, iam, lambda, dynamodb, rds, kms, sqs, sns, cloudwatch,
 *   sts, secretsmanager, logs, glue, athena
 */

#ifndef AWS_DICT_H
#define AWS_DICT_H

#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────
   Mock AWS action table
   Format: "service:ActionName"
───────────────────────────────────────────── */
static const char * const AWS_ACTIONS[] = {
    /* S3 */
    "s3:GetObject",        "s3:PutObject",        "s3:DeleteObject",
    "s3:ListBucket",       "s3:CreateBucket",     "s3:DeleteBucket",
    "s3:GetBucketPolicy",  "s3:PutBucketPolicy",  "s3:GetBucketAcl",
    "s3:PutBucketAcl",     "s3:CopyObject",       "s3:HeadObject",

    /* EC2 */
    "ec2:DescribeInstances",  "ec2:RunInstances",  "ec2:StopInstances",
    "ec2:TerminateInstances", "ec2:StartInstances","ec2:RebootInstances",
    "ec2:DescribeImages",     "ec2:CreateSnapshot","ec2:DeleteSnapshot",
    "ec2:DescribeVolumes",    "ec2:AttachVolume",  "ec2:DetachVolume",
    "ec2:CreateSecurityGroup","ec2:DeleteSecurityGroup",
    "ec2:AuthorizeSecurityGroupIngress",

    /* IAM */
    "iam:CreateUser",    "iam:DeleteUser",    "iam:ListUsers",
    "iam:AttachRolePolicy","iam:DetachRolePolicy","iam:CreateRole",
    "iam:DeleteRole",    "iam:ListRoles",     "iam:PassRole",
    "iam:GetUser",       "iam:UpdateUser",    "iam:CreatePolicy",
    "iam:DeletePolicy",  "iam:ListPolicies",

    /* Lambda */
    "lambda:InvokeFunction",  "lambda:CreateFunction", "lambda:DeleteFunction",
    "lambda:UpdateFunctionCode","lambda:ListFunctions", "lambda:GetFunction",
    "lambda:AddPermission",   "lambda:RemovePermission",

    /* DynamoDB */
    "dynamodb:GetItem",    "dynamodb:PutItem",    "dynamodb:DeleteItem",
    "dynamodb:Query",      "dynamodb:Scan",       "dynamodb:UpdateItem",
    "dynamodb:CreateTable","dynamodb:DeleteTable","dynamodb:ListTables",
    "dynamodb:DescribeTable",

    /* RDS */
    "rds:DescribeDBInstances","rds:CreateDBInstance","rds:DeleteDBInstance",
    "rds:StartDBInstance",    "rds:StopDBInstance",  "rds:RebootDBInstance",
    "rds:CreateDBSnapshot",   "rds:DeleteDBSnapshot",

    /* KMS */
    "kms:Encrypt",  "kms:Decrypt",       "kms:GenerateDataKey",
    "kms:CreateKey","kms:DescribeKey",   "kms:ListKeys",
    "kms:EnableKey","kms:DisableKey",    "kms:ScheduleKeyDeletion",

    /* SQS */
    "sqs:SendMessage",           "sqs:ReceiveMessage",
    "sqs:DeleteMessage",         "sqs:CreateQueue",
    "sqs:DeleteQueue",           "sqs:ListQueues",
    "sqs:GetQueueAttributes",    "sqs:SetQueueAttributes",

    /* SNS */
    "sns:Publish",        "sns:Subscribe",    "sns:Unsubscribe",
    "sns:CreateTopic",    "sns:DeleteTopic",  "sns:ListTopics",
    "sns:ListSubscriptions",

    /* CloudWatch */
    "cloudwatch:PutMetricData",    "cloudwatch:GetMetricStatistics",
    "cloudwatch:ListMetrics",      "cloudwatch:PutMetricAlarm",
    "cloudwatch:DeleteAlarms",     "cloudwatch:DescribeAlarms",

    /* STS */
    "sts:AssumeRole",        "sts:GetCallerIdentity",
    "sts:AssumeRoleWithWebIdentity",

    /* Secrets Manager */
    "secretsmanager:GetSecretValue", "secretsmanager:CreateSecret",
    "secretsmanager:DeleteSecret",   "secretsmanager:ListSecrets",
    "secretsmanager:PutSecretValue",

    /* CloudWatch Logs */
    "logs:CreateLogGroup",  "logs:CreateLogStream",
    "logs:PutLogEvents",    "logs:DescribeLogGroups",
    "logs:DescribeLogStreams","logs:GetLogEvents",

    /* Glue */
    "glue:CreateDatabase",  "glue:GetDatabase",   "glue:DeleteDatabase",
    "glue:CreateTable",     "glue:GetTable",      "glue:DeleteTable",
    "glue:StartJobRun",     "glue:GetJobRun",

    /* Athena */
    "athena:StartQueryExecution","athena:GetQueryExecution",
    "athena:GetQueryResults",    "athena:ListQueryExecutions",

    /* CloudTrail */
    "cloudtrail:LookupEvents",       "cloudtrail:CreateTrail",
    "cloudtrail:DeleteTrail",        "cloudtrail:StartLogging",
    "cloudtrail:StopLogging",        "cloudtrail:DescribeTrails",
    "cloudtrail:GetTrailStatus",     "cloudtrail:PutEventSelectors",

    /* Redshift */
    "redshift:GetClusterCredentials",  "redshift:CreateCluster",
    "redshift:DeleteCluster",          "redshift:DescribeClusters",
    "redshift:PauseCluster",           "redshift:ResumeCluster",
    "redshift:CreateClusterSnapshot",  "redshift:DeleteClusterSnapshot",

    /* Sentinel */
    NULL
};

/* ─────────────────────────────────────────────
   is_known_aws_action
   Returns 1 if 'action' is in the mock dictionary, 0 otherwise.
   Also accepts service-level wildcards, e.g. "s3:*", "iam:*".
───────────────────────────────────────────── */
static inline int is_known_aws_action(const char *action) {
    if (!action) return 0;

    /* Wildcard check: "service:*" — accept if 'service' is a known prefix */
    const char *colon = strchr(action, ':');
    if (colon && *(colon + 1) == '*' && *(colon + 2) == '\0') {
        /* Extract service prefix, e.g. "s3" */
        size_t svc_len = (size_t)(colon - action);
        for (int i = 0; AWS_ACTIONS[i] != NULL; i++) {
            if (strncmp(action, AWS_ACTIONS[i], svc_len) == 0 &&
                AWS_ACTIONS[i][svc_len] == ':') {
                return 1;  /* known service, wildcard is valid */
            }
        }
        return 0;
    }

    /* Exact match */
    for (int i = 0; AWS_ACTIONS[i] != NULL; i++) {
        if (strcmp(action, AWS_ACTIONS[i]) == 0) return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────────
   is_valid_arn
   Validates the ARN format:
     arn:partition:service:region:account-id:resource
   Accepts wildcards: "arn:aws:s3:::*" is valid.
   Special sentinel: "*" (allow all resources) is also accepted.
───────────────────────────────────────────── */
static inline int is_valid_arn(const char *arn) {
    if (!arn) return 0;
    /* Wildcard — explicitly allowed */
    if (strcmp(arn, "*") == 0) return 1;

    /* Must begin with "arn:" */
    if (strncmp(arn, "arn:", 4) != 0) return 0;

    /* Count colon-separated fields: expect exactly 6 fields */
    int colons = 0;
    for (const char *p = arn; *p; p++) {
        if (*p == ':') colons++;
    }
    /* arn : partition : service : region : account : resource
       =  0 :    1     :    2    :    3   :    4    :     5        → 5 colons */
    if (colons < 5) return 0;

    return 1;
}

#endif /* AWS_DICT_H */
