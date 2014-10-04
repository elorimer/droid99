package com.emllabs.droid99;

import java.util.concurrent.ConcurrentLinkedQueue;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.ShortBuffer;

import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.Math;

public class TISoundGenerator implements Runnable
{
	class QueueEntry {
		float frequency;
		float attenuation;
		long timestamp;
		int generator;
	}

	class ToneGenerator
	{
		float frequency;
		float attenuation;
		float increment;
		float angle;
		int blendSample = 55;
		float blendAlpha = 0.0f;

		public ToneGenerator(int _sampleRate)
		{
			attenuation = 0.0f;
			angle = 0.0f;
			frequency = 0;
			update();
		}

		public void update()
		{
			increment = (float)(2.0f*(float)Math.PI)*frequency / sampleRate;
		}
	};


	protected ToneGenerator[] generators;
	protected int sampleRate;

	/*
	 *  Thread-safe queue to share with the CPU emulator (which calls
	 *  updateRegisters in a separate thread).  Then we process the queue
	 *  in generateSamples (called from the main thread run())
	 */
	protected ConcurrentLinkedQueue<QueueEntry> workQ;
	protected AudioTrack track;
	protected long timestamp;
	
	// This is for setOutputStream to allow us to redirect output into a file
	protected FileOutputStream stream = null;
	protected FileOutputStream queueStream = null;		// copy the queue to a file
	protected ShortBuffer shortToBytes = null;
	protected ByteBuffer sampleBytes = null;
	
	// Are we running the sound thread
	protected boolean mPaused = true;					// start paused
	protected Object waitLock = new Object();
	
	// size of the buffers we hand off to the low-level audio engine
	// This sets the latency (e.g. 2048 @ 11Khz =~ 185 ms)
	protected final int BUFFER_SIZE = 2048;
	
	// # of samples to blend between frequencies (55 samples @ 11 Khz =~ 5 ms)
	protected final int BLEND_TIME = 55;
	
	protected final int NUM_TONE_GENERATORS = 3;

	
	public TISoundGenerator(int _sampleRate)
	{
		sampleRate = _sampleRate;
		track = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate,
				               AudioFormat.CHANNEL_OUT_MONO,
				               AudioFormat.ENCODING_PCM_16BIT, BUFFER_SIZE * 2,
				               AudioTrack.MODE_STREAM);
		
		// configure the tone generators
		generators = new ToneGenerator[NUM_TONE_GENERATORS];
		for (int i=0; i < NUM_TONE_GENERATORS; i++)
			generators[i] = new ToneGenerator(_sampleRate);

		workQ = new ConcurrentLinkedQueue<QueueEntry>();
	}

	
	public int generateSamples(short[] samples, int offset)
	{
		int left = samples.length - offset;
		int index = offset;
		long now = System.nanoTime() / 1000;
		
		if (now - timestamp < 0) {
			Log.e("And99", "SoundGenerator: ahead of real-time ...");
			timestamp = now;
		}

		long RTLeft = (now - timestamp) * sampleRate / 1000000;
		left = (int)((left < RTLeft) ? left : RTLeft);

		while (left > 0) {
			QueueEntry work = workQ.peek();
			if (work == null)
				break;
			if (queueStream != null) {
				String entry = work.timestamp + "," + work.frequency + "," + work.attenuation + "\n";
				try {
					queueStream.write(entry.getBytes());
				} catch (IOException e) { Log.e("And99", "error writing to queue stream"); }
			}
			if (work.timestamp - timestamp < 0) {
				Log.i("And99", "SoundGenerator: time went backwards, catching up: " + (work.timestamp - timestamp));
				work.timestamp = timestamp;
			}
			if ((work.timestamp - timestamp) < (left * 1000000 / sampleRate)) {
				workQ.remove();
				
				long nsamples = (work.timestamp - timestamp) * sampleRate / 1000000;
				
				// generate nsamples at the old frequency/attenuation
				for (int i=0; i < nsamples; i++) {
/*
					if (generators[0].blendSample < BLEND_TIME) {
						generators[0].increment += generators[0].blendAlpha;
						generators[0].frequency = (float)(generators[0].increment * sampleRate / (2.0f * Math.PI));
						generators[0].blendSample++;
					}
*/
					samples[index+i] = 0;
					for (int g=0; g < NUM_TONE_GENERATORS; g++) {
						float s = (float)Math.sin(generators[g].angle);
						generators[g].angle += generators[g].increment;
						samples[index+i] += (short)(s * Short.MAX_VALUE * generators[g].attenuation) / NUM_TONE_GENERATORS;
					}
				}
				index += nsamples;
				left -= nsamples;
				timestamp += nsamples * 1000000 / sampleRate;

				// update to the new frequency/attenuation
				int gen = work.generator;
				generators[gen].attenuation = work.attenuation;
				generators[gen].frequency = work.frequency;
				generators[gen].update();
//				generators[0].blendAlpha = ((float)(2.0f*(float)Math.PI)*(work.frequency-generators[0].frequency) / sampleRate) / BLEND_TIME;
//				generators[0].blendSample = 0;
//				frequency  = work.frequency;
//				increment = (float)(2.0f*(float)Math.PI)*frequency / sampleRate;					
			} else
				break;
		}
	
		for (int i=index; i < left+index; i++) {
/*
			if (generators[0].blendSample < BLEND_TIME) {
				generators[0].increment += generators[0].blendAlpha;
				generators[0].frequency = (float)(generators[0].increment * sampleRate / (2.0f * Math.PI));
				generators[0].blendSample++;
			}
*/
			samples[i] = 0;
			for (int g=0; g < NUM_TONE_GENERATORS; g++) {
				float s = (float)Math.sin(generators[g].angle);
				generators[g].angle += generators[g].increment;
				samples[i] += (short)(s * Short.MAX_VALUE * generators[g].attenuation) / NUM_TONE_GENERATORS;
			}
		}
		timestamp += left * 1000000 / sampleRate;
		if (now - timestamp > 50000) {
			Log.i("And99", "SoundGenerator: falling behind (50ms) ... skipping to keep up");
			timestamp = now;
		}

		return left+index-offset;
	}


	public void writeSamples(short[] samples)
	{
		if (stream != null) {
			shortToBytes.clear();
			shortToBytes.put(samples);
			try {
				stream.write(sampleBytes.array());
			} catch (IOException e) {
				Log.e("And99", "error writing sound bytes to file");
			}
		}
		else {
			int bytesSent = track.write(samples, 0, samples.length);
			if (bytesSent < samples.length)
				Log.e("And99", "track.write short write: " + bytesSent);
		}
	}
	

	/*
	 * This is the CPU callback.
	 */
	public void updateRegisters(int generator, int frequency, int attenuation)
    {
		if (generator > (NUM_TONE_GENERATORS-1) || generator < 0)
			return;

		QueueEntry entry = new QueueEntry();
		entry.attenuation = (15 - attenuation) / 15.0f;
		entry.frequency = 111860.8f / frequency;
		entry.timestamp = System.nanoTime() / 1000;
		entry.generator = generator;
		workQ.offer(entry);
    }


	/*
	 * Main method and thread entry point.  Loops generating samples and sending
	 * them to the output (file or AudioTrack)
	 */
	public void run()
	{
		short[] samples = new short[BUFFER_SIZE];

		// wait for the initial start
		synchronized (waitLock) {
			while (mPaused) {
				try {
					waitLock.wait();
				} catch (InterruptedException e) { }
			}
		}
		
		// run forever (but can be paused)
		while (true) {
			int offset = 0;
			/*
			 * generateSamples returns the number of samples it generated
			 * This can be less than the amount requested if we get ahead
			 * of real-time.  In that case, we yield and try again later.
			 */
			while (offset < samples.length) {
				offset += generateSamples(samples, offset);
				Thread.yield();
			}
			writeSamples(samples);
			
			synchronized(waitLock)
			{
				while (mPaused) {
					try {
						waitLock.wait();
					} catch (InterruptedException e) { }
				}
			}
		}
	}
	
	
	public void pause()
	{
		synchronized (waitLock) {
			mPaused = true;
			track.stop();
		}
	}
	
	public void resume()
	{
		synchronized (waitLock) {
			mPaused = false;
			track.play();
			timestamp = System.nanoTime() / 1000;
			waitLock.notifyAll();
		}
	}
	

	/*
	 * Set an output stream to send audio samples into instead of AudioTrack
	 * Mostly for debugging
	 */
	public void setOutputStream(FileOutputStream os)
	{
		stream = os;
		sampleBytes = ByteBuffer.allocate(BUFFER_SIZE * 2);
		shortToBytes = sampleBytes.asShortBuffer();
		track.stop();
	}


	public void setOutputQueue(FileOutputStream os)
	{
		queueStream = os;
	}
}
