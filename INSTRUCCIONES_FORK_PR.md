# 📝 Instrucciones para crear Fork y Pull Request

## Estado Actual
✅ **Branch local creado**: `fase3-caracteristicas-avanzadas`  
✅ **Commit realizado**: `fd92c40` - feat: Fase 3 - Características Avanzadas v0.5.0  
✅ **Todos los cambios guardados**: 32 archivos, ~7,890 líneas agregadas  

## Pasos para crear el Fork y PR

### 1️⃣ Crear el Fork en GitHub
1. Ve a: https://github.com/Gureboy/Control-de-Luces
2. Click en el botón **Fork** (esquina superior derecha)
3. Selecciona tu cuenta para crear el fork

### 2️⃣ Agregar tu fork como remote
```bash
# Agregar tu fork como remote (reemplaza TU_USUARIO con tu username de GitHub)
git remote add myfork https://github.com/TU_USUARIO/Control-de-Luces.git

# Verificar que se agregó correctamente
git remote -v
```

### 3️⃣ Pushear el branch a tu fork
```bash
# Push del branch a tu fork
git push myfork fase3-caracteristicas-avanzadas

# Si te pide autenticación, usa:
# - Tu username de GitHub
# - Un Personal Access Token (no tu contraseña)
```

### 4️⃣ Crear el Pull Request
1. Ve a tu fork: `https://github.com/TU_USUARIO/Control-de-Luces`
2. Verás un mensaje: "fase3-caracteristicas-avanzadas had recent pushes"
3. Click en **"Compare & pull request"**
4. Usa el contenido de `PULL_REQUEST_TEMPLATE.md` para la descripción
5. Click en **"Create pull request"**

## 🔐 Si necesitas autenticación

### Opción A: Personal Access Token (Recomendado)
1. Ve a: https://github.com/settings/tokens
2. Click en "Generate new token (classic)"
3. Dale un nombre descriptivo
4. Selecciona el scope `repo`
5. Genera el token y cópialo
6. Úsalo como contraseña cuando git te lo pida

### Opción B: GitHub CLI
```bash
# Instalar GitHub CLI (si no lo tienes)
# Windows: winget install GitHub.cli
# o descarga desde: https://cli.github.com/

# Autenticarse
gh auth login

# Push con gh
gh repo fork Gureboy/Control-de-Luces --clone=false
git push myfork fase3-caracteristicas-avanzadas
```

## 📋 Verificación
Después de crear el PR, verifica que:
- [ ] El PR apunta de `TU_USUARIO:fase3-caracteristicas-avanzadas` a `Gureboy:main`
- [ ] Todos los archivos se muestran correctamente (32 archivos)
- [ ] No hay conflictos
- [ ] La descripción del PR está completa

## 🆘 Troubleshooting

### Error: "Permission denied"
- Asegúrate de haber hecho fork primero
- Verifica que estás pusheando a TU fork, no al original

### Error: "Authentication failed"
- No uses tu contraseña de GitHub
- Usa un Personal Access Token
- Verifica que el token tenga permisos de `repo`

### El branch no aparece en GitHub
```bash
# Verifica que estás en el branch correcto
git branch
# Deberías ver: * fase3-caracteristicas-avanzadas

# Si no, cámbiate al branch
git checkout fase3-caracteristicas-avanzadas

# Intenta pushear de nuevo
git push -u myfork fase3-caracteristicas-avanzadas
```

## ✅ Resumen de comandos
```bash
# Todo está listo localmente, solo necesitas:
git remote add myfork https://github.com/TU_USUARIO/Control-de-Luces.git
git push myfork fase3-caracteristicas-avanzadas
# Luego crear el PR desde GitHub
```

---
💡 **Nota**: El código ya está commiteado y listo. Solo necesitas hacer el fork y push.