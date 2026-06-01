const fs = require("fs")

// //indexdb文件读取时修复(针对fake的indexdb问题, 原生web没有这个问题)
// const INDEX_DB_TEMPLATE1 = {
//     match:
//         `getRemoteSet:(mount,callback)=>{var entries={};IDBFS.getDB(mount.mountpoint,(err,db)=>{if(err)return callback(err);try{var transaction=db.transaction([IDBFS.DB_STORE_NAME],"readonly");transaction.onerror=e=>{callback(e.target.error);e.preventDefault()};var store=transaction.objectStore(IDBFS.DB_STORE_NAME);var index=store.index("timestamp");index.openKeyCursor().onsuccess=event=>{var cursor=event.target.result;if(!cursor){return callback(null,{type:"remote",db:db,entries:entries})}entries[cursor.primaryKey]={"timestamp":cursor.key};cursor.continue()}}catch(e){return callback(e)}})}`,
//     replace: `getRemoteSet:(mount,callback)=>{var entries={};IDBFS.getDB(mount.mountpoint,(err,db)=>{if(err)return callback(err);try{var called=false;var transaction=db.transaction([IDBFS.DB_STORE_NAME],"readonly");transaction.onerror=(e)=>{callback(e.target.error);e.preventDefault()};transaction.oncomplete=(e)=>{if(called)return;called=true;callback(null,{type:"remote",db:db,entries:entries,})};var store=transaction.objectStore(IDBFS.DB_STORE_NAME);var index=store.index("timestamp");index.openKeyCursor().onsuccess=(event)=>{var cursor=event.target.result;if(!cursor){if(called)return;called=true;return callback(null,{type:"remote",db:db,entries:entries})}entries[cursor.primaryKey]={timestamp:cursor.key};cursor.continue()}}catch(e){return callback(e)}})}`
// }
// //canvas
// const FIND_CANVAS_TEMPLATE = {
//     match:
//         `var findEventTarget=target=>{target=maybeCStringToJsString(target);var domElement=specialHTMLTargets[target]||(typeof document!="undefined"?document.querySelector(target):undefined);return domElement}`,
//     replace: `var findEventTarget=(target)=>{return globalThis.canvas}`
// }
// //gl检测
// const WEB_GL_INSTANCE_TEMPLATE = {
//     match: `ver=="webgl"==gl instanceof WebGLRenderingContext?gl:null`,
//     replace: `gl`
// }
//禁用修改标题
const WEB_TILE_DISABLE_TEMPLATE = {
    match: `function _godot_js_display_window_title_set(p_data){document.title=GodotRuntime.parseString(p_data)}`,
    replace: `function _godot_js_display_window_title_set(p_data){}`
}
//performance
const PERFORMANCE_TIME_TEMPLATE = {
    match: `_emscripten_get_now=()=>performance.now()`,
    replace: `_emscripten_get_now=nowPolyfill`
}
const DISPLAY_PIXEL_RATIO_TEMPLATE = {
    match: `getPixelRatio:function(){return GodotDisplayScreen.hidpi?window.devicePixelRatio||1:1}`,
    replace: `getPixelRatio:function(){if(!GodotDisplayScreen.hidpi){return 1}let ratio=Number(window.devicePixelRatio)||1;try{if(typeof wx!=="undefined"){const info=wx.getWindowInfo?wx.getWindowInfo():wx.getSystemInfoSync&&wx.getSystemInfoSync();if(info){ratio=Number(info.pixelRatio||info.devicePixelRatio||ratio)||ratio}}}catch(e){}return Math.max(1,ratio)}`
}
// getValue, setValue
const GET_VALUE_TEMPLATE = {
    match: `case"i64":abort("to do getValue(i64) use WASM_BIGINT");`,
    replace: `case"i64":return HEAP32[ptr>>2];`
}

const SET_VALUE_TEMPLATE = {
    match: `case"i64":abort("to do setValue(i64) use WASM_BIGINT");`,
    replace: `case"i64":tempI64=[value>>>0,(tempDouble=value,+Math.abs(tempDouble)>=1?tempDouble>0?+Math.floor(tempDouble/4294967296)>>>0:~~+Math.ceil((tempDouble- +(~~tempDouble>>>0))/4294967296)>>>0:0)],HEAP32[ptr>>2]=tempI64[0],HEAP32[ptr+4>>2]=tempI64[1];break;`
}
//mount wx fs
const WX_FS_TEMPLATE = {
    match: `FS.mount(IDBFS,{},path)`,
    replace: `FS.mount(WXMEMFS,{},path)`
}
const WASM_INSTANCEOF_TEMPLATE = {
    match: `e instanceof WebAssembly.RuntimeError`,
    replace: `e`
}
const WRITE_STAT = {
    match: `HEAP64[buf+88>>3]=BigInt(stat.ino);`,
    replace:`if(stat.ino)HEAP64[buf+88>>3]=BigInt(stat.ino);`
}

let originGodotJsContent = fs.readFileSync("./bin/.web_zip/godot.js", "utf-8")

const DEFINE_TEMP_VAR = {
    match: `var noExitRuntime=Module["noExitRuntime"]||false;`,
    replace: `var tempDouble;var tempI64;var noExitRuntime=Module["noExitRuntime"]||false;`
}

const newGodotJsContent = originGodotJsContent.
    replace(WASM_INSTANCEOF_TEMPLATE.match, WASM_INSTANCEOF_TEMPLATE.replace)
//     .replace(FIND_CANVAS_TEMPLATE.match, FIND_CANVAS_TEMPLATE.replace)
//     .replace(WEB_GL_INSTANCE_TEMPLATE.match, WEB_GL_INSTANCE_TEMPLATE.replace)
    .replace(WEB_TILE_DISABLE_TEMPLATE.match, WEB_TILE_DISABLE_TEMPLATE.replace)
    .replace(PERFORMANCE_TIME_TEMPLATE.match, PERFORMANCE_TIME_TEMPLATE.replace)
    .replace(DISPLAY_PIXEL_RATIO_TEMPLATE.match, DISPLAY_PIXEL_RATIO_TEMPLATE.replace)
    .replace(GET_VALUE_TEMPLATE.match, GET_VALUE_TEMPLATE.replace)
    .replace(SET_VALUE_TEMPLATE.match, SET_VALUE_TEMPLATE.replace)
    .replace(WX_FS_TEMPLATE.match, WX_FS_TEMPLATE.replace)
    .replace(WRITE_STAT.match, WRITE_STAT.replace)

console.log("patched")
fs.writeFileSync("./bin/.web_zip/godot.js", newGodotJsContent)
