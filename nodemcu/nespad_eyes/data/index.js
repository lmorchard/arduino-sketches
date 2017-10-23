const $$ = id => document.getElementById(id);

const stahp = event => {
  const e = event || window.event;
  e.preventDefault && e.preventDefault();
  e.stopPropagation && e.stopPropagation();
  e.cancelBubble = true;
  e.returnValue = false;
  return false;
};

const addListener = (el, ev, handler) =>
  el.addEventListener(ev, ev => {
    handler(ev);
    return stahp(ev);
  }, true);

console.log('connecting websocket',
  window.location.hostname === 'localhost'
    ? 'in dev mode'
    : 'to real pumpkin!'
);

const socket = new WebSocket(
  window.location.hostname === 'localhost'
    ? 'ws://192.168.123.113:81'
    : 'ws://' + window.location.hostname + ':81'
);

socket.onopen = ev => {
  console.log('connected', ev);
  socket.send('hello');
};

socket.onclose = socket.onerror = ev => {
  setTimeout(() => window.location.history.reload(), 1000);
};

addListener(document, 'contextmenu', () => {}, true);

const joypad = $$('joypad');

const joypadState = {
  active: false,
  touch: null,
  start: { x: 0, y: 0 },
  end: { x: 0, y: 0 }
};

const getJoypadTouch = ev => {
  for (let i=0; i<ev.changedTouches.length; i++) {
    const touch = ev.changedTouches[i];
    if (touch.identifier === joypadState.touch) { return touch; }
  }
  return null;
}

const JOYPAD_DEADZONE = 50;

const updateJoypadState = data => {
  Object.assign(joypadState, data);

  if (!joypadState.active) {
    joypadState.angle = joypadState.distance = 0;
    socket.send('joypadoff');
  } else {
    const { start, end } = joypadState;
    const x1 = start.x;
    const y1 = start.y;
    const x2 = end.x;
    const y2 = end.y;

    joypadState.angle = (Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI) + 180;
    joypadState.distance = Math.sqrt(Math.pow(x1 - x2, 2) + Math.pow(y1 - y2, 2));

    if (joypadState.distance > JOYPAD_DEADZONE) {
      const { angle } = joypadState;
      if (angle > 20 && angle < 160) {
        socket.send('joypadupon');
      } else {
        socket.send('joypadupoff');
      }
      if (angle > 200 && angle < 340) {
        socket.send('joypaddownon');
      } else {
        socket.send('joypaddownoff');
      }
      if (angle > 120 && angle < 250) {
        socket.send('joypadrighton');
      } else {
        socket.send('joypadrightoff');
      }
      if (angle < 70 || angle > 290) {
        socket.send('joypadlefton');
      } else {
        socket.send('joypadleftoff');
      }
    }
  }

  // console.log(JSON.stringify(joypadState));
};

addListener(joypad, 'mousedown', ev => {
  updateJoypadState({
    active: true,
    start: { x: ev.clientX, y: ev.clientY },
    end: { x: ev.clientX, y: ev.clientY }
  });
});

addListener(joypad, 'mousemove', ev => {
  if (!joypadState.active) { return; }
  updateJoypadState({
    end: { x: ev.clientX, y: ev.clientY }
  });
});

addListener(joypad, 'mouseup', ev => {
  updateJoypadState({ active: false });
});

addListener(joypad, 'touchstart', ev => {
  if (joypadState.active) { return; }
  const touch = ev.changedTouches[0];
  updateJoypadState({
    active: true,
    touch: touch.identifier,
    start: { x: touch.clientX, y: touch.clientY },
    end: { x: touch.clientX, y: touch.clientY }
  });
});

addListener(joypad, 'touchmove', ev => {
  if (!joypadState.active) { return; }
  const touch = getJoypadTouch(ev);
  if (!touch) { return; }
  updateJoypadState({
    end: { x: touch.clientX, y: touch.clientY }
  });
});

addListener(joypad, 'touchend', ev => {
  if (!joypadState.active) { return; }
  const touch = getJoypadTouch(ev);
  if (!touch) { return; }
  updateJoypadState({
    active: false,
    touch: null
  });
});

addListener(joypad, 'touchcancel', ev => {
  if (!joypadState.active) { return; }
  const touch = getJoypadTouch(ev);
  if (!touch) { return; }
  updateJoypadState({
    active: false,
    touch: null
  });
});

['blink', 'talk'].forEach(name => {
  const el = $$(name);
  [
    ['mousedown', 'on'],
    ['mouseup', 'off'],
    ['touchstart', 'on'],
    ['touchend', 'off']
  ].forEach(([event, state]) => addListener(el, event, ev => {
    socket.send(name + state);
  }));
});
