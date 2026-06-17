# Node-RED And MQTT Motor Control

The Node-RED dashboard controls the same pan/tilt servos as the touchscreen Motor app.

## Files

Node-RED flow:

```text
node-red/nodered-supernova-motors-flow.json
```

Example Mosquitto config:

```text
node-red/mosquitto-supernova.conf
```

## MQTT Broker

The MQTT broker must run on the laptop or another computer reachable by the ESP32 touchscreen.

The included Mosquitto config:

```conf
listener 1883 0.0.0.0
allow_anonymous true
persistence false
log_type all
```

Why this matters:

- Node-RED can connect to `127.0.0.1` if it runs on the same laptop as the broker.
- The ESP32 cannot use `localhost`; it must use the laptop network IP address.

Example:

```text
Laptop MQTT broker IP: 172.20.10.2
MQTT port: 1883
```

Then set this in:

```text
esp32-touchscreen/platformio.ini
```

```ini
-DSUPERNOVA_MQTT_HOST=\"172.20.10.2\"
```

Rebuild and upload the touchscreen firmware after changing this value.

## Import Node-RED Flow

1. Open Node-RED.
2. Open the menu.
3. Choose Import.
4. Select `node-red/nodered-supernova-motors-flow.json`, or paste its contents.
5. Import the flow.
6. Deploy.
7. Open the Node-RED dashboard.

The flow contains:

- Up button
- Down button
- Left button
- Right button
- MQTT output node
- MQTT input node for servo state
- Text display for the latest servo state

## MQTT Topics

Command topic:

```text
supernova/motors/cmd
```

Accepted payloads:

```text
up
down
left
right
```

State topic:

```text
supernova/motors/state
```

Example state payload:

```json
{"pan":135,"tilt":135,"source":"status"}
```

## Test Broker From Windows

Check local broker:

```powershell
Test-NetConnection 127.0.0.1 -Port 1883
```

Check broker on laptop network IP:

```powershell
Test-NetConnection 172.20.10.2 -Port 1883
```

Both should show:

```text
TcpTestSucceeded : True
```

If `127.0.0.1` works but the laptop IP does not, the broker is only listening locally or Windows Firewall is blocking port 1883.

## Windows Firewall

If needed, allow MQTT inbound traffic:

```powershell
New-NetFirewallRule -DisplayName "MQTT 1883" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow
```

Run PowerShell as Administrator for this command.

## Expected ESP32 Display Status

Open the Motor app on the touchscreen. It should show:

```text
Broker: YOUR_BROKER_IP
MQTT: online
```

If it shows `MQTT: retry`, check:

- Touchscreen WiFi is connected.
- Laptop and touchscreen are on the same network.
- MQTT broker is running.
- `SUPERNOVA_MQTT_HOST` is the laptop IP.
- Windows Firewall allows port 1883.

