package com.emllabs.droid99;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.opengl.Matrix;
import android.util.Log;

/*
 * OpenGL rendering.  Handles rendering the view in both portrait and landscape
 * modes.  The view consists of three pieces - the background/border fill,
 * the VDP screen overlay and the keyboard image.
 */
public class MyRenderer implements GLSurfaceView.Renderer
{
	private FloatBuffer vertexBuffer, vertexBuffer2, bgVertexBuffer;
	private ShortBuffer vertexElements;
	private FloatBuffer texCoordBuffer, texCoordBuffer2;
	private int[] glTex = new int[2];
	private byte[] pixels = new byte[256*256*3];
	private long time = System.currentTimeMillis();
	private int fps = 0;
	public boolean fullScreenMode = false;
	private float[] borderGLColor = new float[3];
	private float texelOffset = 0.5f / 256.0f;
	
	private final String vertexShaderCode =
		"uniform mat4 projection;" +
		"attribute vec4 vPosition;" +
		"attribute vec2 vTexCoord;" +
		"varying mediump vec2 fTexCoord;" +
		"void main() {" +
		"  gl_Position = projection * vPosition;" +
		"  fTexCoord = vTexCoord;" +
		"}";

	// simply texture map the VDP image (pixels)
	private final String fragmentShaderCode =
		"precision mediump float;" +
		"varying vec2 fTexCoord;" +
		"uniform sampler2D texture;" +
		"void main() {" +
		"  gl_FragColor = texture2D(texture, fTexCoord);" +
		"}";

	// the border is a solid color (we reuse the vertex shader even though
	// we don't need the texturing
	private final String bgFragmentShaderCode =
		"precision mediump float;" +
		"uniform vec4 bgColor;" +
		"void main() { gl_FragColor = bgColor; }";
	private int vertexShader;
	private int fragmentShader;
	private int bgFragmentShader;
	private int mprogram, bgProgram;
	private float[] projectionMatrix;
	private Context context;
	private TI99Simulator simulator;
	
	public MyRenderer(Context c, TI99Simulator sim)
	{
		context = c;
		simulator = sim;
	}
	
	protected void render(byte[] pixels, byte[] registers)
	{
		int palette[] = {0x0, 0x0, 0x21C842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d, 0x42ebf5, 0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba, 0xcccccc, 0xffffff};
		int i, j, l, k;
		
		if (simulator.isDebugPaused())
			return;
		
		ByteBuffer vdpMemory = simulator.vdpMemory;
		if (!vdpMemory.hasArray()) {
			Log.e("And99", "no array");
			return;
		}
		byte[] vdpBytes = vdpMemory.array();
		
		int bitmap = (registers[0] & 0x2);
		
		// Sprite processing
		class SpriteData {
			int x, y, color;
			byte[] pattern;
		};
		
		int spriteSize = ((registers[1] >> 1) & 0x1);
		int bytesPerPattern = (spriteSize == 0) ? 8 : 32;
		int spriteMag = (registers[1] & 0x1);
		int spriteAttributes = ((registers[5] & 0x7f) * 0x80);
		int spritePatterns = ((registers[6] & 0x7) * 0x800);
		byte[] spriteTable = new byte[4*32];
		System.arraycopy(vdpBytes, spriteAttributes, spriteTable, 0, 4*32);
		int nSprites = 0;
		SpriteData[] spriteList = new SpriteData[32];
		for (i=0; i < 32; i++) {
			// off-screen. datasheet specifies == D0 to break processing
			if ((spriteTable[i*4] & 0xff) >= 0xD0)
				break;
			// transparent
			if ((spriteTable[i*4+3] & 0xf) == 0)
				continue;
			
			int s = nSprites++;
			spriteList[s] = new SpriteData();
			spriteList[s].x = spriteTable[i*4+1] & 0xff;
			spriteList[s].y = (spriteTable[i*4] & 0xff) + 1;
			spriteList[s].color = palette[spriteTable[i*4+3] & 0xf];
			spriteList[s].pattern = new byte[bytesPerPattern];
			
			int baseAddr = spritePatterns + (spriteTable[i*4+2]&0xff) * 8;
			for (int ii=0; ii<bytesPerPattern; ii++)
				spriteList[s].pattern[ii] = vdpBytes[baseAddr + ii];
		}
		
		int patAddr = ((registers[2] & 0xf) * 0x400);
		int background = palette[(registers[7] & 0xf)];
		borderGLColor[0] = ((background >> 16) & 0xFF) / 256.0f;
		borderGLColor[1] = ((background >> 8) & 0xFF) / 256.0f;
		borderGLColor[2] = (background & 0xFF) / 256.0f;
		
		
		byte[] buffer = new byte[8];
		if (bitmap == 0) {
			int colorAddr = ((registers[3] & 0xff) * 0x40);
			int charAddr = ((registers[4] & 0x7) * 0x800);
			byte[] colorsMap = new byte[32];
			System.arraycopy(vdpBytes, colorAddr, colorsMap, 0, 32);
			for (i=0; i < 24; i++) {
				for (j=0; j < 32; j++) {
					byte ch = (byte) (vdpBytes[patAddr + i*32 + j] & 0xff);
					for (l=0; l < 8; l++)
						buffer[l] = vdpBytes[charAddr + ((ch&0xff) * 8) + l];
					byte ticolor = colorsMap[(ch & 0xff) >> 3];
					int fg = ((ticolor&0xff) > 15) ? palette[((ticolor & 0xff)>> 4)] : background;
					int bg = (((ticolor & 0xf) != 0) ? palette[(ticolor & 0xf)] : background);
				
					for (l = 0; l < 8; l++) {
						for (k = 0; k < 8; k++) {
							int c;
							if (((buffer[l] >> (7-k)) & 0x1) == 1)
								c = fg;
							else
								c = bg;
							int index = (i*8+l)*256 + j*8+k;
							pixels[index*3] = (byte) ((c >> 16) & 0xFF);
							pixels[index*3+1] = (byte)((c >> 8) & 0xFF);
							pixels[index*3+2] = (byte)((c & 0xFF));
						}
					}
				}
			}
		}
		else {
			// bitmap mode
			int colorAddr = ((registers[3] & 0x80) << 6);
			int charAddr = ((registers[4] & 0x4) << 6);
			byte[] color = new byte[8];
			for (i=0; i < 24; i++) {
				int section = i >> 3;
				for (j=0; j < 32; j++) {
					byte ch = (byte)(vdpBytes[patAddr + i*32 + j] & 0xff);
					for (l=0; l < 8; l++) {
						buffer[l] = vdpBytes[charAddr + ((ch&0xff) * 8) + l + (section * 0x800)];
						color[l] = vdpBytes[colorAddr + ((ch&0xff) * 8) + l + (section * 0x800)];					
					}
				
					for (l = 0; l < 8; l++) {
						byte ticolor = color[l];
						int fg = ((ticolor&0xff) > 15) ? palette[((ticolor & 0xff)>> 4)] : background;
						int bg = (((ticolor & 0xf) != 0) ? palette[(ticolor & 0xf)] : background);

						for (k = 0; k < 8; k++) {
							int c;
							if (((buffer[l] >> (7-k)) & 0x1) == 1)
								c = fg;
							else
								c = bg;
							int index = (i*8+l)*256 + j*8+k;
							pixels[index*3] = (byte) ((c >> 16) & 0xFF);
							pixels[index*3+1] = (byte)((c >> 8) & 0xFF);
							pixels[index*3+2] = (byte)((c & 0xFF));
						}
					}
				}
			}
		}
		
		for (int s=nSprites-1; s >= 0; s--) {
			// just doing mag = 0; need to expand it
			int color = spriteList[s].color;
			renderBlock(spriteList[s].x, spriteList[s].y, spriteList[s].pattern, 0, color, spriteMag);
			if (spriteSize == 1) {
				// render 2x2 block
				int shift = 8 + spriteMag * 8;
				renderBlock(spriteList[s].x, spriteList[s].y+shift, spriteList[s].pattern, 8, color, spriteMag);
				renderBlock(spriteList[s].x+shift, spriteList[s].y, spriteList[s].pattern, 16, color, spriteMag);
				renderBlock(spriteList[s].x+shift, spriteList[s].y+shift, spriteList[s].pattern, 24, color, spriteMag);
			}
		}
	}


	protected void renderBlock(int x, int y, byte[] pattern, int offset, int color, int mag)
	{
		if (mag == 1) {
			for (int yy=y; yy < y+16; yy++) {
				int index = yy * 256 + x;
				byte b = pattern[(yy-y)/2 + offset];
				for (int xx=index; xx < index + 16; xx++) {
					int bit = (xx - index) / 2;
					if (((b >> (7-bit)) & 0x1) == 0x1) {
						pixels[xx*3] = (byte)((color >> 16) & 0xFF);
						pixels[xx*3+1] = (byte)((color >> 8) & 0xFF);
						pixels[xx*3+2] = (byte)(color & 0xFF);
					}
				}
			}
			return;
		}
		
		for (int yy=y; yy < y+8; yy++) {
			int index = yy * 256 + x;
			byte b = pattern[yy-y + offset];
			for (int xx=index; xx < index + 8; xx++) {
				int bit = xx - index;
				if (((b >> (7-bit)) & 0x1) == 0x1) {
					pixels[xx*3] = (byte)((color >> 16) & 0xFF);
					pixels[xx*3+1] = (byte)((color >> 8) & 0xFF);
					pixels[xx*3+2] = (byte)(color & 0xFF);
				}
			}
		}
	}

	
	void init()
	{
		projectionMatrix = new float[16];
		Matrix.orthoM(projectionMatrix, 0, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
		float squareCoords[] = {-1.0f, 0.75f, 0.0f,
			-1.0f, -0.75f, 0.0f,
			1.0f, -0.75f, 0.0f,
			1.0f, 0.75f, 0.0f
		};
//		float texelOffset = 0.5f/256.0f;		// texels are sampled in the center and for GL_LINEAR
												// we need to be correct (still seems to get chopped...)
		float texCoords[] = {0.0f, 0.0f, 0.0f-texelOffset, 0.75f-texelOffset, 1.0f, 0.75f-texelOffset, 1.0f, 0.0f-texelOffset };
		float texCoords2[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
		short drawOrder[] = { 0, 1, 2, 0, 2, 3 };

		GLES20.glGenTextures(2, glTex, 0);
		for (int i=0; i < 2; i++) {
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTex[i]);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
		}
		
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTex[1]);
		Bitmap keyboardBmp = BitmapFactory.decodeResource(context.getResources(), R.drawable.keyboard_front);
		GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, keyboardBmp, 0);
		
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTex[0]);
		byte[] registers = simulator.GetVDPRegisters();
		render(pixels, registers);
		GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGB, 256, 256, 0, GLES20.GL_RGB,
				GLES20.GL_UNSIGNED_BYTE, ByteBuffer.wrap(pixels));
		
		ByteBuffer bb = ByteBuffer.allocateDirect(squareCoords.length * 4);
		bb.order(ByteOrder.nativeOrder());
		vertexBuffer = bb.asFloatBuffer();
		vertexBuffer.put(squareCoords);
		vertexBuffer.position(0);
		
		ByteBuffer bbb = ByteBuffer.allocateDirect(squareCoords.length * 4);
		bbb.order(ByteOrder.nativeOrder());
		vertexBuffer2 = bbb.asFloatBuffer();
		vertexBuffer2.put(squareCoords);
		vertexBuffer2.position(0);
		
		// initialize bgVertexBuffer
		ByteBuffer dlb = ByteBuffer.allocateDirect(drawOrder.length * 2);
		dlb.order(ByteOrder.nativeOrder());
		vertexElements = dlb.asShortBuffer();
		vertexElements.put(drawOrder);
		vertexElements.position(0);

		ByteBuffer tb = ByteBuffer.allocateDirect(texCoords.length * 4);
		tb.order(ByteOrder.nativeOrder());
		texCoordBuffer = tb.asFloatBuffer();
		texCoordBuffer.put(texCoords);
		texCoordBuffer.position(0);
		
		ByteBuffer tbb = ByteBuffer.allocateDirect(texCoords2.length * 4);
		tbb.order(ByteOrder.nativeOrder());
		texCoordBuffer2 = tbb.asFloatBuffer();
		texCoordBuffer2.put(texCoords2);
		texCoordBuffer2.position(0);
		
		vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexShaderCode);
		fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentShaderCode);
		bgFragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, bgFragmentShaderCode);

		mprogram = GLES20.glCreateProgram();
		GLES20.glAttachShader(mprogram, vertexShader);
		GLES20.glAttachShader(mprogram, fragmentShader);
		GLES20.glLinkProgram(mprogram);

		bgProgram = GLES20.glCreateProgram();
		GLES20.glAttachShader(bgProgram,  vertexShader);
		GLES20.glAttachShader(bgProgram, bgFragmentShader);
		GLES20.glLinkProgram(bgProgram);
	}
	
	public static int loadShader(int type, String code)
	{
		int shader = GLES20.glCreateShader(type);
		GLES20.glShaderSource(shader, code);
		GLES20.glCompileShader(shader);
		
		return shader;
	}
	
	public void onSurfaceCreated(GL10 unused, EGLConfig config)
	{
		GLES20.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		init();
	}
	
	public void onDrawFrame(GL10 unused)
	{
		GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
		
		/*
		 * Render the background/border color in portrait mode
		 */
		GLES20.glUseProgram(bgProgram);
		int mProjectionHandle = GLES20.glGetUniformLocation(bgProgram, "projection");
		int mBgColorHandle = GLES20.glGetUniformLocation(bgProgram, "bgColor");
		int mPositionHandle = GLES20.glGetAttribLocation(bgProgram, "vPosition");
		GLES20.glUniformMatrix4fv(mProjectionHandle, 1, false, projectionMatrix, 0);
		GLES20.glEnableVertexAttribArray(mPositionHandle);
		GLES20.glUniform4f(mBgColorHandle, borderGLColor[0], borderGLColor[1], borderGLColor[2], 1.0f);
		GLES20.glVertexAttribPointer(mPositionHandle, 3, GLES20.GL_FLOAT, false, 0, bgVertexBuffer);
		if (!fullScreenMode) {
			// draw the background
			GLES20.glDrawElements(GLES20.GL_TRIANGLES, 6, GLES20.GL_UNSIGNED_SHORT, vertexElements);
		}

		/*
		 * Render the main VDP screen
		 */
		GLES20.glUseProgram(mprogram);
		mProjectionHandle = GLES20.glGetUniformLocation(mprogram, "projection");
		mPositionHandle = GLES20.glGetAttribLocation(mprogram, "vPosition");
		int mTextureHandle = GLES20.glGetAttribLocation(mprogram, "vTexCoord");
		int tex = GLES20.glGetUniformLocation(mprogram, "texture");
		GLES20.glUniformMatrix4fv(mProjectionHandle, 1, false, projectionMatrix, 0);
		GLES20.glEnableVertexAttribArray(mPositionHandle);
		GLES20.glEnableVertexAttribArray(mTextureHandle);
		GLES20.glVertexAttribPointer(mPositionHandle, 3, GLES20.GL_FLOAT, false, 0, vertexBuffer);
		GLES20.glVertexAttribPointer(mTextureHandle, 2, GLES20.GL_FLOAT, false, 0, texCoordBuffer);

		GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
		GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTex[0]);
		byte[] registers = simulator.GetVDPRegisters();
//		if (registers[9] == 1) {
			render(pixels, registers);
//		}
		GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, 256, 192, GLES20.GL_RGB, GLES20.GL_UNSIGNED_BYTE, ByteBuffer.wrap(pixels));
//			simulator.ClearVDPDirty();
//		}
		GLES20.glUniform1i(tex, 0);
		
		// draw the VDP window
		GLES20.glDrawElements(GLES20.GL_TRIANGLES, 6, GLES20.GL_UNSIGNED_SHORT, vertexElements);

		// draw the keyboard if not in full-screen mode
		if (!fullScreenMode) {
			GLES20.glVertexAttribPointer(mPositionHandle, 3, GLES20.GL_FLOAT, false, 0, vertexBuffer2);
			GLES20.glVertexAttribPointer(mTextureHandle, 2, GLES20.GL_FLOAT, false, 0, texCoordBuffer2);
			GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, glTex[1]);
			GLES20.glUniform1i(tex, 0);
			GLES20.glDrawElements(GLES20.GL_TRIANGLES, 6, GLES20.GL_UNSIGNED_SHORT, vertexElements);
		}
		GLES20.glDisableVertexAttribArray(mPositionHandle);
		GLES20.glDisableVertexAttribArray(mTextureHandle);
		
		if (System.currentTimeMillis() - time >= 1000) {
//			And99Activity.statusBar.setText("FPS: " + fps);
			Log.i("And99", "FPS = " + fps);
			fps = 0;
			time = System.currentTimeMillis();
		}
		fps++;

		if ((registers[1] & 0x40) == 0x40 && simulator.replay == false)
			simulator.DoVDPInterrupt();
	}
	
	public void onSurfaceChanged(GL10 unused, int width, int height)
	{
		float squareCoords[], bgCoords[] = { }, aspect;
		
		// Are we portrait or landscape?  In landscape mode, we turn off the keyboard
		// and render full screen
		fullScreenMode = false;
		if (width > height)
			fullScreenMode = true;
		
		GLES20.glViewport(0, 0, width, height);

		if (!fullScreenMode) {
			aspect = (float)height / (float)width;
			Matrix.orthoM(projectionMatrix, 0, -1.0f, 1.0f, -aspect, aspect, -1.0f, 1.0f);
		
			float portraitCoords[] = {-0.9142857f, aspect-0.042857f, 0.0f,
					-0.9142857f, aspect-1.414287f, 0.0f,
					0.9142857f, aspect-1.414287f, 0.0f,
					0.9142857f, aspect-0.042857f, 0.0f
				};

			float[] tmp = {-1.0f, aspect, 0.0f,
					-1.0f, aspect-1.5f, 0.0f,
					1.0f, aspect-1.5f, 0.0f,
					1.0f, aspect, 0.0f
				};

			squareCoords = portraitCoords;
			bgCoords = tmp;
		}
		else {
			aspect = (float)width / (float)height;
			Matrix.orthoM(projectionMatrix,  0, -aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
			float landscapeCoords[] = {-1.333f, 1.0f, 0.0f,
				-1.333f, -1.0f, 0.0f,
				1.333f, -1.0f, 0.0f,
				1.333f, 1.0f, 0.0f
			};
			squareCoords = landscapeCoords;
		}
		
		float keyCoords[] = {-1.0f, aspect-1.5f, 0.0f,
			-1.0f, aspect-2.398f, 0.0f,
			1.0f, aspect-2.398f, 0.0f,
			1.0f, aspect-1.5f, 0.0f
		};
		
		ByteBuffer bb = ByteBuffer.allocateDirect(squareCoords.length * 4);
		bb.order(ByteOrder.nativeOrder());
		vertexBuffer = bb.asFloatBuffer();
		vertexBuffer.put(squareCoords);
		vertexBuffer.position(0);
		
		ByteBuffer bgbb = ByteBuffer.allocateDirect(bgCoords.length * 4);
		bgbb.order(ByteOrder.nativeOrder());
		bgVertexBuffer = bgbb.asFloatBuffer();
		bgVertexBuffer.put(bgCoords);
		bgVertexBuffer.position(0);

		ByteBuffer bbb = ByteBuffer.allocateDirect(keyCoords.length * 4);
		bbb.order(ByteOrder.nativeOrder());
		vertexBuffer2 = bbb.asFloatBuffer();
		vertexBuffer2.put(keyCoords);
		vertexBuffer2.position(0);
	}
}
