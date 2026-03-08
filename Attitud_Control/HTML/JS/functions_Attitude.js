/*

Autor: Claudio Arzamendia Systems
Fecha: 2026-02-15 04:00:08


functions_Attitude.js - Funciones para actualizar el instrumento de actitud (horizonte artificial)
Envia los datos de actitud (pitch, roll) al ESP32 para que los refleje en el horizonte artificial,
y en todas las otras terminales.  

Rangos:
- Roll: -30° a +30°
- Pitch: -20° a +20° (±50px en el joystick)

*/ 

// Variables globales para el joystick
let joystickDragging = false;
let joystickCenterX = 40;  // Centro del joystick de 80px
let joystickCenterY = 40;
let joystickRadius = 28;   // Radio máximo de movimiento del knob (80/2 - 10 para el knob)
let zeroActive = false;
let currentPitch = 0;  // Valor actual de pitch en grados
let currentRoll = 0;   // Valor actual de roll en grados

// Función para actualizar la posición del knob basándose en valores de pitch/roll
function updateAttitudeKnobPosition(pitchValue, rollValue) {
  const knob = document.getElementById('knob');
  const armLine = document.getElementById('joystick-arm-line');
  
  if (!knob) return;
  
  // Calcular posición X desde roll: roll (-30° a +30°) → x (-28 a +28 px)
  const knobX = (rollValue / 30) * joystickRadius + joystickCenterX;
  
  // Calcular posición Y desde pitch: pitch (-20° a +20°) → y limitado a ±28px (50px = 20°)
  const pitchPx = (pitchValue / 20) * 50;
  const limitedY = Math.max(-joystickRadius, Math.min(joystickRadius, pitchPx));
  const knobY = joystickCenterY + limitedY;
  
  // Actualizar posición visual del knob
  knob.style.left = knobX + 'px';
  knob.style.top = knobY + 'px';
  
  // Actualizar línea del brazo
  if (armLine) {
    armLine.setAttribute('x2', knobX);
    armLine.setAttribute('y2', knobY);
  }
}

function isBrakeOn() {
  return window.brakeOnState === true;
}

function updateAttitudeBrakeUI(isOn) {
  const box = document.querySelector('.attitude-control-box');
  const joystick = document.getElementById('joystick');
  const zeroBtn = document.getElementById('atti-zero-btn');
  const pitchUpBtn = document.getElementById('pitch-up-btn');
  const pitchDownBtn = document.getElementById('pitch-down-btn');
  const rollUpBtn = document.getElementById('roll-up-btn');
  const rollDownBtn = document.getElementById('roll-down-btn');

  if (box) {
    box.classList.toggle('is-brake-on', isOn === true);
  }
  if (joystick) {
    joystick.style.pointerEvents = isOn ? 'none' : 'auto';
  }
  if (zeroBtn) {
    zeroBtn.disabled = isOn;
    zeroBtn.textContent = isOn ? 'Zero: LOCK' : 'Zero: OFF';
  }
  if (pitchUpBtn) pitchUpBtn.disabled = isOn;
  if (pitchDownBtn) pitchDownBtn.disabled = isOn;
  if (rollUpBtn) rollUpBtn.disabled = isOn;
  if (rollDownBtn) rollDownBtn.disabled = isOn;
}

function setupAttitudeControls() {
  const knob = document.getElementById('knob');
  const joystick = document.getElementById('joystick');
  const coordsDisplay = document.getElementById('coords');
  const zeroBtn = document.getElementById('atti-zero-btn');
  const armLine = document.getElementById('joystick-arm-line');
  
  // Elementos nuevos: valores y botones de pitch/roll
  const pitchValueEl = document.getElementById('pitch-value');
  const rollValueEl = document.getElementById('roll-value');
  const pitchUpBtn = document.getElementById('pitch-up-btn');
  const pitchDownBtn = document.getElementById('pitch-down-btn');
  const rollUpBtn = document.getElementById('roll-up-btn');
  const rollDownBtn = document.getElementById('roll-down-btn');

  if (!knob || !joystick) {
    console.warn('No se encontró el joystick en el DOM');
    return;
  }

  // Aplicar estado inicial de frenos
  updateAttitudeBrakeUI(isBrakeOn());

  // Centrar el knob inicialmente
  updateKnobPosition(joystickCenterX, joystickCenterY);

  // Botón Zero - anima suavemente a cero en 3 segundos
  if (zeroBtn) {
    zeroBtn.addEventListener('click', () => {
      if (isBrakeOn()) return;
      if (zeroActive) return; // Ya está animando
      
      zeroActive = true;
      zeroBtn.textContent = 'Zero: ON';
      zeroBtn.style.background = 'rgb(255, 0, 0)';
      
      // Obtener posición actual del knob
      const currentX = parseFloat(knob.style.left)  || joystickCenterX;
      const currentY = parseFloat(knob.style.top) || joystickCenterY;
      
      // Calcular valores iniciales de pitch/roll desde la posición del knob
      const startX = currentX - joystickCenterX;
      const startY = currentY - joystickCenterY;
      
      const duration = 8000; // 8 segundos (más gradual)
      const startTime = Date.now();
      
      function animateToZero() {
        const elapsed = Date.now() - startTime;
        const progress = Math.min(1, elapsed / duration);
        
        // Easing suave (ease-out)
        const easeProgress = 1 - Math.pow(1 - progress, 3);
        
        // Interpolar hacia el centro
        const x = startX * (1 - easeProgress);
        const y = startY * (1 - easeProgress);
        
        // Actualizar posición visual del knob
        updateKnobPosition(joystickCenterX + x, joystickCenterY + y);
        
        // Calcular ángulos actuales
        const rollDeg = (x / joystickRadius) * 30;
        const pitchPx = Math.max(-50, Math.min(50, y));
        const pitchDeg = (pitchPx / 50) * 20;
        
        // Enviar al ESP32
        sendAttitudeToESP32("pitchValue", pitchDeg);
        sendAttitudeToESP32("rollValue", rollDeg);
        
        // Actualizar variables globales durante animación
        currentPitch = pitchDeg;
        currentRoll = rollDeg;
        
        // Mostrar coordenadas
        if (coordsDisplay) {
          coordsDisplay.textContent = `roll: ${rollDeg.toFixed(1)}°, pitch: ${pitchDeg.toFixed(1)}°`;
        }
        if (pitchValueEl) pitchValueEl.textContent = pitchDeg.toFixed(1) + '°';
        if (rollValueEl) rollValueEl.textContent = rollDeg.toFixed(1) + '°';
        
        if (progress < 1) {
          requestAnimationFrame(animateToZero);
        } else {
          // Animación completada - volver a Zero: OFF
          zeroActive = false;
          zeroBtn.textContent = 'Zero: OFF';
          zeroBtn.style.background = '#444';
          
          // Asegurar valores finales exactos
          currentPitch = 0;
          currentRoll = 0;
          updateKnobPosition(joystickCenterX, joystickCenterY);
          sendAttitudeToESP32("pitchValue", 0);
          sendAttitudeToESP32("rollValue", 0);
          if (coordsDisplay) coordsDisplay.textContent = 'roll: 0°, pitch: 0°';
          if (pitchValueEl) pitchValueEl.textContent = '0.0°';
          if (rollValueEl) rollValueEl.textContent = '0.0°';
        }
      }
      
      animateToZero();
    });
  }

  // Eventos del joystick
  knob.addEventListener('mousedown', startDrag);
  knob.addEventListener('touchstart', startDrag, { passive: false });
  document.addEventListener('mousemove', onDrag);
  document.addEventListener('touchmove', onDrag, { passive: false });
  document.addEventListener('mouseup', endDrag);
  document.addEventListener('touchend', endDrag);

  // Eventos de los botones de flechas Pitch/Roll
  if (pitchUpBtn) {
    pitchUpBtn.addEventListener('click', () => {
      if (isBrakeOn()) return;
      if (zeroActive) return;
      currentPitch = Math.max(-20, currentPitch - 1);  // Arriba = pitch negativo (nariz arriba)
      updatePitchRollFromButtons();
    });
  }
  if (pitchDownBtn) {
    pitchDownBtn.addEventListener('click', () => {
      if (isBrakeOn()) return;
      if (zeroActive) return;
      currentPitch = Math.min(20, currentPitch + 1);   // Abajo = pitch positivo (nariz abajo)
      updatePitchRollFromButtons();
    });
  }
  if (rollUpBtn) {
    rollUpBtn.addEventListener('click', () => {
      if (isBrakeOn()) return;
      if (zeroActive) return;
      currentRoll = Math.min(30, currentRoll + 1);     // Roll derecha
      updatePitchRollFromButtons();
    });
  }
  if (rollDownBtn) {
    rollDownBtn.addEventListener('click', () => {
      if (isBrakeOn()) return;
      if (zeroActive) return;
      currentRoll = Math.max(-30, currentRoll - 1);    // Roll izquierda
      updatePitchRollFromButtons();
    });
  }

  // Función auxiliar para actualizar desde botones
  function updatePitchRollFromButtons() {
    // Actualizar displays
    if (pitchValueEl) pitchValueEl.textContent = currentPitch.toFixed(1) + '°';
    if (rollValueEl) rollValueEl.textContent = currentRoll.toFixed(1) + '°';
    
    // Actualizar posición del knob según los valores actuales
    const knobX = (currentRoll / 30) * joystickRadius + joystickCenterX;
    const knobY = (currentPitch / 20) * 50;  // 50px = 20°
    const limitedY = Math.max(-joystickRadius, Math.min(joystickRadius, knobY));
    updateKnobPosition(knobX, joystickCenterY + limitedY);
    
    // Enviar al ESP32
    sendAttitudeToESP32("pitchValue", currentPitch);
    sendAttitudeToESP32("rollValue", currentRoll);
  }

  function startDrag(e) {
    if (isBrakeOn()) return;
    if (zeroActive) return; // No mover si Zero está activo
    joystickDragging = true;
    knob.style.cursor = 'grabbing';
    e.preventDefault();
  }

  function onDrag(e) {
    if (isBrakeOn()) return;
    if (!joystickDragging || zeroActive) return;
    e.preventDefault();

    const rect = joystick.getBoundingClientRect();
    let clientX, clientY;
    
    if (e.touches) {
      clientX = e.touches[0].clientX;
      clientY = e.touches[0].clientY;
    } else {
      clientX = e.clientX;
      clientY = e.clientY;
    }

    // Posición relativa al centro del joystick
    let x = clientX - rect.left - joystickCenterX;
    let y = clientY - rect.top - joystickCenterY;

    // Limitar al radio máximo
    const dist = Math.sqrt(x * x + y * y);
    if (dist > joystickRadius) {
      x = (x / dist) * joystickRadius;
      y = (y / dist) * joystickRadius;
    }

    // Actualizar posición visual
    updateKnobPosition(joystickCenterX + x, joystickCenterY + y);

    // Calcular ángulos
    // Roll: x → -30° a +30° (joystickRadius = ±30°)
    // Pitch: y → -20° a +20° (50px = 20°, entonces joystickRadius(70) limitamos a ±50)
    const rollDeg = (x / joystickRadius) * 30;
    const pitchPx = Math.max(-50, Math.min(50, y));
    const pitchDeg = (pitchPx / 50) * 20;

    // Actualizar variables globales
    currentPitch = pitchDeg;
    currentRoll = rollDeg;

    // Actualizar displays de Pitch/Roll en los botones
    if (pitchValueEl) pitchValueEl.textContent = pitchDeg.toFixed(1) + '°';
    if (rollValueEl) rollValueEl.textContent = rollDeg.toFixed(1) + '°';

    // Enviar al ESP32
    sendAttitudeToESP32("pitchValue", pitchDeg);
    sendAttitudeToESP32("rollValue", rollDeg);
  }

  function endDrag() {
    if (joystickDragging) {
      joystickDragging = false;
      knob.style.cursor = 'grab';
    }
  }

  function updateKnobPosition(x, y) {
    if (knob) {
      // El knob usa transform: translate(-50%, -50%), así que x,y son el centro
      knob.style.left = x + 'px';
      knob.style.top = y + 'px';
    }
    if (armLine) {
      armLine.setAttribute('x2', x);
      armLine.setAttribute('y2', y);
    }
  }
}

function sendAttitudeToESP32(DataVar, DataValue) {
  getWebSocketInstance(function(ws) {
    console.log('Enviando actitud al ESP32:', DataVar, DataValue);
    ws.send(JSON.stringify({ [DataVar]: DataValue }));
  });
}   

function updateAttitudeControl(pitchValue, rollValue) {
  // Actualizar el instrumento visualmente
  const ball = document.getElementById('AttCon_ball');
  const dial = document.getElementById('AttCon_dial');
  const pitchValueEl = document.getElementById('pitch-value');
  const rollValueEl = document.getElementById('roll-value');
  
  if (ball) {
    // Pitch: mover verticalmente (±50px para ±20°)
    const pitchPx = (pitchValue / 20) * 50;
    // Primero rotate, luego translateY (orden importante!)
    ball.style.transform = `rotate(${rollValue}deg) translateY(${pitchPx}px)`;
  }
  
  if (dial) {
    // Roll: rotar (±30°)
    dial.style.transform = `rotate(${rollValue}deg)`;
  }

  // Actualizar displays de valores numéricos
  if (pitchValueEl) pitchValueEl.textContent = pitchValue.toFixed(1) + '°';
  if (rollValueEl) rollValueEl.textContent = rollValue.toFixed(1) + '°';

  // Actualizar display de coordenadas si existe
  const coordsDisplay = document.getElementById('coords');
  if (coordsDisplay) {
    coordsDisplay.textContent = `roll: ${rollValue.toFixed(1)}°, pitch: ${pitchValue.toFixed(1)}°`;
  }
  
  // Actualizar posición visual del knob del joystick
  updateAttitudeKnobPosition(pitchValue, rollValue);
}

function initializeAttitudeControls() {
  setupAttitudeControls();}

// Alias para compatibilidad con mainHTML.cpp
function initAttitudeControls() {
  setupAttitudeControls();
}



// Alterna la visibilidad del cristal roto en el instrumento Attitude
function toggleAttitudeBrokenCrystal() {
  const crystal = document.getElementById('attitude_broken_crystal12');
  if (crystal) {
    crystal.classList.toggle('visible');
  }
}