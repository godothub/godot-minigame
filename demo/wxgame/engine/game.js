
import './godot-sdk'
import './godot'
const exe = '/engine/godot';
const pack = '/engine/demo-pck.bin';

let promise = GODOTSDK.startGame(exe, pack)