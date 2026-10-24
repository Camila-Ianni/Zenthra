from google import genai
import os
from dotenv import load_dotenv

load_dotenv()
client = genai.Client(api_key=os.getenv("GOOGLE_API_KEY"))

print("--- MODELOS QUE SOPORTAN LIVE API (Bidi) ---")
for model in client.models.list():
    if 'bidiGenerateContent' in model.supported_generation_methods:
        print(f"ID: {model.name} | Display: {model.display_name}")