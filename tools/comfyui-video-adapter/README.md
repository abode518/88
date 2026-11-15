# جسر ComfyUI → واجهة الفيديو العامة

يحوّل [ComfyUI](https://github.com/comfyanywhere/ComfyUI) المحلي (أو أي سيرفر ComfyUI عندك) إلى الشكل العام اللي يتوقعه `server/engines/video.js` بالمشروع الرئيسي، بدون تعديل أي شيء في الموقع.

## لماذا تحتاجه

الموقع الرئيسي يتوقع:
- `POST {VIDEO_BASE_URL}/generate` → `{ job_id }`
- `GET {VIDEO_BASE_URL}/status/:id` → `{ status: 'pending'|'done'|'failed', video_url? }`

ComfyUI نفسه يتكلم بصيغة مختلفة (`/prompt`, `/history/:id`, graph كامل بدل برومبت نصي بسيط). هذا الجسر يترجم بين الاثنين.

## التركيب

```bash
cd tools/comfyui-video-adapter
npm install
cp .env.example .env
```

### 1. جهّز ComfyUI محلياً

نصّب ComfyUI وموديل فيديو مفتوح تشغّله محلياً بالكامل (بلا أي API خارجي):
- **Wan2.1** (Alibaba, مفتوح، جودة عالية t2v/i2v) — عندهم workflow جاهز بصفحة ComfyUI examples.
- **HunyuanVideo** (Tencent, مفتوح) — أثقل بالعتاد بس جودة أعلى.
- **AnimateDiff** على SD 1.5/SDXL — أخف وأسرع، مناسب لعتاد أضعف.

ابني الـ workflow بواجهة ComfyUI (أو حمّل مثال جاهز للموديل اللي اخترته)، شغّله مرة ببرومبت تجريبي وتأكد يخرج فيديو عادي من الواجهة نفسها قبل ما تربطه بالموقع.

### 2. صدّر الـ workflow بصيغة API

بواجهة ComfyUI: Settings ⚙️ → فعّل "Enable Dev mode options" → بعدها يظهر زر **"Save (API Format)"** بدل الحفظ العادي. اضغطه واحفظ الملف كـ `workflow.json` بنفس هذا المجلد.

### 3. حدّد رقم عنصر البرومبت

افتح `workflow.json` بأي محرر نص، دوّر على العنصر اللي `class_type` يساوي `CLIPTextEncode` (أو مكافئه بموديلك) واللي فيه نص البرومبت الإيجابي — مفتاحه الرقمي (مثلاً `"6"`) هو `PROMPT_NODE_ID`. ضعه بـ `.env`:

```
PROMPT_NODE_ID=6
PROMPT_FIELD=text
```

(لو عندك عنصر يتحكم بعدد الفريمات وتبي تربط مدة الفيديو، سجّل رقمه بـ `FRAMES_NODE_ID`/`FRAMES_FIELD` + `FRAMES_PER_SECOND` — اختياري.)

### 4. شغّل الجسر

```bash
npm start
```

### 5. اربطه بالموقع الرئيسي

بـ `.env` الموقع الرئيسي:
```
VIDEO_BASE_URL=http://localhost:9100
```

انتهى — زر "توليد فيديو" بالواجهة الحين يشغّل ComfyUI المحلي، وينزّل الناتج تلقائياً لما يخلص.

## ملاحظة

البرومبت يُحقن بالعنصر المحدد **كما هو حرفياً**، بدون أي فلترة أو تعديل من هذا الجسر. القيد الوحيد هو الموديل نفسه اللي شغّلته محلياً — وبما إنه عندك بالكامل (أوزان مفتوحة، بلا اتصال خارجي)، ما فيه رقابة غير اللي الموديل نفسه مدرّب عليها.
