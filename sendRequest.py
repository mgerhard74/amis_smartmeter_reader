import socket
import time
import sys
import json

# Konfiguration
TARGET_IP = "192.168.1.255"
PORT = 2223
MESSAGE = '{"id":9,"method":"EM1.GetStatus","params":{"id":0}}'
INTERVAL = 1.0  # Sekunden

# Counter
requests_sent = 0
responses_received = 0
invalid_responses = 0
response_times = []
last_request_time = None
last_valid_src = None

# Socket-Setup
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(1)

print("Starte Broadcast-Loop (Ctrl+C zum Beenden)")

try:
    while True:
        try:
            # Request senden und Zeit speichern
            last_request_time = time.time()
            sock.sendto(MESSAGE.encode("utf-8"), (TARGET_IP, PORT))
            requests_sent += 1

            data, addr = sock.recvfrom(1024)
            response_time = (time.time() - last_request_time) * 1000  # in ms

            # validate JSON response structure
            try:
                payload = data.decode("utf-8")
                obj = json.loads(payload)
                valid = False
                if isinstance(obj, dict) and "result" in obj and isinstance(obj["result"], dict) and "src" in obj:
                    res = obj["result"]
                    # single-phase: expect 'act_power'
                    if "act_power" in res:
                        try:
                            float(res["act_power"])
                            valid = True
                        except (ValueError, TypeError):
                            valid = False
                    # three-phase: expect a_act_power, b_act_power, c_act_power, total_act_power
                    elif all(k in res for k in ("a_act_power","b_act_power","c_act_power","total_act_power")):
                        try:
                            float(res["a_act_power"]); float(res["b_act_power"]); float(res["c_act_power"]); float(res["total_act_power"])
                            valid = True
                        except (ValueError, TypeError):
                            valid = False
                if valid:
                    response_times.append(response_time)
                    responses_received += 1
                    last_valid_src = obj.get("src")
                else:
                    invalid_responses += 1
            except (UnicodeDecodeError, json.JSONDecodeError):
                invalid_responses += 1

        except socket.timeout:
            pass

        # Statistik berechnen
        if response_times:
            min_time = min(response_times)
            max_time = max(response_times)
            avg_time = sum(response_times) / len(response_times)
            last_time = response_times[-1]
            stats = f" | Last: {last_time:6.1f} ms | Avg: {avg_time:6.1f} ms | Min: {min_time:6.1f} ms | Max: {max_time:6.1f} ms"
        else:
            stats = ""

        # Statuszeile überschreiben
        loss = requests_sent - responses_received - invalid_responses
        srcinfo = f" | Last_src:{last_valid_src}" if last_valid_src else ""
        sys.stdout.write(
            f"\rsent/received/invalid/loss: {requests_sent} / {responses_received} / {invalid_responses} / {loss} {stats}{srcinfo}"
        )
        sys.stdout.flush()

        time.sleep(INTERVAL)

except KeyboardInterrupt:
    print("\n\nBeendet durch Benutzer")

finally:
    sock.close()
