#!/usr/bin/env python3
"""
server.py — CloudPol AI Web Server (Gemini 2.0 Flash via google-genai)
"""

import os, re, sys, json
from pathlib import Path

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS

# ── Load .env ──────────────────────────────────────────────────────────────
try:
    from dotenv import load_dotenv
    for ep in [Path(__file__).parent/".env", Path(__file__).parent.parent/".env"]:
        if ep.exists(): load_dotenv(ep); break
except ImportError:
    pass

from google import genai
from google.genai import types

from prompts import SYSTEM_PROMPT
import compiler_runner as cr

# ── Configure Gemini ───────────────────────────────────────────────────────
API_KEY = os.environ.get("GOOGLE_API_KEY","").strip()
if not API_KEY:
    print("❌  GOOGLE_API_KEY not set.", file=sys.stderr); sys.exit(1)

client     = genai.Client(api_key=API_KEY)
MODEL_NAME = "gemini-3.1-pro-preview"

POLICY_PAT = re.compile(r"<POLICY>(.*?)</POLICY>", re.DOTALL | re.IGNORECASE)

# Per-session history: session_id → list of Content objects
# google-genai uses plain dicts {"role": "user"|"model", "parts": [{"text": ...}]}
sessions: dict[str, list] = {}

# ── Flask app ──────────────────────────────────────────────────────────────
UI_DIR = Path(__file__).parent / "ui"
app = Flask(__name__, static_folder=str(UI_DIR/"static"))
CORS(app)

# ── UI route ───────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return send_from_directory(str(UI_DIR), "index.html")

@app.route("/static/<path:path>")
def static_files(path):
    return send_from_directory(str(UI_DIR/"static"), path)

# ── Chat route ─────────────────────────────────────────────────────────────
@app.route("/api/chat", methods=["POST"])
def chat():
    data       = request.get_json(force=True)
    session_id = data.get("session_id", "default")
    user_msg   = data.get("message","").strip()
    if not user_msg:
        return jsonify({"error": "Empty message"}), 400

    history = sessions.setdefault(session_id, [])

    def _gemini(msg: str) -> str:
        """Send a message within the running history and return assistant text."""
        hist_copy = list(history)
        hist_copy.append({"role": "user", "parts": [{"text": msg}]})

        resp = client.models.generate_content(
            model   = MODEL_NAME,
            contents= hist_copy,
            config  = types.GenerateContentConfig(
                system_instruction=SYSTEM_PROMPT,
                temperature=0.2,
                max_output_tokens=1024,
            ),
        )
        answer = resp.text.strip()
        # Update persistent history
        history.append({"role": "user",  "parts": [{"text": msg}]})
        history.append({"role": "model", "parts": [{"text": answer}]})
        sessions[session_id] = history
        return answer

    reply       = _gemini(user_msg)
    pol_match   = POLICY_PAT.search(reply)
    policy_src  = pol_match.group(1).strip() if pol_match else None
    clean_reply = POLICY_PAT.sub("", reply).strip() if pol_match else reply

    compiler_out = None
    if policy_src:
        result = cr.run(policy_src, emit_iam=True)
        compiler_out = {
            "success":    result.success,
            "returncode": result.returncode,
            "stdout":     result.stdout,
            "stderr":     result.stderr,
            "iam_json":   result.iam_file,
            "pol_source": policy_src,
        }
        if not result.success:
            fb_reply = _gemini(
                f"[SYSTEM - COMPILER FEEDBACK] The compiler rejected the policy:\n"
                f"{result.stderr}\nExplain and ask for corrections."
            )
            clean_reply = fb_reply

    return jsonify({
        "reply":    clean_reply,
        "policy":   policy_src,
        "compiler": compiler_out,
    })

# ── Reset route ────────────────────────────────────────────────────────────
@app.route("/api/reset", methods=["POST"])
def reset():
    data = request.get_json(force=True)
    sessions.pop(data.get("session_id","default"), None)
    return jsonify({"ok": True})

@app.route("/api/health")
def health():
    return jsonify({"status": "ok", "model": MODEL_NAME})

# ── Run ────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    print(f"🚀  CloudPol AI  →  http://localhost:{port}")
    app.run(host="0.0.0.0", port=port, debug=False)
