package com.emllabs.droid99;

import java.io.IOException;
import java.io.InputStream;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Toast;
import android.view.MotionEvent;

public class And99Activity extends Activity
{
	private GLSurfaceView mGLView;
	
	protected boolean turbo = true;
	protected boolean replay = false;
	protected boolean enableDebug = true;
	protected String cartridge = "DEMO";

	TI99Simulator simulator;
	
	static final int SETTINGS_RESULT = 22;
	
	protected enum JoystickButton {
		FIRE(0), LEFT(1), RIGHT(2), DOWN(3), UP(4);
		public int bit;
		private JoystickButton(int c) { bit = c; }
	}
	
	@Override
	public void onPause()
	{
		simulator.breakCPU();
		super.onPause();
	}
	
	@Override
	public void onResume()
	{
		simulator.continueCPU();
		super.onResume();
	}
	
	
	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        int rotation = getResources().getConfiguration().orientation;
        And99Application app = (And99Application)getApplication();
    	simulator = app.simulator;

        if (rotation == Configuration.ORIENTATION_PORTRAIT)
        {
        	mGLView = new MyGLSurfaceView(this, simulator);

        	LinearLayout ll = new LinearLayout(this);
        	ll.setOrientation(LinearLayout.VERTICAL);
        	ll.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        
        	LinearLayout buttonRowLayout = new LinearLayout(this);
        	buttonRowLayout.setOrientation(LinearLayout.HORIZONTAL);
 
        	Button saveButton = new Button(this);
        	saveButton.setText("Save");
        	saveButton.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 1.0f));
        	saveButton.setOnClickListener(new OnClickListener() {
        		public void onClick(View v) {
        			simulator.breakCPU();
        			// XXX:  need to wait for the simulator to finish
        			try {
        				Thread.sleep(10);
        			} catch (InterruptedException e) {
        				// TODO Auto-generated catch block
        			}
        			simulator.SaveImage();
        			simulator.continueCPU();
        			CharSequence msg = "Image saved.";
        			Toast toast = Toast.makeText(getApplicationContext(), msg, Toast.LENGTH_SHORT);
        			toast.show();
        		}
        	});
        	Button loadButton = new Button(this);
        	loadButton.setText("Load");
        	loadButton.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 1.0f));
        	loadButton.setOnClickListener(new OnClickListener() {
        		public void onClick(View v) {
        			simulator.breakCPU();
        			// 	XXX:  need to wait for simulator to finish
        			try {
        				Thread.sleep(10);
        			} catch (InterruptedException e) {
        				//
        			}
        			simulator.LoadImage();
        			simulator.continueCPU();
        			CharSequence msg = "Image restored.";
        			Toast toast = Toast.makeText(getApplicationContext(), msg, Toast.LENGTH_SHORT);
        			toast.show();
        		}
        	});
        	buttonRowLayout.addView(saveButton);
        	buttonRowLayout.addView(loadButton);
        
        	Button settingsButton = new Button(this);
        	settingsButton.setText("Settings");
        	settingsButton.setOnClickListener(new OnClickListener() {
        		public void onClick(View v)
        		{
        			Intent i = new Intent(getBaseContext(), SettingsActivity.class);
        			i.putExtra("com.emllabs.droid99.CONFIG_TURBO", turbo);
        			i.putExtra("com.emllabs.droid99.CONFIG_REPLAY", replay);
        			i.putExtra("com.emllabs.droid99.CONFIG_DEBUG", enableDebug);
        			i.putExtra("com.emllabs.droid99.CONFIG_CARTRIDGE", cartridge);
        			startActivityForResult(i, SETTINGS_RESULT);
        		}
        	});
        	settingsButton.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 1.0f));
        	buttonRowLayout.addView(settingsButton);
        
        	mGLView.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 1.0f));

        	ll.addView(mGLView);
        	buttonRowLayout.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 0.0f));
        	ll.addView(buttonRowLayout);
        	View joystick = getLayoutInflater().inflate(R.layout.joy, ll, false);
        	Button rightJoyBtn = (Button)joystick.findViewById(R.id.rightJoyBtn);
        	rightJoyBtn.setOnTouchListener(new View.OnTouchListener() {
        		public boolean onTouch(View v, MotionEvent evt) {
        			return joystickHandler(evt, JoystickButton.RIGHT);
        		}
        	});
        	Button leftJoyBtn = (Button)joystick.findViewById(R.id.leftJoyBtn);
        	leftJoyBtn.setOnTouchListener(new View.OnTouchListener() {
        		public boolean onTouch(View v, MotionEvent evt) {
        			return joystickHandler(evt, JoystickButton.LEFT);
        		}
        	});
        	Button upJoyBtn = (Button)joystick.findViewById(R.id.upJoyBtn);
        	upJoyBtn.setOnTouchListener(new View.OnTouchListener() {
        		public boolean onTouch(View v, MotionEvent evt) {
        			return joystickHandler(evt, JoystickButton.UP);
        		}
        	});
        	Button downJoyBtn = (Button)joystick.findViewById(R.id.downJoyBtn);
        	downJoyBtn.setOnTouchListener(new View.OnTouchListener() {
        		public boolean onTouch(View v, MotionEvent evt) {
        			return joystickHandler(evt, JoystickButton.DOWN);
        		}
        	});
        	Button fireJoyBtn = (Button)joystick.findViewById(R.id.fireJoyBtn);
        	fireJoyBtn.setOnTouchListener(new View.OnTouchListener() {
        		public boolean onTouch(View v, MotionEvent evt) {
        			return joystickHandler(evt, JoystickButton.FIRE);
        		}
        	});
        
        	ll.addView(joystick);
        	setContentView(ll);
        }
        else {
        	// Landscape
            mGLView = new MyGLSurfaceView(this, app.simulator);
        	setContentView(mGLView);
        }
    }

    
    public boolean joystickHandler(MotionEvent e, JoystickButton button)
    {
    	int state;
    	if (e.getActionMasked() == MotionEvent.ACTION_DOWN)
    		state = 1;
    	else if (e.getActionMasked() == MotionEvent.ACTION_UP)
    		state = 0;
    	else
    		return false;
    	simulator.TIKeyboardChange(button.bit, 6, state);
    	return true;
    }
    
    
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
    	if (requestCode == SETTINGS_RESULT) {
    		if (resultCode == RESULT_OK) {
    			String cartGROM = data.getStringExtra("com.emllabs.droid99.CONFIG_CARTRIDGE_GROM");
    			String cartConsole = data.getStringExtra("com.emllabs.droid99.CONFIG_CARTRIDGE_CONSOLE");
    			turbo = data.getBooleanExtra("com.emllabs.droid99.CONFIG_TURBO", true);
    			replay = data.getBooleanExtra("com.emllabs.droid99.CONFIG_REPLAY", false);
    			enableDebug = data.getBooleanExtra("com.emllabs.droid99.CONFIG_DEBUG", true);
    			String DSKImgFile = data.getStringExtra("com.emllabs.droid99.CONFIG_DSKIMG");
    			
    	        byte[] cbuffer = new byte[40960];
    	        byte[] dbuffer = null;
    	        int nread;

    	        try {
    	            AssetManager assetMgr = getAssets();
    	            InputStream ios = assetMgr.open(cartGROM);
    	            nread = ios.read(cbuffer);
    	            Log.i("And99", "Read " + nread + " bytes from cartridge: " + cartGROM);
    	            if (cartConsole != "") {
    	            	dbuffer = new byte[8192];
    	            	ios = assetMgr.open(cartConsole);
    	            	nread = ios.read(dbuffer);
    	            	Log.i("And99", "Read " + nread + " bytes from cartridge: " + cartConsole);
    	            }
    	        } catch (IOException i) {
    	        	Log.e("And99", "IO exception reading asset file: " + cartGROM);
    	        }
    			
    			simulator.reset();
    			simulator.SetCartridge(cbuffer, dbuffer);
    			
    			if (DSKImgFile != null)
    				simulator.LoadDSKImage(DSKImgFile);
    		}
    	}	
    }
}
