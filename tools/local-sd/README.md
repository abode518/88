# سيرفر صور محلي سريع — SD-Turbo

بديل لـ A1111/ComfyUI لما يكون عندك CPU بس (بدون GPU) — موديل `stabilityai/sd-turbo` يحتاج خطوة وحدة أو خطوتين بس، فيطلع صورة خلال ١٥-٣٠ ثانية على ٤ أنوية، بدل دقائق مع موديل SD عادي.

يحاكي شكل `/sdapi/v1/txt2img` تبع A1111 بالضبط — يشتغل مباشرة مع الموقع الرئيسي بدون أي تعديل، فقط:

```
IMAGE_API_MODE=sdwebui
IMAGE_BASE_URL=http://localhost:7860
```

## التشغيل

```bash
pip install -r requirements.txt
python3 -m uvicorn server:app --host 0.0.0.0 --port 7860
```

أول تشغيل ينزّل الموديل (~2GB) ويخزّنه بـ `../../models/hf_cache`.

## ملاحظة

الجودة أقل من SD كامل الخطوات (طبيعي، هذا الثمن مقابل السرعة). لو عندك GPU حقيقي، ارجع لـ A1111/ComfyUI بموديل كامل (Pony Diffusion، إلخ) زي الموصى به بالـ README الرئيسي — جودة أعلى بكثير وبرضو سريع لأن عندك GPU.
