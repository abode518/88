# server.py — سيرفر توليد صور محلي سريع بموديل SD-Turbo (خطوة واحدة، مناسب لـ CPU)
# يحاكي شكل /sdapi/v1/txt2img تبع A1111 عشان يشتغل مباشرة مع IMAGE_API_MODE=sdwebui بدون أي تعديل بالموقع
import base64
import io
import os

import torch
from diffusers import AutoPipelineForText2Image
from fastapi import FastAPI
from pydantic import BaseModel

CACHE_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "models", "hf_cache")

print("تحميل الموديل (مرة وحدة عند التشغيل)...")
pipe = AutoPipelineForText2Image.from_pretrained(
    "stabilityai/sd-turbo", dtype=torch.float32, cache_dir=CACHE_DIR
)
pipe = pipe.to("cpu")
pipe.set_progress_bar_config(disable=True)
print("الموديل جاهز.")

app = FastAPI()


class Txt2ImgRequest(BaseModel):
    prompt: str
    negative_prompt: str = ""
    width: int = 512
    height: int = 512
    steps: int = 2  # SD-Turbo مصمم لعدد خطوات قليل جدًا — هذا سر السرعة


@app.post("/sdapi/v1/txt2img")
def txt2img(req: Txt2ImgRequest):
    # SD-Turbo لا يستخدم guidance scale (يحتاج 0.0) ولا negative prompt فعليًا، لكن نقبلها بالواجهة للتوافق
    image = pipe(
        prompt=req.prompt,
        num_inference_steps=max(1, min(req.steps, 4)),
        guidance_scale=0.0,
        width=req.width,
        height=req.height,
    ).images[0]

    buf = io.BytesIO()
    image.save(buf, format="PNG")
    b64 = base64.b64encode(buf.getvalue()).decode("utf-8")
    return {"images": [b64]}


@app.get("/")
def health():
    return {"status": "ok", "model": "stabilityai/sd-turbo"}
