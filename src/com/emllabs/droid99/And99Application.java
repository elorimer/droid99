package com.emllabs.droid99;

//import java.io.File;
//import java.io.FileNotFoundException;
//import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import android.app.Application;
import android.content.res.AssetManager;
import android.util.Log;

public class And99Application extends Application
{
	public 	TI99Simulator simulator;
	public SocketDebugBridge debugBridge;
	public Thread simSound;
	
	@Override
	public void onCreate()
	{
		super.onCreate();

        byte[] buffer = new byte[8192];
        byte[] fdcbuffer = new byte[8192];
        byte[] cbuffer = new byte[33024];
        byte[] gbuffer = new byte[24576];

        try {
            AssetManager assetMgr = getAssets();
            InputStream ios = assetMgr.open("994AROM.BIN");
            ios.read(buffer);
            ios = assetMgr.open("994AGROM.BIN");
            ios.read(gbuffer);
            ios = assetMgr.open("DEMOG.BIN");
            ios.read(cbuffer);
            ios = assetMgr.open("FDC-ROM.BIN");
            ios.read(fdcbuffer);
        } catch (IOException i) {
        	Log.i("And99", "I/O exception reading asset file");
        }
        
        simulator = new TI99Simulator(buffer, gbuffer);
        simulator.SetCartridge(cbuffer, null);
        simulator.LoadFDCROM(fdcbuffer);
        Thread simThread = new Thread(simulator);
//        simulator.breakCPU();			// start paused and wait for the debugger
        simThread.start();
        
        debugBridge = new SocketDebugBridge(1234, simulator, getApplicationContext());
        Thread debugThread = new Thread(debugBridge);
        debugThread.start();

//        FileOutputStream soundFile;
        TISoundGenerator soundChip = new TISoundGenerator(11025);

/*
        try {
        	Log.i("And99", "putting the sound file in: " + getExternalFilesDir(null));
			File f = new File(getExternalFilesDir(null), "sound.dat");
			soundFile = new FileOutputStream(f);
//	        soundChip.setOutputStream(soundFile);
		} catch (FileNotFoundException e1) {
			Log.e("And99", "Failed to open output file for sound: sound.dat");
		}
*/
        simSound = new Thread(soundChip);
        simulator.registerSoundGenerator(soundChip);
        simSound.start();
        soundChip.resume();
	}
}
