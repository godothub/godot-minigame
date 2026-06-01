(function (global) {
    const root = global;
    const windowObject = root.window || root;
    const gameGlobal = root.GameGlobal || (root.GameGlobal = {});
    const wxApi = root.wx;

    class GodotLoader {
        constructor(onScreenCanvas, config) {
            this.onScreenCanvas = onScreenCanvas;
            this.offScreenCanvas = document.createElement("canvas");
            this.config = config;
            this.progress = 0;
            this.currentText = config.textConfig.firstStartText;
            this.backgroundImage = null;
            this.iconImage = null;
            this.dpr = this.getDevicePixelRatio();
            this.resizeHandler = this.resizeCanvases.bind(this);

            this.initWebGL();
            this.loadImages();
            this.resizeCanvases();
            windowObject.addEventListener("resize", this.resizeHandler);

            this.render();
            this.loadGameEngine();
        }

        getWindowInfo() {
            if (wxApi && typeof wxApi.getWindowInfo === "function") {
                return wxApi.getWindowInfo();
            }
            if (wxApi && typeof wxApi.getSystemInfoSync === "function") {
                return wxApi.getSystemInfoSync();
            }
            return null;
        }

        getDevicePixelRatio() {
            const info = this.getWindowInfo();
            const ratio =
                Number(info && (info.pixelRatio || info.devicePixelRatio)) ||
                Number(windowObject.devicePixelRatio) ||
                1;

            windowObject.devicePixelRatio = ratio;
            gameGlobal.__godotMinigamePixelRatio = ratio;
            return Math.max(1, ratio);
        }

        getViewportSize() {
            const info = this.getWindowInfo();
            if (info && info.windowWidth && info.windowHeight) {
                return {
                    width: info.windowWidth,
                    height: info.windowHeight,
                };
            }
            return {
                width: windowObject.innerWidth,
                height: windowObject.innerHeight,
            };
        }

        initWebGL() {
            const attrs = {
                alpha: false,
                antialias: false,
                depth: true,
                enableExtensionsByDefault: 1,
                explicitSwapControl: 1,
                failIfMajorPerformanceCaveat: false,
                majorVersion: 2,
                minorVersion: 0,
                powerPreference: "default",
                premultipliedAlpha: true,
                preserveDrawingBuffer: true,
                proxyContextToMainThread: 0,
                renderViaOffscreenBackBuffer: true,
                stencil: false,
            };

            this.gl = this.onScreenCanvas.getContext("webgl2", attrs);
            if (!this.gl) {
                console.error("Unable to initialize WebGL.");
                return;
            }

            const vertexShaderSource = `
                attribute vec2 a_position;
                attribute vec2 a_texCoord;
                varying vec2 v_texCoord;
                void main() {
                    gl_Position = vec4(a_position, 0.0, 1.0);
                    v_texCoord = vec2(a_texCoord.x, 1.0 - a_texCoord.y);
                }
            `;

            const fragmentShaderSource = `
                precision highp float;
                uniform sampler2D u_image;
                varying vec2 v_texCoord;
                void main() {
                    gl_FragColor = texture2D(u_image, v_texCoord);
                }
            `;

            const vertexShader = this.createShader(this.gl.VERTEX_SHADER, vertexShaderSource);
            const fragmentShader = this.createShader(this.gl.FRAGMENT_SHADER, fragmentShaderSource);
            this.program = this.createProgram(vertexShader, fragmentShader);
            this.positionLocation = this.gl.getAttribLocation(this.program, "a_position");
            this.texCoordLocation = this.gl.getAttribLocation(this.program, "a_texCoord");

            this.positionBuffer = this.gl.createBuffer();
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
            this.gl.bufferData(
                this.gl.ARRAY_BUFFER,
                new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]),
                this.gl.STATIC_DRAW
            );

            this.texCoordBuffer = this.gl.createBuffer();
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
            this.gl.bufferData(
                this.gl.ARRAY_BUFFER,
                new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]),
                this.gl.STATIC_DRAW
            );

            this.texture = this.gl.createTexture();
            this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
            this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
            this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
            this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
            this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.LINEAR);
        }

        createShader(type, source) {
            const shader = this.gl.createShader(type);
            this.gl.shaderSource(shader, source);
            this.gl.compileShader(shader);
            if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS)) {
                console.error("Shader compile failed: " + this.gl.getShaderInfoLog(shader));
                this.gl.deleteShader(shader);
                return null;
            }
            return shader;
        }

        createProgram(vertexShader, fragmentShader) {
            const program = this.gl.createProgram();
            this.gl.attachShader(program, vertexShader);
            this.gl.attachShader(program, fragmentShader);
            this.gl.linkProgram(program);
            if (!this.gl.getProgramParameter(program, this.gl.LINK_STATUS)) {
                console.error("Program link failed: " + this.gl.getProgramInfoLog(program));
                return null;
            }
            return program;
        }

        loadImages() {
            if (this.config.materialConfig.backgroundImage) {
                this.backgroundImage = new Image();
                this.backgroundImage.src = this.config.materialConfig.backgroundImage;
                this.backgroundImage.onload = this.render.bind(this);
            }

            if (this.config.materialConfig.iconImage) {
                this.iconImage = new Image();
                this.iconImage.src = this.config.materialConfig.iconImage;
                this.iconImage.onload = this.render.bind(this);
            }
        }

        resizeCanvases() {
            const viewport = this.getViewportSize();
            const width = viewport.width;
            const height = viewport.height;

            this.dpr = this.getDevicePixelRatio();
            this.onScreenCanvas.width = width * this.dpr;
            this.onScreenCanvas.height = height * this.dpr;
            this.onScreenCanvas.style.width = width + "px";
            this.onScreenCanvas.style.height = height + "px";
            this.offScreenCanvas.width = width * this.dpr;
            this.offScreenCanvas.height = height * this.dpr;

            if (this.gl) {
                this.gl.viewport(0, 0, this.onScreenCanvas.width, this.onScreenCanvas.height);
            }

            this.render();
        }

        drawRoundedRect(ctx, x, y, width, height, radius) {
            if (width < 2 * radius) {
                radius = width / 2;
            }
            if (height < 2 * radius) {
                radius = height / 2;
            }
            ctx.beginPath();
            ctx.moveTo(x + radius, y);
            ctx.arcTo(x + width, y, x + width, y + height, radius);
            ctx.arcTo(x + width, y + height, x, y + height, radius);
            ctx.arcTo(x, y + height, x, y, radius);
            ctx.arcTo(x, y, x + width, y, radius);
            ctx.closePath();
            ctx.fill();
        }

        drawCoverImage(ctx, image, width, height) {
            if (!image.width || !image.height) {
                ctx.drawImage(image, 0, 0, width, height);
                return;
            }

            const scale = Math.max(width / image.width, height / image.height);
            const drawWidth = image.width * scale;
            const drawHeight = image.height * scale;
            ctx.drawImage(image, (width - drawWidth) / 2, (height - drawHeight) / 2, drawWidth, drawHeight);
        }

        render() {
            if (!this.gl) {
                return;
            }

            const ctx = this.offScreenCanvas.getContext("2d");
            const width = this.offScreenCanvas.width;
            const height = this.offScreenCanvas.height;

            ctx.clearRect(0, 0, width, height);
            ctx.imageSmoothingEnabled = true;
            ctx.imageSmoothingQuality = "high";

            if (this.backgroundImage) {
                this.drawCoverImage(ctx, this.backgroundImage, width, height);
            }

            const barConfig = this.config.barConfig.style;
            const iconConfig = this.config.iconConfig.style;
            const barX = (width - barConfig.width * this.dpr) / 2;
            const barY =
                height -
                iconConfig.bottom * this.dpr -
                iconConfig.height * this.dpr -
                30 * this.dpr -
                barConfig.height * this.dpr;

            ctx.fillStyle = barConfig.backgroundColor;
            this.drawRoundedRect(
                ctx,
                barX,
                barY,
                barConfig.width * this.dpr,
                barConfig.height * this.dpr,
                barConfig.borderRadius * this.dpr
            );

            if (this.progress > 0) {
                const progressWidth = Math.max(
                    0,
                    (barConfig.width * this.dpr - 2 * barConfig.padding * this.dpr) * this.progress
                );
                ctx.fillStyle = barConfig.foregroundColor;
                this.drawRoundedRect(
                    ctx,
                    barX + barConfig.padding * this.dpr,
                    barY + barConfig.padding * this.dpr,
                    progressWidth,
                    barConfig.height * this.dpr - 2 * barConfig.padding * this.dpr,
                    (barConfig.borderRadius - barConfig.padding) * this.dpr
                );
            }

            const textConfig = this.config.textConfig;
            ctx.font = textConfig.style.fontSize * this.dpr + "px Arial";
            ctx.fillStyle = textConfig.style.color;
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText(this.currentText, width / 2, barY + (barConfig.height * this.dpr) / 2);

            if (this.config.iconConfig.visible && this.iconImage) {
                const iconX = (width - iconConfig.width * this.dpr) / 2;
                const iconY = height - iconConfig.bottom * this.dpr - iconConfig.height * this.dpr;
                ctx.drawImage(this.iconImage, iconX, iconY, iconConfig.width * this.dpr, iconConfig.height * this.dpr);
            }

            this.renderToWebGL();
        }

        renderToWebGL() {
            if (!this.gl) {
                return;
            }

            this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
            this.gl.texImage2D(
                this.gl.TEXTURE_2D,
                0,
                this.gl.RGBA,
                this.gl.RGBA,
                this.gl.UNSIGNED_BYTE,
                this.offScreenCanvas
            );

            this.gl.useProgram(this.program);

            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
            this.gl.enableVertexAttribArray(this.positionLocation);
            this.gl.vertexAttribPointer(this.positionLocation, 2, this.gl.FLOAT, false, 0, 0);

            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texCoordBuffer);
            this.gl.enableVertexAttribArray(this.texCoordLocation);
            this.gl.vertexAttribPointer(this.texCoordLocation, 2, this.gl.FLOAT, false, 0, 0);

            this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);

            if (typeof this.gl.flush === "function") {
                this.gl.flush();
            }
            if (typeof this.gl.commit === "function") {
                this.gl.commit();
            }
        }

        normalizeProgress(progress) {
            const value = Number(progress) || 0;
            return Math.max(0, Math.min(1, value > 1 ? value / 100 : value));
        }

        updateProgress(progress, text) {
            const normalized = this.normalizeProgress(progress);
            this.progress = Math.max(this.progress || 0, normalized);
            if (text) {
                this.currentText = text;
            }
            this.render();

            if (typeof windowObject.requestAnimationFrame === "function") {
                windowObject.requestAnimationFrame(() => this.render());
            }
        }

        loadGameEngine() {
            if (!wxApi || typeof wxApi.loadSubpackage !== "function") {
                return;
            }

            const task = wxApi.loadSubpackage({
                name: "engine",
                success: () => {
                    this.progress = 1;
                    this.updateProgress(this.progress, this.config.textConfig.initText);
                },
            });

            if (task && typeof task.onProgressUpdate === "function") {
                task.onProgressUpdate(({ progress }) => {
                    this.updateProgress(progress, this.config.textConfig.downloadingText[0]);
                });
            }
        }

        cleanup() {
            windowObject.removeEventListener("resize", this.resizeHandler);

            if (!this.gl) {
                return;
            }

            this.gl.deleteProgram(this.program);
            this.gl.deleteBuffer(this.positionBuffer);
            this.gl.deleteBuffer(this.texCoordBuffer);
            this.gl.deleteTexture(this.texture);
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
            this.gl.bindTexture(this.gl.TEXTURE_2D, null);
            this.gl.useProgram(null);
            this.gl.disable(this.gl.BLEND);
            this.gl.disable(this.gl.DEPTH_TEST);
            this.gl.disable(this.gl.CULL_FACE);
            this.gl.viewport(0, 0, this.gl.canvas.width, this.gl.canvas.height);
            this.gl.clearColor(0, 0, 0, 0);
            this.gl.clear(this.gl.COLOR_BUFFER_BIT);
        }
    }

    gameGlobal.GodotLoader = GodotLoader;
})(typeof globalThis !== "undefined" ? globalThis : this);
