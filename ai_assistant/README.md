# CloudPol AI Policy Assistant

A conversational CLI agent that turns plain-English descriptions into
valid CloudPol policy files and compiles them into AWS IAM JSON.

Powered by **Groq** (`llama-3.3-70b-versatile`) — fast, free inference.

---

## Prerequisites

1. Build the CloudPol compiler first (from the project root):
   ```bash
   make
   ```

2. Get a free **Groq API key** at https://console.groq.com

---

## Setup

```bash
cd ai_assistant

# Install Python dependencies
pip install -r requirements.txt

# Set your API key (one of two methods):

# Method 1 – shell environment
export GROQ_API_KEY=your_key_here

# Method 2 – .env file
echo "GROQ_API_KEY=your_key_here" > .env
```

---

## Running

```bash
python agent.py
```

---

## Example Session

```
[Agent] Hello! I'm ready to help you create CloudPol policies...

You: Allow the DevOps team to read from S3

[Agent] Got it! A few quick details:
         What is the target S3 resource? Please provide an ARN
         (e.g. 'arn:aws:s3:::my-bucket') or '*' for all buckets.

You: arn:aws:s3:::prod-assets

[Agent] Should this allow be restricted to any conditions?
         (e.g. specific IP, MFA required, time of day, region)
         Type 'no' if it should be unconditional.

You: Only from the office network 10.0.0.0/8 and MFA must be enabled

--- Generated CloudPol DSL ---
ALLOW ROLE "DevOps"
      ACTION "s3:GetObject"
      ON RESOURCE "arn:aws:s3:::prod-assets"
      WHERE ip == "10.0.0.0/8" AND mfa == TRUE;
------------------------------

[System] Compiling policy…

✅ Compilation successful!

Generated AWS IAM Policy JSON:
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "CloudPol_DevOps_1",
      "Effect": "Allow",
      "Principal": {"AWS": "*"},
      "Action": "s3:GetObject",
      "Resource": "arn:aws:s3:::prod-assets",
      "Condition": {
        "IpAddress": { "aws:SourceIp": "10.0.0.0/8" }
      }
    }
  ]
}
```

---

## Agent Intelligence

The agent will automatically:

| Situation | Behaviour |
|-----------|-----------|
| Missing effect (ALLOW/DENY) | Asks which effect you want |
| Vague action ("read from S3") | Asks which specific `s3:*` action |
| No resource ARN provided | Asks for ARN or constructs one |
| `mfa` value not boolean | Re-asks with TRUE/FALSE |
| `time` value outside 0–23 | Warns and asks to correct |
| ALLOW + DENY conflict on same triple | Warns about conflict, asks for resolution |
| Compiler rejects the policy | Feeds errors back and asks for corrections |

---

## Commands

| Command | Description |
|---------|-------------|
| `reset` | Start a new conversation |
| `show`  | Print the last generated CloudPol DSL |
| `help`  | Show command list |
| `quit` / `exit` | Exit the assistant |

---

## Supported AWS Services

The agent knows about: **S3, EC2, IAM, Lambda, DynamoDB, RDS, KMS, SQS, SNS,
CloudWatch, STS, Secrets Manager, CloudTrail, Redshift, Glue, Athena, Logs**

---

## Architecture

```
agent.py (ReAct loop)
    │
    ├── prompts.py          — System prompt + follow-up templates
    ├── compiler_runner.py  — subprocess wrapper for the C compiler
    └── Groq API            — LLM inference (llama-3.3-70b-versatile)
```
