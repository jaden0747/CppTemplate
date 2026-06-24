"""
test_client.py — Python test client for the CppPrototype CLI server.

Connects to the telnet-based CLI server and exercises:
  1. coding list          — enumerate all registered settings items
  2. coding get <item>    — retrieve a single item as JSON
  3. coding set           — modify a member at runtime
  4. coding get           — verify the modification took effect
  5. coding export        — save settings to a file

Usage:
    python tests/test_client.py                   # defaults: localhost:5000
    python tests/test_client.py --host 10.0.0.1 --port 9000
"""

import argparse
import json
import socket
import sys
import time


# ---------------------------------------------------------------------------
# Low-level telnet helpers
# ---------------------------------------------------------------------------
TELNET_TIMEOUT = 5.0   # seconds per receive
EOL = "\r\n"           # daniele77/cli expects CRLF


def connect(host: str, port: int) -> socket.socket:
    """Open a TCP connection and wait for the initial prompt."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TELNET_TIMEOUT)
    sock.connect((host, port))
    # Consume the welcome banner / initial prompt
    _recv_until_prompt(sock)
    return sock


def send_command(sock: socket.socket, cmd: str) -> str:
    """Send a command string and return the response text (up to the next prompt)."""
    sock.sendall((cmd + EOL).encode("utf-8"))
    return _recv_until_prompt(sock)


def _recv_until_prompt(sock: socket.socket) -> str:
    """
    Read from socket until we see a '>' prompt character (daniele77/cli uses
    the menu name followed by '>').  Collects everything before the prompt.
    """
    buf = b""
    deadline = time.monotonic() + TELNET_TIMEOUT
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        buf += chunk
        # daniele77/cli prompts look like "app> " or "coding> "
        text = buf.decode("utf-8", errors="replace")
        if "> " in text.split("\n")[-1]:
            break
    return _strip_telnet(buf.decode("utf-8", errors="replace"))


def _strip_telnet(text: str) -> str:
    """Remove telnet IAC sequences, echoed command, and trailing prompt."""
    import re
    # Strip telnet IAC negotiation bytes
    text = re.sub(r"\xff[\xfb-\xfe].", "", text)
    lines = text.split("\n")
    # Remove the trailing prompt line (e.g. "app> ")
    if lines and "> " in lines[-1]:
        lines = lines[:-1]
    # The first line(s) are the echoed command — skip until actual output.
    # daniele77/cli echoes "command args\r" then outputs the result.
    # We drop lines that start with the prompt or look like the echoed command.
    cleaned = []
    for line in lines:
        stripped = line.strip().replace("\r", "")
        # Skip empty lines and lines containing the prompt echo
        if not stripped:
            continue
        cleaned.append(stripped)
    # The first line is typically the echoed command — remove it
    if cleaned:
        cleaned = cleaned[1:]
    return "\n".join(cleaned).strip()


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------
def test_list_all(sock: socket.socket) -> bool:
    """Verify 'coding list' returns all registered items."""
    print("\n--- Test: coding list (all) ---")
    resp = send_command(sock, "coding list")
    print(resp)

    ok = "VehicleModel" in resp and "EngineStandalone" in resp
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected VehicleModel and EngineStandalone")
    return ok


def test_list_item(sock: socket.socket) -> bool:
    """Verify 'coding list VehicleModel' shows members."""
    print("\n--- Test: coding list VehicleModel ---")
    resp = send_command(sock, "coding list VehicleModel")
    print(resp)

    ok = "modelFilePath" in resp and "componentNames" in resp
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected modelFilePath and componentNames")
    return ok


def test_get_item(sock: socket.socket) -> bool:
    """Verify 'coding get EngineStandalone' returns valid JSON."""
    print("\n--- Test: coding get EngineStandalone ---")
    resp = send_command(sock, "coding get EngineStandalone")
    print(resp)

    try:
        data = json.loads(resp)
        ok = "renderDistance" in data
    except json.JSONDecodeError:
        ok = False
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected valid JSON with renderDistance")
    return ok


def test_set_and_verify(sock: socket.socket) -> bool:
    """Modify a setting and verify the change persists."""
    print("\n--- Test: coding set + verify ---")

    # Set renderDistance to 999.0
    resp = send_command(sock, 'coding set EngineStandalone renderDistance 999.0')
    print(f"  set response: {resp}")

    # Read back
    resp = send_command(sock, "coding get EngineStandalone")
    print(f"  get response: {resp}")

    try:
        data = json.loads(resp)
        ok = abs(data.get("renderDistance", 0) - 999.0) < 0.01
    except json.JSONDecodeError:
        ok = False
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected renderDistance == 999.0")
    return ok


def test_set_string(sock: socket.socket) -> bool:
    """Modify a string setting."""
    print("\n--- Test: coding set string value ---")

    resp = send_command(sock, 'coding set VehicleModel modelFilePath "\"custom/path.osg\""')
    print(f"  set response: {resp}")

    resp = send_command(sock, "coding get VehicleModel")
    try:
        data = json.loads(resp)
        ok = data.get("modelFilePath") == "custom/path.osg"
    except json.JSONDecodeError:
        ok = False
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected modelFilePath == custom/path.osg")
    return ok


def test_component_names_prefix(sock: socket.socket) -> bool:
    """
    Read VehicleModel.componentNames, add '_' prefix to every component name,
    write it back, then verify each value starts with '_'.
    """
    print("\n--- Test: VehicleModel componentNames _ prefix ---")

    # 1. Read current state of VehicleModel
    resp = send_command(sock, "coding get VehicleModel")
    try:
        vehicle = json.loads(resp)
    except json.JSONDecodeError as e:
        print(f"  -> FAIL: could not parse VehicleModel JSON: {e}")
        return False

    current_names: dict = vehicle.get("componentNames", {})
    if not current_names:
        print("  -> FAIL: componentNames is empty or missing")
        return False

    print(f"  Before: {json.dumps(current_names, indent=4)}")

    # 2. Add '_' prefix to every component name value
    prefixed_names = {k: "_" + v for k, v in current_names.items()}

    # 3. Send the patched componentNames object back
    # The 'coding set' command takes: <item> <member> <json_value>
    # The JSON value must be a valid JSON literal — use json.dumps for safety.
    payload = json.dumps(prefixed_names)
    resp = send_command(sock, f"coding set VehicleModel componentNames '{payload}'")
    print(f"  set response: {resp}")

    # 4. Read back and verify
    resp = send_command(sock, "coding get VehicleModel")
    try:
        vehicle_after = json.loads(resp)
    except json.JSONDecodeError as e:
        print(f"  -> FAIL: could not parse updated VehicleModel JSON: {e}")
        return False

    updated_names: dict = vehicle_after.get("componentNames", {})
    print(f"  After: {json.dumps(updated_names, indent=4)}")

    failures = [k for k, v in updated_names.items() if not v.startswith("_")]
    if failures:
        print(f"  -> FAIL: these members do not have '_' prefix: {failures}")
        return False

    print(f"  -> PASS: all {len(updated_names)} component names have '_' prefix")
    return True


def test_export(sock: socket.socket) -> bool:
    """Verify export command succeeds."""
    print("\n--- Test: coding export ---")
    resp = send_command(sock, "coding export test_export.json")
    print(resp)

    ok = "Exported" in resp or "export" in resp.lower()
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected success message")
    return ok


def test_status(sock: socket.socket) -> bool:
    """Verify the 'status' command works."""
    print("\n--- Test: status ---")
    resp = send_command(sock, "status")
    print(resp)

    ok = "VehicleModel" in resp
    print(f"  -> {'PASS' if ok else 'FAIL'}: expected item listing")
    return ok


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Test client for CppPrototype CLI server")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=5000, help="Server port (default: 5000)")
    args = parser.parse_args()

    print(f"Connecting to {args.host}:{args.port}...")
    try:
        sock = connect(args.host, args.port)
    except (ConnectionRefusedError, OSError) as e:
        print(f"ERROR: Cannot connect to {args.host}:{args.port} — {e}")
        print("Make sure cli_server.exe is running.")
        sys.exit(1)

    tests = [
        test_list_all,
        test_list_item,
        test_get_item,
        test_set_and_verify,
        test_set_string,
        test_component_names_prefix,
        test_export,
        test_status,
    ]

    passed = 0
    failed = 0
    for test in tests:
        try:
            if test(sock):
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"  -> ERROR: {e}")
            failed += 1

    sock.close()

    print(f"\n{'='*50}")
    print(f"Results: {passed} passed, {failed} failed out of {len(tests)} tests")
    print(f"{'='*50}")

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
