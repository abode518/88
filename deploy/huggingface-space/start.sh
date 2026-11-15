#!/bin/bash
# start.sh — يشغّل محرك الشات (llama.cpp) ومحرك الصور (SD-Turbo) بالخلفية، ثم الموقع الرئيسي بالمقدمة
set -e

echo "تشغيل محرك الشات (llama.cpp server)..."
python3 -m llama_cpp.server \
  --model /app/models/model.gguf \
  --host 0.0.0.0 --port 8080 \
  --chat_format chatml --n_ctx 4096 --n_threads "$(nproc)" &

echo "تشغيل محرك الصور (SD-Turbo)..."
cd /app/tools/local-sd
python3 -m uvicorn server:app --host 0.0.0.0 --port 7861 &
cd /app

# مهلة بسيطة لحد ما الموديلات تخلص تحميل بالذاكرة — الموقع نفسه يعطي خطأ واضح لو طلبت قبل لا يجهزوا
sleep 8

echo "تشغيل الموقع الرئيسي..."
exec node server/index.js
