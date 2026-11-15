// config.js — قراءة متغيرات البيئة لمحركات الشات/الصور/الفيديو
require('dotenv').config();

module.exports = {
  port: process.env.PORT || 3000,

  llm: {
    baseUrl: process.env.LLM_BASE_URL || '',
    apiKey: process.env.LLM_API_KEY || '',
    model: process.env.LLM_MODEL || '',
  },

  image: {
    mode: process.env.IMAGE_API_MODE || 'sdwebui', // sdwebui | openai
    baseUrl: process.env.IMAGE_BASE_URL || '',
    apiKey: process.env.IMAGE_API_KEY || '',
  },

  video: {
    baseUrl: process.env.VIDEO_BASE_URL || '',
    apiKey: process.env.VIDEO_API_KEY || '',
  },
};
