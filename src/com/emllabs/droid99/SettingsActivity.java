package com.emllabs.droid99;

import android.app.Activity;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.TextView;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.content.pm.PackageManager;

public class SettingsActivity extends Activity
{
	protected boolean turbo;
	protected boolean replay;
	protected boolean debug;
	protected String cartridge;
	protected Spinner cartSpinner;
	protected String ROMPickFile = null;
	protected String DSKPickFile = null;
	
//	protected CheckBox turboChk;
//  protected CheckBox replayChk;
	protected CheckBox debugChk;
	
	protected String[][] cartridgeData = {
		{ "Demonstration",		"DEMOG.BIN",		"" },
		{ "A-MAZE-ING",			"AMAZEG.BIN",		"" },
		{ "Car Wars",			"CarWarsG.Bin",		"" },
		{ "Diagnostics",		"DiagnosG.Bin",		"" },
		{ "Tunnels of Doom",	"TunDoomG.Bin",		"" },
		{ "Blackjack & Poker",	"BlakjakG.Bin",		"" },
		{ "Football",			"FootbalG.Bin",		"" },
		{ "Hustle",				"HustleG.Bin",		"" },
		{ "Video Chess",		"V-CHESSG.BIN",		"V-CHESSC.BIN" },
		{ "TI Invaders",		"TI-InvaG.Bin",		"TI-InvaC.Bin" },
		{ "Parsec",				"PARSECG.BIN",		"PARSECC.BIN" },
		{ "Early Learning Fun", "ErLnFunG.Bin",     "" },
		{ "Music Maker",		"Mus-MakG.Bin",		"" },
		{ "Mini Memory",		"MiniMemG.Bin",		"MiniMemC.Bin" },
		{ "Editor/Assember",    "TIEAG.BIN",        "" }
	};
	
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        // unpack the passed in data
        turbo = getIntent().getBooleanExtra("com.emllabs.droid99.CONFIG_TURBO", true);
        replay = getIntent().getBooleanExtra("com.emllabs.droid99.CONFIG_REPLAY", false);
        debug = getIntent().getBooleanExtra("com.emllabs.droid99.CONFIG_DEBUG", true);
        cartridge = getIntent().getStringExtra("com.emllabs.droid99.CONFIG_CARTRIDGE");
         
        setContentView(R.layout.main);
        String[] cartridgeNames = new String[cartridgeData.length];
        for (int i=0; i < cartridgeData.length; i++)
        	cartridgeNames[i] = cartridgeData[i][0];

        cartSpinner = (Spinner) findViewById(R.id.cartridgeSpinner);
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, cartridgeNames);
        cartSpinner.setAdapter(adapter);
        
//        turboChk = (CheckBox)findViewById(R.id.turboChk);
//        replayChk = (CheckBox)findViewById(R.id.recordReplayChk);
        debugChk = (CheckBox)findViewById(R.id.debugChk);
        // NULL checks
//        turboChk.setChecked(turbo);
//        replayChk.setChecked(replay);
        debugChk.setChecked(debug);
        

        // Add button listener
        
        Button applyButton = (Button)findViewById(R.id.applyBtn);
    	applyButton.setOnClickListener(new OnClickListener() {
    		public void onClick(View v) {
    			Intent ret = new Intent();
    			int pos = cartSpinner.getSelectedItemPosition();
    			if (pos == Spinner.INVALID_POSITION) {
    				Log.i("And99", "No cartridge selected.  Using default DEMO cart");
    				pos = 0;
    			}
   				String cartGROM = cartridgeData[pos][1];
   				String cartConsole = cartridgeData[pos][2];
    			
    			ret.putExtra("com.emllabs.droid99.CONFIG_CARTRIDGE_GROM", cartGROM);
    			ret.putExtra("com.emllabs.droid99.CONFIG_CARTRIDGE_CONSOLE", cartConsole);
//    			ret.putExtra("com.emllabs.droid99.CONFIG_TURBO", turboChk.isChecked());
//    			ret.putExtra("com.emllabs.droid99.CONFIG_REPLAY", replayChk.isChecked());
    			ret.putExtra("com.emllabs.droid99.CONFIG_TURBO", false);
    			ret.putExtra("com.emllabs.droid99.CONFIG_REPLAY", false);
    			ret.putExtra("com.emllabs.droid99.CONFIG_DEBUG", debugChk.isChecked());
    			ret.putExtra("com.emllabs.droid99.CONFIG_DSKIMG", DSKPickFile);
    			setResult(RESULT_OK, ret);
    			finish();
    		}
    	});
    	
    	boolean installed = false;
    	PackageManager pm = getPackageManager();
    	try {
    		pm.getPackageInfo("org.openintents.filemanager", PackageManager.GET_ACTIVITIES);
    		installed = true;
    	} catch (PackageManager.NameNotFoundException e) { 	}
    	
    	Button loadDskButton = (Button)findViewById(R.id.ldDsk1Img);
    	Button findROMButton = (Button)findViewById(R.id.loadROMBtn);
    	findROMButton.setOnClickListener(new OnClickListener() {
    		public void onClick(View v) {
    			Intent filePicker = new Intent("org.openintents.action.PICK_FILE");
    			filePicker.setData(Uri.parse("file:///sdcard"));
    			filePicker.putExtra("org.openintents.extra.TITLE",  "Select a ROM");
    			filePicker.putExtra("org.openintents.extra.BUTTON_TEXT",  "Select");
    			try {
    				startActivityForResult(filePicker, 1);
    			} catch (ActivityNotFoundException a) {
    				Log.e("And99", "No intent.  Install the app");
    				return;
    			}
    		}
    	});
    	loadDskButton.setOnClickListener(new OnClickListener() {
    		public void onClick(View v) {
    			Intent filePicker = new Intent("org.openintents.action.PICK_FILE");
    			filePicker.setData(Uri.parse("file:///sdcard"));
    			filePicker.putExtra("org.openintents.extra.TITLE",  "Select a DSK image");
    			filePicker.putExtra("org.openintents.extra.BUTTON_TEXT",  "Select");
    			try {
    				startActivityForResult(filePicker, 2);
    			} catch (ActivityNotFoundException a) {
    				Log.e("And99", "No intent.  Install the app");
    				return;
    			}
    		}
    	});

    	findROMButton.setVisibility(installed ? View.VISIBLE : View.GONE);
    	loadDskButton.setVisibility(installed ? View.VISIBLE : View.GONE);
    	TextView pleaseInstall = (TextView)findViewById(R.id.pleaseInstall);
    	pleaseInstall.setVisibility(installed ? View.GONE : View.VISIBLE);
    }
    

    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
    	if (resultCode == RESULT_CANCELED) {
    		Log.e("And99", "File pick activity canceled");
    		return;
    	}
    	
    	String s = data.getDataString();
    	if (s.startsWith("file://"))
    		s = s.substring(7);
    	
    	if (requestCode == 1) {
    		ROMPickFile = s;
    		Log.i("And99", "ROM picked: " + ROMPickFile);
    	}
    	if (requestCode == 2) {
    		DSKPickFile = s;
    		Log.i("And99", "DSK picked: " + DSKPickFile);
    	}
    }
}
