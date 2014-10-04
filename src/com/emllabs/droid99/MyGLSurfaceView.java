package com.emllabs.droid99;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.util.Log;

public class MyGLSurfaceView extends GLSurfaceView
{
	MyRenderer renderer;
	TI99Simulator simulator;
	protected boolean shiftLatch = false;
	protected boolean fcntLatch = false;
	protected boolean ctlLatch = false;
	protected int AndroidCRURows[] = new int[256];
	protected int AndroidCRUCols[] = new int[256];
	
	public MyGLSurfaceView(Context context, TI99Simulator sim) {
		super(context);
		setEGLContextClientVersion(2);
		simulator = sim;
		
		for (int i=0; i < 256; i++) {
			AndroidCRURows[i] = -1;
			AndroidCRUCols[i] = -1;
		}
		
		AndroidCRURows[KeyEvent.KEYCODE_0] = 3;
		AndroidCRURows[KeyEvent.KEYCODE_1] = 4;
		AndroidCRURows[KeyEvent.KEYCODE_2] = 4;
		AndroidCRURows[KeyEvent.KEYCODE_3] = 4; 
		AndroidCRURows[KeyEvent.KEYCODE_4] = 4; 
		AndroidCRURows[KeyEvent.KEYCODE_5] = 4; 
		AndroidCRURows[KeyEvent.KEYCODE_6] = 3;
		AndroidCRURows[KeyEvent.KEYCODE_7] = 3;
		AndroidCRURows[KeyEvent.KEYCODE_8] = 3;
		AndroidCRURows[KeyEvent.KEYCODE_9] = 3;
		AndroidCRURows[KeyEvent.KEYCODE_A] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_B] = 7;
		AndroidCRURows[KeyEvent.KEYCODE_C] = 7;
		AndroidCRURows[KeyEvent.KEYCODE_D] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_E] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_F] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_G] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_H] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_I] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_J] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_K] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_L] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_M] = 0;
		AndroidCRURows[KeyEvent.KEYCODE_N] = 0;
		AndroidCRURows[KeyEvent.KEYCODE_O] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_P] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_Q] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_R] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_S] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_T] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_U] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_V] = 7;
		AndroidCRURows[KeyEvent.KEYCODE_W] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_X] = 7;
		AndroidCRURows[KeyEvent.KEYCODE_Y] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_Z] = 7;
		AndroidCRURows[KeyEvent.KEYCODE_EQUALS] = 0;
		AndroidCRURows[KeyEvent.KEYCODE_SPACE] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_ENTER] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_PERIOD] = 0;
		AndroidCRURows[KeyEvent.KEYCODE_COMMA] = 0;
		AndroidCRURows[KeyEvent.KEYCODE_SLASH] = 0;
		AndroidCRURows[KeyEvent.KEYCODE_SEMICOLON] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_SHIFT_LEFT] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_SHIFT_RIGHT] = 5;
		AndroidCRURows[KeyEvent.KEYCODE_CTRL_LEFT] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_CTRL_RIGHT] = 6;
		AndroidCRURows[KeyEvent.KEYCODE_BACK] = 4;				// overload the function key
		AndroidCRURows[KeyEvent.KEYCODE_DPAD_UP] = 4;
		AndroidCRURows[KeyEvent.KEYCODE_DPAD_DOWN] = 3;
		AndroidCRURows[KeyEvent.KEYCODE_DPAD_RIGHT] = 2;
		AndroidCRURows[KeyEvent.KEYCODE_DPAD_LEFT] = 1;
		AndroidCRURows[KeyEvent.KEYCODE_TAB] = 0; 

		AndroidCRUCols[KeyEvent.KEYCODE_0] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_1] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_2] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_3] = 2; 
		AndroidCRUCols[KeyEvent.KEYCODE_4] = 3; 
		AndroidCRUCols[KeyEvent.KEYCODE_5] = 4; 
		AndroidCRUCols[KeyEvent.KEYCODE_6] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_7] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_8] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_9] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_A] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_B] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_C] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_D] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_E] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_F] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_G] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_H] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_I] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_J] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_K] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_L] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_M] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_N] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_O] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_P] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_Q] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_R] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_S] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_T] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_U] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_V] = 3;
		AndroidCRUCols[KeyEvent.KEYCODE_W] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_X] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_Y] = 4;
		AndroidCRUCols[KeyEvent.KEYCODE_Z] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_EQUALS] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_SPACE] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_ENTER] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_PERIOD] = 1;
		AndroidCRUCols[KeyEvent.KEYCODE_COMMA] = 2;
		AndroidCRUCols[KeyEvent.KEYCODE_SLASH] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_SEMICOLON] = 5;
		AndroidCRUCols[KeyEvent.KEYCODE_SHIFT_LEFT] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_SHIFT_RIGHT] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_CTRL_LEFT] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_CTRL_RIGHT] = 0;
		AndroidCRUCols[KeyEvent.KEYCODE_BACK] = 0;				// overload the function key
		AndroidCRUCols[KeyEvent.KEYCODE_DPAD_UP] = 6;
		AndroidCRUCols[KeyEvent.KEYCODE_DPAD_DOWN] = 6;
		AndroidCRUCols[KeyEvent.KEYCODE_DPAD_RIGHT] = 6;
		AndroidCRUCols[KeyEvent.KEYCODE_DPAD_LEFT] = 6;
		AndroidCRUCols[KeyEvent.KEYCODE_TAB] = 6; 
		
		renderer = new MyRenderer(context, simulator);
		setRenderer(renderer);
		
		setFocusable(true);
		requestFocus();
	}
	
	public boolean onTouchEvent(final MotionEvent event) {
		// no on-screen keyboard in full screen mode
		if (renderer.fullScreenMode)
			return true;
		int i, j = 0;
		float rows[] = { 0.0325f, 0.2087f, 0.3988f, 0.5843f, 0.7606f, 0.955f };
		float cols[][] = { 
			{ 0.0125f, 0.0979f, 0.1833f, 0.2646f, 0.3458f, 0.4313f, 0.5146f, 0.5938f, 0.6792f, 0.7646f, 0.8458f, 0.9255f },
			{ 0.0563f, 0.1375f, 0.2208f, 0.3042f, 0.3854f, 0.4708f, 0.5542f, 0.6375f, 0.7208f, 0.8021f, 0.8877f, 0.9708f },
			{ 0.0771f, 0.1542f, 0.2417f, 0.3229f, 0.4063f, 0.4917f, 0.5729f, 0.6583f, 0.7417f, 0.8250f, 0.9104f, 0.9917f },
			{ 0.0333f, 0.1146f, 0.1979f, 0.2813f, 0.3646f, 0.4479f, 0.5333f, 0.6146f, 0.6979f, 0.7833f, 0.8667f, 0.9896f },
			{ 0.0313f, 0.1149f, 0.1958f, 0.8667f, 0.9542f, 0.9542f, 0.9542f, 0.9542f, 0.9542f, 0.9542f, 0.9542f, 0.9542f },
		};
		int CRURows[][] = {
			{ 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 0 },
			{ 6, 6, 6, 6, 6, 2, 2, 2, 2, 2, 0 },
			{ 5, 5, 5, 5, 5, 1, 1, 1, 1, 1, 2 },
			{ 5, 7, 7, 7, 7, 7, 0, 0, 0, 0, 5 },
			{ 4, 6, 1, 4, 4, 4, 4, 4, 4, 4, 4 },
		};
		int CRUCols[][] = {
			{ 5, 1, 2, 3, 4, 4, 3, 2, 1, 5, 0 },
			{ 5, 1, 2, 3, 4, 4, 3, 2, 1, 5, 5 },
			{ 5, 1, 2, 3, 4, 4, 3, 2, 1, 5, 0 },
			{ 0, 5, 1, 2, 3, 4, 4, 3, 2, 1, 0 },
			{ 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		};
//		if (event.getActionMasked() != MotionEvent.ACTION_DOWN
//				&& event.getActionMasked() != MotionEvent.ACTION_UP)
//			return false;
		int state = (event.getActionMasked() == MotionEvent.ACTION_DOWN) ? 1 : 0;
		float keyboardTop = getWidth() / 256.0f * 192.0f;
		float keyboardBottom = keyboardTop + (getWidth() / 512.0f * 230.0f);
		float x = event.getX() / getWidth();
		float yOffset = (event.getY() - keyboardTop) / (keyboardBottom - keyboardTop);
		if (yOffset < rows[0] || yOffset > rows[5])
			return true;
		
		// search for key
		for (i=1; i < 5; i++)
			if (yOffset < rows[i]) break;
		i--;
		if (x < cols[i][0] || x > cols[i][11])
			return true;
		for (j=1; j < 11; j++)
			if (x < cols[i][j]) break;
		
		if ((j-1 == 0 || j-1 == 10) && i == 3) {
			// shift key pressed or released
			if (state == 1)
				shiftLatch = true;
			return true;
		}
		if (j-1 == 1 && i == 4) {
			if (state == 1)
				ctlLatch = true;
			return true;
		}
		if (j-1 == 3 && i == 4) {
			if (state == 1)
				fcntLatch = true;
			return true;
		}

		// convert to CRU
		if (shiftLatch) {
			simulator.TIKeyboardChange(5, 0, state);
			if (state == 0)
				shiftLatch = false;
		}
		if (fcntLatch) {
			if (state == 1) {
				simulator.TIKeyboardChange(4, 0, state);
				simulator.TIKeyboardChange(CRURows[i][j-1], CRUCols[i][j-1], state);
				return true;
			}
			fcntLatch = false;
			simulator.TIKeyboardChange(CRURows[i][j-1], CRUCols[i][j-1], state);
			simulator.TIKeyboardChange(4, 0, state);
			return true;
		}
		if (ctlLatch) {
			simulator.TIKeyboardChange(6, 0, state);
			if (state == 0)
				ctlLatch = false;
		}
		
		simulator.TIKeyboardChange(CRURows[i][j-1], CRUCols[i][j-1], state);
		return true;
	}
	
	public boolean onKeyDown(int keycode, KeyEvent event)
	{
		// check keycode in range
		if (keycode > 255 || AndroidCRURows[keycode] < 0 || AndroidCRUCols[keycode] < 0) {
			Log.i("And99", "onKeyDown invalid key: " + keycode);
			return true;
		}
		simulator.TIKeyboardChange(AndroidCRURows[keycode], AndroidCRUCols[keycode], 1);
		return true;
	}

	public boolean onKeyUp(int keycode, KeyEvent event)
	{
		// check keycode in range
		if (keycode > 255 || AndroidCRURows[keycode] < 0 || AndroidCRUCols[keycode] < 0) {
			Log.i("And99", "onKeyUp invalid key: " + keycode);
			return true;
		}
		simulator.TIKeyboardChange(AndroidCRURows[keycode], AndroidCRUCols[keycode], 0);
		return true;
	}

}
