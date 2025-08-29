from fastapi import FastAPI, Depends, Header, HTTPException
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import paho.mqtt.client as mqtt
from langchain.agents import initialize_agent, AgentType, Tool
from langchain_google_genai import ChatGoogleGenerativeAI
from supabase import create_client, Client
import requests
import uvicorn

# -------------------- SUPABASE CONFIG --------------------
SUPABASE_URL = "https://wczwmlhzsousesecnvnu.supabase.co"
SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6IndjendtbGh6c291c2VzZWNudm51Iiwicm9sZSI6InNlcnZpY2Vfcm9sZSIsImlhdCI6MTc1NjQxODQ5MSwiZXhwIjoyMDcxOTk0NDkxfQ.Nr5Y9O_0siANu87PPCWEr0jptn5nFyobmde_lBt-VRE"  # service_role
supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

SUPABASE_AUTH_URL = f"{SUPABASE_URL}/auth/v1/user"

def verify_token(authorization: str = Header(...)):
    """Verify Supabase JWT from frontend login"""
    try:
        token = authorization.split(" ")[1]  # remove 'Bearer '
        resp = requests.get(SUPABASE_AUTH_URL, headers={"Authorization": f"Bearer {token}"})
        if resp.status_code != 200:
            raise HTTPException(status_code=401, detail="Invalid or expired token")
        return resp.json()  # contains user info (id, email, etc.)
    except Exception:
        raise HTTPException(status_code=401, detail="Not authenticated")

# -------------------- MQTT CONFIG --------------------
BROKER = "e10ae8e9b2d54a6d96198879f7d83758.s1.eu.hivemq.cloud"
PORT = 8883
MQTT_USER = "hivemq.webclient.1756145271255"
MQTT_PASS = "1KTtGo38;2$pMw:Ze>Fd"

TOPIC_IR1 = "home/ir1"
TOPIC_IR2 = "home/ir2"
TOPIC_SERVO = "home/servo"
TOPIC_LDR = "home/ldr"
TOPIC_FIRE = "home/fire"

mqtt_client = mqtt.Client()
mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)
mqtt_client.tls_set()   # 🔒 TLS enabled

latest_ir1 = None
latest_ir2 = None
latest_ldr = None
latest_fire = None

# -------------------- MQTT CALLBACKS --------------------
def on_connect(client, userdata, flags, rc):
    print("Connected with result code", rc)
    client.subscribe(TOPIC_IR1)
    client.subscribe(TOPIC_IR2)
    client.subscribe(TOPIC_LDR)
    client.subscribe(TOPIC_FIRE)

def on_message(client, userdata, msg):
    global latest_ir1, latest_ir2, latest_ldr, latest_fire
    try:
        sensor_value = int(msg.payload.decode())
        print(f"📡 MQTT message: topic={msg.topic}, value={sensor_value}")
        sensor_name = None

        if msg.topic == TOPIC_IR1:
            latest_ir1 = sensor_value
            sensor_name = "ir1"
        elif msg.topic == TOPIC_IR2:
            latest_ir2 = sensor_value
            sensor_name = "ir2"
        elif msg.topic == TOPIC_LDR:
            latest_ldr = sensor_value
            sensor_name = "ldr"
        elif msg.topic == TOPIC_FIRE:
            latest_fire = sensor_value
            sensor_name = "fire"

        if sensor_name:
            response = supabase.table("sensor_readings").insert({
                "sensor_name": sensor_name,
                "value": sensor_value
            }).execute()
            print(f"✅ Inserted into Supabase: {response}")

            if sensor_name == "fire" and sensor_value == 1:
                supabase.table("alerts").insert({
                    "message": "🔥 Fire detected!"
                }).execute()

    except ValueError:
        pass

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(BROKER, PORT, 60)
mqtt_client.loop_start()

# -------------------- TOOL FUNCTIONS --------------------
def get_ir1_reading(_=None):
    return str(latest_ir1) if latest_ir1 is not None else "IR1 reading not available"

def get_ir2_reading(_=None):
    return str(latest_ir2) if latest_ir2 is not None else "IR2 reading not available"

def get_ldr_reading(_=None):
    return str(latest_ldr) if latest_ldr is not None else "LDR reading not available"

def get_fire_status(_=None):
    if latest_fire is None:
        return "Fire sensor reading not available"
    return "FIRE DETECTED!" if latest_fire == 1 else "No fire detected"

def move_servo(angle: str):
    try:
        angle_int = int(angle)
        if angle_int not in [0, 90]:
            return "Invalid angle. Must be 0 or 90."
    except ValueError:
        return "Invalid angle. Provide a 0 or 90 degree angle."
    mqtt_client.publish(TOPIC_SERVO, str(angle_int))
    supabase.table("access_logs").insert({"action": f"Servo moved to {angle_int}°"}).execute()
    return f"Servo moved to {angle_int}°"

# -------------------- LANGCHAIN AGENT --------------------
tools = [
    Tool(name="Get IR1 Reading", func=get_ir1_reading, description="Get the latest IR1 sensor reading."),
    Tool(name="Get IR2 Reading", func=get_ir2_reading, description="Get the latest IR2 sensor reading."),
    Tool(name="Get LDR Reading", func=get_ldr_reading, description="Get the latest LDR sensor reading."),
    Tool(name="Get Fire Status", func=get_fire_status, description="Check if fire is detected."),
    Tool(name="Move Servo", func=move_servo, description="Move servo to a specified angle."),
]

system_prompt = """
You are an IoT smart road analyst assistant.
Always use the tools. Do not explain unless required.
"""

llm = ChatGoogleGenerativeAI(
    model="gemini-2.5-flash",
    google_api_key="AIzaSyAJyXzk6ZGyae-N6oQ_r49x5hGPi098gb8"
)

agent = initialize_agent(
    tools=tools,
    llm=llm,
    agent=AgentType.STRUCTURED_CHAT_ZERO_SHOT_REACT_DESCRIPTION,
    verbose=True,
    handle_parsing_errors=True,
    return_only_outputs=True
)

# -------------------- FASTAPI APP --------------------
class ChatRequest(BaseModel):
    text: str

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # change later for security
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.post("/chat")
async def chat(request: ChatRequest):
    try:
        reply = agent.invoke({
            "input": request.text,
            "chat_history": [{"role": "system", "content": system_prompt}]
        })
        return JSONResponse({"reply": reply.get("output", "")})
    except Exception as e:
        return JSONResponse({"error": str(e)}, status_code=500)

@app.get("/history/{sensor}")
async def get_history(sensor: str, user=Depends(verify_token)):
    response = supabase.table("sensor_readings").select("*").eq("sensor_name", sensor).order("created_at", desc=True).limit(20).execute()
    return response.data

@app.get("/alerts")
async def get_alerts(user=Depends(verify_token)):
    response = supabase.table("alerts").select("*").order("created_at", desc=True).execute()
    return response.data

@app.get("/logs")
async def get_logs(user=Depends(verify_token)):
    response = supabase.table("access_logs").select("*").order("created_at", desc=True).execute()
    return response.data

# -------------------- RUN --------------------
if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
