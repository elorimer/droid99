package com.emllabs.droid99;

import java.nio.ByteBuffer;

import android.util.Log;

public class TI99Simulator implements Runnable
{
	public ByteBuffer vdpMemory;
	static final int VDP_MEMORY_SIZE = 16384;
	
	// synchronization
	protected boolean debugBreak;
	protected Object breakLock = new Object();
//	protected boolean done;

//	protected boolean running;

	public boolean replay;
	public int debugPC;
	protected SocketDebugBridge debugger = null;
	protected TISoundGenerator SoundEngine = null;
	public volatile boolean singleStepGPL = false;
	public int debugGROM;
	public byte[] cartGROM = null, cartRAM = null, consoleGROM = null;
	
	public int[] GPLCounts;
	
	public TI99Simulator(byte[] ROM, byte[] GROM)
	{
		replay = false;
		
		// initialize VDP memory
		vdpMemory = ByteBuffer.allocateDirect(VDP_MEMORY_SIZE);
		SetVDPMemory(vdpMemory, VDP_MEMORY_SIZE);
        int success = CreateCPU(ROM, GROM, replay);
        // if success != 1 throw something
        
//        running = true;
//        done = false;
        debugBreak = false;		// set this to break into the debugger
        debugPC = 0x8000;	    // never never land
        debugGROM = 0;

        GPLCounts = new int[32768+0x6000];
        for (int i=0; i < 32768+0x6000; i++)
        	GPLCounts[i] = 0;
        
        consoleGROM = GROM;
	}
	
	public void attachDebugger(SocketDebugBridge d)
	{
		debugger = d;
	}
	
	public void detachDebugger()
	{
		debugger = null;
	}
	
	public void SetCartridge(byte[] grom, byte[] ram)
	{
		if (grom != null) {
			LoadCartGROM(grom);
			cartGROM = grom;
		}
		if (ram != null) {
			LoadCartRAM(ram);
			cartRAM = ram;
		}
	}
	
	public void breakCPU()
	{
		synchronized (breakLock) {
			debugBreak = true;
			if (SoundEngine != null)
				SoundEngine.pause();
		}
	}
	
	public void continueCPU()
	{
		synchronized (breakLock) {
			debugBreak = false;
			if (SoundEngine != null)
				SoundEngine.resume();
			breakLock.notifyAll();
		}
	}
	
	public boolean isDebugPaused()
	{
		synchronized (breakLock) {
			return debugBreak;
		}
	}
	
	public void setBreakpoint(int pc)
	{
		debugPC = pc >> 1;
	}
	
	public void DoVDPInterrupt()
	{
		if (!isDebugPaused())
			VDPInterrupt();
	}
	
	public void registerSoundGenerator(TISoundGenerator soundObj)
	{
		SoundEngine = soundObj;
		SetupSound(soundObj);
	}
	
	/*
	public void stop()
	{
		running = false;

		// spin waiting for it
		while (!done) {
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) { }
		}
		
		if (SoundEngine != null)
			SoundEngine.pause();
	}
	*/
	
	public void reset()
	{
		// assert that we only call this when we're paused
		ResetCPU();
//		running = true;
		SoundEngine.resume();
	}
	
	
	public void run()
	{
//		done = false;
		long lastRunTSC = System.nanoTime() / 333;		// 3 MHz = 333 ns/cycle
		
		while (true) {
			synchronized (breakLock) {
				while (debugBreak) {
					try {
						breakLock.wait();
						// start the clock again with ~ 0 cycles
						lastRunTSC = System.nanoTime() / 333;
					} catch (InterruptedException e) { }
				}
			}
			int cyclesToRun = (int)(System.nanoTime() / 333 - lastRunTSC);

			// max 120 Hz @ 3 MHz
			if (cyclesToRun < 25000) {
				Thread.yield();
				continue;
			}
			int cyclesLeft = RunCPU(debugPC, cyclesToRun);
			
			// credit whatever cycles are remaining to the next period
			lastRunTSC = System.nanoTime() / 333 - cyclesLeft;
			
			// we stopped with positive cycles left.  This is either a debug
			// breakpoint (debugPC) or an exception in the CPU emulator
			if (cyclesLeft > 0) {
				// let the debugger handle it if there is one attached
				if (debugger != null) {
					synchronized (breakLock) {
						debugBreak = true;
					}
					debugger.trap();
				}
				else {
					String regs = DumpRegisters();
					Log.i("And99", "CPU stuck:");
					Log.i("And99", regs);
					break;
				}
			}
			/* else if (singleStepGPL && debugger != null) {
				debugBreak = true;
				debugger.trap();
			} else if (debugger != null && debugGROM > 0 && debugGROM == GetGROMAddress()) {
				debugBreak = true;
				debugger.trap();
			}
			*/
			lastRunTSC += StepCPU();
//			GPLCounts[GetGROMAddress()]++;
			Thread.yield();
		}
		
//		done = true;
	}
	
    protected native int StepCPU();
    protected native int RunCPU(int breakpoint, int cycles);
    protected native String DumpRegisters();
    protected native void SetVDPMemory(ByteBuffer buffer, int size);
    protected native int CreateCPU(byte[] rom, byte[] grom, boolean replay);
    protected native void ResetCPU();
    protected native void LoadCartGROM(byte[] data);
    protected native void LoadCartRAM(byte[] data);
    protected native void LoadFDCROM(byte[] data);
    protected native void VDPInterrupt();
    protected native void SaveImage();
    protected native void LoadImage();
    protected native void LoadDSKImage(String fname);
    protected native void ClearException();
    protected native int GetGROMAddress();
    protected native int GetGROMByte();
    protected native byte[] ReadCPUMemory();
    protected native void SetupSound(Object o);
    protected native byte[] GetVDPRegisters();
    protected native void ClearVDPDirty();
    protected native int[] GetICounts();
    protected native void ResetICounts();
    protected native void TIKeyboardChange(int row, int col, int state);
    
    protected native int[] GetVDPCounts();
    protected native void ClearVDPCounts();
	
    static {
    	System.loadLibrary("TMS9900-JNI");
    }
}
