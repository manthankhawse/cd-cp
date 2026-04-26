#!/usr/bin/env python3
"""
agent.py — CloudPol AI Policy Assistant (Gemini 2.0 Flash CLI mode)
For the Web UI, run server.py instead.
"""
import os, re, sys
from pathlib import Path

try:
    from dotenv import load_dotenv
    for ep in [Path(__file__).parent/".env", Path(__file__).parent.parent/".env"]:
        if ep.exists(): load_dotenv(ep); break
except ImportError:
    pass

try:
    from google import genai
    from google.genai import types
except ImportError:
    print("❌  Run:  pip install google-genai"); sys.exit(1)

from prompts import SYSTEM_PROMPT
import compiler_runner as cr

API_KEY = os.environ.get("GOOGLE_API_KEY","").strip()
if not API_KEY: print("❌  GOOGLE_API_KEY not set."); sys.exit(1)

client     = genai.Client(api_key=API_KEY)
MODEL_NAME = "gemini-3.1-pro-preview"
POLICY_PAT = re.compile(r"<POLICY>(.*?)</POLICY>", re.DOTALL | re.IGNORECASE)
BANNER     = "\n╔══════════════════════════════════════╗\n║  CloudPol AI · Gemini 2.0 · CLI Mode  ║\n║  'quit' exit · 'reset' · 'server.py' for UI ║\n╚══════════════════════════════════════╝\n"

def _ai(t):  print(f"\033[96m[AI]\033[0m {t}\n")
def _sys(t): print(f"\033[93m[•]\033[0m {t}\n")

class Agent:
    def __init__(self):
        self.history: list = []
        self.last_pol: str | None = None

    def _call(self, msg: str) -> str:
        self.history.append({"role":"user","parts":[{"text":msg}]})
        resp = client.models.generate_content(
            model=MODEL_NAME, contents=self.history,
            config=types.GenerateContentConfig(
                system_instruction=SYSTEM_PROMPT, temperature=0.2, max_output_tokens=1024))
        answer = resp.text.strip()
        self.history.append({"role":"model","parts":[{"text":answer}]})
        return answer

    def turn(self, msg: str) -> bool:
        cmd = msg.strip().lower()
        if cmd in ("quit","exit"): print("Goodbye!"); return False
        if cmd == "reset":   self.__init__(); _sys("Reset."); return True
        if cmd == "show":    print(self.last_pol or "(none)"); return True

        try: reply = self._call(msg)
        except Exception as e: print(f"Error: {e}"); return True

        m = POLICY_PAT.search(reply)
        if m:
            pol = m.group(1).strip()
            self.last_pol = pol
            clean = POLICY_PAT.sub("",reply).strip()
            if clean: _ai(clean)
            print("\033[95m--- CloudPol DSL ---\033[0m\n" + pol + "\n\033[95m--------------------\033[0m\n")
            _sys("Compiling…")
            r = cr.run(pol, emit_iam=True)
            if r.success:
                print("✅ OK!\n" + (r.iam_file or ""))
            else:
                print("❌ Error:\n" + r.stderr)
                fb = self._call(f"[COMPILER FEEDBACK]\n{r.stderr}\nExplain and ask for corrections.")
                _ai(fb)
        else:
            _ai(reply)
        return True

def main():
    print(BANNER)
    agent = Agent()
    _ai("Hello! Describe your access policy in plain English.")
    while True:
        try: u = input("\033[92mYou:\033[0m ").strip()
        except (KeyboardInterrupt, EOFError): print("\nBye!"); break
        if u and not agent.turn(u): break

if __name__ == "__main__": main()
