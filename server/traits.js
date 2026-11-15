// traits.js — وصف موحّد لصفات الشخصية (جنس + سمات جسدية) يُستخدم بمحرك الشات ومحرك الصور

const GENDER_LABELS = {
  male: 'ذكر',
  female: 'أنثى',
  femboy: 'فيمبوي (مظهر وطاقة أنثوية في جسد ذكوري)',
  tomboy: 'تومبوي (مظهر وطاقة ذكورية في جسد أنثوي)',
  futa: 'فوتا (جسد أنثوي ويملك قضيب)',
};

// تاغات إنجليزية مختصرة تُستخدم بالضبط كما هي داخل برومبت الصور (sdwebui/openai)
const GENDER_IMAGE_TAGS = {
  male: 'male,',
  female: 'female,',
  femboy: 'male body, feminine features, androgynous,',
  tomboy: 'female body, masculine features, androgynous,',
  futa: 'futanari, female body with penis,',
};

const HAS_PENIS = ['male', 'femboy', 'futa'];

// جملة عربية توصف السمات الجسدية المختارة — تُستخدم بـ system prompt الشات
function describeTraitsArabic(character) {
  const parts = [];
  if (character.hair_color) parts.push(`شعر ${character.hair_color}`);
  if (character.hairstyle) parts.push(`قصة الشعر ${character.hairstyle}`);
  if (character.eye_color) parts.push(`عيون ${character.eye_color}`);
  if (character.breast_size) parts.push(`حجم الصدر ${character.breast_size}`);
  if (character.butt_size) parts.push(`حجم الطيز ${character.butt_size}`);
  if (character.penis_size && HAS_PENIS.includes(character.gender)) parts.push(`حجم القضيب ${character.penis_size}`);
  return parts.join('، ');
}

// تاغات إنجليزية تُحقن ببرومبت الصور — نفس القيم المختارة بدون ترجمة أو تعديل
function describeTraitsImageTags(character) {
  const tags = [];
  if (character.hair_color) tags.push(`${character.hair_color} hair`);
  if (character.hairstyle) tags.push(`${character.hairstyle} hairstyle`);
  if (character.eye_color) tags.push(`${character.eye_color} eyes`);
  if (character.breast_size) tags.push(`${character.breast_size} breasts`);
  if (character.butt_size) tags.push(`${character.butt_size} ass`);
  if (character.penis_size && HAS_PENIS.includes(character.gender)) tags.push(`${character.penis_size} penis`);
  return tags.join(', ');
}

module.exports = { GENDER_LABELS, GENDER_IMAGE_TAGS, HAS_PENIS, describeTraitsArabic, describeTraitsImageTags };
