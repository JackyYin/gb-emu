import GameBoyModule from '../dist/gameboy.js';

const SCREEN_W = 160;
const SCREEN_H = 144;

const canvas = document.getElementById('screen');
const ctx = canvas.getContext('2d');
const romInput = document.getElementById('rom-file');
const bootInput = document.getElementById('boot-rom');
const statusEl = document.getElementById('status');

let gbModule = null;
let romData = null;
let bootRomData = null;
let bootRomReady = false;

const keyMap = {
    'ArrowUp': 0x04,
    'ArrowDown': 0x08,
    'ArrowLeft': 0x02,
    'ArrowRight': 0x01,
    'z': 0x20,
    'x': 0x10,
    'Enter': 0x80,
    'Shift': 0x40,
};

let joypadState = 0xFF;

document.addEventListener('keydown', (e) => {
    if (keyMap[e.key] !== undefined) {
        joypadState &= ~keyMap[e.key];
        if (gbModule) {
            gbModule._gb_set_joypad_wasm(joypadState);
        }
        e.preventDefault();
    }
});

document.addEventListener('keyup', (e) => {
    if (keyMap[e.key] !== undefined) {
        joypadState |= keyMap[e.key];
        if (gbModule) {
            gbModule._gb_set_joypad_wasm(joypadState);
        }
    }
});

document.querySelectorAll('.gamepad .btn').forEach(btn => {
    const key = btn.dataset.key;
    if (!keyMap[key]) return;

    const press = (e) => {
        e.preventDefault();
        joypadState &= ~keyMap[key];
        if (gbModule) {
            gbModule._gb_set_joypad_wasm(joypadState);
        }
    };

    const release = (e) => {
        e.preventDefault();
        joypadState |= keyMap[key];
        if (gbModule) {
            gbModule._gb_set_joypad_wasm(joypadState);
        }
    };

    btn.addEventListener('mousedown', press);
    btn.addEventListener('mouseup', release);
    btn.addEventListener('mouseleave', release);
    btn.addEventListener('touchstart', press, { passive: false });
    btn.addEventListener('touchend', release, { passive: false });
    btn.addEventListener('touchcancel', release, { passive: false });
});

romInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    statusEl.textContent = 'Loading ROM...';

    try {
        const buffer = await file.arrayBuffer();
        romData = new Uint8Array(buffer);

        if (!gbModule) {
            console.log('Initializing WASM module...');
            gbModule = await GameBoyModule();
            console.log('WASM module loaded, initializing emulator...');
            gbModule._gb_init_wasm();
            console.log('Emulator initialized');
        }

        statusEl.textContent = `Loaded: ${file.name} (no boot ROM)`;

        console.log('Loading ROM, size:', romData.length);
        const romPtr = gbModule._malloc(romData.length);
        gbModule.HEAPU8.set(romData, romPtr);
        gbModule._gb_load_rom_wasm(romPtr, romData.length);

        console.log('ROM loaded, starting emulation');
        startEmulation();
    } catch (err) {
        console.error('Error loading ROM:', err);
        statusEl.textContent = `Error: ${err.message}`;
    }
});

function startEmulation() {
    if (!gbModule) return;

    const imageData = ctx.createImageData(SCREEN_W, SCREEN_H);

    function frame() {
        gbModule._gb_run_frame_wasm();

        const fbPtr = gbModule._gb_get_framebuffer_wasm();
        const fb = new Uint32Array(gbModule.HEAPU8.buffer, fbPtr, SCREEN_W * SCREEN_H);

        for (let i = 0; i < SCREEN_W * SCREEN_H; i++) {
            imageData.data[i * 4] = fb[i] & 0xFF;
            imageData.data[i * 4 + 1] = (fb[i] >> 8) & 0xFF;
            imageData.data[i * 4 + 2] = (fb[i] >> 16) & 0xFF;
            imageData.data[i * 4 + 3] = 0xFF;
        }
        ctx.putImageData(imageData, 0, 0);
        requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
}
