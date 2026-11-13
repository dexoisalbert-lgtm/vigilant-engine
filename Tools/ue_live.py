"""Drive the already-open Unreal Editor without restarting it.

Uses Unreal's built-in Python remote execution (local machine only, enabled in
Config/DefaultEngine.ini). Commands:

  python Tools/ue_live.py ping               check the editor is reachable
  python Tools/ue_live.py livecode           Live Coding compile of .cpp changes
  python Tools/ue_live.py tuning             reimport Data/SwimTuning.csv
  python Tools/ue_live.py play | stop        start / stop Play In Editor
  python Tools/ue_live.py exec "<python>"    run Python inside the editor
  python Tools/ue_live.py file <script.py>   run a Python file inside the editor

Live Coding handles changes to function bodies in .cpp files. Changes to
headers (new members, UPROPERTYs, classes) still need a full rebuild with the
editor closed.
"""
from pathlib import Path
import sys
import time

PROJECT = Path(__file__).resolve().parent.parent
ENGINE_PY = Path(r"E:\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
sys.path.insert(0, str(ENGINE_PY))
import remote_execution  # noqa: E402  (ships with Unreal)

TUNING = r'''
import unreal
csv = r"{csv}"
table = unreal.EditorAssetLibrary.load_asset("/Game/Data/DT_SwimTuning")
struct = unreal.load_object(None, "/Script/Paleotrench.PaleotrenchSwimSettings")
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv, struct)
ok = ok and unreal.EditorAssetLibrary.save_loaded_asset(table)
print("TUNING REIMPORTED" if ok else "TUNING REIMPORT FAILED")
'''.format(csv=PROJECT / "Data" / "SwimTuning.csv")

PLAY = '''
import unreal
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
print("PLAY REQUESTED")
'''

STOP = '''
import unreal
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
print("STOP REQUESTED")
'''

LIVECODE = '''
import unreal
unreal.SystemLibrary.execute_console_command(None, "LiveCoding.Compile")
print("LIVE CODING STARTED")
'''


def run(code):
    remote = remote_execution.RemoteExecution()
    remote.start()
    try:
        deadline = time.time() + 8
        while not remote.remote_nodes and time.time() < deadline:
            time.sleep(0.2)
        nodes = [n for n in remote.remote_nodes if "Paleotrench" in str(n.get("project_name", ""))]
        if not nodes:
            print("ERROR: no running Paleotrench editor found (is it open, with remote execution enabled?)")
            return False
        remote.open_command_connection(nodes[0]["node_id"])
        result = remote.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
        for line in result.get("output", []):
            print(line.get("output", "").rstrip())
        if not result.get("success"):
            print("ERROR:", result.get("result"))
        return bool(result.get("success"))
    finally:
        remote.stop()


def wait_for_live_coding(log_path, start_size, timeout=300):
    """Follow the editor log until Live Coding reports an outcome."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        time.sleep(1)
        try:
            with open(log_path, "r", encoding="utf-8", errors="replace") as log:
                log.seek(start_size)
                text = log.read()
        except OSError:
            continue
        for line in text.splitlines():
            if "LogLiveCoding" not in line:
                continue
            lower = line.lower()
            if any(word in lower for word in ("succeeded", "failed", "no changes", "cancelled")):
                print(line)
                return "succeeded" in lower or "no changes" in lower
            if "error" in lower:
                print(line)
    print("ERROR: Live Coding did not report a result within", timeout, "s")
    return False


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    command = argv[1]
    if command == "ping":
        return 0 if run('print("EDITOR REACHABLE")') else 1
    if command == "tuning":
        return 0 if run(TUNING) else 1
    if command == "play":
        return 0 if run(PLAY) else 1
    if command == "stop":
        return 0 if run(STOP) else 1
    if command == "exec" and len(argv) > 2:
        return 0 if run(argv[2]) else 1
    if command == "file" and len(argv) > 2:
        return 0 if run(Path(argv[2]).read_text(encoding="utf-8")) else 1
    if command == "livecode":
        log_path = PROJECT / "Saved" / "Logs" / "Paleotrench.log"
        start = log_path.stat().st_size if log_path.exists() else 0
        if not run(LIVECODE):
            return 1
        return 0 if wait_for_live_coding(log_path, start) else 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
