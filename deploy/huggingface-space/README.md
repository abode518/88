# نشر كامل (شات + صور) على Hugging Face Spaces — مجاني

خطة CPU المجانية بتاعت Hugging Face Spaces أسخى بكثير من Render: ذاكرة أكبر، ما يحتاج بطاقة ائتمان أبدًا للخطة المجانية الأساسية. هذا يشغّل **نفس الموقع + نفس الموديلات** اللي جرّبناها بالجلسة (Josiefied-7B للشات، SD-Turbo للصور) على رابط عام دائم.

## الخطوات

### ١. أنشئ Space جديد
افتح [huggingface.co/new-space](https://huggingface.co/new-space):
- SDK: **Docker**
- Hardware: **CPU basic** (مجاني)
- Visibility: اختر Public أو Private حسب رغبتك

بعد الإنشاء يعطيك رابط git زي:
```
https://huggingface.co/spaces/<اسمك>/<اسم-السبيس>
```

### ٢. ادفع الكود له
من جهازك (بعد ما تسوي `git clone` للريبو الأساسي):
```bash
git clone -b claude/character-image-video-creation-8rooup https://github.com/abode518/88.git
cd 88
git remote add space https://huggingface.co/spaces/<اسمك>/<اسم-السبيس>
git push space claude/character-image-video-creation-8rooup:main
```
(أول مرة بيطلب تسجيل دخول Hugging Face — استخدم اسم المستخدم + access token تسويه من Settings → Access Tokens)

### ٣. انتظر البناء
البناء بياخذ وقت أطول من Render — ينزّل ويخبز موديلين (~٧GB) داخل الصورة. تابع من تبويب "Logs" بصفحة الـ Space.

### ٤. افتح الرابط
بعد ما يخلص البناء، الـ Space يعطيك رابط عام دائم زي:
```
https://<اسمك>-<اسم-السبيس>.hf.space
```
شات وصور شغّالين عليه فعليًا من أول فتحة، بدون ما تضبط أي شي إضافي بـ `.env` — كل شي مخبوز جاهز بالصورة.

## ملاحظات

- **البناء بياخذ وقت** (تحميل الموديلات + تثبيت torch) — ممكن ١٠-٢٠ دقيقة أول مرة
- لو الـ Space وقف أو طلع خطأ ذاكرة، قلّل `--n_ctx` بـ `start.sh` (حط 2048 بدل 4096) أو جرب Hardware أعلى (مو مجاني)
- تحديث لاحق: أي `git push` جديد لنفس الـ remote يعيد البناء تلقائي
