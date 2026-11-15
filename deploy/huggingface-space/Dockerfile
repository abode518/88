# Dockerfile — نشر كامل (شات + صور) على Hugging Face Spaces (خطة CPU المجانية)
# الموديلات تُنزَّل وتُخبز داخل الصورة وقت البناء، فما يحتاج تحميل كل مرة يشتغل الكونتينر
FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl ca-certificates gnupg build-essential cmake git \
    && curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# ---- تبعيات Node ----
COPY package*.json ./
RUN npm install --omit=dev

# ---- تبعيات Python (CPU فقط) ----
RUN pip install --no-cache-dir torch --index-url https://download.pytorch.org/whl/cpu \
    && pip install --no-cache-dir diffusers transformers accelerate safetensors pillow \
       fastapi "uvicorn[standard]" "llama-cpp-python[server]"

# ---- كود الموقع ----
COPY server ./server
COPY public ./public
COPY tools/local-sd ./tools/local-sd

# ---- خبز موديل الشات داخل الصورة (Josiefied-Qwen2.5-7B-Instruct-abliterated-v2, Q4_K_M) ----
RUN mkdir -p models && curl -L -o models/model.gguf \
    "https://huggingface.co/Lucy-in-the-Sky/Josiefied-Qwen2.5-7B-Instruct-abliterated-v2-Q4_K_M-GGUF/resolve/main/josiefied-qwen2.5-7b-instruct-abliterated-v2-q4_k_m.gguf"

# ---- خبز موديل الصور (SD-Turbo) داخل الصورة ----
RUN python3 -c "from diffusers import AutoPipelineForText2Image; import torch; \
    AutoPipelineForText2Image.from_pretrained('stabilityai/sd-turbo', dtype=torch.float32, cache_dir='models/hf_cache')"

COPY deploy/huggingface-space/start.sh ./start.sh
RUN chmod +x start.sh

# Hugging Face Spaces يوجّه الترافيك الخارجي لهذا المنفذ فقط
EXPOSE 7860
ENV PORT=7860 \
    LLM_BASE_URL=http://localhost:8080/v1 \
    LLM_API_KEY=none \
    LLM_MODEL=josiefied-qwen2.5-7b-instruct-abliterated-v2 \
    IMAGE_API_MODE=sdwebui \
    IMAGE_BASE_URL=http://localhost:7861 \
    IMAGE_API_KEY=none \
    VIDEO_BASE_URL= \
    VIDEO_API_KEY=

CMD ["./start.sh"]
