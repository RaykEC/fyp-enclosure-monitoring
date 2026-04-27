import paho.mqtt.client as mqtt

# --- Configuration ---
BROKER_IP = "192.168.100.2"
BROKER_PORT = 1883
MQTT_USER = "backend_server"
MQTT_PASSWORD = "test"
TOPIC = "panel/#"

# --- Callback: fires when connection is established ---
def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to Mosquitto broker successfully!")
        client.subscribe(TOPIC)
        print(f"Subscribed to topic: {TOPIC}")
    else:
        print(f"Connection failed. Reason code: {reason_code}")

# --- Callback: fires when a message arrives ---
def on_message(client, userdata, message):
    topic = message.topic
    payload = message.payload.decode("utf-8")
    print(f"[RECEIVED] Topic: {topic} | Value: {payload}")

# --- Setup client ---
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message

# --- Connect and listen ---
print(f"Connecting to broker at {BROKER_IP}:{BROKER_PORT}...")
client.connect(BROKER_IP, BROKER_PORT, 60)
client.loop_forever()